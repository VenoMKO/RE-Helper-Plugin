// Copyright Epic Games, Inc. All Rights Reserved.

#include "REHelperCommands.h"

#define LOCTEXT_NAMESPACE "FREHelperModule"

void FREHelperCommands::RegisterCommands()
{
	UI_COMMAND(ImportMaterials, "Import materials...", "Create material hierarchy from Real Editor materials dump file.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(AssignDefaults, "Set defaults...", "Assign default materials to imported geometry.", EUserInterfaceActionType::Button, FInputGesture());
	UI_COMMAND(ImportActors, "Import actors...", "Import actors from a T3D file.", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE