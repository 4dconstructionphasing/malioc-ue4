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
#include "MaliOCCompilerManager.h"

static const FString MOC_FTP_URL = TEXT("http://malideveloper.arm.com/downloads/tools/moc/5.3/");
static const FString EULA_URL = FPaths::Combine(*MOC_FTP_URL, TEXT("EULA.txt"));
static const FString OFFLINE_COMPILER_FOLDER_TO_EXTRACT = TEXT("Mali_Offline_Compiler_v5.3.0");

#if (PLATFORM_WINDOWS)
static const FString DLL_NAME = TEXT("compiler_manager.dll");
static const FString OFFLINE_COMPILER_DOWNLOAD_NAME = TEXT("Mali_Offline_Compiler_v5.3.0.1259ce_Windows_x64.zip");
#elif (PLATFORM_LINUX)
static const FString DLL_NAME = TEXT("libcompiler_manager.so");
static const FString OFFLINE_COMPILER_DOWNLOAD_NAME = TEXT("Mali_Offline_Compiler_v5.3.0.1259ce_Linux_x64.tgz");
#elif (PLATFORM_MAC)
static const FString DLL_NAME = TEXT("libcompiler_manager.dylib");
static const FString OFFLINE_COMPILER_DOWNLOAD_NAME = TEXT("Mali_Offline_Compiler_v5.3.0.1259ce_MacOSX_x64.tgz");
#endif

static const FString OFFLINE_COMPILER_DOWNLOAD_URL = FPaths::Combine(*MOC_FTP_URL, *OFFLINE_COMPILER_DOWNLOAD_NAME);

static const FString MaliOC_FOLDER_PATH = FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::EnginePluginsDir(), TEXT("Editor/"), TEXT("MaliOC/")));

static const FString FULL_COMPILER_PATH = FPaths::Combine(*MaliOC_FOLDER_PATH, *OFFLINE_COMPILER_FOLDER_TO_EXTRACT);

static const FString FULL_DLL_PATH = FPaths::Combine(*FULL_COMPILER_PATH, *DLL_NAME);

malicm_version FCompilerManager::GetExpectedCompilerManagerVersion()
{
    malicm_version version;
    version.major = 4u;
    version.minor = 0u;
    version.patch = 1u;
    return version;
}

const FString& FCompilerManager::GetMaliOCFolderPath()
{
    return MaliOC_FOLDER_PATH;
}

const FString& FCompilerManager::GetFullCompilerPath()
{
    return FULL_COMPILER_PATH;
}

const FString& FCompilerManager::GetDLLName()
{
    return DLL_NAME;
}

const FString& FCompilerManager::GetFullDLLPath()
{
    return FULL_DLL_PATH;
}

const FString& FCompilerManager::GetEULADownloadURL()
{
    return EULA_URL;
}

const FString& FCompilerManager::GetOfflineCompilerDownloadName()
{
    return OFFLINE_COMPILER_DOWNLOAD_NAME;
}

const FString& FCompilerManager::GetOfflineCompilerDownloadURL()
{
    return OFFLINE_COMPILER_DOWNLOAD_URL;
}

const FString& FCompilerManager::GetOfflineCompilerFolderToExtract()
{
    return OFFLINE_COMPILER_FOLDER_TO_EXTRACT;
}

bool FCompilerManager::Initialize(bool Silent)
{
    // Abort if we're double initialising, as it's probably a symptom of erroneous programming somewhere
    check(!CompilerManager.IsValid());
    CompilerManager = MakeShareable(new FCompilerManager);

    if (!CompilerManager->bIsValid)
    {
        if (!Silent)
        {
            UE_LOG(MaliOfflineCompiler, Error, TEXT("Failed to load compiler manager DLL"));
        }
        CompilerManager.Reset();
        return false;
    }

    const FString CompilerPath = GetFullCompilerPath();

    bool success = CompilerManager->_malicm_initialize_libraries(TCHAR_TO_ANSI(*CompilerPath));

    if (!success)
    {
        if (!Silent)
        {
            UE_LOG(MaliOfflineCompiler, Error, TEXT("Could not initialize compiler libraries"));
        }
        CompilerManager.Reset();
        return false;
    }

    malicm_version version;
    CompilerManager->_malicm_get_manager_version(&version);
    const malicm_version expectedVersion = GetExpectedCompilerManagerVersion();
    if (version.major != expectedVersion.major || version.minor != expectedVersion.minor || version.patch != expectedVersion.patch)
    {
        if (!Silent)
        {
            UE_LOG(MaliOfflineCompiler, Error, TEXT("Compiler manager version is not expected version"));
        }
        CompilerManager.Reset();
        return false;
    }

    return true;
}

void FCompilerManager::Deinitialize()
{
    if (CompilerManager.IsValid())
    {
        CompilerManager->_malicm_release_libraries();
        CompilerManager.Reset();
    }
}

const FCompilerManager* FCompilerManager::Get()
{
    return CompilerManager.Get();
}

bool FCompilerManager::CompilerManagerDLLExists()
{
    const FString DLLPath = GetFullDLLPath();

    return FPaths::FileExists(DLLPath);
}

TSharedPtr<class FCompilerManager> FCompilerManager::CompilerManager = nullptr;

FCompilerManager::~FCompilerManager()
{
    if (DLLHandle != nullptr)
    {
        FPlatformProcess::FreeDllHandle(DLLHandle);
    }
}

FCompilerManager::FCompilerManager()
{
    if (!CompilerManagerDLLExists())
    {
        return;
    }

    DLLHandle = FPlatformProcess::GetDllHandle(*GetFullDLLPath());

    if (DLLHandle == nullptr)
    {
        return;
    }

    bool allLoadedSuccessfully = true;

#define LOAD_FUNCTION(HANDLE_NAME) _ ## HANDLE_NAME = (decltype(HANDLE_NAME)*)FPlatformProcess::GetDllExport(DLLHandle, TEXT(#HANDLE_NAME)); allLoadedSuccessfully = allLoadedSuccessfully && (( _ ## HANDLE_NAME ) != nullptr);

    LOAD_FUNCTION(malicm_initialize_libraries);
    LOAD_FUNCTION(malicm_release_libraries);
    LOAD_FUNCTION(malicm_get_manager_version);
    LOAD_FUNCTION(malicm_release_compiler_outputs);
    LOAD_FUNCTION(malicm_get_driver_name);
    LOAD_FUNCTION(malicm_get_core_name);
    LOAD_FUNCTION(malicm_get_core_revision);
    LOAD_FUNCTION(malicm_is_binary_output_supported);
    LOAD_FUNCTION(malicm_is_prerotate_supported);
    LOAD_FUNCTION(malicm_get_api_name);
    LOAD_FUNCTION(malicm_get_highest_api_version);
    LOAD_FUNCTION(malicm_get_extensions);
    LOAD_FUNCTION(malicm_get_compilers);
    LOAD_FUNCTION(malicm_release_compilers);
    LOAD_FUNCTION(malicm_compile);

#undef LOAD_FUNCTION

    if (!allLoadedSuccessfully)
    {
        return;
    }

    bIsValid = true;
}
