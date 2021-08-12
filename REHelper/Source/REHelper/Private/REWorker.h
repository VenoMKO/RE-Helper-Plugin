#pragma once
#include "CoreMinimal.h"

class REWorker {
public:
  REWorker() = delete;
  ~REWorker() = delete;

  // Load RE material dump and create necessary Materials and Instances. Returns number of created assets or -1 on error.
  static TArray<class UObject*> ImportMaterials(const FString& Path, FString& OutError);
  // Load RE material map and assign existing materials to mesh assets. Returns number of modified assets or -1 on error.
  static int32 AssignDefaultMaterials(const FString& Path, FString& OutError);
  // Apply correct compression settings and SRGB flag to textures. Returns number of modified assets or -1 on error.
  static int32 FixTextures(const FString& Path, FString& OutError);
  // Set correct materials for SpeedTree actors in the Level
  static int32 FixSpeedTrees(const FString& Path, ULevel* Level, FString& OutError);
  // Import sound cues
  static TArray<class UObject*> ImportSoundCues(const FString& Path, FString& OutError);
  // Import single/custom sound cue
  static class UObject* ImportSingleCue(const FString& Path, FString& OutError);
private:
  // Create and connect parameters
  static void SetupMasterMaterial(UMaterial* UnrealMaterial, struct RMaterial* RealMaterial, bool& Error);
  // Recursively create Material Instance Constants with correct parameter overrides
  static TArray<class UObject*> CreateMaterialInstance(struct RMaterial* RealMaterial, class UFactory* MiFactory, bool& Error);
};