/*
* Copyright 2015 ARM Limited
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "MaliOCPrivatePCH.h"
#include "MaliOCExtensionTab.h"
#include "MaliOCStyle.h"
#include "MaliOCAsyncReportGenerator.h"
#include "MaliOCReportWidgetGenerator.h"
#include "MaliOCCompilerManager.h"
#include "STextComboBox.h"
#include "SHyperLink.h"

#define LOCTEXT_NAMESPACE "MaliOC"

/* Tab generator for the Material Editor and Material Instance Editors */
class FMaterialEditorTabGeneratorImpl final : public ITabGenerator, public TSharedFromThis<FMaterialEditorTabGeneratorImpl>, private FTickableEditorObject
{
public:
    static TSharedRef<ITabGenerator> create(TSharedRef<IMaterialEditor> Editor)
    {
        TSharedRef<FMaterialEditorTabGeneratorImpl> generator(new FMaterialEditorTabGeneratorImpl(Editor));
        generator->InitializeWidgets();

        return generator;
    }

    virtual TSharedRef<SWidget> GetExtensionTab() override
    {
        return ExtensionTab.ToSharedRef();
    }

    virtual ~FMaterialEditorTabGeneratorImpl() = default;
    FMaterialEditorTabGeneratorImpl(const FMaterialEditorTabGeneratorImpl&) = delete;
    FMaterialEditorTabGeneratorImpl(FMaterialEditorTabGeneratorImpl&&) = delete;
    FMaterialEditorTabGeneratorImpl& operator=(const FMaterialEditorTabGeneratorImpl&) = delete;
    FMaterialEditorTabGeneratorImpl& operator=(FMaterialEditorTabGeneratorImpl&&) = delete;

private:

    /* Material editor we're attached to */
    TWeakPtr<IMaterialEditor> MaterialEditor;

    /* The content of the tab in the material editor */
    TSharedPtr<SWidget> ExtensionTab;

    /* Core selection dropdown menu */
    TSharedPtr<STextComboBox> CoreDropDown = nullptr;
    /* Array of core names for core dropdown menu */
    TArray<TSharedPtr<FString>> CoreNames;
    /* Currently selected core. Will never be null after initialization */
    const FMaliCore* SelectedCore;

    /* Revision selection dropdown menu */
    TSharedPtr<STextComboBox> RevDropDown = nullptr;
    /* Array of revision names for revision dropdown menu */
    TArray<TSharedPtr<FString>> CoreRevNames;
    /* Currently selected revision. Will never be null after initialization */
    const FMaliCoreRevision* SelectedRev;

    /* Driver selection dropdown menu */
    TSharedPtr<STextComboBox> DriverDropDown = nullptr;
    /* Array of driver names for driver dropdown menu */
    TArray<TSharedPtr<FString>> CoreRevDriverNames;
    /* Currently selected driver. Will never be null after initialization */
    const FMaliDriver* SelectedDriver;

    /* Platform selection dropdown menu */
    TSharedPtr<STextComboBox> PlatformDropDown = nullptr;
    /* Array of platform names for platform dropdown menu */
    TArray<TSharedPtr<FString>> PlatformNames;
    /* Currently selected platform. Will never be null after initialization */
    const FMaliPlatform* SelectedPlatform;

    /* Widget slot where the output of the widget generator goes */
    SVerticalBox::FSlot* OutputSlot = nullptr;

    /* Report widget generator. Generates the shader report widget from the output of the shader compiler */
    TSharedPtr<FReportWidgetGenerator> WidgetGenerator = nullptr;

    FMaterialEditorTabGeneratorImpl(TWeakPtr<IMaterialEditor> Editor)
        : MaterialEditor(Editor)
    {
    }

    BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION;
    void InitializeWidgets()
    {
        check(FAsyncCompiler::Get() != nullptr);
        // Create the core list
        const auto& cores = FAsyncCompiler::Get()->GetCores();
        check(cores.Num() > 0);
        for (const auto& core : cores)
        {
            CoreNames.Add(MakeShareable(new FString(core->GetName())));
        }
        SelectedCore = &cores[0].Get();
        UpdateRevisionList();
        UpdateDriverList();
        UpdateAPIList();

        const auto AreButtonsPressable = [&]() -> bool
        {
            return !IsCompilationInProgress();
        };

        ExtensionTab =
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                // Drop down menus for core/compiler/rev selection
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .Padding(2.0f, 2.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .FillWidth(0.35f)
                        .MaxWidth(70.0f)
                        .Padding(2.0f, 0.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("CoreDDLabel", "Core"))
                            .Font(FMaliOCStyle::GetNormalFontStyle())
                        ]
                        + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .MaxWidth(200.0f)
                            .Padding(2.0f, 0.0f)
                            [
                                SAssignNew(CoreDropDown, STextComboBox)
                                .OptionsSource(&CoreNames)
                                .OnSelectionChanged(this, &FMaterialEditorTabGeneratorImpl::OnCoreSelectionChanged)
                                .InitiallySelectedItem(CoreNames[0])
                                .Font(FMaliOCStyle::GetNormalFontStyle())
                                .IsEnabled_Lambda(AreButtonsPressable)
                            ]
                    ]
                    + SVerticalBox::Slot()
                        .Padding(2.0f, 2.0f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .FillWidth(0.35f)
                            .MaxWidth(70.0f)
                            .Padding(2.0f, 0.0f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("RevisionDDLabel", "Revision"))
                                .Font(FMaliOCStyle::GetNormalFontStyle())
                            ]
                            + SHorizontalBox::Slot()
                                .FillWidth(1.0f)
                                .MaxWidth(200.0f)
                                .Padding(2.0f, 0.0f)
                                [
                                    SAssignNew(RevDropDown, STextComboBox)
                                    .OptionsSource(&CoreRevNames)
                                    .OnSelectionChanged(this, &FMaterialEditorTabGeneratorImpl::OnRevisionSelectionChanged)
                                    .Font(FMaliOCStyle::GetNormalFontStyle())
                                    .IsEnabled_Lambda(AreButtonsPressable)
                                    .InitiallySelectedItem(CoreRevNames[0])
                                ]
                        ]
                    + SVerticalBox::Slot()
                        .Padding(2.0f, 2.0f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .FillWidth(0.35f)
                            .MaxWidth(70.0f)
                            .Padding(2.0f, 0.0f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("DriverDDLabel", "Driver"))
                                .Font(FMaliOCStyle::GetNormalFontStyle())
                            ]
                            + SHorizontalBox::Slot()
                                .FillWidth(1.0f)
                                .MaxWidth(200.0f)
                                .Padding(2.0f, 0.0f)
                                [
                                    SAssignNew(DriverDropDown, STextComboBox)
                                    .OptionsSource(&CoreRevDriverNames)
                                    .OnSelectionChanged(this, &FMaterialEditorTabGeneratorImpl::OnDriverSelectionChanged)
                                    .Font(FMaliOCStyle::GetNormalFontStyle())
                                    .IsEnabled_Lambda(AreButtonsPressable)
                                    .InitiallySelectedItem(CoreRevDriverNames[0])
                                ]
                        ]
                    + SVerticalBox::Slot()
                        .Padding(2.0f, 2.0f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .FillWidth(0.35f)
                            .MaxWidth(70.0f)
                            .Padding(2.0f, 0.0f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("APIDDLabel", "API"))
                                .Font(FMaliOCStyle::GetNormalFontStyle())
                            ]
                            + SHorizontalBox::Slot()
                                .FillWidth(1.0f)
                                .MaxWidth(200.0f)
                                .Padding(2.0f, 0.0f)
                                [
                                    SAssignNew(PlatformDropDown, STextComboBox)
                                    .OptionsSource(&PlatformNames)
                                    .OnSelectionChanged(this, &FMaterialEditorTabGeneratorImpl::OnAPISelectionChanged)
                                    .Font(FMaliOCStyle::GetNormalFontStyle())
                                    .IsEnabled_Lambda(AreButtonsPressable)
                                    .InitiallySelectedItem(PlatformNames[0])
                                ]
                        ]
                ]
                // Compile button
                + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(2.0f, 2.0f)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("CompileShadersButton", "Compile"))
                        .ToolTipText(LOCTEXT("CompileShadersButtonToolTip", "Compile all shaders for this material using the selected ARM Mali GPU core, core revision, and driver."))
                        .ContentPadding(3)
                        .OnClicked(this, &FMaterialEditorTabGeneratorImpl::BeginReportGenerationAsync)
                        .VAlign(VAlign_Center)
                        .HAlign(HAlign_Center)
                        .IsEnabled_Lambda(AreButtonsPressable)
                    ]
            ]
        // Separator
        + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SSeparator)
            ]
        // Slot for compilation output widgets
        + SVerticalBox::Slot()
            .Expose(OutputSlot)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                .HAlign(HAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("MaliOCHelpString", "Click \"Compile\" to see the estimated shader performance statistics for the current material."))
                    .AutoWrapText(true)
                    .MinDesiredWidth(10000.0f)
                    .Justification(ETextJustify::Center)
                    .Margin(10.0f)
                ]
            ];
    }
    END_SLATE_FUNCTION_BUILD_OPTIMIZATION;

    /* Start generating a report on a new thread when the user clicks compile */
    FReply BeginReportGenerationAsync()
    {
        // It shouldn't be possible to click compile while we're generating a report, but handle it anyway
        if (IsCompilationInProgress())
        {
            return FReply::Handled();
        }

        auto ME = MaterialEditor.Pin();
        check(ME.IsValid());
        auto matint = ME->GetMaterialInterface();

        // This should start report creation on a worker thread
        auto ReportGenerator = MakeShareable(new FAsyncReportGenerator(matint, *SelectedPlatform));

        // Wrap up the report generator in an object that takes the report and generates us a nice widget to display
        WidgetGenerator = MakeShareable(new FReportWidgetGenerator(ReportGenerator));

        // The report won't be ready yet so the widget generator should show us its throbber
        OutputSlot->AttachWidget(WidgetGenerator->GetWidget());

        return FReply::Handled();
    }

    /* Check the currently selected core and update the set of revisions and currently selected revision */
    void UpdateRevisionList()
    {
        CoreRevNames.Empty();
        const auto& revisions = SelectedCore->GetRevisions();
        for (const auto& revision : revisions)
        {
            CoreRevNames.Add(MakeShareable(new FString(revision->GetName())));
        }
        SelectedRev = &revisions[0].Get();
    }

    /* Check the currently selected revision and update the set of drivers and currently selected driver */
    void UpdateDriverList()
    {
        CoreRevDriverNames.Empty();
        const auto& drivers = SelectedRev->GetDrivers();
        for (const auto& driver : drivers)
        {
            CoreRevDriverNames.Add(MakeShareable(new FString(driver->GetName())));
        }
        SelectedDriver = &drivers[0].Get();
    }

    /* Check the currently selected driver and update the set of APIs and currently selected API */
    void UpdateAPIList()
    {
        PlatformNames.Empty();
        const auto& platforms = SelectedDriver->GetPlatforms();
        for (const auto& platform : platforms)
        {
            PlatformNames.Add(MakeShareable(new FString(platform->GetName())));
        }
        SelectedPlatform = &platforms[0].Get();
    }

    /* Update the content of the revision dropdown */
    void UpdateRevisionDropDown()
    {
        UpdateRevisionList();
        RevDropDown->RefreshOptions();
        RevDropDown->SetSelectedItem(CoreRevNames[0]);
    }

    /* Update the content of the driver dropdown */
    void UpdateDriverDropDown()
    {
        UpdateDriverList();
        DriverDropDown->RefreshOptions();
        DriverDropDown->SetSelectedItem(CoreRevDriverNames[0]);
    }

    /* Update the content of the platform dropdown */
    void UpdatePlatformDropDown()
    {
        UpdateAPIList();
        PlatformDropDown->RefreshOptions();
        PlatformDropDown->SetSelectedItem(PlatformNames[0]);
    }

    /* Callback when the user changes the selected core */
    void OnCoreSelectionChanged(TSharedPtr<FString> selectedCoreName, ESelectInfo::Type InSelectionInfo)
    {
        // Early out if the new selection is the same as the old one
        check(SelectedCore != nullptr);
        if (SelectedCore->GetName().Equals(*selectedCoreName))
        {
            return;
        }

        // Validate that the user made a physically possible selection
        bool found = false;

        const auto& cores = FAsyncCompiler::Get()->GetCores();
        for (const auto& core : cores)
        {
            if (core->GetName().Equals(*selectedCoreName))
            {
                SelectedCore = &core.Get();
                found = true;
                break;
            }
        }

        check(found);

        // Update dependent dropdowns
        UpdateRevisionDropDown();
        UpdateDriverDropDown();
        UpdatePlatformDropDown();
    }

    void OnRevisionSelectionChanged(TSharedPtr<FString> selectedRevName, ESelectInfo::Type InSelectionInfo)
    {
        // Early out if the new selection is the same as the old one
        check(SelectedRev != nullptr);
        if (SelectedRev->GetName().Equals(*selectedRevName))
        {
            return;
        }

        // Validate that the user made a physically possible selection
        bool found = false;

        const auto& revisions = SelectedCore->GetRevisions();
        for (const auto& revision : revisions)
        {
            if (revision->GetName().Equals(*selectedRevName))
            {
                SelectedRev = &revision.Get();
                found = true;
                break;
            }
        }

        check(found);

        // Update dependent dropdowns
        UpdateDriverDropDown();
        UpdatePlatformDropDown();
    }

    void OnDriverSelectionChanged(TSharedPtr<FString> selectedDriverName, ESelectInfo::Type InSelectionInfo)
    {
        // Early out if the new selection is the same as the old one
        check(SelectedDriver != nullptr);
        if (SelectedDriver->GetName().Equals(*selectedDriverName))
        {
            return;
        }

        // Validate that the user made a physically possible selection
        bool found = false;

        const auto& drivers = SelectedRev->GetDrivers();
        for (const auto& driver : drivers)
        {
            if (driver->GetName().Equals(*selectedDriverName))
            {
                SelectedDriver = &driver.Get();
                found = true;
                break;
            }
        }
        check(found);

        // Update dependent dropdowns
        UpdatePlatformDropDown();
    }

    void OnAPISelectionChanged(TSharedPtr<FString> SelectedPlatformName, ESelectInfo::Type InSelectionInfo)
    {
        // Early out if the new selection is the same as the old one
        check(SelectedPlatform != nullptr);
        if (SelectedPlatform->GetName().Equals(*SelectedPlatformName))
        {
            return;
        }

        // Validate that the user made a physically possible selection
        bool found = false;

        const auto& platforms = SelectedDriver->GetPlatforms();
        for (const auto& pla : platforms)
        {
            if (pla->GetName().Equals(*SelectedPlatformName))
            {
                SelectedPlatform = &pla.Get();
                found = true;
                break;
            }
        }
        check(found);
    }

    /* Return true if compilation is currently in progress */
    bool IsCompilationInProgress() const
    {
        return WidgetGenerator.IsValid() && (WidgetGenerator->IsCompilationComplete() != true);
    }

    virtual bool IsTickable() const override
    {
        return true;
    }

    virtual void Tick(float DeltaTime) override
    {
        // Each frame, check if we have a valid widget generator
        // If we do, attach its generated widget to the output slot
        if (WidgetGenerator.IsValid())
        {
            OutputSlot->AttachWidget(WidgetGenerator->GetWidget());
        }
    }

    virtual TStatId GetStatId() const override
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FMaterialEditorTabGeneratorImpl, STATGROUP_Tickables);
    }
};

