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
#include "MaliOCAsyncCompiler.h"
#include "MaliOCCompilerManager.h"

// Copied from various GL headers. Elected to copy this in rather than deal with unpleasant cross-platform ifdeffery
// OpenGLShaders.h has dependencies on various GL headers and relies on the including source file to resolve them
typedef unsigned int GLenum;
typedef int GLint;
typedef unsigned int GLuint;
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_TESS_EVALUATION_SHADER 0x8E87
#define GL_TESS_CONTROL_SHADER 0x8E88
#define GL_GEOMETRY_SHADER 0x8DD9
#define GL_COMPUTE_SHADER 0x91B9
#include "OpenGLShaders.h"

TSharedPtr<class FAsyncCompiler> FAsyncCompiler::AsyncCompiler = nullptr;

/**
 * @param CoreName name of the core
 * @return true if specified core is blacklisted (for compatibility or deprecation reasons)
 */
bool IsCoreBlacklisted(const FString& CoreName)
{
    return false;
}

/**
* @param DriverName name of the driver
* @return true if specified driver is blacklisted (for compatibility or deprecation reasons)
*/
bool IsDriverBlacklisted(const FString& DriverName)
{
    return false;
}

bool FAsyncCompiler::Initialize(bool Silent)
{
    // Abort if we're double initialising, as it's probably a symptom of erroneous programming somewhere
    check(!AsyncCompiler.IsValid());
    bool success = FCompilerManager::Initialize(Silent);

    if (!success)
    {
        if (!Silent)
        {
            UE_LOG(MaliOfflineCompiler, Error, TEXT("Could not initialise Async Compiler as compiler libraries were not successfully loaded"));
        }
        return false;
    }

    AsyncCompiler = MakeShareable(new FAsyncCompiler);

    if (AsyncCompiler->GetCores().Num() == 0)
    {
        // No point of having a compiler if we don't have any compilation targets
        AsyncCompiler.Reset();
        return false;
    }

    return true;
}

void FAsyncCompiler::Deinitialize()
{
    if (AsyncCompiler.IsValid())
    {
        // Null the current job
        // This should run its destructor, which will safely kill the job
        // This MUST be done before we deinit the compiler manager, else the compiler DLL will be released while
        // the job is still using it
        AsyncCompiler->CurrentJob = nullptr;
        AsyncCompiler.Reset();
    }
    FCompilerManager::Deinitialize();
}

FAsyncCompiler* FAsyncCompiler::Get()
{
    return AsyncCompiler.Get();
}

FAsyncCompiler::FAsyncCompiler()
{
    const FCompilerManager* compilerManager = FCompilerManager::Get();
    check(compilerManager != nullptr);

    // Get an exhaustive list of all compilers
    malicm_compiler* compilers;
    unsigned int numCompilers = 0;
    compilerManager->_malicm_get_compilers(&compilers, &numCompilers, nullptr, nullptr, nullptr, "openglessl", nullptr, 0);

    // Create the list of compilers and its data
    // For each compiler, check if we haven't blacklisted it and add it to the list of cores we support

    for (unsigned int i = 0; i < numCompilers; i++)
    {
        const malicm_compiler compiler = compilers[i];
        const FString coreName = compilerManager->_malicm_get_core_name(compilers[i]);
        const FString revisionName = compilerManager->_malicm_get_core_revision(compilers[i]);
        const FString driverName = compilerManager->_malicm_get_driver_name(compilers[i]);
        const unsigned int MaxAPI = compilerManager->_malicm_get_highest_api_version(compilers[i]);

        if (IsCoreBlacklisted(coreName))
        {
            continue;
        }
        if (IsDriverBlacklisted(driverName))
        {
            continue;
        }

        TUniqueObj<FMaliCore>* core = nullptr;

        // See if we already have a core with the same name as the current compiler core
        for (TUniqueObj<FMaliCore>& maliCore : MaliCores)
        {
            if (maliCore->GetName().Equals(coreName))
            {
                core = &maliCore;
                break;
            }
        }

        // If we didn't have a core with the same name, make one
        if (core == nullptr)
        {
            core = &MaliCores[MaliCores.Emplace(coreName)];
        }

        const FString Extensions = compilerManager->_malicm_get_extensions(compiler);

        // The AEP platform will be enabled in some future release
        if (false)
        {
            // Add AEP if supported
            if (Extensions.Contains(TEXT("GL_ANDROID_extension_pack_es31a")) && Extensions.Contains(TEXT("GL_EXT_color_buffer_half_float")))
            {
                (*core)->AddRevision(revisionName, driverName, compiler, MaxAPI, Extensions, FString(TEXT("OpenGL ES 3.1 AEP")), EShaderPlatform::SP_OPENGL_ES31_EXT);
            }
        }

        // GLES2 is always supported
        (*core)->AddRevision(revisionName, driverName, compiler, MaxAPI, Extensions, FString(TEXT("OpenGL ES 2.0")), EShaderPlatform::SP_OPENGL_ES2_ANDROID);
    }

    compilerManager->_malicm_release_compilers(&compilers, numCompilers);
}

