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

#pragma once
#include "MaliSCPrivatePCH.h"
#include "compiler_manager/compiler_manager.h"

/* Hierarchy is Core->Revision->Driver->Platform as this is how we display things in the UI */
class FMaliCoreRevision;
class FMaliDriver;
class FMaliPlatform;

/** A Mali Core (i.e. Mali-400, Mali-T600, etc.)*/
class FMaliCore final
{
public:
    FMaliCore(const FString& MaliCoreName);
    ~FMaliCore() = default;
    FMaliCore(const FMaliCore&) = delete;
    FMaliCore(FMaliCore&&) = delete;
    FMaliCore& operator=(const FMaliCore&) = delete;
    FMaliCore& operator=(FMaliCore&&) = delete;

    /** Add a new revision to this core with the given params */
    void AddRevision(const FString& RevisionName, const FString& DriverName, malicm_compiler Compiler, unsigned int MaxAPI, const FString Extensions, const FString& PlatformName, EShaderPlatform Platform);
    const FString& GetName() const;
    /** Get an array of the revisions that this core has */
    const TArray<TUniqueObj<FMaliCoreRevision>>& GetRevisions() const;
private:
    const FString CoreName;
    TArray<TUniqueObj<FMaliCoreRevision>> Revisions;
};

/** A Mali Core Revision (i.e. Mali-T600 r0p1, etc.) */
class FMaliCoreRevision final
{
public:
    FMaliCoreRevision(const FString& MaliRevisionName, const FMaliCore& MaliCore);
    ~FMaliCoreRevision() = default;
    FMaliCoreRevision(const FMaliCoreRevision&) = delete;
    FMaliCoreRevision(FMaliCoreRevision&&) = delete;
    FMaliCoreRevision& operator=(const FMaliCoreRevision&) = delete;
    FMaliCoreRevision& operator=(FMaliCoreRevision&&) = delete;

    /** Add a new driver to this core revision with the given params */
    void AddDriver(const FString& DriverName, malicm_compiler Compiler, unsigned int MaxAPI, const FString Extensions, const FString& PlatformName, EShaderPlatform Platform);
    const FString& GetName() const;
    /** Get the core that this revision belongs to */
    const FMaliCore& GetCore() const;
    /** Get an array of the drivers that this core revision has */
    const TArray<TUniqueObj<FMaliDriver>>& GetDrivers() const;
private:
    const FString RevisionName;
    const FMaliCore& Core;
    TArray<TUniqueObj<FMaliDriver>> Drivers;
};

/** A Mali Core Revision Driver (i.e. Mali-T600_r5p0-00rel0, etc.) */
class FMaliDriver final
{
public:
    FMaliDriver(const FString& MaliDriverName, const FMaliCoreRevision& MaliRevision, malicm_compiler MaliCompiler, unsigned int MaliMaxAPI, const FString MaliExtensions);
    ~FMaliDriver() = default;
    FMaliDriver(const FMaliDriver&) = delete;
    FMaliDriver(FMaliDriver&&) = delete;
    FMaliDriver& operator=(const FMaliDriver&) = delete;
    FMaliDriver& operator=(FMaliDriver&&) = delete;

    /** Add a new shader platform to this core revision with the given params */
    void AddShaderPlatform(const FString& PlatformName, EShaderPlatform Platform);
    const FString& GetName() const;
    /** Get the core revision that this driver belongs to */
    const FMaliCoreRevision& GetRevision() const;
    /** Get the compiler for this core revision and driver*/
    malicm_compiler GetCompiler() const;
    /** Get the highest GLES API version this supports. 100 means ES 2.0, 300 means ES 3.0, 310 means ES 3.1 */
    unsigned int GetMaxAPI() const;
    /** Get an array of the platforms that this core revision and driver supports */
    const TArray<TUniqueObj<FMaliPlatform>>& GetPlatforms() const;
    /** Get a space separated list of the extensions this core revision driver supports */
    const FString& GetExtensions() const;
private:
    const FString DriverName;
    const FMaliCoreRevision& Revision;
    const malicm_compiler Compiler;
    TArray<TUniqueObj<FMaliPlatform>> Platforms;
    const unsigned int MaxAPI;
    const FString Extensions;
};

