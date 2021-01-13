#include "REWorker.h"

#include "MaterialShared.h"

#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionReflectionVectorWS.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureSampleParameterCube.h"

#include "Misc/FileHelper.h"
#include "Misc/ScopedSlowTask.h"
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "PackageTools.h"
#include "Engine/TextureCube.h"
#include "Engine/StaticMeshActor.h"

namespace
{
  // Value separator in RE dumps. Must match RE implementation.
  const TCHAR* VSEP = TEXT("\t");

  inline void FixObjectName(FString& Name)
  {
    Name.RemoveFromStart(TEXT("/"));
    Name.RemoveFromStart(TEXT("Content/"));
    Name.StartsWith(TEXT("Game/")) == true ? Name.InsertAt(0, TEXT("/")) : Name.InsertAt(0, TEXT("/Game/"));
  }

  template <typename T>
  T* FindResource(FString Name)
  {
    FixObjectName(Name);
    // FindObject for some reasons doesn't like untyped search. Use LoadObject<T>() instead. 
    return LoadObject<T>(nullptr, *Name);
  }

  template <typename T>
  T* CreateAsset(FString Name, UFactory* Factory)
  {
    FixObjectName(Name);

    FString PackageName;
    IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

    AssetTools.CreateUniqueAssetName(Name, TEXT(""), PackageName, Name);
    
    if (T* Asset = Cast<T>(AssetTools.CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), T::StaticClass(), Factory)))
    {
      FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
      AssetRegistryModule.Get().AssetCreated(Asset);
      return Asset;
    }
    
    return nullptr;
  }
}

struct RMaterial {
  FString Name;
  FString Class;
  FString ParentName;

  TMap<FString, bool> BoolParameters;
  TMap<FString, float> ScalarParameters;
  TMap<FString, FString> TextureParameters;
  TMap<FString, FString> TextureAParameters;
  TMap<FString, FLinearColor> VectorParameters;

  bool TwoSided = false;

  RMaterial* Parent = nullptr;
  UObject* UnrealMaterial = nullptr;

  // Serialize from RE dump. Modifies Idx. Returns false on error.
  bool ReadFromArray(const TArray<FString>& Lines, int32& Idx)
  {
    const FString& Line = Lines[Idx];
    if (!Line.Len())
    {
      return false;
    }

    int32 Pos1 = Line.Find(TEXT(" "));
    int32 Pos2 = 0;

    if (Pos1 == INDEX_NONE)
    {
      return false;
    }

    Class = Line.Mid(0, Pos1);

    if (Class == "Material")
    {
      ParentName.Empty();
      Name = Line.Mid(Pos1 + 1);
    }
    else
    {
      Pos2 = Line.Find(TEXT(" "), ESearchCase::IgnoreCase, ESearchDir::FromStart, Pos1 + 1);
      if (Pos2 == INDEX_NONE)
      {
        return false;
      }

      ParentName = Line.Mid(Pos1 + 1, Pos2 - Pos1 - 1);
      Name = Line.Mid(Pos2 + 1);
    }

    while (++Idx < Lines.Num())
    {
      if (!Lines[Idx].StartsWith(TEXT(" ")))
      {
        break;
      }
      FString Trimmed = Lines[Idx].TrimStartAndEnd();
      if (Trimmed == TEXT("TwoSided"))
      {
        TwoSided = true;
        continue;
      }

      Pos1 = Trimmed.Find(VSEP);
      if (Pos1 == INDEX_NONE)
      {
        break;
      }

      Pos2 = Trimmed.Find(VSEP, ESearchCase::IgnoreCase, ESearchDir::FromStart, Pos1 + 1);
      if (Pos2 == INDEX_NONE)
      {
        break;
      }

      FString Type = Trimmed.Mid(0, Pos1);
      FString Parameter = Trimmed.Mid(Pos1 + 1, Pos2 - Pos1 - 1);
      if (Type == TEXT("Texture"))
      {
        FString Value = Trimmed.Mid(Pos2 + 1);
        TextureParameters.Add(Parameter, Value);
      }
      else if (Type == TEXT("TextureA"))
      {
        FString Value = Trimmed.Mid(Pos2 + 1);
        TextureParameters.Add(Parameter, Value);
        TextureAParameters.Add(Parameter + TEXT("_Alpha"), Value + TEXT("_Alpha"));
      }
      else if (Type == TEXT("Scalar"))
      {
        FString Value = Trimmed.Mid(Pos2 + 1);
        float V = FCString::Atof(*Value);
        ScalarParameters.Add(Parameter, V);
      }
      else if (Type == TEXT("Bool"))
      {
        FString Value = Trimmed.Mid(Pos2 + 1);
        bool V = FCString::ToBool(*Value);
        BoolParameters.Add(Parameter, V);
      }
      else if (Type == TEXT("Vector"))
      {
        Pos1 = Pos2;
        Pos2 = Trimmed.Find(VSEP, ESearchCase::IgnoreCase, ESearchDir::FromStart, Pos1 + 1);
        if (Pos2 == INDEX_NONE)
        {
          break;
        }
        FString Value = Trimmed.Mid(Pos1 + 1, Pos2 - Pos1 - 1);
        float R = FCString::Atof(*Value);
        Pos1 = Pos2;
        Pos2 = Trimmed.Find(VSEP, ESearchCase::IgnoreCase, ESearchDir::FromStart, Pos1 + 1);
        if (Pos2 == INDEX_NONE)
        {
          break;
        }
        Value = Trimmed.Mid(Pos1 + 1, Pos2 - Pos1 - 1);
        float G = FCString::Atof(*Value);
        Pos1 = Pos2;
        Pos2 = Trimmed.Find(VSEP, ESearchCase::IgnoreCase, ESearchDir::FromStart, Pos1 + 1);
        if (Pos2 == INDEX_NONE)
        {
          break;
        }
        Value = Trimmed.Mid(Pos1 + 1, Pos2 - Pos1 - 1);
        float B = FCString::Atof(*Value);
        Value = Trimmed.Mid(Pos2 + 1);
        float A = FCString::Atof(*Value);
        VectorParameters.Add(Parameter, FLinearColor(R, G, B, A));
      }
    }
    Idx--;
    return true;
  }
};

