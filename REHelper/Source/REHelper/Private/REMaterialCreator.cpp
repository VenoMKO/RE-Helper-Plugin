#include "REMaterialCreator.h"

#include "Misc/FileHelper.h"

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
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureSampleParameterCube.h"
#include "MaterialShared.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"

namespace
{
  const TCHAR* VSEP = TEXT("\t");
  bool ParseEntryHeader(const FString& Line, FString& OutClass, FString& OutName, FString& OutParent)
  {
    if (!Line.Len())
    {
      return false;
    }
    int32 Pos = Line.Find(TEXT(" "));
    if (Pos == INDEX_NONE)
    {
      return false;
    }
    OutClass = Line.Mid(0, Pos);

    if (OutClass == "Material")
    {
      OutParent.Empty();
      OutName = Line.Mid(Pos + 1);
      return OutName.Len() > 0;
    }

    int32 Pos2 = Line.Find(TEXT(" "), ESearchCase::IgnoreCase, ESearchDir::FromStart, Pos + 1);

    if (Pos2 == INDEX_NONE)
    {
      return false;
    }

    OutParent = Line.Mid(Pos + 1, Pos2 - Pos - 1);
    OutName = Line.Mid(Pos2 + 1);

    return OutName.Len() > 0;
  }

  template <typename T>
  T* FindResource(FString Name)
  {
    Name.RemoveFromStart(TEXT("/"));
    Name.RemoveFromStart(TEXT("Content/"));
    Name.StartsWith(TEXT("Game/")) == true ? Name.InsertAt(0, TEXT("/")) : Name.InsertAt(0, TEXT("/Game/"));
    return LoadObject<T>(nullptr, *Name);
  }
}


int32 REMaterialCreator::CreateMasterMaterials()
{
  if (!Materials.Num())
  {
    Error = TEXT("Nothing to process!");
    return -1;
  }

  int32 Counter = 0;

  UMaterialFactoryNew* MAT_Factory = NewObject<UMaterialFactoryNew>();
  for (MaterialRecord& Material : Materials)
  {
    if (Material.Parent || Material.UnrealMaterial)
    {
      continue;
    }

    bool Created = false;
    UObject* Asset = CreateMaterialAsset(Material.Name, UMaterial::StaticClass(), MAT_Factory, Created);
    if (!Asset)
    {
      UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to find/create Master Material \"%s\""), *Material.Name);
      bHasLogErrors = true;
      continue;
    }
    if (Created)
    {
      UMaterial* UnrealMat = Cast<UMaterial>(Asset);
      SetupMasterMaterial(UnrealMat, Material);
      Counter++;
    }
    Material.UnrealMaterial = Asset;
  }
  return Counter;
}

bool REMaterialCreator::AssignDefaults(const FString& Path)
{
  TArray<FString> Items;
  FFileHelper::LoadFileToStringArray(Items, *Path);
  if (!Items.Num())
  {
    return false;
  }

  for (int32 Idx = 0; Idx < Items.Num(); ++Idx)
  {
    if (Items[Idx].StartsWith(TEXT(" ")))
    {
      continue;
    }
    FString Name = TEXT("/") + Items[Idx];
    UObject* Asset = FindResource<UObject>(*Name);

    if (!Asset)
    {
      continue;
    }

    TArray<UMaterialInterface*> Defaults;
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
      }
      Defaults.Add(Material);
    }
    Idx--;
    if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
    {
      for (int32 MatIdx = 0; MatIdx < StaticMesh->StaticMaterials.Num(); ++MatIdx)
      {
        if (MatIdx < Defaults.Num() && Defaults[MatIdx])
        {
          StaticMesh->StaticMaterials[MatIdx].MaterialInterface = Defaults[MatIdx];
        }
      }
      StaticMesh->PostEditChange();
    }
  }

  return true;
}