/** A Mali Platform (One of either OpenGL ES 2.0 or OpenGL ES 3.1 AEP ) */
class FMaliPlatform final
{
public:
    FMaliPlatform(const FString& MaliPlatformName, const FMaliDriver& MaliDriver, EShaderPlatform MaliPlatform);
    ~FMaliPlatform() = default;
    FMaliPlatform(const FMaliPlatform&) = delete;
    FMaliPlatform(FMaliPlatform&&) = delete;
    FMaliPlatform& operator=(const FMaliPlatform&) = delete;
    FMaliPlatform& operator=(FMaliPlatform&&) = delete;

    const FString& GetName() const;
    /** Get the driver that this platform belongs to */
    const FMaliDriver& GetDriver() const;
    /** Get the UE4 platform that corresponds to this Mali Platform */
    EShaderPlatform GetPlatform() const;
private:
    const FString PlatformName;
    const FMaliDriver& Driver;
    const EShaderPlatform Platform;
};

/** Raw output of the shader compiler (parsed into a nice structure) */
struct FMaliSCRawCompilerOutput
{
    struct FCommonOutput
    {
        FString ShaderName;
        EShaderFrequency Frequency;
        FString VertexFactoryName;
        FString SourceCode;
        TArray<FString> Warnings;
    };

    struct FErrorOutput
    {
        FCommonOutput CommonOutput;
        TArray<FString> Errors;
    };

    struct FMidgardOutput
    {
        struct FRenderTarget
        {
            int render_target = 0;

            int work_registers_used = 0;
            int uniform_registers_used = 0;

            float arithmetic_cycles = 0;
            float arithmetic_shortest_path = 0;
            float arithmetic_longest_path = 0;
            float load_store_cycles = 0;
            float load_store_shortest_path = 0;
            float load_store_longest_path = 0;
            float texture_cycles = 0;
            float texture_shortest_path = 0;
            float texture_longest_path = 0;
            bool spilling_used = false;
        };

        FCommonOutput CommonOutput;
        TArray<FRenderTarget> RenderTargets;
    };

    struct FUtgardOutput
    {
        FCommonOutput CommonOutput;
        int32 min_number_of_cycles = 0;
        int32 max_number_of_cycles = 0;
        int32 n_instruction_words = 0;
    };

    TArray<FErrorOutput> ErrorOutput;
    TArray<FMidgardOutput> MidgardOutput;
    TArray<FUtgardOutput> UtgardOutput;
};

/** Compilation job handle. Used to start a job */
class FCompileJobHandle final : private FRunnable
{
public:
    /** Return true if compilation has completed */
    bool IsCompilationFinished() const
    {
        return bIsCompilationComplete;
    }

    /** Return the total number of shaders to be compiled */
    uint32 GetTotalShaders() const
    {
        return TotalNumShaders;
    }

    /** Return the number of shaders that have already been compiled */
    uint32 GetNumCompiledShaders() const
    {
        return NumCompiledShaders.GetValue();
    }

    /**
     * @return the raw output of the compiler (packaged into a convenient data structure).
     * IsCompilationFinished() must have returned true before it is valid to call this function, else an assertion is triggered
     */
    TSharedRef<const FMaliSCRawCompilerOutput> GetRawCompilerOutput() const
    {
        check(IsCompilationFinished());
        return RawCompilerOutput;
    }

    virtual ~FCompileJobHandle() override
    {
        if (Thread)
        {
            // Wait for the thread to complete, if it's still running
            Thread->Kill(true);
            delete Thread;
        }
    }

    FCompileJobHandle(const FCompileJobHandle&) = delete;
    FCompileJobHandle(FCompileJobHandle&&) = delete;
    FCompileJobHandle& operator=(const FCompileJobHandle&) = delete;
    FCompileJobHandle& operator=(FCompileJobHandle&&) = delete;

private:
    // We want only the async compiler to be able to create handles and start compilations
    friend class FAsyncCompiler;