struct RTexture {
  FString Name;
  FString Compression;
  FString Source;
  bool SRGB = false;
  bool IsDXT = false;

  bool ReadFromLine(const FString& Line)
  {
    int32 Pos1 = Line.Find(VSEP);
    if (Pos1 == INDEX_NONE)
    {
      return false;
    }
    Compression = Line.Mid(0, Pos1);
    uint32 Pos2 = Line.Find(VSEP, ESearchCase::IgnoreCase, ESearchDir::FromStart, Pos1 + 1);
    if (Pos2 == INDEX_NONE)
    {
      return false;
    }
    {
      FString Value = Line.Mid(Pos1 + 1, Pos2 - Pos1 - 1);
      SRGB = FCString::ToBool(*Value);
    }
    Pos1 = Pos2;
    Pos2 = Line.Find(VSEP, ESearchCase::IgnoreCase, ESearchDir::FromStart, Pos1 + 1);
    if (Pos2 == INDEX_NONE)
    {
      return false;
    }
    {
      FString Value = Line.Mid(Pos1 + 1, Pos2 - Pos1 - 1);
      IsDXT = FCString::ToBool(*Value);
    }
    Pos1 = Pos2;
    Pos2 = Line.Find(VSEP, ESearchCase::IgnoreCase, ESearchDir::FromStart, Pos1 + 1);
    if (Pos2 == INDEX_NONE)
    {
      return false;
    }
    Name = Line.Mid(Pos1 + 1, Pos2 - Pos1 - 1);
    Pos1 = Pos2;
    Source = Line.Mid(Pos1 + 1);
    return Name.Len() > 0 && Source.Len() > 0;
  }
};

