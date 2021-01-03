// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

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

private:
	TSharedPtr<class FUICommandList> PluginCommands;
};