void FAsyncCompiler::Tick(float DeltaTime)
{
    // The job we were running is now complete. Release the reference
    if (CurrentJob.IsValid() && CurrentJob->IsCompilationFinished())
    {
        CurrentJob = nullptr;
    }

    // No running job, but there are pending jobs. Let's start one
    if (!CurrentJob.IsValid() && !Jobs.IsEmpty())
    {
        Jobs.Dequeue(CurrentJob);
        CurrentJob->BeginCompilationAsync();
    }
}

TSharedRef<const FCompileJobHandle> FAsyncCompiler::AddJob(TRefCountPtr<FMaterialShaderMap> ShaderMap, const FMaliPlatform& Platform)
{
    // Create the job handle and add it to the job queue. New jobs get set in motion in Tick()
    TSharedRef<FCompileJobHandle> handle = MakeShareable(new FCompileJobHandle(ShaderMap, Platform));

    Jobs.Enqueue(handle);

    return handle;
}

void FAsyncCompiler::FinishCompilation()
{
    // Ticking here makes sure that, if something was waiting in the queue, it moves into the current job slot
    Tick(0.0f);
    while (CurrentJob.IsValid())
    {
        FPlatformProcess::Sleep(0.01f);
        Tick(0.0f);
    }
}

const TArray<TUniqueObj<FMaliCore>>& FAsyncCompiler::GetCores() const
{
    return MaliCores;
}

/** Get the GL Device capabilities for the specified Mali platform, used for processing the GLSL outputted by the cross compiler */
void GetMaliPlatformOpenGLShaderDeviceCapabilities(const FMaliPlatform& Platform, FOpenGLShaderDeviceCapabilities& Capabilities)
{
    FMemory::Memzero(Capabilities);

    const FString& ExtensionsString = Platform.GetDriver().GetExtensions();

    Capabilities.TargetPlatform = EOpenGLShaderTargetPlatform::OGLSTP_Android;
    Capabilities.MaxRHIShaderPlatform = Platform.GetPlatform();

    Capabilities.bUseES30ShadingLanguage = Platform.GetDriver().GetMaxAPI() >= 300;

    // This one is desktop GL only for now. Support will need to be added when this is no longer true.
    Capabilities.bSupportsSeparateShaderObjects = false;

    Capabilities.bSupportsStandardDerivativesExtension = Platform.GetDriver().GetMaxAPI() >= 300 || ExtensionsString.Contains(TEXT("GL_OES_standard_derivatives"));

    Capabilities.bSupportsRenderTargetFormat_PF_FloatRGBA = ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_half_float"));
    Capabilities.bSupportsShaderFramebufferFetch = ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch")) || ExtensionsString.Contains(TEXT("GL_NV_shader_framebuffer_fetch")) || ExtensionsString.Contains(TEXT("GL_ARM_shader_framebuffer_fetch"));

    // These two deal with bugs that are not present in the supported compilers, so we can just set them to false
    Capabilities.bRequiresDontEmitPrecisionForTextureSamplers = false;
    Capabilities.bRequiresTextureCubeLodEXTToTextureCubeLodDefine = false;

    // This should be true for all Mali-400 platforms (and false for all GLES3 platforms, as textureLOD is in core GLES3)
    Capabilities.bSupportsShaderTextureLod = ExtensionsString.Contains(TEXT("GL_EXT_shader_texture_lod"));

    // If the Renderer String is detected to contain "Mali-400", SupportsShaderTextureCubeLod is set to false, regardless of whether GL_EXT_shader_texture_lod is supported
    // The bug this works around isn't present in the supported compilers, but maintain the behaviour anyway so we get accurate statistics
    Capabilities.bSupportsShaderTextureCubeLod = !Platform.GetDriver().GetRevision().GetCore().GetName().Contains(TEXT("Mali-400"));

    // Only required on platforms where 8 or fewer varyings are available. Mali-400 supports at least 12, and Midgard even more.
    Capabilities.bRequiresGLFragCoordVaryingLimitHack = false;
    // This information isn't stored anywhere in the offline compiler (though we could find it out by brute force), so just set it to 12. It won't be used either way.
    Capabilities.MaxVaryingVectors = 12;

    // Not required on Mali
    Capabilities.bRequiresTexture2DPrecisionHack = false;
}

