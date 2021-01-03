// Copyright Epic Games, Inc. All Rights Reserved.

#include "REHelper.h"
#include "REWorker.h"
#include "REHelperStyle.h"
#include "REHelperCommands.h"
#include "Misc/MessageDialog.h"
#include "Misc/FileHelper.h"
#include "ToolMenus.h"

#include "DesktopPlatformModule.h"
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "PackageTools.h"

static const FName REHelperTabName("REHelper");

#define LOCTEXT_NAMESPACE "FREHelperModule"

void FREHelperModule::StartupModule()
{
  FREHelperStyle::Initialize();
  FREHelperStyle::ReloadTextures();
  FREHelperCommands::Register();
  PluginCommands = MakeShareable(new FUICommandList);
  PluginCommands->MapAction(FREHelperCommands::Get().ImportMaterials, FExecuteAction::CreateRaw(this, &FREHelperModule::OnImportMaterialsClicked), FCanExecuteAction());
  PluginCommands->MapAction(FREHelperCommands::Get().AssignDefaults, FExecuteAction::CreateRaw(this, &FREHelperModule::OnAssignDefaultsClicked), FCanExecuteAction());
  PluginCommands->MapAction(FREHelperCommands::Get().ImportActors, FExecuteAction::CreateRaw(this, &FREHelperModule::OnImportActorsClicked), FCanExecuteAction());
  UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FREHelperModule::RegisterMenus));
}

void FREHelperModule::ShutdownModule()
{
  UToolMenus::UnRegisterStartupCallback(this);
  UToolMenus::UnregisterOwner(this);
  FREHelperStyle::Shutdown();
  FREHelperCommands::Unregister();
}

void FREHelperModule::OnImportMaterialsClicked()
{
  if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
  {
    TArray<FString> FilePaths;
    FString Filter = TEXT("Real Editors materials|MaterialsList.txt");
    if (DesktopPlatform->OpenFileDialog(nullptr, TEXT("Import Real Editor's material output..."), TEXT(""), TEXT("MaterialsList.txt"), Filter, EFileDialogFlags::None, FilePaths) && FilePaths.Num())
    {
      FScopedTransaction Transaction(NSLOCTEXT("REHelper", "ImportMaterials", "Import Materials to Project"));
      
      FString ErrorMessage;
      int32 Num = REWorker::ImportMaterials(FilePaths[0], ErrorMessage);
      
      FText Title;
      FText Message;
      if (Num < 0)
      {
        Title = FText::FromString(TEXT("Error!"));
        Message = FText::FromString(ErrorMessage);
      }
      else if (Num == 0)
      {
        if (ErrorMessage.Len())
        {
          Title = FText::FromString(TEXT("Error!"));
          Message = FText::FromString(TEXT("Failed to import any material.") + ErrorMessage);
        }
        else
        {
          Title = FText::FromString(TEXT("Nothing to import!"));
          Message = FText::FromString(TEXT("All materials already exist."));
        }
      }
      else
      {
        Title = FText::FromString(TEXT("Done!"));
        Message = FText::FromString(FString::Printf(TEXT("Imported %d materials."), Num) + ErrorMessage);
      }
      FMessageDialog::Open(EAppMsgType::Ok, Message, &Title);
    }
  }
}

