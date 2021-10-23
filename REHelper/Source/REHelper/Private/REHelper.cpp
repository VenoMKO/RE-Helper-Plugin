// Copyright Epic Games, Inc. All Rights Reserved.

#include "REHelper.h"
#include "REWorker.h"

#include "Editor.h"

#include "REHelperStyle.h"
#include "REHelperCommands.h"
#include "Misc/MessageDialog.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "ContentBrowserModule.h"
#include "ToolMenus.h"

#include "DesktopPlatform/Public/IDesktopPlatform.h"
#include "DesktopPlatform/Public/DesktopPlatformModule.h"

#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Misc/ScopedSlowTask.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "PackageTools.h"

#include "ScopedTransaction.h"

static const FName REHelperTabName("REHelper");

#define LOCTEXT_NAMESPACE "FREHelperModule"

void FREHelperModule::StartupModule()
{
  FREHelperStyle::Initialize();
  FREHelperStyle::ReloadTextures();
  FREHelperCommands::Register();
  PluginCommands = MakeShareable(new FUICommandList);
  PluginCommands->MapAction(FREHelperCommands::Get().FixTextures, FExecuteAction::CreateRaw(this, &FREHelperModule::OnFixTexturesClicked), FCanExecuteAction());
  PluginCommands->MapAction(FREHelperCommands::Get().ImportCues, FExecuteAction::CreateRaw(this, &FREHelperModule::OnImportCuesClicked), FCanExecuteAction());
  PluginCommands->MapAction(FREHelperCommands::Get().ImportMaterials, FExecuteAction::CreateRaw(this, &FREHelperModule::OnImportMaterialsClicked), FCanExecuteAction());
  PluginCommands->MapAction(FREHelperCommands::Get().AssignDefaults, FExecuteAction::CreateRaw(this, &FREHelperModule::OnAssignDefaultsClicked), FCanExecuteAction());
  PluginCommands->MapAction(FREHelperCommands::Get().ImportActors, FExecuteAction::CreateRaw(this, &FREHelperModule::OnImportActorsClicked), FCanExecuteAction());
  PluginCommands->MapAction(FREHelperCommands::Get().FixSpeedTrees, FExecuteAction::CreateRaw(this, &FREHelperModule::OnFixSpeedTreesClicked), FCanExecuteAction());
  PluginCommands->MapAction(FREHelperCommands::Get().ImportSingleCue, FExecuteAction::CreateRaw(this, &FREHelperModule::OnImportSingleCueClicked), FCanExecuteAction());
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
      TArray<UObject*> Result = REWorker::ImportMaterials(FilePaths[0], ErrorMessage);
      FText Title;
      FText Message;
      if (Result.Num() < 0)
      {
        Title = FText::FromString(TEXT("Error!"));
        Message = FText::FromString(ErrorMessage);
      }
      else if (Result.Num() == 0)
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
        Message = FText::FromString(FString::Printf(TEXT("Imported %d materials."), Result.Num()) + ErrorMessage);
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
      FScopedSlowTask Task(.0f, NSLOCTEXT("REHelper", "ImportingActors", "Importing actors..."));
      Task.MakeDialog();
      FScopedTransaction Transaction(NSLOCTEXT("REHelper", "ImportActors", "Import Actors To Level"));
      GEditor->edactPasteSelected(GEditor->GetEditorWorldContext().World(), false, false, true, &Input);
      GEditor->SelectNone(false, true, false);
    }
  }
}

void FREHelperModule::OnFixTexturesClicked()
{
  if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
  {
    TArray<FString> FilePaths;
    FString Filter = TEXT("Real Editors textures|Textures.txt");
    if (DesktopPlatform->OpenFileDialog(nullptr, TEXT("Open Real Editor's texture output..."), TEXT(""), TEXT("Textures.txt"), Filter, EFileDialogFlags::None, FilePaths) && FilePaths.Num())
    {
      FScopedTransaction Transaction(NSLOCTEXT("REHelper", "FixTextures", "Fix imported textures"));
      FString ErrorMessage;
      int32 Num = REWorker::FixTextures(FilePaths[0], ErrorMessage);

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
          Title = FText::FromString(TEXT("There are no textures in the list!"));
          Message = FText::FromString(TEXT("Make sure you've selected a correct Textures.txt file."));
        }
      }
      else
      {
        Title = FText::FromString(TEXT("Done!"));
        Message = FText::FromString(FString::Printf(TEXT("Processed %d textures."), Num) + ErrorMessage);
      }
      FMessageDialog::Open(EAppMsgType::Ok, Message, &Title);
    }
  }
}