uint32 FCompileJobHandle::JobCounter(0);

void FCompileJobHandle::BeginCompilationAsync()
{
    // All threads are launched from the UI thread, so we don't need to worry about JobCounter being non atomic
    Thread = FRunnableThread::Create(this, *FString::Printf(TEXT("MaliOCCompileJob %d"), JobCounter));
    JobCounter += 1;
}

void FCompileJobHandle::Exit()
{
    bIsCompilationComplete = true;
}

uint32 FCompileJobHandle::Run()
{
    for (const TPair<FShaderId, FShader*> shader : OutShaders)
    {
        const EShaderFrequency freq = shader.Value->GetType()->GetFrequency();

        // We only support vertex and fragment shaders for now
        if (freq != EShaderFrequency::SF_Pixel && freq != EShaderFrequency::SF_Vertex)
        {
            FMaliOCRawCompilerOutput::FErrorOutput error;
            error.CommonOutput.ShaderName = shader.Value->GetType()->GetName();
            error.CommonOutput.Frequency = freq;
            error.Errors.Add(TEXT("Cross compiler produced invalid output"));
            error.Errors.Add(TEXT("The shader type is neither fragment nor vertex"));
            RawCompilerOutput->ErrorOutput.Add(MoveTemp(error));
        }
        else
        {
            // Extract the GLSL code from the shader
            const TArray<uint8>& code = shader.Value->GetCode();

            FShaderCodeReader ShaderCode(code);
            FMemoryReader Ar(code, true);

            Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());

            FOpenGLCodeHeader Header = { 0 };

            Ar << Header;

            const int32 CodeOffset = Ar.Tell();

            TArray<ANSICHAR> GlslCodeOriginal;
            const ANSICHAR* source = (ANSICHAR*)code.GetData() + CodeOffset;
            GlslCodeOriginal.Append(source, FCStringAnsi::Strlen(source) + 1);

            const GLenum TypeEnum = [&]() -> GLenum
            {
                if (freq == EShaderFrequency::SF_Vertex) return GL_VERTEX_SHADER;
                if (freq == EShaderFrequency::SF_Pixel) return GL_FRAGMENT_SHADER;
                check(false);
                return 0;
            }();

            // Get the capabilites that the selected Mali device supports
            FOpenGLShaderDeviceCapabilities Capabilities;
            GetMaliPlatformOpenGLShaderDeviceCapabilities(Platform, Capabilities);

            // Take the generic GLSL that came out of the cross compiler and add in whatever hacks/workarounds/special features are required for the best results for the Mali core
            TArray<ANSICHAR> GlslCode;
            GLSLToDeviceCompatibleGLSL(GlslCodeOriginal, Header.ShaderName, TypeEnum, Capabilities, GlslCode);

            // Take the specialized shader code and run it through the offline compiler
            const char* type = nullptr;
            if (freq == EShaderFrequency::SF_Pixel)
            {
                type = "fragment";
            }
            else if (freq == EShaderFrequency::SF_Vertex)
            {
                type = "vertex";
            }

            malioc_outputs outputs;

            const bool ran = FCompilerManager::Get()->_malicm_compile(&outputs, GlslCode.GetData(), type, nullptr, 0, false, false, nullptr, 0, Platform.GetDriver().GetCompiler());

            // Handle the output of the compiler
            AppendNewRawCompilerOutput(ran, outputs, GlslCode.GetData(), shader.Value);

            FCompilerManager::Get()->_malicm_release_compiler_outputs(&outputs);
        }

        NumCompiledShaders.Increment();
    }
    return 0;
}

FMaliOCRawCompilerOutput::FMidgardOutput ParseMidgardFlexibleOutputs(const malioc_key_value_pairs* const flexible_outputs, unsigned int size)
{
    FMaliOCRawCompilerOutput::FMidgardOutput output;

    for (unsigned int i = 0; i < size; ++i)
    {
        FMaliOCRawCompilerOutput::FMidgardOutput::FRenderTarget rt;

        /* Pull out the data from the flexible output list. */
        for (unsigned int j = 0; j < flexible_outputs[i].number_of_entries - 1; j += 2)
        {
            if (FCStringAnsi::Strcmp(flexible_outputs[i].list[j], "render_target") == 0)
            {
                rt.render_target = FCStringAnsi::Atoi(flexible_outputs[i].list[j + 1]);
            }
            else if (FCStringAnsi::Strcmp(flexible_outputs[i].list[j], "work_registers_used") == 0)
            {
                rt.work_registers_used = FCStringAnsi::Atoi(flexible_outputs[i].list[j + 1]);
            }
            else if (FCStringAnsi::Strcmp(flexible_outputs[i].list[j], "uniform_registers_used") == 0)
            {
                rt.uniform_registers_used = FCStringAnsi::Atoi(flexible_outputs[i].list[j + 1]);
            }
            else if (FCStringAnsi::Strcmp(flexible_outputs[i].list[j], "arithmetic_cycles") == 0)
            {
                rt.arithmetic_cycles = FCStringAnsi::Atof(flexible_outputs[i].list[j + 1]);
            }
            else if (FCStringAnsi::Strcmp(flexible_outputs[i].list[j], "arithmetic_shortest_path") == 0)
            {
                rt.arithmetic_shortest_path = FCStringAnsi::Atof(flexible_outputs[i].list[j + 1]);
            }
            else if (FCStringAnsi::Strcmp(flexible_outputs[i].list[j], "arithmetic_longest_path") == 0)
            {
                rt.arithmetic_longest_path = FCStringAnsi::Atof(flexible_outputs[i].list[j + 1]);
            }
            else if (FCStringAnsi::Strcmp(flexible_outputs[i].list[j], "load_store_cycles") == 0)
            {
                rt.load_store_cycles = FCStringAnsi::Atof(flexible_outputs[i].list[j + 1]);
            }
            else if (FCStringAnsi::Strcmp(flexible_outputs[i].list[j], "load_store_shortest_path") == 0)
            {
                rt.load_store_shortest_path = FCStringAnsi::Atof(flexible_outputs[i].list[j + 1]);
            }
            else if (FCStringAnsi::Strcmp(flexible_outputs[i].list[j], "load_store_longest_path") == 0)
            {
                rt.load_store_longest_path = FCStringAnsi::Atof(flexible_outputs[i].list[j + 1]);
            }
            else if (FCStringAnsi::Strcmp(flexible_outputs[i].list[j], "texture_cycles") == 0)
            {
                rt.texture_cycles = FCStringAnsi::Atof(flexible_outputs[i].list[j + 1]);
            }
            else if (FCStringAnsi::Strcmp(flexible_outputs[i].list[j], "texture_shortest_path") == 0)
            {
                rt.texture_shortest_path = FCStringAnsi::Atof(flexible_outputs[i].list[j + 1]);
            }
            else if (FCStringAnsi::Strcmp(flexible_outputs[i].list[j], "texture_longest_path") == 0)
            {
                rt.texture_longest_path = FCStringAnsi::Atof(flexible_outputs[i].list[j + 1]);
            }
            else if (FCStringAnsi::Strcmp(flexible_outputs[i].list[j], "spilling_used") == 0)
            {
                rt.spilling_used = (FCStringAnsi::Strcmp(flexible_outputs[i].list[j + 1], "true") == 0);
            }
        }

        output.RenderTargets.Add(MoveTemp(rt));
    }

    return output;
}