bool REMaterialCreator::LoadList(const FString& Path)
{
  Materials.Empty();
  TArray<FString> MaterialsList;
  FFileHelper::LoadFileToStringArray(MaterialsList, *Path);
  if (!MaterialsList.Num())
  {
    Error = TEXT("MaterialList.txt is empty!");
    return false;
  }

  for (int32 Idx = 0; Idx < MaterialsList.Num(); ++Idx)
  {
    if (MaterialsList[Idx].StartsWith(TEXT(" ")))
    {
      continue;
    }
    FString Name;
    FString Class;
    FString ParentName;
    if (!ParseEntryHeader(MaterialsList[Idx], Class, Name, ParentName))
    {
      continue;
    }

    MaterialRecord Record;
    Record.Name = Name;
    Record.Class = Class;
    Record.ParentName = ParentName;
    while (++Idx < MaterialsList.Num())
    {
      if (!MaterialsList[Idx].StartsWith(TEXT(" ")))
      {
        break;
      }
      FString Trimmed = MaterialsList[Idx].TrimStartAndEnd();
      if (Trimmed == TEXT("bDoubleSided"))
      {
        Record.bIsDoubleSided = true;
        continue;
      }
      int32 Pos1 = Trimmed.Find(VSEP);
      if (Pos1 == INDEX_NONE)
      {
        break;
      }
      int32 Pos2 = Trimmed.Find(VSEP, ESearchCase::IgnoreCase, ESearchDir::FromStart, Pos1 + 1);
      if (Pos2 == INDEX_NONE)
      {
        break;
      }

      FString Type = Trimmed.Mid(0, Pos1);
      FString Parameter = Trimmed.Mid(Pos1 + 1, Pos2 - Pos1 - 1);
      if (Type == TEXT("Texture"))
      {
        FString Value = Trimmed.Mid(Pos2 + 1);
        Record.TextureParameters.Add(Parameter, Value);
      }
      else if (Type == TEXT("Cube"))
      {
        FString Value = Trimmed.Mid(Pos2 + 1);
        Record.CubeParameters.Add(Parameter, Value);
      }
      else if (Type == TEXT("Scalar"))
      {
        FString Value = Trimmed.Mid(Pos2 + 1);
        float V = FCString::Atof(*Value);
        Record.ScalarParameters.Add(Parameter, V);
      }
      else if (Type == TEXT("Bool"))
      {
        FString Value = Trimmed.Mid(Pos2 + 1);
        bool V = FCString::ToBool(*Value);
        Record.BoolParameters.Add(Parameter, V);
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
        Record.VectorParameters.Add(Parameter, FLinearColor(R, G, B, A));
      }
    }
    Materials.Add(Record);
    Idx--;
  }

  if (!Materials.Num())
  {
    Error = TEXT("MaterialList.txt is empty!");
    return false;
  }

  for (MaterialRecord& Material : Materials)
  {
    if (!Material.ParentName.Len())
    {
      continue;
    }
    Material.Parent = Materials.FindByPredicate([&](const MaterialRecord& a) {
      return a.Name == Material.ParentName;
    });
    if (!Material.Parent)
    {
      UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to find parent material \"%s\" for \"%s\""), *Material.ParentName, *Material.Name);
      bHasLogErrors = true;
    }
  }

  return true;
}

