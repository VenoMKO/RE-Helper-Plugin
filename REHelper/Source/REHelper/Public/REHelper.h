// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "REMaterialCreator.h"

class FREHelperModule : public IModuleInterface
{
public:
	void OnImportMaterialsClicked();
	void OnAssignDefaultsClicked();
	void OnImportActorsClicked();

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
private:
	void RegisterMenus();
	void CreateMaterials(const FString& path);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
	REMaterialCreator MaterialCreator;
};