FMaliOCRawCompilerOutput::FUtgardOutput ParseUtgardFlexibleOutputs(const malioc_key_value_pairs* const flexible_outputs, unsigned int size)
{
    FMaliOCRawCompilerOutput::FUtgardOutput output;

    /* Pull out the data from the flexible output list. */
    for (unsigned int i = 0; i < flexible_outputs[0].number_of_entries - 1; i += 2)
    {
        if (FCStringAnsi::Strcmp(flexible_outputs[0].list[i], "min_number_of_cycles") == 0)
        {
            output.min_number_of_cycles = FCStringAnsi::Atoi(flexible_outputs[0].list[i + 1]);
        }
        else if (FCStringAnsi::Strcmp(flexible_outputs[0].list[i], "max_number_of_cycles") == 0)
        {
            output.max_number_of_cycles = FCStringAnsi::Atoi(flexible_outputs[0].list[i + 1]);
        }
        else if (FCStringAnsi::Strcmp(flexible_outputs[0].list[i], "n_instruction_words") == 0)
        {
            output.n_instruction_words = FCStringAnsi::Atoi(flexible_outputs[0].list[i + 1]);
        }
    }

    return output;
}

/** Mappings between programmatic vertex factory name and pretty vertex factory name */
static const TMap<FString, FString> VertexFactoryPrettyNameMap = []()
{
    TMap<FString, FString> map;

    map.Add(TEXT("FLocalVertexFactory"), TEXT("Default Usage"));
    map.Add(TEXT("TGPUSkinVertexFactoryfalse"), TEXT("Used with Skeletal Mesh"));
    map.Add(TEXT("TGPUSkinVertexFactorytrue"), TEXT("Used with Skeletal Mesh"));
    map.Add(TEXT("FLandscapeVertexFactoryMobile"), TEXT("Used with Landscape"));
    map.Add(TEXT("FLandscapeVertexFactory"), TEXT("Used with Landscape"));
    map.Add(TEXT("FLandscapeXYOffsetVertexFactory"), TEXT("Used with Landscape"));
    map.Add(TEXT("FParticleSpriteVertexFactory"), TEXT("Used with Particle Sprites"));
    map.Add(TEXT("FGPUSpriteVertexFactory"), TEXT("Used with Particle Sprites"));
    map.Add(TEXT("FParticleBeamTrailVertexFactory"), TEXT("Used with Beam Trails"));
    map.Add(TEXT("FMeshParticleVertexFactory"), TEXT("Used with Mesh Particles"));
    map.Add(TEXT("FMeshParticleVertexFactoryEmulatedInstancing"), TEXT("Used with Mesh Particles"));
    map.Add(TEXT("TGPUSkinMorphVertexFactoryfalse"), TEXT("Used with Morph Targets"));
    map.Add(TEXT("FSplineMeshVertexFactory"), TEXT("Used with Spline Meshes"));
    map.Add(TEXT("FInstancedStaticMeshVertexFactory"), TEXT("Used with Instanced Static Meshes"));
    map.Add(TEXT("FEmulatedInstancedStaticMeshVertexFactory"), TEXT("Used with Instanced Static Meshes"));
    map.Add(TEXT("TGPUSkinMorphVertexFactorytrue"), TEXT("Used with Skeletal Mesh and Morph Targets"));
    map.Add(TEXT("TGPUSkinAPEXClothVertexFactoryfalse"), TEXT("Used with Skeletal Mesh and Clothing"));
    map.Add(TEXT("TGPUSkinAPEXClothVertexFactorytrue"), TEXT("Used with Skeletal Mesh and Clothing"));

    return map;
}();

/**
 * @param VertexFactoryName the vertex factory name
 * @return The pretty vertex factory name corresponding to VertexFactoryName
 */
FString GetBeautifiedVertexFactoryName(const FString& VertexFactoryName)
{
    const FString* prettyName = VertexFactoryPrettyNameMap.Find(VertexFactoryName);
    if (prettyName != nullptr)
    {
        return *prettyName;
    }

    check(VertexFactoryName.Len() == 0); // Programming error if we let through a case which has a name

    return TEXT("No Vertex Factory");
}