int32 REWorker::ImportMaterials(const FString& Path, FString& OutError)
{
  TArray<FString> Lines;
  FFileHelper::LoadFileToStringArray(Lines, *Path);
  if (!Lines.Num())
  {
    OutError = TEXT("The file appears to be empty!");
    return -1;
  }

  TArray<RMaterial> Materials;
  for (int32 Idx = 0; Idx < Lines.Num(); ++Idx)
  {
    if (Lines[Idx].StartsWith(TEXT(" ")))
    {
      // If RMaterial::ReadFromArray failed Idx may not be correct. Find next entry
      continue;
    }

    RMaterial Material;
    int32 StartIdx = Idx;
    if (!Material.ReadFromArray(Lines, Idx))
    {
      UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to parse material entry at line %d"), StartIdx + 1);
      OutError = TEXT("Some errors occured. See the Output Log for details.");
      continue;
    }
    Materials.Add(Material);
  }

  if (!Materials.Num())
  {
    OutError = TEXT("Failed to parse the file. Make sure it's not corrupted.");
    return -1;
  }

  FScopedSlowTask Task((float)Materials.Num(), NSLOCTEXT("REHelper", "ImportMaterials", "Importing materials..."));
  Task.MakeDialog();

  // Link parents and create master materials
  int32 Result = 0;
  UMaterialFactoryNew* MatFactory = NewObject<UMaterialFactoryNew>();
  for (RMaterial& Material : Materials)
  {
    if (!Material.ParentName.Len())
    {
      Task.EnterProgressFrame(1.f, FText::FromString(TEXT("Importing: ") + Material.Name));
      // Material is a MasterMaterial. Check if it does not exist and create it.
      UMaterial* Asset = FindResource<UMaterial>(Material.Name);
      if (!Asset)
      {
        Asset = CreateAsset<UMaterial>(Material.Name, MatFactory);
        if (Asset)
        {
          Result++;
          bool Error = false;
          SetupMasterMaterial(Asset, &Material, Error);
          if (Error && !OutError.Len())
          {
            OutError = TEXT("Some errors occured. See the Output Log for details.");
          }
        }
        else
        {
          UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to find/create Master Material \"%s\""), *Material.Name);
          OutError = TEXT("Some errors occured. See the Output Log for details.");
        }
      }
      Material.UnrealMaterial = Asset;
      continue;
    }

    Material.Parent = Materials.FindByPredicate([&](const RMaterial& a) {
      return a.Name == Material.ParentName;
    });

    if (!Material.Parent)
    {
      UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to find parent material \"%s\" for \"%s\". For some reasons Real Editor didn't include it in the dump file."), *Material.ParentName, *Material.Name);
      OutError = TEXT("Some errors occurred. See the Output Log for details.");
    }
  }

  // Create Material Instances. Skip Mater Materials.
  bool AnyMiErrors = false;
  UMaterialInstanceConstantFactoryNew* MiFactory = NewObject<UMaterialInstanceConstantFactoryNew>();
  for (RMaterial& Material : Materials)
  {
    if (!Material.ParentName.Len() || !Material.Parent)
    {
      // Master Material
      continue;
    }
    Task.EnterProgressFrame(1.f, FText::FromString(TEXT("Importing: ") + Material.Name));
    Result += CreateMaterialInstance(&Material, MiFactory, AnyMiErrors);
  }
  if (AnyMiErrors)
  {
    OutError = TEXT("Some errors occurred. See the Output Log for details.");
  }
  return Result;
}

