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
#include "MaliOCPrivatePCH.h"
#include "compiler_manager/compiler_manager.h"

class FCompilerManager final
{
    // Static interface
public:
    /**
     * Initialize the compiler manager. If initialization failed, any calls to Get() will return nullptr.
     * @param Silent suppress error output if initialization failure
     * @return true if initialization success
     */
    static bool Initialize(bool Silent = false);

    /**
     * Deinit the compiler manager. Makes no assumptions about whether init succeeded or not
     * Make sure all compilation has finished before deinitialization, else we'll crash
     */
    static void Deinitialize();

    /* @return a valid pointer to the compiler manager if init success, else nullptr */
    static const FCompilerManager* Get();

    /** Expected compiler manager version */
    static malicm_version GetExpectedCompilerManagerVersion();

    /** @return true if the compiler manager DLL can be found on disk */
    static bool CompilerManagerDLLExists();

    /** @return the path to the OfflineCompiler folder inside the MaliOC folder */
    static const FString& GetFullCompilerPath();

    /** @return the name of the Compiler Manager DLL for this platform, including the extension */
    static const FString& GetDLLName();

    /** @return the full path of the Compiler Manager DLL for this platform, including the extension */
    static const FString& GetFullDLLPath();

    /** @return the full URL of the Offline Compiler EULA */
    static const FString& GetEULADownloadURL();

    /** @return the filename of the Offline Compiler download for this platform */
    static const FString& GetOfflineCompilerDownloadName();

    /** @return the full URL of the Offline Compiler download for this platform */
    static const FString& GetOfflineCompilerDownloadURL();

    /** @return the folder to extract from the Offline Compiler download */
    static const FString& GetOfflineCompilerFolderToExtract();

private:
    /** Compiler Manager singleton */
    static TSharedPtr<class FCompilerManager> CompilerManager;

    // Instance interface
public:
    decltype(malicm_initialize_libraries)* _malicm_initialize_libraries = nullptr;
    decltype(malicm_release_libraries)* _malicm_release_libraries = nullptr;
    decltype(malicm_get_manager_version)* _malicm_get_manager_version = nullptr;
    decltype(malicm_release_compiler_outputs)* _malicm_release_compiler_outputs = nullptr;
    decltype(malicm_get_driver_name)* _malicm_get_driver_name = nullptr;
    decltype(malicm_get_core_name)* _malicm_get_core_name = nullptr;
    decltype(malicm_get_core_revision)* _malicm_get_core_revision = nullptr;
    decltype(malicm_is_binary_output_supported)* _malicm_is_binary_output_supported = nullptr;
    decltype(malicm_is_prerotate_supported)* _malicm_is_prerotate_supported = nullptr;
    decltype(malicm_get_api_name)* _malicm_get_api_name = nullptr;
    decltype(malicm_get_highest_api_version)* _malicm_get_highest_api_version = nullptr;
    decltype(malicm_get_extensions)* _malicm_get_extensions = nullptr;
    decltype(malicm_get_compilers)* _malicm_get_compilers = nullptr;
    decltype(malicm_release_compilers)* _malicm_release_compilers = nullptr;
    decltype(malicm_compile)* _malicm_compile = nullptr;

    ~FCompilerManager();
    FCompilerManager(const FCompilerManager&) = delete;
    FCompilerManager(FCompilerManager&&) = delete;
    FCompilerManager& operator=(const FCompilerManager&) = delete;
    FCompilerManager& operator=(FCompilerManager&&) = delete;

private:
    FCompilerManager();

    /** Whether the compiler manager was successfully initialized */
    bool bIsValid = false;
    /** Handle to the DLL*/
    void* DLLHandle = nullptr;
};