void FREHelperModule::OnAssignDefaultsClicked()
{
  if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
  {
    TArray<FString> FilePaths;
    FString Filter = TEXT("Real Editors default materials|DefaultMaterials.txt");
    if (DesktopPlatform->OpenFileDialog(nullptr, TEXT("Open default materials map..."), TEXT(""), TEXT(""), Filter, EFileDialogFlags::None, FilePaths))
    {
      FScopedTransaction Transaction(NSLOCTEXT("REHelper", "AssignDefaults", "Assign default materials to assets"));

      FString ErrorMessage;
      int32 Num = REWorker::AssignDefaultMaterials(FilePaths[0], ErrorMessage);

      FText Title;
      FText Message;
      if (Num < 0)
      {
        Title = FText::FromString(TEXT("Error!"));
        Message = FText::FromString(ErrorMessage);
      }
      else if (Num == 0)
      {
        if (ErrorMessage.Len())
        {
          Title = FText::FromString(TEXT("Error!"));
          Message = FText::FromString(TEXT("Failed to process any assets.") + ErrorMessage);
        }
        else
        {
          Title = FText::FromString(TEXT("Nothing to change!"));
          Message = FText::FromString(TEXT("All assets have there default materials."));
        }
      }
      else
      {
        Title = FText::FromString(TEXT("Done!"));
        Message = FText::FromString(FString::Printf(TEXT("Processed %d assets."), Num) + ErrorMessage);
      }
      FMessageDialog::Open(EAppMsgType::Ok, Message, &Title);
    }
  }
}

void FREHelperModule::OnImportActorsClicked()
{
  UWorld* World = GEditor->GetEditorWorldContext().World();
  if (!World)
  {
    FText Title = FText::FromString(TEXT("Error!"));
    FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("No World is open. Can't import actors!"), &Title);
    return;
  }
  else if (!World->GetCurrentLevel())
  {
    FText Title = FText::FromString(TEXT("No levels loaded!"));
    FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Load a level to import actors to!"), &Title);
    return;
  }
  else if (World->GetCurrentLevel()->bLocked)
  {
    FText Title = FText::FromString(TEXT("The Level is locked!"));
    FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Unlock the level, or load a different one!"), &Title);
    return;
  }
  if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
  {
    TArray<FString> OutFiles;
    FString Filter = TEXT("T3D Level dump|*.t3d");
    if (DesktopPlatform->OpenFileDialog(nullptr, TEXT("Import T3D Level dump"), TEXT(""), TEXT(""), Filter, EFileDialogFlags::None, OutFiles))
    {
      FString Input;
      FFileHelper::LoadFileToString(Input, *OutFiles[0]);
      if (!Input.StartsWith(TEXT("BEGIN MAP")))
      {
        FText Title = FText::FromString(TEXT("Error!"));
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("The file is not a valid T3D file!"), &Title);
        return;
      }
      FScopedTransaction Transaction(NSLOCTEXT("REHelper", "ImportActors", "Import Actors To Level"));
      GEditor->edactPasteSelected(GEditor->GetEditorWorldContext().World(), false, false, true, &Input);
      GEditor->SelectNone(false, true, false);
    }
  }
}

void FREHelperModule::RegisterMenus()
{
  FToolMenuOwnerScoped OwnerScoped(this);
  {
    UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
    {
      FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Content");
      {
        auto MenuBuilder = FNewToolMenuDelegate::CreateLambda([&](UToolMenu* InSubMenu) {
          FToolMenuSection& SubMenuSection = InSubMenu->AddSection("Section", LOCTEXT("REHelperPlugin", "RE Helper"));
          SubMenuSection.AddMenuEntry(FREHelperCommands::Get().ImportMaterials).SetCommandList(PluginCommands);
          SubMenuSection.AddMenuEntry(FREHelperCommands::Get().AssignDefaults).SetCommandList(PluginCommands);
          SubMenuSection.AddMenuEntry(FREHelperCommands::Get().ImportActors).SetCommandList(PluginCommands);
        });
        Section.AddEntry(FToolMenuEntry::InitComboButton(
          "REHelperActions",
          FUIAction(),
          MenuBuilder,
          LOCTEXT("REHelperPlugin", "RE Helper"),
          LOCTEXT("REHelperPluginTooltip", "Real Editor plugin"),
          FSlateIcon(FSlateIcon(FREHelperStyle::GetStyleSetName(), "REHelper.PluginAction")),
          false)
        );
      }
    }
  }
}

#undef LOCTEXT_NAMESPACE
  
IMPLEMENT_MODULE(FREHelperModule, REHelper)