    FCompileJobHandle(TRefCountPtr<FMaterialShaderMap> MaterialShaderMap, const FMaliPlatform& MaliPlatform) :
        ShaderMap(MaterialShaderMap),
        Platform(MaliPlatform),
        RawCompilerOutput(MakeShareable(new FMaliSCRawCompilerOutput))
    {
        // Get the list of shaders from the shader map and put them in OutShaders
        ShaderMap->GetShaderList(OutShaders);
        TotalNumShaders = OutShaders.Num();
    }

    virtual uint32 Run() override;

    virtual void Exit() override;

    /** Begin compilation on another thread. */
    void BeginCompilationAsync();

    /** Add another compiler output to our compiler output array */
    void AppendNewRawCompilerOutput(bool bCompilerRan, malioc_outputs& outputs, const char* GLSL, FShader* Shader);

    /** Shader map from the material we're compiling for */
    TRefCountPtr<FMaterialShaderMap> ShaderMap;
    /** List of shaders in the shader map */
    TMap< FShaderId, FShader* > OutShaders;
    /** Mali platform (core, revision, driver and API) we're compiling for */
    const FMaliPlatform& Platform;
    /** Raw output of the shader compiler*/
    TSharedRef<FMaliSCRawCompilerOutput> RawCompilerOutput;
    /** Thread we perform compilation on */
    FRunnableThread* Thread;
    /** The total number of shaders we'll be compiling */
    uint32 TotalNumShaders;
    /** Threadsafe counter that will be incremented by the compiling thread and read by the UI thread */
    FThreadSafeCounter NumCompiledShaders = 0;
    /** True when compilation is complete */
    bool bIsCompilationComplete = false;

    /** Job counter used to give each thread a unique ID*/
    static uint32 JobCounter;
};

class FAsyncCompiler final : private FTickableEditorObject
{
public:
    /**
     * Initialize the async compiler. Also initialises the compiler manager.
     * @param Silent if true, suppress error log output on failure
     * @return true if initialization success
     */
    static bool Initialize(bool Silent = false);

    /**
     * Deinit the compiler. Safe to call even if initialization failed. Will gracefully clean up any pending jobs.
     * Also deinits the compiler manager.
     */
    static void Deinitialize();

    /** @return a valid pointer to the async compiler if init success, else nullptr */
    static FAsyncCompiler* Get();

    /**
     * Constructs and adds a new job to the queue.
     * @param ShaderMap the material shader map
     * @param Platform the Mali platform to compile for
     * @return a shared ref to the handle of the job that can be used to track progress
     */
    TSharedRef<const FCompileJobHandle> AddJob(TRefCountPtr<FMaterialShaderMap> ShaderMap, const FMaliPlatform& Platform);

    /** Block until all compilation has completed */
    void FinishCompilation();

    /**
     * @return the list of all cores that can be used for compilation.
     * All cores and all their core revisions, drivers and platforms will remain allocated in memory until the Async Compiler is deinitialized
     * (i.e. when the MaliSC module unloads)
     */
    const TArray<TUniqueObj<FMaliCore>>& GetCores() const;

    ~FAsyncCompiler() = default;
    FAsyncCompiler(const FAsyncCompiler&) = delete;
    FAsyncCompiler(FAsyncCompiler&&) = delete;
    FAsyncCompiler& operator=(const FAsyncCompiler&) = delete;
    FAsyncCompiler& operator=(FAsyncCompiler&&) = delete;
private:
    FAsyncCompiler();

    /** Compiler singleton */
    static TSharedPtr<class FAsyncCompiler> AsyncCompiler;

    /** Compiler's current job, if it has one */
    TSharedPtr<FCompileJobHandle> CurrentJob = nullptr;
    /** Single producer single consumer queue of all outstanding jobs */
    TQueue<TSharedPtr<FCompileJobHandle>, EQueueMode::Type::Spsc> Jobs;
    /** Array of all Mali cores we can compile for, and their revisions, drivers, and supported APIs */
    TArray<TUniqueObj<FMaliCore>> MaliCores;

    // FTickableEditorObject functions

    /** Compiler tick */
    virtual void Tick(float DeltaTime) override;

    virtual bool IsTickable() const override
    {
        return true;
    }

    virtual TStatId GetStatId() const override
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncCompiler, STATGROUP_Tickables);
    }
};
