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
#include "MaliOCAsyncCompiler.h"
#include "SDockTab.h"

#define LOCTEXT_NAMESPACE "MaliOC"

DEFINE_LOG_CATEGORY(MaliOfflineCompiler)

static const FName MaliOCTabID(TEXT("MaliOCTab"));

const FString& GetMaliOCPluginFolderPath()
{
    static const FString path = FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::EnginePluginsDir(), TEXT("MaliOfflineCompiler/")));
    return path;
}

/** Singleton that registers the Mali Offline Compiler button with Slate */
class FMaliOCCommands final : public TCommands < FMaliOCCommands >
{
public:
    FMaliOCCommands() :
        TCommands<FMaliOCCommands>(TEXT("MaliOC"),
        NSLOCTEXT("Contexts", "MaliOC", "Mali Offline Compiler"),
        NAME_None,
        FMaliOCStyle::Get()->GetStyleSetName())
    {
    }

    TSharedPtr< FUICommandInfo > OpenMaliOCPanel;

    virtual void RegisterCommands() override
    {
        UI_COMMAND(OpenMaliOCPanel, "Offline Compiler", "Shows the Mali Offline Compiler Pane", EUserInterfaceActionType::Button, FInputGesture());
    }
};

/** For each MaterialEditor, MaterialFunctionEditor, or MaterialInstanceEditor created, we create an instance of FMaliOCMaterialEditorExtension */
class FMaliOCMaterialEditorExtension final : public TSharedFromThis < FMaliOCMaterialEditorExtension >
{
public:

    /*
     * @param Editor The material editor to hook into
     * @return a new material editor extension that's hooked into Editor
     */
    static TSharedRef<FMaliOCMaterialEditorExtension> CreateForMaterialEditor(TSharedRef<IMaterialEditor> Editor)
    {
        TSharedRef<FMaliOCMaterialEditorExtension> NewMaterialEditorExtension(new FMaliOCMaterialEditorExtension(Editor, FMaterialEditorTabGenerator::create(Editor)));
        NewMaterialEditorExtension->Initialize(TEXT("Graph"));
        return NewMaterialEditorExtension;
    }

    /*
    * @param Editor The material function editor to hook into
    * @return a new material editor extension that's hooked into Editor
    */
    static TSharedRef<FMaliOCMaterialEditorExtension> CreateForMaterialFunctionEditor(TSharedRef<IMaterialEditor> Editor)
    {
        TSharedRef<FMaliOCMaterialEditorExtension> NewMaterialEditorExtension(new FMaliOCMaterialEditorExtension(Editor, FMaterialFunctionEditorTabGenerator::create()));
        NewMaterialEditorExtension->Initialize(TEXT("Graph"));
        return NewMaterialEditorExtension;
    }

    /*
    * @param Editor The material instance editor to hook into
    * @return a new material editor extension that's hooked into Editor
    */
    static TSharedRef<FMaliOCMaterialEditorExtension> CreateForMaterialInstanceEditor(TSharedRef<IMaterialEditor> Editor)
    {
        TSharedRef<FMaliOCMaterialEditorExtension> NewMaterialEditorExtension(new FMaliOCMaterialEditorExtension(Editor, FMaterialEditorTabGenerator::create(Editor)));
        NewMaterialEditorExtension->Initialize(TEXT("Command"));
        return NewMaterialEditorExtension;
    }

    // Typically, this extension will be destroyed during Material Editor destruction, in which case all of the code inside this destructor is unnecessary
    // However, when we reload the module while there are still Material Editors open, the extensions will be destroyed earlier than the Material Editor, so we need to clean up after ourselves as best we can
    // In practice, this doesn't make a difference, as Slate will crash when trying to draw the UI of a module that has been reloaded while bits of it are still in use
    // Just close all Material Editors before reloading this module and we won't have a problem
    ~FMaliOCMaterialEditorExtension()
    {
        // Unhook from the toolbar extender
        if (ToolbarExtensionManagerPtr.IsValid())
        {
            ToolbarExtensionManagerPtr.Pin()->RemoveExtender(ToolbarExtender);
        }

        // Unhook from the tab spawner and despawner
        if (MaterialEditor.IsValid())
        {
            auto me = MaterialEditor.Pin();

            me->OnRegisterTabSpawners().RemoveAll(this);
            me->OnUnregisterTabSpawners().RemoveAll(this);
        }
    }

    FMaliOCMaterialEditorExtension(const FMaliOCMaterialEditorExtension&) = delete;
    FMaliOCMaterialEditorExtension(FMaliOCMaterialEditorExtension&&) = delete;
    FMaliOCMaterialEditorExtension& operator=(const FMaliOCMaterialEditorExtension&) = delete;
    FMaliOCMaterialEditorExtension& operator=(FMaliOCMaterialEditorExtension&&) = delete;

    TWeakPtr<IMaterialEditor> GetMaterialEditor()
    {
        return MaterialEditor;
    }

private:

    FMaliOCMaterialEditorExtension(TWeakPtr<IMaterialEditor> Editor, TSharedRef<ITabGenerator> TabGenerator)
        : MaterialEditor(Editor)
        , ExtensionTabGenerator(TabGenerator)
        , PluginCommands(MakeShareable(new FUICommandList))
    {
    }

    void Initialize(const FName& ToolbarExtensionPoint)
    {
        // Hook into the lifecycle of the editor itself
        auto ed = MaterialEditor.Pin();
        ed->OnRegisterTabSpawners().AddSP(this, &FMaliOCMaterialEditorExtension::OnRegisterTabSpawners);
        ed->OnUnregisterTabSpawners().AddSP(this, &FMaliOCMaterialEditorExtension::OnUnregisterTabSpawners);

        // Create the commands and map their actions
        PluginCommands->MapAction(
            FMaliOCCommands::Get().OpenMaliOCPanel,
            FExecuteAction::CreateSP(this, &FMaliOCMaterialEditorExtension::OnClickMaliOCToolbarButton),
            FCanExecuteAction::CreateSP(this, &FMaliOCMaterialEditorExtension::IsMaliOCTabNotOpen));

        auto ToolbarExtensionManagerRef = MaterialEditor.Pin()->GetToolBarExtensibilityManager();
        ToolbarExtender = MakeShareable(new FExtender);
        ToolbarExtender->AddToolBarExtension(ToolbarExtensionPoint, EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateSP(this, &FMaliOCMaterialEditorExtension::OnAddToolbarExtension));

        ToolbarExtensionManagerRef->AddExtender(ToolbarExtender);
        // Save a weak ref to the extension manager. We don't want to interfere with its lifetime
        ToolbarExtensionManagerPtr = ToolbarExtensionManagerRef;
    }

    /** Creates the MaliOC tab on click of the toolbar button */
    void OnClickMaliOCToolbarButton()
    {
        if (TabManagerPtr.IsValid())
        {
            TabManagerPtr.Pin()->InvokeTab(MaliOCTabID);
        }
    }

    /** Registered with the material editor to add the MaliOC button */
    void OnAddToolbarExtension(FToolBarBuilder &builder)
    {
        builder.AddToolBarButton(FMaliOCCommands::Get().OpenMaliOCPanel);
    }

    /** Registered with the material editor to add the MaliOC tab */
    void OnRegisterTabSpawners(const TSharedRef<FTabManager>& TabManager)
    {
        TabManagerPtr = TabManager;

        TabManager->RegisterTabSpawner(MaliOCTabID, FOnSpawnTab::CreateSP(this, &FMaliOCMaterialEditorExtension::OnSpawnMaliOCTab))
            .SetDisplayName(LOCTEXT("MaliOCTab", "Offline Compiler"))
            .SetGroup(TabManager->GetLocalWorkspaceMenuRoot()->GetChildItems()[0])
            .SetIcon(FSlateIcon(FMaliOCStyle::Get()->GetStyleSetName(), "MaliOC.MaliOCIcon16"))
            .SetTooltipText(LOCTEXT("MaliOCTabTooltip", "Offline Compiler"));
    }