int32 REWorker::AssignDefaultMaterials(const FString& Path, FString& OutError)
{
  TArray<FString> Items;
  FFileHelper::LoadFileToStringArray(Items, *Path);
  if (!Items.Num())
  {
    OutError = TEXT("The file appears to be empty!");
    return -1;
  }

  FScopedSlowTask Task((float)Items.Num(), NSLOCTEXT("REHelper", "AssigningMaterials", "Assigning defaults..."));
  Task.MakeDialog();

  int32 Result = 0;
  for (int32 Idx = 0; Idx < Items.Num(); ++Idx)
  {
    Task.EnterProgressFrame(1.f);
    if (Items[Idx].StartsWith(TEXT(" ")))
    {
      continue;
    }
    FString Name = TEXT("/") + Items[Idx];
    UObject* Asset = FindResource<UObject>(*Name);

    if (!Asset)
    {
      UE_LOG(LogTemp, Error, TEXT("RE Helper: Coudn't find asset %s"), *Name);
      OutError = TEXT("Some errors occured. See the Output Log for details.");
      continue;
    }

    TArray<UMaterialInterface*> Defaults;
    TArray<UMaterialInterface*> Leafs;
    while (++Idx < Items.Num())
    {
      if (!Items[Idx].StartsWith(TEXT(" ")))
      {
        break;
      }
      FString MaterialName = Items[Idx].TrimStartAndEnd();
      if (MaterialName == TEXT("None"))
      {
        Defaults.Add(nullptr);
        continue;
      }
      UMaterialInterface* Material = FindResource<UMaterialInterface>(MaterialName);
      if (!Material)
      {
        UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to find Material \"%s\" for object \"%s\""), *MaterialName, *Name);
        OutError = TEXT("Some errors occured. See the Output Log for details.");
      }
      if (MaterialName.EndsWith(TEXT("_leafs")))
      {
        Leafs.Add(Material);
      }
      else
      {
        Defaults.Add(Material);
      }
    }
    Idx--;

    bool AnyChanges = false;
    if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
    {
      FString MeshName;
      StaticMesh->GetName(MeshName);
      int32 MatIdx = 0;

      // Handle SpeedTrees separately
      if (Leafs.Num())
      {
        for (UMaterialInterface* Material : Defaults)
        {
          if (!Material)
          {
            continue;
          }
          FString MatName;
          Material->GetName(MatName);
          for (; MatIdx < StaticMesh->StaticMaterials.Num(); ++MatIdx)
          {
            FString SlotName = StaticMesh->StaticMaterials[MatIdx].MaterialSlotName.ToString();
            if (!SlotName.EndsWith(TEXT("_leafs")))
            {
              StaticMesh->StaticMaterials[MatIdx].MaterialInterface = Material;
              AnyChanges = true;
              MatIdx++;
              break;
            }
          }
        }

        MatIdx = 0;
        for (UMaterialInterface* Material : Leafs)
        {
          FString MatName;
          Material->GetName(MatName);
          for (; MatIdx < StaticMesh->StaticMaterials.Num(); ++MatIdx)
          {
            FString SlotName = StaticMesh->StaticMaterials[MatIdx].MaterialSlotName.ToString();
            if (SlotName.EndsWith(TEXT("_leafs")))
            {
              StaticMesh->StaticMaterials[MatIdx].MaterialInterface = Material;
              AnyChanges = true;
              MatIdx++;
              break;
            }
          }
        }
      }
      else
      {
        // Regular static mesh
        for (; MatIdx < StaticMesh->StaticMaterials.Num(); ++MatIdx)
        {
          if (MatIdx < Defaults.Num() && Defaults[MatIdx])
          {
            StaticMesh->StaticMaterials[MatIdx].MaterialInterface = Defaults[MatIdx];
            AnyChanges = true;
          }
        }
      }
    }
    else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Asset))
    {
      
      for (int32 MatIdx = 0; MatIdx < SkeletalMesh->Materials.Num(); ++MatIdx)
      {
        if (MatIdx < Defaults.Num() && Defaults[MatIdx])
        {
          SkeletalMesh->Materials[MatIdx].MaterialInterface = Defaults[MatIdx];
          AnyChanges = true;
        }
      }
    }

    if (AnyChanges)
    {
      Result++;
      Asset->PostEditChange();
    }
  }
  return Result;
}

int32 REWorker::FixTextures(const FString& Path, FString& OutError)
{
  TArray<FString> Lines;
  FFileHelper::LoadFileToStringArray(Lines, *Path);
  

  TArray<RTexture> Textures;
  for (const FString& Line : Lines)
  {
    RTexture Texture;
    if (Texture.ReadFromLine(Line))
    {
      Textures.Add(Texture);
    }
  }

  if (!Textures.Num())
  {
    OutError = TEXT("The file appears to be empty!");
    return -1;
  }

  FScopedSlowTask Task((float)Textures.Num(), NSLOCTEXT("REHelper", "FixTextures", "Fixing textures..."));
  Task.MakeDialog();

  int32 Result = 0;
  for (const RTexture& Texture : Textures)
  {
    // TODO: import the asset?
    if (UTexture* Asset = FindResource<UTexture>(Texture.Name))
    {
      if (Texture.Compression == TEXT("TC_Grayscale"))
      {
        Asset->CompressionSettings = TC_Grayscale;
        Asset->SRGB = false;
      }
      else if (Texture.Compression.StartsWith(TEXT("TC_Normalmap")))
      {
        Asset->CompressionSettings = TC_Normalmap;
        Asset->SRGB = false;
      }
      else if (!Texture.IsDXT)
      {
        Asset->CompressionSettings = TC_Masks;
        Asset->SRGB = false;
      }
      else
      {
        Asset->CompressionSettings = TC_Default;
        Asset->SRGB = Texture.SRGB;
      }
      Asset->PostEditChange();
      Asset->GetPackage()->SetDirtyFlag(true);
      FAssetRegistryModule::AssetCreated(Asset);
      Task.EnterProgressFrame(1.f);
      Result++;
      continue;
    }
  }
  return Result;
}