void FREHelperModule::OnFixSpeedTreesClicked()
{
  UWorld* World = GEditor->GetEditorWorldContext().World();
  if (!World)
  {
    FText Title = FText::FromString(TEXT("Error!"));
    FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("No World is open. Can't iterate SpeedTrees!"), &Title);
    return;
  }
  else if (!World->GetCurrentLevel())
  {
    FText Title = FText::FromString(TEXT("No levels loaded!"));
    FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Load a level to iterate SpeedTrees!"), &Title);
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
    FString Filter = TEXT("SpeedTree Material Overrides|SpeedTreeOverrides.txt");
    if (DesktopPlatform->OpenFileDialog(nullptr, TEXT("Open SpeedTreeOverrides file"), TEXT(""), TEXT("SpeedTreeOverrides.txt"), Filter, EFileDialogFlags::None, OutFiles))
    {
      FString ErrorMessage;
      int32 Num = REWorker::FixSpeedTrees(OutFiles[0], World->GetCurrentLevel(), ErrorMessage);

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
          Message = FText::FromString(TEXT("Failed to process any actors. ") + ErrorMessage);
        }
        else
        {
          Title = FText::FromString(TEXT("There are no actors in the list!"));
          Message = FText::FromString(TEXT("Make sure you've selected a correct SpeedTreeOverrides.txt file and loaded the correct level."));
        }
      }
      else
      {
        Title = FText::FromString(TEXT("Done!"));
        Message = FText::FromString(FString::Printf(TEXT("Processed %d actors. "), Num) + ErrorMessage);
      }
      FMessageDialog::Open(EAppMsgType::Ok, Message, &Title);
    }
  }
}

void FREHelperModule::OnImportCuesClicked()
{
  if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
  {
    TArray<FString> FilePaths;
    FString Filter = TEXT("Real Editors cues|Cues.txt");
    if (DesktopPlatform->OpenFileDialog(nullptr, TEXT("Open Real Editor's cues output..."), TEXT(""), TEXT("Cues.txt"), Filter, EFileDialogFlags::None, FilePaths) && FilePaths.Num())
    {
      FScopedTransaction Transaction(NSLOCTEXT("REHelper", "ImportCues", "Importing CUEs"));
      FString ErrorMessage;
      TArray<UObject*> Result = REWorker::ImportSoundCues(FilePaths[0], ErrorMessage);
      if (Result.Num())
      {
        /* May crash Editor
        IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
        ContentBrowser.SyncBrowserToAssets(Result);*/
        FText Title = FText::FromString(TEXT("Done!"));
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(TEXT("Imported %d CUEs"), Result.Num())), &Title);
      }
      else if (ErrorMessage.Len())
      {
        FText Title = FText::FromString(TEXT("Error!"));
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(ErrorMessage), &Title);
        return;
      }
    }
  }
}

void FREHelperModule::OnImportSingleCueClicked()
{
  if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
  {
    TArray<FString> FilePaths;
    FString Filter = TEXT("Real Editors Cue file|*.cue");
    if (DesktopPlatform->OpenFileDialog(nullptr, TEXT("Open Real Editor's cue export..."), TEXT(""), TEXT("*.cue"), Filter, EFileDialogFlags::None, FilePaths) && FilePaths.Num())
    {
      FScopedTransaction Transaction(NSLOCTEXT("REHelper", "ImportCue", "Importing a CUE"));
      FString ErrorMessage;
      if (UObject* Result = REWorker::ImportSingleCue(FilePaths[0], ErrorMessage))
      {
        IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
        if (ContentBrowser.HasPrimaryContentBrowser())
        {
          TArray<UObject*> Tmp({ Result });
          ContentBrowser.SyncBrowserToAssets(Tmp);
        }
      }
      else if (ErrorMessage.Len())
      {
        FText Title = FText::FromString(TEXT("Error!"));
        FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(ErrorMessage), &Title);
        return;
      }
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
          SubMenuSection.AddMenuEntry(FREHelperCommands::Get().FixTextures).SetCommandList(PluginCommands);
          SubMenuSection.AddMenuEntry(FREHelperCommands::Get().ImportCues).SetCommandList(PluginCommands);
          SubMenuSection.AddMenuEntry(FREHelperCommands::Get().ImportMaterials).SetCommandList(PluginCommands);
          SubMenuSection.AddMenuEntry(FREHelperCommands::Get().AssignDefaults).SetCommandList(PluginCommands);
          SubMenuSection.AddMenuEntry(FREHelperCommands::Get().ImportActors).SetCommandList(PluginCommands);
          SubMenuSection.AddMenuEntry(FREHelperCommands::Get().FixSpeedTrees).SetCommandList(PluginCommands);
          SubMenuSection.AddSeparator("RE_SEP");
          SubMenuSection.AddMenuEntry(FREHelperCommands::Get().ImportSingleCue).SetCommandList(PluginCommands);
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