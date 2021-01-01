#pragma once
#include "CoreMinimal.h"

struct MaterialRecord {
  FString Name;
  FString Class;
  FString ParentName;
  TMap<FString, FString> TextureParameters;
  TMap<FString, float> ScalarParameters;
  TMap<FString, FLinearColor> VectorParameters;
  TMap<FString, FString> CubeParameters;
  TMap<FString, bool> BoolParameters;
  bool bIsDoubleSided = false;
  MaterialRecord* Parent = nullptr;
  UObject* UnrealMaterial = nullptr;
};

class REMaterialCreator {
public:
  static bool AssignDefaults(const FString& Path);

  bool LoadList(const FString& Path);

  int32 CreateMasterMaterials();
  int32 CreateMaterialInstances();

  FText GetError() const
  {
    return FText::FromString(Error);
  }

  bool GetHasLogErrors() const
  {
    return bHasLogErrors;
  }

private:
  void SetupMasterMaterial(UMaterial* UnrealMaterial, const MaterialRecord& RealMaterial);
  int32 CreateMaterialInstanceR(MaterialRecord& RealMaterial);
  UObject* CreateMaterialAsset(FString Name, UClass* Class, UFactory* Factory, bool& Created);

private:
  TArray<MaterialRecord> Materials;
  FString Error;
  bool bHasLogErrors = false;
};