    /** Create the MaliOC tab */
    TSharedRef<SDockTab> OnSpawnMaliOCTab(const FSpawnTabArgs& Args)
    {
        return SAssignNew(MaliOCTabPtr, SDockTab)
            .Label(LOCTEXT("MaliOCTab", "Offline Compiler"))
            [
                SNew(SBorder)
                .Padding(2.0f)
                .BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .FillHeight(1)
                    [
                        ExtensionTabGenerator->GetExtensionTab()
                    ]
                ]
            ];
    }

    /** Registered with the material editor so we neatly clean up after ourselves */
    void OnUnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager)
    {
        TabManager->UnregisterTabSpawner(MaliOCTabID);
    }

    /** Returns true if the tab is NOT open. Used to grey out the MaliOC button when the tab is open */
    bool IsMaliOCTabNotOpen() const
    {
        return !MaliOCTabPtr.IsValid();
    }

    /** The material editor we're hooked in to. Weak ref to not interfere with the lifecycle */
    TWeakPtr<IMaterialEditor> MaterialEditor;
    /** The material editor's toolbar extension manager. Used to clean up after ourselves. Weak ref to not interfere with the lifecycle */
    TWeakPtr<FExtensibilityManager> ToolbarExtensionManagerPtr;
    /** The material editor's tab manager. Used to clean up after ourselves. Weak ref to not interfere with the lifecycle */
    TWeakPtr<FTabManager> TabManagerPtr;

    /** Weak reference to the tab created in the material editor. We know if the tab is open or not depending on whether this pointer points to a live object */
    TWeakPtr<SDockTab> MaliOCTabPtr;

    /** Generates the content to display in the tab */
    TSharedRef<ITabGenerator> ExtensionTabGenerator;

    /** Toolbar buttons added to ToolbarExtender */
    TSharedPtr<FUICommandList> PluginCommands;
    /** ToolbarExtender registered with material editor */
    TSharedPtr<FExtender> ToolbarExtender;
};

class FMaliOC final : public TSharedFromThis < FMaliOC >
{
public:
    void Initialize()
    {
        // Initialise the styleset. This loads icons from disk and registers them appropriately
        FMaliOCStyle::Initialize();

        // Register the buttons we'll be using with Slate
        FMaliOCCommands::Register();

        // Load a reference to the material editor module
        IMaterialEditorModule& MEModule = FModuleManager::LoadModuleChecked<IMaterialEditorModule>("MaterialEditor");

        // Set up a listener for the various material editor opening events
        MEModule.OnMaterialEditorOpened().AddSP(this, &FMaliOC::OnMaterialEditorOpened);
        MEModule.OnMaterialFunctionEditorOpened().AddSP(this, &FMaliOC::OnMaterialFunctionEditorOpened);
        MEModule.OnMaterialInstanceEditorOpened().AddSP(this, &FMaliOC::OnMaterialInstanceEditorOpened);

        // Attempt to load the Async compiler. This will fail if the user has not yet downloaded the compiler manager
        // This scenario will be dealt with by the material editor tab generator
        FAsyncCompiler::Initialize();
    }

    /** Deinit the offline compiler module. This function makes no assumptions about whether initialisation was successful or not, so all deinit is safe */
    void Deinitialize()
    {
        // Remove the OnClose message from any still open material editors
        // In theory, using smart pointers to hook into the material editors means that we don't need to manually clean up after ourselves
        // In practice, if we recompile this module, the material editor may call smart pointer methods where the code is in a now unloaded dll, resulting in a segfault
        // If we close all material editors before attempting to reload and recompile the MaliOC module, we avoid this and everything is fine
        for (auto& ee : EditorExtensions)
        {
            auto me = ee->GetMaterialEditor().Pin();
            if (me.IsValid())
            {
                me->OnMaterialEditorClosed().RemoveAll(this);
            }
        }

        // Explicitly destroy all extensions that might still be alive. Any remaining hooks will be unhooked
        EditorExtensions.Empty();

        // Get a pointer to the material editor module if it's still alive.
        // If we're shutting down, there's a chance the material editor module has already shut down
        IMaterialEditorModule* MEModule = FModuleManager::GetModulePtr<IMaterialEditorModule>("MaterialEditor");

        // Unhook ourselves from the material editor module
        // If we don't do this when we reload or recompile the OSC module, the engine will crash when you try and open a new material editor
        if (MEModule != nullptr)
        {
            MEModule->OnMaterialEditorOpened().RemoveAll(this);
            MEModule->OnMaterialFunctionEditorOpened().RemoveAll(this);
            MEModule->OnMaterialInstanceEditorOpened().RemoveAll(this);
        }

        // Unload the Async compiler
        FAsyncCompiler::Deinitialize();

        // Unregister our commands
        FMaliOCCommands::Unregister();

        // Shut down the style set
        FMaliOCStyle::Deinitialize();
    }