void FCompileJobHandle::AppendNewRawCompilerOutput(bool bCompilerRan, malioc_outputs& outputs, const char* GLSL, FShader* Shader)
{
    FMaliOCRawCompilerOutput::FCommonOutput commonOutput;
    commonOutput.ShaderName = Shader->GetType()->GetName();
    commonOutput.Frequency = Shader->GetType()->GetFrequency();
    FString VertexFactoryType;
    const auto* vft = Shader->GetVertexFactoryType();
    if (vft)
    {
        VertexFactoryType = vft->GetName();
    }

    commonOutput.VertexFactoryName = GetBeautifiedVertexFactoryName(VertexFactoryType);

    commonOutput.SourceCode = ANSI_TO_TCHAR(GLSL);

    // Add an error if the compiler didn't even run
    if (!bCompilerRan)
    {
        FMaliOCRawCompilerOutput::FErrorOutput error;
        error.CommonOutput = MoveTemp(commonOutput);
        error.Errors.Add(TEXT("Compiler could not be run"));
        RawCompilerOutput->ErrorOutput.Add(MoveTemp(error));
        return;
    }

    // outputs is only valid if the compiler actually ran
    for (unsigned int i = 0; i < outputs.number_of_warnings; i++)
    {
        commonOutput.Warnings.Add(ANSI_TO_TCHAR(outputs.warnings[i]));
    }

    // Add an error if the compiler returned any errors
    if (outputs.number_of_errors != 0)
    {
        FMaliOCRawCompilerOutput::FErrorOutput error;
        error.CommonOutput = MoveTemp(commonOutput);
        for (unsigned int i = 0; i < outputs.number_of_errors; i++)
        {
            error.Errors.Add(ANSI_TO_TCHAR(outputs.errors[i]));
        }
        RawCompilerOutput->ErrorOutput.Add(MoveTemp(error));
        return;
    }

    // There must be at least one output to print
    if (outputs.number_of_flexible_outputs == 0)
    {
        FMaliOCRawCompilerOutput::FErrorOutput error;
        error.CommonOutput = MoveTemp(commonOutput);
        error.Errors.Add(TEXT("No verbose output from compiler"));
        RawCompilerOutput->ErrorOutput.Add(MoveTemp(error));
        return;
    }

    // Each flexible output corresponds to one render target
    // Each flexible output has a list of key-value pairs. Keys have even indexes, and values odd

    enum class EMaliArch
    {
        None,
        Utgard,
        Midgard
    };

    EMaliArch outputArch = EMaliArch::None;

    // Check the very first flexible output for the architecture information
    for (unsigned int i = 0; i < outputs.flexible_outputs[0].number_of_entries - 1; i += 2)
    {
        if (strcmp(outputs.flexible_outputs[0].list[i], "architecture") == 0)
        {
            if (strcmp(outputs.flexible_outputs[0].list[i + 1], "midgard") == 0)
            {
                outputArch = EMaliArch::Midgard;
                break;
            }
            else if (strcmp(outputs.flexible_outputs[0].list[i + 1], "utgard") == 0)
            {
                outputArch = EMaliArch::Utgard;
                break;
            }
        }
    }

    const bool bNoArch = (outputArch == EMaliArch::None);
    const bool bUtgardTooManyRTs = (outputArch == EMaliArch::Utgard) && (outputs.number_of_flexible_outputs != 1);

    // Either we didn't find an architecture or we received an invalid format for Utgard
    if (bNoArch || bUtgardTooManyRTs)
    {
        FMaliOCRawCompilerOutput::FErrorOutput error;
        error.CommonOutput = MoveTemp(commonOutput);
        error.Errors.Add(TEXT("Unknown verbose output format from compiler"));
        RawCompilerOutput->ErrorOutput.Add(MoveTemp(error));
        return;
    }

    // We have valid output. Create either a midgard or utgard report depending on the output
    switch (outputArch)
    {
    case EMaliArch::Midgard:
    {
        FMaliOCRawCompilerOutput::FMidgardOutput MidgardOutput = ParseMidgardFlexibleOutputs(outputs.flexible_outputs, outputs.number_of_flexible_outputs);
        MidgardOutput.CommonOutput = MoveTemp(commonOutput);
        RawCompilerOutput->MidgardOutput.Add(MoveTemp(MidgardOutput));
        break;
    }
    case EMaliArch::Utgard:
    {
        FMaliOCRawCompilerOutput::FUtgardOutput UtgardOutput = ParseUtgardFlexibleOutputs(outputs.flexible_outputs, outputs.number_of_flexible_outputs);
        UtgardOutput.CommonOutput = MoveTemp(commonOutput);
        RawCompilerOutput->UtgardOutput.Add(MoveTemp(UtgardOutput));
        break;
    }
    default:
    {
        check(false);
    }
    }
}

