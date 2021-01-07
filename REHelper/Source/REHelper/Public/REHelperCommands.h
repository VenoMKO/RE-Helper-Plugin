// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "REHelperStyle.h"

class FREHelperCommands : public TCommands<FREHelperCommands>
{
public:

	FREHelperCommands()
		: TCommands<FREHelperCommands>(TEXT("REHelper"), NSLOCTEXT("Contexts", "REHelper", "REHelper Plugin"), NAME_None, FREHelperStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr<FUICommandInfo> ImportMaterials;
	TSharedPtr<FUICommandInfo> AssignDefaults;
	TSharedPtr<FUICommandInfo> ImportActors;
	TSharedPtr<FUICommandInfo> FixTextures;
};
