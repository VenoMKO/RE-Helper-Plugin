#pragma once
#include "CoreMinimal.h"

class REWorker {
public:
  REWorker() = delete;
  ~REWorker() = delete;

  // Load RE material dump and create necessary Materials and Instances. Returns number of created assets or -1 on error.
  static int32 ImportMaterials(const FString& Path, FString& OutError);
  // Load RE material map and assign existing materials to mesh assets. Returns number of modified assets or -1 on error.
  static int32 AssignDefaultMaterials(const FString& Path, FString& OutError);
  // Apply correct compression settings and SRGB flag to textures. Returns number of modified assets or -1 on error.
  static int32 FixTextures(const FString& Path, FString& OutError);

private:
  // Create and connect parameters
  static void SetupMasterMaterial(UMaterial* UnrealMaterial, struct RMaterial* RealMaterial, bool& Error);
  // Recursively create Material Instance Constants with correct parameter overrides
  static int32 CreateMaterialInstance(struct RMaterial* RealMaterial, UFactory* MiFactory, bool& Error);
};