/* Wrapper around the Material Editor tab generator. Used to prompt the user to download the compiler manager. Once the compiler manager has been downloaded, will automatically replace itself with the material editor tab generator.*/
class FMaterialEditorTabGeneratorImplWrapper final : public ITabGenerator, public TSharedFromThis<FMaterialEditorTabGeneratorImplWrapper>, private FTickableEditorObject
{
public:
    static TSharedRef<ITabGenerator> create(TSharedRef<IMaterialEditor> Editor)
    {
        TSharedRef<FMaterialEditorTabGeneratorImplWrapper> generator(new FMaterialEditorTabGeneratorImplWrapper(Editor));
        generator->InitializeWidgets();

        return generator;
    }

    virtual TSharedRef<SWidget> GetExtensionTab() override
    {
        return ExtensionTab.ToSharedRef();
    }

    virtual ~FMaterialEditorTabGeneratorImplWrapper() = default;
    FMaterialEditorTabGeneratorImplWrapper(const FMaterialEditorTabGeneratorImplWrapper&) = delete;
    FMaterialEditorTabGeneratorImplWrapper(FMaterialEditorTabGeneratorImplWrapper&&) = delete;
    FMaterialEditorTabGeneratorImplWrapper& operator=(const FMaterialEditorTabGeneratorImplWrapper&) = delete;
    FMaterialEditorTabGeneratorImplWrapper& operator=(FMaterialEditorTabGeneratorImplWrapper&&) = delete;

private:
    FMaterialEditorTabGeneratorImplWrapper(TWeakPtr<IMaterialEditor> Editor)
        : MaterialEditor(Editor)
    {
    }