int32 REWorker::FixSpeedTrees(const FString& Path, ULevel* Level, FString& OutError)
{
  TArray<FString> Lines;
  FFileHelper::LoadFileToStringArray(Lines, *Path);
  if (!Lines.Num())
  {
    OutError = TEXT("The file appears to be empty!");
    return -1;
  }

  int32 Result = 0;
  TMap<FString, TMap<FString, UMaterialInterface*>> MaterialMap;
  for (int32 Idx = 0; Idx < Lines.Num(); ++Idx)
  {
    if (Lines[Idx].StartsWith(TEXT(" ")))
    {
      continue;
    }
    FString Name = Lines[Idx];
    MaterialMap.Add(Name, TMap<FString, UMaterialInterface*>());
    while (++Idx < Lines.Num())
    {
      if (!Lines[Idx].StartsWith(TEXT(" ")))
      {
        break;
      }
      FString Trimmed = Lines[Idx].TrimStartAndEnd();
      int32 Pos = Trimmed.Find(VSEP);
      if (Pos == INDEX_NONE)
      {
        continue;
      }
      FString MaterialName = Trimmed.Mid(Pos + 1);
      if (MaterialName != TEXT("None"))
      {
        UMaterialInterface* Material = FindResource<UMaterialInterface>(MaterialName);
        MaterialMap[Name].Add(Trimmed.Mid(0, Pos), Material);
        if (!Material)
        {
          UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to find Material \"%s\" for actor \"%s\""), *MaterialName, *Name);
          OutError = TEXT("Some errors occured. See the Output Log for details.");
        }
      }
      else
      {
        MaterialMap[Name].Add(Trimmed.Mid(0, Pos), nullptr);
      }
    }
    Idx--;
  }

  if (!MaterialMap.Num())
  {
    OutError = TEXT("The file appears to be empty!");
    return -1;
  }

  for (const auto& ActorEntry : MaterialMap)
  {
    for (AActor* UntypedActor : Level->Actors)
    {
      if (!UntypedActor || UntypedActor->GetActorLabel() != ActorEntry.Key)
      {
        continue;
      }
      if (AStaticMeshActor* Actor = Cast<AStaticMeshActor>(UntypedActor))
      {
        if (UStaticMeshComponent* Component = Actor->GetStaticMeshComponent())
        {
          TArray<FName> SlotNames = Component->GetMaterialSlotNames();
          for (const auto& ActorMaterialInfo : ActorEntry.Value)
          {
            for (const FName& Slot : SlotNames)
            {
              if (Slot.ToString().EndsWith(ActorMaterialInfo.Key))
              {
                Component->SetMaterialByName(Slot, ActorMaterialInfo.Value);
              }
            }
          }
          Component->PostEditChange();
          Result++;
        }
      }
    }
  }
  return Result;
}