FMaliCore::FMaliCore(const FString& MaliCoreName) :
CoreName(MaliCoreName)
{
}

void FMaliCore::AddRevision(const FString& RevisionName, const FString& DriverName, malicm_compiler Compiler, unsigned int MaxAPI, const FString Extensions, const FString& PlatformName, EShaderPlatform Platform)
{
    TUniqueObj<FMaliCoreRevision>* Rev = nullptr;

    for (auto& Revision : Revisions)
    {
        if (Revision->GetName().Equals(RevisionName))
        {
            Rev = &Revision;
            break;
        }
    }

    if (Rev == nullptr)
    {
        Rev = &Revisions[Revisions.Emplace(RevisionName, *this)];
    }

    (*Rev)->AddDriver(DriverName, Compiler, MaxAPI, Extensions, PlatformName, Platform);
}

const FString& FMaliCore::GetName() const
{
    return CoreName;
}

const TArray<TUniqueObj<FMaliCoreRevision>>& FMaliCore::GetRevisions() const
{
    return Revisions;
}

FMaliCoreRevision::FMaliCoreRevision(const FString& MaliRevisionName, const FMaliCore& MaliCore) :
RevisionName(MaliRevisionName),
Core(MaliCore)
{
}

void FMaliCoreRevision::AddDriver(const FString& DriverName, malicm_compiler Compiler, unsigned int MaxAPI, const FString Extensions, const FString& PlatformName, EShaderPlatform Platform)
{
    TUniqueObj<FMaliDriver>* Dri = nullptr;

    for (auto& Driver : Drivers)
    {
        if (Driver->GetName().Equals(DriverName))
        {
            Dri = &Driver;
            break;
        }
    }

    if (Dri == nullptr)
    {
        Dri = &Drivers[Drivers.Emplace(DriverName, *this, Compiler, MaxAPI, Extensions)];
    }

    (*Dri)->AddShaderPlatform(PlatformName, Platform);
}

const FString& FMaliCoreRevision::GetName() const
{
    return RevisionName;
}

const FMaliCore& FMaliCoreRevision::GetCore() const
{
    return Core;
}

const TArray<TUniqueObj<FMaliDriver>>& FMaliCoreRevision::GetDrivers() const
{
    return Drivers;
}

FMaliDriver::FMaliDriver(const FString& MaliDriverName, const FMaliCoreRevision& MaliRevision, malicm_compiler MaliCompiler, unsigned int MaliMaxAPI, const FString MaliExtensions) :
DriverName(MaliDriverName),
Revision(MaliRevision),
Compiler(MaliCompiler),
MaxAPI(MaliMaxAPI),
Extensions(MaliExtensions)
{
}

void FMaliDriver::AddShaderPlatform(const FString& PlatformName, EShaderPlatform Platform)
{
    Platforms.Emplace(PlatformName, *this, Platform);
}

const FString& FMaliDriver::GetName() const
{
    return DriverName;
}
const FMaliCoreRevision& FMaliDriver::GetRevision() const
{
    return Revision;
}

malicm_compiler FMaliDriver::GetCompiler() const
{
    return Compiler;
}

unsigned int FMaliDriver::GetMaxAPI() const
{
    return MaxAPI;
}

const FString& FMaliDriver::GetExtensions() const
{
    return Extensions;
}

const TArray<TUniqueObj<FMaliPlatform>>& FMaliDriver::GetPlatforms() const
{
    return Platforms;
}

FMaliPlatform::FMaliPlatform(const FString& MaliPlatformName, const FMaliDriver& MaliDriver, EShaderPlatform MaliPlatform) :
PlatformName(MaliPlatformName),
Driver(MaliDriver),
Platform(MaliPlatform)
{
}

const FString& FMaliPlatform::GetName() const
{
    return PlatformName;
}

const FMaliDriver& FMaliPlatform::GetDriver() const
{
    return Driver;
}
EShaderPlatform FMaliPlatform::GetPlatform() const
{
    return Platform;
}