    /* Material editor we're attached to */
    TWeakPtr<IMaterialEditor> MaterialEditor;
    /* The content of the tab in the material editor */
    TSharedPtr<SHorizontalBox> ExtensionTab;
    /* Tab Generator we're a wrapper around. Will be null until conditions are met */
    TSharedPtr<ITabGenerator> WrappedGenerator = nullptr;
    /* Timer for polling whether the compiler manager DLL exists and we can load it */
    float lastInitAttempt = 0;
    /* Period between initialization attempts */
    const float InitializationPeriod = 1.0f;

    BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION;
    void InitializeWidgets()
    {
        const auto OpenMaliOCFolder = [&]() -> void
        {
            FPlatformProcess::ExploreFolder(*FCompilerManager::GetMaliOCFolderPath());
        };

        const auto LaunchDownloadURL = [&]() -> void
        {
            FPlatformProcess::LaunchURL(*FCompilerManager::GetOfflineCompilerDownloadURL(), nullptr, nullptr);
        };

        const auto LaunchEULAURL = [&]() -> void
        {
            FPlatformProcess::LaunchURL(*FCompilerManager::GetEULADownloadURL(), nullptr, nullptr);
        };

        ExtensionTab =
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Center)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("TabGenWrapperEULAIntro", "Some additional files are required to use the Mali Offline Compiler Plugin for Unreal Engine 4. By downloading these files, you acknowledge that you accept the End User License Agreement for the Mali GPU Offline Compiler."))
                        .AutoWrapText(true)
                        .MinDesiredWidth(10000.0f)
                        .Margin(10.0f)
                    ]
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .HAlign(HAlign_Center)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .Padding(10.0f)
                            .AutoWidth()
                            [
                                SNew(SHyperlink)
                                .Text(LOCTEXT("EULADisplayName", "End User Licence Agreement for the Mali Offline Compiler"))
                                .ToolTipText(FText::FromString(FCompilerManager::GetEULADownloadURL()))
                                .OnNavigate_Lambda(LaunchEULAURL)
                            ]
                        ]
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("TabGenWrapperIntro", "To use the Mali Offline Compiler Plugin for Unreal Engine 4, you need to click below to download the Mali Offline Compiler:"))
                            .AutoWrapText(true)
                            .MinDesiredWidth(10000.0f)
                            .Margin(10.0f)
                        ]
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .HAlign(HAlign_Center)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .Padding(10.0f)
                            .AutoWidth()
                            [
                                SNew(SHyperlink)
                                .Text(FText::FromString(FCompilerManager::GetOfflineCompilerDownloadName()))
                                .ToolTipText(FText::FromString(FCompilerManager::GetOfflineCompilerDownloadURL()))
                                .OnNavigate_Lambda(LaunchDownloadURL)
                            ]
                        ]
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::Format(LOCTEXT("TabGenWrapperExtract", "And extract the {0} folder from the archive into:"), FText::FromString(FCompilerManager::GetOfflineCompilerFolderToExtract())))
                            .AutoWrapText(true)
                            .MinDesiredWidth(10000.0f)
                            .Margin(10.0f)
                        ]
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        .HAlign(HAlign_Center)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .Padding(10.0f)
                            .AutoWidth()
                            [
                                SNew(SHyperlink)
                                .Text(FText::FromString(FCompilerManager::GetMaliOCFolderPath()))
                                .ToolTipText((LOCTEXT("TabGenWrapperOpenFolderTooltip", "Open this folder using the system file explorer")))
                                .OnNavigate_Lambda(OpenMaliOCFolder)
                            ]
                        ]
                    + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::Format(LOCTEXT("TabGenWrapperFinishedOldDLLExists", "When you've done this correctly, there will be a {0} folder inside the above folder, and this message will automatically be replaced by the Mali Offline Compiler Plugin."), FText::FromString(FCompilerManager::GetOfflineCompilerFolderToExtract())))
                            .AutoWrapText(true)
                            .MinDesiredWidth(10000.0f)
                            .Margin(10.0f)
                        ]
                ]
            ];
    }
    END_SLATE_FUNCTION_BUILD_OPTIMIZATION;

    virtual bool IsTickable() const override
    {
        return true;
    }

    virtual void Tick(float DeltaTime) override
    {
        lastInitAttempt += DeltaTime;
        if (lastInitAttempt < InitializationPeriod)
        {
            return;
        }
        lastInitAttempt = 0;

        // Each frame, check if the compiler manager has loaded
        // If it has, create the actual tab generator and pass it through
        if (!WrappedGenerator.IsValid())
        {
            if (FAsyncCompiler::Get() == nullptr)
            {
                FAsyncCompiler::Initialize(true);
            }

            if (FAsyncCompiler::Get() != nullptr)
            {
                auto ME = MaterialEditor.Pin();
                check(ME.IsValid());
                WrappedGenerator = FMaterialEditorTabGeneratorImpl::create(ME.ToSharedRef());
                ExtensionTab->ClearChildren();
                ExtensionTab->AddSlot().AttachWidget(WrappedGenerator->GetExtensionTab());
            }
        }
    }

    virtual TStatId GetStatId() const override
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FMaterialEditorTabGeneratorImplWrapper, STATGROUP_Tickables);
    }
};