void REWorker::SetupMasterMaterial(UMaterial* UnrealMaterial, RMaterial* RealMaterial, bool& Error)
{
  if (!UnrealMaterial)
  {
    return;
  }

  UnrealMaterial->TwoSided = RealMaterial->TwoSided;

  int32 PosX = 240;
  bool PosY = false;

  auto GetPosX = [&](int32 Step = 240) {
    int32 RetVal = PosX;
    PosX += Step;
    return -RetVal;
  };

  auto GetPosY = [&](int32 Y) {
    int32 RetVal = PosY ? Y : -Y;
    PosY = !PosY;
    return RetVal;
  };

  FExpressionInput* Input = nullptr;

  FString SkipDiffuseParameter;
  for (const auto& P : RealMaterial->TextureParameters)
  {
    if (P.Key == TEXT("DiffuseMap"))
    {
      UTexture* Texture = nullptr;
      if (P.Value.Len() && P.Value != TEXT("None"))
      {
        Texture = FindResource<UTexture>(P.Value);
      }
      if (!Texture)
      {
        break;
      }

      SkipDiffuseParameter = P.Key;

      if (Cast<UTextureCube>(Texture))
      {
        // Cube as a BaseColor? I don't think so.
        continue;
      }
      UMaterialExpressionAdd* Root = NewObject<UMaterialExpressionAdd>(UnrealMaterial);
      UnrealMaterial->Expressions.Add(Root);
      Root->Desc = TEXT("Delete me!");
      Root->bCommentBubbleVisible = true;

      UnrealMaterial->BaseColor.Expression = Root;
      Root->MaterialExpressionEditorX = GetPosX();

      UMaterialExpressionTextureSampleParameter2D* Param = NewObject<UMaterialExpressionTextureSampleParameter2D>(UnrealMaterial);
      UnrealMaterial->Expressions.Add(Param);

      Param->MaterialExpressionEditorX = -PosX;
      Param->MaterialExpressionEditorY = GetPosY(324);
      Param->ParameterName = *P.Key;
      Param->Texture = Texture;
      if (Texture->IsNormalMap())
      {
        Param->SamplerType = SAMPLERTYPE_Normal;
      }
      else if (Texture->CompressionSettings == TC_Grayscale)
      {
        Param->SamplerType = SAMPLERTYPE_Grayscale;
      }
      Root->A.Expression = Param;

      UMaterialExpressionMultiply* Mul = NewObject<UMaterialExpressionMultiply>(UnrealMaterial);
      UnrealMaterial->Expressions.Add(Mul);
      Mul->MaterialExpressionEditorX = GetPosX();
      Mul->Desc = TEXT("Delete me!");
      Mul->bCommentBubbleVisible = true;
      Root->B.Expression = Mul;
      Input = &Mul->A;

      UMaterialExpressionConstant* B = NewObject<UMaterialExpressionConstant>(UnrealMaterial);
      UnrealMaterial->Expressions.Add(B);
      B->Desc = TEXT("Delete me!");
      B->bCommentBubbleVisible = true;
      B->MaterialExpressionEditorY = 60;
      B->MaterialExpressionEditorX = GetPosX();
      B->R = 0.f;
      Mul->B.Expression = B;
      break;
    }
  }

  if (!Input)
  {
    UMaterialExpressionClamp* RootClamp = NewObject<UMaterialExpressionClamp>(UnrealMaterial);
    UnrealMaterial->Expressions.Add(RootClamp);
    UnrealMaterial->BaseColor.Expression = RootClamp;
    RootClamp->MaterialExpressionEditorX = GetPosX();
    RootClamp->Desc = TEXT("Delete me!");
    RootClamp->bCommentBubbleVisible = true;

    Input = &RootClamp->Input;
  }
  UMaterialExpressionReflectionVectorWS* ReflectionVector = nullptr;

  for (const auto& P : RealMaterial->TextureParameters)
  {
    if (SkipDiffuseParameter.Len() && P.Key == SkipDiffuseParameter)
    {
      continue;
    }

    UTexture* Texture = nullptr;
    if (P.Value.Len() && P.Value != TEXT("None"))
    {
      Texture = FindResource<UTexture>(P.Value);
    }

    if (!Texture)
    {
      Error = true;
      UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to create a Texture Parameter \"%s\" for Material \"%s\""), *P.Key, *RealMaterial->Name);
      continue;
    }

    UMaterialExpressionAdd* Add = NewObject<UMaterialExpressionAdd>(UnrealMaterial);
    UnrealMaterial->Expressions.Add(Add);
    Add->Desc = TEXT("Delete me!");
    Add->bCommentBubbleVisible = true;
    Add->MaterialExpressionEditorX = GetPosX();
    Input->Expression = Add;
    Input = &Add->A;

    UMaterialExpressionTextureSampleParameter* Param = nullptr;
    if (Cast<UTextureCube>(Texture))
    {
      Param = NewObject<UMaterialExpressionTextureSampleParameterCube>(UnrealMaterial);
      if (!ReflectionVector)
      {
        ReflectionVector = NewObject<UMaterialExpressionReflectionVectorWS>(UnrealMaterial);
        UnrealMaterial->Expressions.Add(ReflectionVector);
      }
      Param->Coordinates.Expression = ReflectionVector;
    }
    else
    {
      Param = NewObject<UMaterialExpressionTextureSampleParameter2D>(UnrealMaterial);
    }
    UnrealMaterial->Expressions.Add(Param);

    Param->MaterialExpressionEditorX = -PosX;
    Param->MaterialExpressionEditorY = GetPosY(324);
    Param->ParameterName = *P.Key;
    
    Input->Expression = Param;
    Input = &Add->B;
    
    Param->Texture = Texture;
    if (Texture->IsNormalMap())
    {
      Param->SamplerType = SAMPLERTYPE_Normal;
    }
    else if (Texture->CompressionSettings == TC_Grayscale)
    {
      Param->SamplerType = Texture->SRGB ? SAMPLERTYPE_Grayscale : SAMPLERTYPE_LinearGrayscale;
    }
    else if (Texture->CompressionSettings == TC_Masks)
    {
      Param->SamplerType = SAMPLERTYPE_Masks;
    }
    else
    {
      Param->SamplerType = Texture->SRGB ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
    }
  }

  if (ReflectionVector)
  {
    ReflectionVector->MaterialExpressionEditorX = -(PosX + 240);
    ReflectionVector->MaterialExpressionEditorY = -512;
  }

  for (const auto& P : RealMaterial->TextureAParameters)
  {
    UTexture* Texture = nullptr;
    if (P.Value.Len() && P.Value != TEXT("None"))
    {
      Texture = FindResource<UTexture>(P.Value);
    }

    if (!Texture)
    {
      Error = true;
      UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to create a Texture Parameter \"%s\" for Material \"%s\""), *P.Key, *RealMaterial->Name);
      continue;
    }

    UMaterialExpressionAdd* Add = NewObject<UMaterialExpressionAdd>(UnrealMaterial);
    UnrealMaterial->Expressions.Add(Add);
    Add->Desc = TEXT("Delete me!");
    Add->bCommentBubbleVisible = true;
    Add->MaterialExpressionEditorX = GetPosX();
    Input->Expression = Add;
    Input = &Add->A;

    UMaterialExpressionTextureSampleParameter* Param = NewObject<UMaterialExpressionTextureSampleParameter2D>(UnrealMaterial);
    UnrealMaterial->Expressions.Add(Param);

    Param->MaterialExpressionEditorX = -PosX;
    Param->MaterialExpressionEditorY = GetPosY(324);
    Param->ParameterName = *P.Key;

    Input->Expression = Param;
    Input = &Add->B;

    Param->Texture = Texture;
    Param->SamplerType = Texture->SRGB ? SAMPLERTYPE_Grayscale : SAMPLERTYPE_LinearGrayscale;
  }

  for (const auto& P : RealMaterial->ScalarParameters)
  {
    UMaterialExpressionAdd* Add = NewObject<UMaterialExpressionAdd>(UnrealMaterial);
    UnrealMaterial->Expressions.Add(Add);
    Add->Desc = TEXT("Delete me!");
    Add->bCommentBubbleVisible = true;
    Add->MaterialExpressionEditorX = GetPosX();
    Input->Expression = Add;
    Input = &Add->A;
    UMaterialExpressionScalarParameter* Param = NewObject<UMaterialExpressionScalarParameter>(UnrealMaterial);
    UnrealMaterial->Expressions.Add(Param);
    Param->MaterialExpressionEditorX = -PosX;
    Param->MaterialExpressionEditorY = GetPosY(160);
    Param->ParameterName = *P.Key;
    Param->DefaultValue = P.Value;
    Input->Expression = Param;
    Input = &Add->B;
  }
  for (const auto& P : RealMaterial->VectorParameters)
  {
    UMaterialExpressionAdd* Add = NewObject<UMaterialExpressionAdd>(UnrealMaterial);
    UnrealMaterial->Expressions.Add(Add);
    Add->MaterialExpressionEditorX = GetPosX();
    Add->Desc = TEXT("Delete me!");
    Add->bCommentBubbleVisible = true;
    Input->Expression = Add;
    Input = &Add->A;
    UMaterialExpressionVectorParameter* Param = NewObject<UMaterialExpressionVectorParameter>(UnrealMaterial);
    UnrealMaterial->Expressions.Add(Param);
    Param->MaterialExpressionEditorX = -PosX;
    Param->MaterialExpressionEditorY = GetPosY(324);
    Param->ParameterName = *P.Key;
    Param->DefaultValue = P.Value;
    Input->Expression = Param;
    Input = &Add->B;
  }

  {
    // A BaseColor plug for materials with no parameters
    UMaterialExpressionConstant* Plug = NewObject<UMaterialExpressionConstant>(UnrealMaterial);
    UnrealMaterial->Expressions.Add(Plug);
    Plug->MaterialExpressionEditorX = GetPosX();
    Plug->R = 1.f;
    Plug->Desc = TEXT("Delete me!");
    Plug->bCommentBubbleVisible = true;
    Input->Expression = Plug;
  }

  PosX = 340;
  for (const auto& P : RealMaterial->BoolParameters)
  {
    UMaterialExpressionStaticSwitchParameter* Param = NewObject<UMaterialExpressionStaticSwitchParameter>(UnrealMaterial);
    UnrealMaterial->Expressions.Add(Param);
    Param->ParameterName = *P.Key;
    Param->MaterialExpressionEditorX = GetPosX(340);
    Param->MaterialExpressionEditorY = 580;
    Param->DefaultValue = P.Value;
  }

  UnrealMaterial->PostEditChange();
  RealMaterial->UnrealMaterial = UnrealMaterial;
}