    /** Register the extension by adding a hook to listen for material editor close events */
    void RegisterMaterialEditorExtension(TSharedRef<FMaliOCMaterialEditorExtension>& extension, TSharedRef<IMaterialEditor>& editor)
    {
        // Add the extension to the list of open editors
        EditorExtensions.Add(extension);

        // Hook into the lifecycle of the editor
        editor->OnMaterialEditorClosed().AddSP(this, &FMaliOC::OnMaterialEditorClosed, TWeakPtr<FMaliOCMaterialEditorExtension>(extension));
    }

    /** Callback when a new material editor gets opened */
    void OnMaterialEditorOpened(TWeakPtr<IMaterialEditor> editor)
    {
        auto edPtr = editor.Pin();

        if (edPtr.IsValid())
        {
            auto edRef = edPtr.ToSharedRef();
            auto extension = FMaliOCMaterialEditorExtension::CreateForMaterialEditor(edRef);
            RegisterMaterialEditorExtension(extension, edRef);
        }
    }

    /** Callback when a new material function editor gets opened */
    void OnMaterialFunctionEditorOpened(TWeakPtr<IMaterialEditor> editor)
    {
        auto edPtr = editor.Pin();

        if (edPtr.IsValid())
        {
            auto edRef = edPtr.ToSharedRef();
            auto extension = FMaliOCMaterialEditorExtension::CreateForMaterialFunctionEditor(edRef);
            RegisterMaterialEditorExtension(extension, edRef);
        }
    }

    /** Callback when a new material instance editor gets opened */
    void OnMaterialInstanceEditorOpened(TWeakPtr<IMaterialEditor> editor)
    {
        auto edPtr = editor.Pin();

        if (edPtr.IsValid())
        {
            auto edRef = edPtr.ToSharedRef();
            auto extension = FMaliOCMaterialEditorExtension::CreateForMaterialInstanceEditor(edRef);
            RegisterMaterialEditorExtension(extension, edRef);
        }
    }

    /** Callback when a material editor of any kind gets closed */
    void OnMaterialEditorClosed(TWeakPtr<FMaliOCMaterialEditorExtension> extension)
    {
        auto Ext = extension.Pin().ToSharedRef();

        int32 numRemoved = EditorExtensions.Remove(Ext);
        // We should have removed exactly one editor extension from the array
        check(numRemoved == 1);

        // We should now be holding the very last reference to the extension.
        // Ext will be destroyed when we go out of scope
        check(Ext.IsUnique());
    }

    /** Array of live editor extensions. For each material (function/instance) editor, there should be one editor extension */
    TArray<TSharedRef<FMaliOCMaterialEditorExtension>> EditorExtensions;

    FMaliOC() = default;
    ~FMaliOC() = default;
    FMaliOC(const FMaliOC&) = delete;
    FMaliOC(FMaliOC&&) = delete;
    FMaliOC& operator=(const FMaliOC&) = delete;
    FMaliOC& operator=(FMaliOC&&) = delete;
};

/** Wrapper around FMaliOC. Helps us to make guarantees about when resources are allocated and deallocated */
class FMaliOCModule final : public IModuleInterface
{
    virtual void StartupModule() override
    {
        /* We extend from TSharedFromThis<> in order to create shared pointer delegates to hook into the
        Material Editor module. However, it is illegal to create shared pointer delegates to an object
        that is in the process of being constructed. Therefore, we do all the heavy lifting in the Initialize
        function after the constructor has run. */
        MaliOCImpl = MakeShareable(new FMaliOC);
        MaliOCImpl->Initialize();
    }

    virtual void PreUnloadCallback() override
    {
        MaliOCImpl->Deinitialize();
        MaliOCImpl.Reset();
    }

    /** Module implementation */
    TSharedPtr<FMaliOC> MaliOCImpl;
public:
    FMaliOCModule() = default;

private:
    virtual ~FMaliOCModule() = default;
    FMaliOCModule(const FMaliOCModule&) = delete;
    FMaliOCModule(FMaliOCModule&&) = delete;
    FMaliOCModule& operator=(const FMaliOCModule&) = delete;
    FMaliOCModule& operator=(FMaliOCModule&&) = delete;
};

IMPLEMENT_MODULE(FMaliOCModule, MaliOC)

#undef LOCTEXT_NAMESPACE