void REMaterialCreator::SetupMasterMaterial(UMaterial* UnrealMaterial, const MaterialRecord& RealMaterial)
{
  if (!UnrealMaterial)
  {
    return;
  }

  UnrealMaterial->TwoSided = RealMaterial.bIsDoubleSided;

  int32 XStep = 240;
  int32 PosX = XStep;
  bool PosY = false;

  auto GetPosX = [&]() {
    int32 RetVal = PosX;
    PosX += XStep;
    return -RetVal;
  };

  auto GetPosY = [&](int32 Y) {
    int32 RetVal = PosY ? Y : -Y;
    PosY = !PosY;
    return RetVal;
  };

  FExpressionInput* Input = nullptr;

  FString SkipDiffuseParameter;
  for (const auto& P : RealMaterial.TextureParameters)
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

      UMaterialExpressionAdd* Root = NewObject<UMaterialExpressionAdd>(UnrealMaterial);
      Root->Desc = TEXT("Delete me!");
      Root->bCommentBubbleVisible = true;
      UnrealMaterial->Expressions.Add(Root);
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

  for (const auto& P : RealMaterial.TextureParameters)
  {
    if (SkipDiffuseParameter.Len() && P.Key == SkipDiffuseParameter)
    {
      continue;
    }
    UMaterialExpressionAdd* Add = NewObject<UMaterialExpressionAdd>(UnrealMaterial);
    UnrealMaterial->Expressions.Add(Add);
    Add->Desc = TEXT("Delete me!");
    Add->bCommentBubbleVisible = true;
    Add->MaterialExpressionEditorX = GetPosX();
    Input->Expression = Add;
    Input = &Add->A;
    UMaterialExpressionTextureSampleParameter2D* Param = NewObject<UMaterialExpressionTextureSampleParameter2D>(UnrealMaterial);
    UnrealMaterial->Expressions.Add(Param);
    Param->MaterialExpressionEditorX = -PosX;
    Param->MaterialExpressionEditorY = GetPosY(324);
    Param->ParameterName = *P.Key;
    Input->Expression = Param;
    Input = &Add->B;
    if (P.Value.Len() && P.Value != TEXT("None"))
    {
      if (UTexture* Texture = FindResource<UTexture>(P.Value))
      {
        Param->Texture = Texture;
        if (Texture->IsNormalMap())
        {
          Param->SamplerType = SAMPLERTYPE_Normal;
        }
        else if (Texture->CompressionSettings == TC_Grayscale)
        {
          Param->SamplerType = SAMPLERTYPE_Grayscale;
        }
      }
      else
      {
        UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to find Texture Cube \"%s\" for parameter \"%s\" of Material \"%s\""), *P.Value, *P.Key, *RealMaterial.Name);
        bHasLogErrors = true;
      }
    }
    else
    {
      UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to set default Texture for parameter \"%s\" of Material \"%s\""), *P.Key, *RealMaterial.Name);
      bHasLogErrors = true;
    }
  }
  for (const auto& P : RealMaterial.CubeParameters)
  {
    if (!ReflectionVector)
    {
      ReflectionVector = NewObject<UMaterialExpressionReflectionVectorWS>(UnrealMaterial);
      UnrealMaterial->Expressions.Add(ReflectionVector);
    }
    UMaterialExpressionAdd* Add = NewObject<UMaterialExpressionAdd>(UnrealMaterial);
    UnrealMaterial->Expressions.Add(Add);
    Add->Desc = TEXT("Delete me!");
    Add->bCommentBubbleVisible = true;
    Add->MaterialExpressionEditorX = GetPosX();
    Input->Expression = Add;
    Input = &Add->A;
    UMaterialExpressionTextureSampleParameterCube* Param = NewObject<UMaterialExpressionTextureSampleParameterCube>(UnrealMaterial);
    UnrealMaterial->Expressions.Add(Param);
    Param->MaterialExpressionEditorX = -PosX;
    Param->MaterialExpressionEditorY = GetPosY(324);
    Param->ParameterName = *P.Key;
    Param->Coordinates.Expression = ReflectionVector;
    Input->Expression = Param;
    Input = &Add->B;
    if (P.Value.Len() && P.Value != TEXT("None"))
    {
      if (UTexture* Texture = FindResource<UTexture>(P.Value))
      {
        Param->Texture = Texture;
      }
      else
      {
        UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to find Texture Cube \"%s\" for parameter \"%s\" of Material \"%s\""), *P.Value, *P.Key, *RealMaterial.Name);
        bHasLogErrors = true;
      }
    }
    else
    {
      UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to set default Texture Cube for parameter \"%s\" of Material \"%s\""), *P.Key, *RealMaterial.Name);
      bHasLogErrors = true;
    }
  }
  if (ReflectionVector)
  {
    ReflectionVector->MaterialExpressionEditorX = -(PosX + XStep);
    ReflectionVector->MaterialExpressionEditorY = -324;
    ReflectionVector->Desc = TEXT("Delete me!");
    ReflectionVector->bCommentBubbleVisible = true;
  }
  for (const auto& P : RealMaterial.ScalarParameters)
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
  for (const auto& P : RealMaterial.VectorParameters)
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

  XStep = 340;
  PosX = XStep;
  
  for (const auto& P : RealMaterial.BoolParameters)
  {
    UMaterialExpressionStaticBoolParameter* Param = NewObject<UMaterialExpressionStaticBoolParameter>(UnrealMaterial);
    UnrealMaterial->Expressions.Add(Param);
    Param->ParameterName = *P.Key;
    Param->MaterialExpressionEditorX = GetPosX();
    Param->MaterialExpressionEditorY = 580;
    Param->DefaultValue = P.Value;
  }

  UnrealMaterial->PostEditChange();
}

