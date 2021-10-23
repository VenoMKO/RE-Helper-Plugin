// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class REHelper : ModuleRules
{
  public REHelper(ReadOnlyTargetRules Target) : base(Target)
  {
    PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
    
    PublicIncludePaths.AddRange(
      new string[] {
        // ... add public include paths required here ...
      }
      );
        
    
    PrivateIncludePaths.AddRange(
      new string[] {
        "DesktopPlatform",
      }
      );
      
    
    PublicDependencyModuleNames.AddRange(
      new string[]
      {
        "Core",
        "DesktopPlatform",
        "UnrealEd",
        "AudioEditor",
      }
      );
      
    
    PrivateDependencyModuleNames.AddRange(
      new string[]
      {
        "Projects",
        "InputCore",
        "UnrealEd",
        "ToolMenus",
        "CoreUObject",
        "Engine",
        "Slate",
        "SlateCore",
        // ... add private dependencies that you statically link with here ...	
      }
      );
    
    
    DynamicallyLoadedModuleNames.AddRange(
      new string[]
      {
        // ... add any modules that your module loads dynamically here ...
      }
      );
  }
}
