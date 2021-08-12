// Copyright Epic Games, Inc. All Rights Reserved.

#include "REHelperCommands.h"

#define LOCTEXT_NAMESPACE "FREHelperModule"

void FREHelperCommands::RegisterCommands()
{
	UI_COMMAND(ImportMaterials, "Import materials...", "Create material hierarchy from Real Editor materials dump file.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(AssignDefaults, "Set defaults...", "Assign default materials to imported geometry.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ImportActors, "Import actors...", "Import actors from a T3D file.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(FixTextures, "Fix textures...", "Fix texture compression settings.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(FixSpeedTrees, "Fix SpeedTrees...", "Assign correct materials for each SpeedTree actor.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ImportCues, "Import Cue list...", "Import sound cues from a list file.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ImportSingleCue, "Import a Cue...", "Import a single cue file.", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