int32 REMaterialCreator::CreateMaterialInstances()
{
  if (!Materials.Num())
  {
    Error = TEXT("Nothing to process!");
    return -1;
  }

  int32 Counter = 0;

  UMaterialInstanceConstantFactoryNew* MI_Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
  for (MaterialRecord& Material : Materials)
  {
    if (Material.UnrealMaterial || !Material.Parent)
    {
      continue;
    }
    Counter += CreateMaterialInstanceR(Material);
  }
  return Counter;
}

int32 REMaterialCreator::CreateMaterialInstanceR(MaterialRecord& RealMaterial)
{
  int32 Count = 0;
  if (RealMaterial.Parent)
  {
    // Create parent materials
    Count += CreateMaterialInstanceR(*RealMaterial.Parent);
  }
  else
  {
    // Master Materials have no parent. We've created master materials in a previous step.
    return 0;
  }

  if (RealMaterial.UnrealMaterial)
  {
    // The Material Instance has been created earlier
    return Count;
  }

  bool Created = false;
  UMaterialInstanceConstantFactoryNew* MI_Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
  MI_Factory->InitialParent = Cast<UMaterialInterface>(RealMaterial.Parent->UnrealMaterial);
  UObject* Asset = CreateMaterialAsset(RealMaterial.Name, UMaterialInstanceConstant::StaticClass(), MI_Factory, Created);
  if (!Asset)
  {
    UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to find/create Material Instance Constant \"%s\""), *RealMaterial.Name);
    bHasLogErrors = true;
    return Count;
  }
  else if (Created)
  {
    Count++;
    RealMaterial.UnrealMaterial = Asset;
    // TODO: combine textures and cubes
    UMaterialInstanceConstant* MI = Cast<UMaterialInstanceConstant>(Asset);
    for (const auto& P : RealMaterial.TextureParameters)
    {
      if (!P.Value.Len() || P.Value == TEXT("None"))
      {
        continue;
      }

      if (UTexture* Texture = FindResource<UTexture>(P.Value))
      {
        FMaterialParameterInfo Info;
        Info.Name = *P.Key;
        MI->SetTextureParameterValueEditorOnly(Info, Texture);
      }
      else
      {
        UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to find Texture \"%s\" for parameter \"%s\" of Material Instance \"%s\""), *P.Value, *P.Key, *RealMaterial.Name);
        bHasLogErrors = true;
      }
    }

    for (const auto& P : RealMaterial.CubeParameters)
    {
      if (!P.Value.Len() || P.Value == TEXT("None"))
      {
        continue;
      }

      if (UTexture* Texture = FindResource<UTexture>(P.Value))
      {
        FMaterialParameterInfo Info;
        Info.Name = *P.Key;
        MI->SetTextureParameterValueEditorOnly(Info, Texture);
      }
      else
      {
        UE_LOG(LogTemp, Error, TEXT("RE Helper: Failed to find Texture Cube \"%s\" for parameter \"%s\" of Material Instance \"%s\""), *P.Value, *P.Key, *RealMaterial.Name);
        bHasLogErrors = true;
      }
    }

    for (const auto& P : RealMaterial.ScalarParameters)
    {
      FMaterialParameterInfo Info;
      Info.Name = *P.Key;
      MI->SetScalarParameterValueEditorOnly(Info, P.Value);
    }

    for (const auto& P : RealMaterial.VectorParameters)
    {
      FMaterialParameterInfo Info;
      Info.Name = *P.Key;
      MI->SetVectorParameterValueEditorOnly(Info, P.Value);
    }

    MI->PostEditChange();
  }

  return Count;
}

UObject* REMaterialCreator::CreateMaterialAsset(FString Name, UClass* Class, UFactory* Factory, bool& Created)
{
  Created = false;
  FString PackageName;
  IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

  Name.RemoveFromStart(TEXT("/"));
  Name.RemoveFromStart(TEXT("Content/"));
  Name.StartsWith(TEXT("Game/")) == true ? Name.InsertAt(0, TEXT("/")) : Name.InsertAt(0, TEXT("/Game/"));

  if (UObject* object = FindObject<UObject>(ANY_PACKAGE, *Name))
  {
    return object;
  }

  AssetTools.CreateUniqueAssetName(Name, TEXT(""), PackageName, Name);
  if (UObject* Asset = AssetTools.CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), Class, Factory))
  {
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    AssetRegistryModule.Get().AssetCreated(Asset);
    Created = true;
    return Asset;
  }
  return nullptr;
}