int32 REWorker::CreateMaterialInstance(RMaterial* RealMaterial, UFactory* MiFactory, bool& Error)
{
  int32 Count = 0;
  if (RealMaterial->Parent)
  {
    // Create parent Material Instances
    Count += CreateMaterialInstance(RealMaterial->Parent, MiFactory, Error);
  }
  else
  {
    // RealMaterial is a Master Materials. We've created it earlier.
    return 0;
  }

  if (RealMaterial->UnrealMaterial)
  {
    // The Material Instance has been created earlier
    return Count;
  }

  if (UMaterialInstanceConstant* Asset = FindResource<UMaterialInstanceConstant>(RealMaterial->Name))
  {
    RealMaterial->UnrealMaterial = Asset;
    return Count;
  }

  Cast<UMaterialInstanceConstantFactoryNew>(MiFactory)->InitialParent = Cast<UMaterialInterface>(RealMaterial->Parent->UnrealMaterial);
  if (UMaterialInstanceConstant* Asset = CreateAsset<UMaterialInstanceConstant>(RealMaterial->Name, MiFactory))
  {
    RealMaterial->UnrealMaterial = Asset;
    Count++;
    for (const auto& P : RealMaterial->TextureParameters)
    {
      if (!P.Value.Len() || P.Value == TEXT("None"))
      {
        continue;
      }

      if (UTexture* Texture = FindResource<UTexture>(P.Value))
      {
        FMaterialParameterInfo Info;
        Info.Name = *P.Key;
        Asset->SetTextureParameterValueEditorOnly(Info, Texture);
      }
      else
      {
        UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to find Texture \"%s\" for parameter \"%s\" of Material Instance \"%s\""), *P.Value, *P.Key, *RealMaterial->Name);
        Error = true;
      }
    }

    for (const auto& P : RealMaterial->TextureAParameters)
    {
      if (!P.Value.Len() || P.Value == TEXT("None"))
      {
        continue;
      }

      if (UTexture* Texture = FindResource<UTexture>(P.Value))
      {
        FMaterialParameterInfo Info;
        Info.Name = *P.Key;
        Asset->SetTextureParameterValueEditorOnly(Info, Texture);
      }
      else
      {
        UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to find Texture \"%s\" for parameter \"%s\" of Material Instance \"%s\""), *P.Value, *P.Key, *RealMaterial->Name);
        Error = true;
      }
    }

    for (const auto& P : RealMaterial->ScalarParameters)
    {
      FMaterialParameterInfo Info;
      Info.Name = *P.Key;
      Asset->SetScalarParameterValueEditorOnly(Info, P.Value);
    }

    for (const auto& P : RealMaterial->VectorParameters)
    {
      FMaterialParameterInfo Info;
      Info.Name = *P.Key;
      Asset->SetVectorParameterValueEditorOnly(Info, P.Value);
    }

    Asset->PostEditChange();
  }
  else
  {
    UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to find/create Material Instance Constant \"%s\""), *RealMaterial->Name);
    Error = true;
  }
  return Count;
}