/* Tab generator for the Material Function Editor */
class FMaterialFunctionEditorTabGeneratorImpl final : public ITabGenerator, public TSharedFromThis < FMaterialFunctionEditorTabGeneratorImpl >
{
public:
    static TSharedRef<ITabGenerator> create()
    {
        TSharedRef<FMaterialFunctionEditorTabGeneratorImpl> generator(new FMaterialFunctionEditorTabGeneratorImpl);
        generator->InitializeWidgets();

        return generator;
    }

    virtual TSharedRef<SWidget> GetExtensionTab() override
    {
        return ExtensionTab.ToSharedRef();
    }

    FMaterialFunctionEditorTabGeneratorImpl() = default;
    virtual ~FMaterialFunctionEditorTabGeneratorImpl() = default;
    FMaterialFunctionEditorTabGeneratorImpl(const FMaterialFunctionEditorTabGeneratorImpl&) = delete;
    FMaterialFunctionEditorTabGeneratorImpl(FMaterialFunctionEditorTabGeneratorImpl&&) = delete;
    FMaterialFunctionEditorTabGeneratorImpl& operator=(const FMaterialFunctionEditorTabGeneratorImpl&) = delete;
    FMaterialFunctionEditorTabGeneratorImpl& operator=(FMaterialFunctionEditorTabGeneratorImpl&&) = delete;

private:
    TSharedPtr<SWidget> ExtensionTab;

    BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION;
    void InitializeWidgets()
    {
        // We need to create an error message widget for Material Function Editors, as we can't get valid statistics for a Material Function
        ExtensionTab =
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Center)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("MaterialFunctionEditorErrorString", "Shader statistics are unsupported for Material Functions"))
                .AutoWrapText(true)
                .MinDesiredWidth(10000.0f)
                .Justification(ETextJustify::Center)
                .Margin(10.0f)
            ];
    }
    END_SLATE_FUNCTION_BUILD_OPTIMIZATION;
};

TSharedRef<ITabGenerator> FMaterialEditorTabGenerator::create(TSharedRef<IMaterialEditor> editor)
{
    if (FAsyncCompiler::Get() == nullptr)
    {
        // If the compiler isn't loaded, that probably means we couldn't find the compiler manager + libs
        // Create a wrapper that will prompt the user to download the compiler manager
        // Once the compiler manager is loaded, the wrapper will just be a pass through for the regular tab generator
        return FMaterialEditorTabGeneratorImplWrapper::create(editor);
    }
    else
    {
        return FMaterialEditorTabGeneratorImpl::create(editor);
    }
}

TSharedRef<ITabGenerator> FMaterialFunctionEditorTabGenerator::create()
{
    return FMaterialFunctionEditorTabGeneratorImpl::create();
}

#undef LOCTEXT_NAMESPACE
