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

/**
 * \file compiler_manager.h
 * \brief An API for selecting and using offline compilers
 * \details Searches predefined folders for any libraries implementing the
 *          malioc API and provides functions for picking which library to use
 *          as well as the hardware core to compile for.
 * \version 1.0.0
 */

#ifndef COMPILER_MANAGER_H
#define COMPILER_MANAGER_H

#include "malioc_api.h"

#if defined COMPILER_MANAGER_LIBRARY
    #if defined _WIN32
        #define MALIOC_COMPILER_MANAGER_EXPORT __declspec(dllexport)
    #else
        #define MALIOC_COMPILER_MANAGER_EXPORT __attribute__((visibility("default")))
    #endif
#else
    #define MALIOC_COMPILER_MANAGER_EXPORT
#endif

/* An incomplete type which represents compilers. */
typedef unsigned int malicm_compiler;

typedef struct
{
    unsigned int major;
    unsigned int minor;
    unsigned int patch;
}malicm_version;

/* Prevent C++ name mangling to allow use as a library. */
extern "C"
{
/**
 * \brief   Load the offline compiler libraries from a given path.
 * \details This will load the compiler libraries from a path prefixed with the string supplied.
 * \param   library_path[in] The required library path.
 * \return  Will return true if the libraries were initialized correctly and false if not.
 */
MALIOC_COMPILER_MANAGER_EXPORT bool malicm_initialize_libraries(const char* library_path);

/**
 * \brief Release the offline compiler libraries.
 */
MALIOC_COMPILER_MANAGER_EXPORT void malicm_release_libraries(void);

/**
 * \brief Get the version of the compiler_manager.
 * \param version[out] The version of the compiler manager..
 */
MALIOC_COMPILER_MANAGER_EXPORT void malicm_get_manager_version(malicm_version* version);

/**
 * \brief Release the memory allocated for the compiler outputs.
 * \param outputs[in] The outputs to be released.
 */
MALIOC_COMPILER_MANAGER_EXPORT void malicm_release_compiler_outputs(malioc_outputs* outputs);

/**
 * \brief Get the driver name for a compiler.
 * \param compiler[in] The compiler struct.
 * \return The name driver for the compiler.
 */
MALIOC_COMPILER_MANAGER_EXPORT const char* malicm_get_driver_name(malicm_compiler compiler);

/**
 * \brief Get the core name for a compiler.
 * \param compiler[in] The compiler struct.
 * \return The core name for the compiler.
 */
MALIOC_COMPILER_MANAGER_EXPORT const char* malicm_get_core_name(malicm_compiler compiler);

/**
 * \brief Get the core revision for a compiler.
 * \param compiler[in] The compiler struct.
 * \return The core revision for the compiler.
 */
MALIOC_COMPILER_MANAGER_EXPORT const char* malicm_get_core_revision(malicm_compiler compiler);

/**
 * \brief Get whether the compiler supports binary output or not.
 * \param compiler[in] The compiler struct.
 * \return True if the compiler supports binary output, false otherwise.
 */
MALIOC_COMPILER_MANAGER_EXPORT bool malicm_is_binary_output_supported(malicm_compiler compiler);

/**
 * \brief Get whether the compiler supports prerotate or not.
 * \param compiler[in] The compiler struct.
 * \return True if the compiler supports prerotate, false otherwise.
 */
MALIOC_COMPILER_MANAGER_EXPORT bool malicm_is_prerotate_supported(malicm_compiler compiler);

/**
 * \brief Get the API name for a compiler.
 * \param compiler[in] The compiler struct.
 * \return The API name for the compiler.
 */
MALIOC_COMPILER_MANAGER_EXPORT const char* malicm_get_api_name(malicm_compiler compiler);

/**
 * \brief Get the highest API version supported by this compiler (you can assume backwards compatibility).
 * \param compiler[in] The compiler struct.
 * \return The highest API version supported by this compiler (0 if no versions supported). Note that for
 *         OpenGL ES, the returned value corresponds to the shader language version, NOT the API version.
 *         For instance, a value of 100 corresponds to OpenGL ES 2.0, not OpenGL ES 1.0, while a value of
 *         300 corresponds to OpenGL ES 3.0
 */
MALIOC_COMPILER_MANAGER_EXPORT unsigned int malicm_get_highest_api_version(malicm_compiler compiler);

/**
 * \brief Return the list of supported extensions for a given compiler.
 * \param compiler[in] The compiler struct.
 * \return A space separated list of extensions.
 */
MALIOC_COMPILER_MANAGER_EXPORT const char* malicm_get_extensions(malicm_compiler compiler);

/**
 * \brief   Get a list of compilers.
 * \details This function can be used to get a set of compilers which support a specified set of parameters.
 *          To search using only some of the parameters, pass NULL or 0 in for the parameters
 *          you don't care about.
 *
 * \param compilers[out]            An array of compilers.
 * \param number_of_compilers[out]  The number of compilers in the compilers array.
 * \param driver_name[in]           A driver name to match. Only compilers with this driver name will be
 *                                  returned. Can be NULL (see above).
 * \param core_name[in]             A core name to match. Only compilers with this core name will be returned.
 *                                  Can be NULL (see above).
 * \param core_version[in]          A core version to match. Only compilers with this core version will be
 *                                  returned. Can be NULL (see above).
 * \param compiler_type[in]         A compiler type to match. Only compilers with this compiler type will be
 *                                  returned. Can be NULL (see above).
 * \param binary_output[in]         Whether you want the compilers returned to support binary output. Must be
 *                                  either the string "true" or "false". Can be NULL (see above).
 * \param highest_api_version[in]   The highest API version to match. Only compilers which support
 *                                  greater than or equal versions will be returned. Can be 0 (see above).
 */
MALIOC_COMPILER_MANAGER_EXPORT void malicm_get_compilers(malicm_compiler** compilers,
                                                         unsigned int* number_of_compilers,
                                                         const char* driver_name,
                                                         const char* core_name,
                                                         const char* core_version,
                                                         const char* compiler_type,
                                                         const char* binary_output,
                                                         unsigned int highest_api_version);

/**
 * \brief Release the memory allocated for all the compilers in an array and the array itself.
 * \param compilers[in] An array of compilers.
 * \param number_of_compilers[in] The number of compilers in the array.
 */
MALIOC_COMPILER_MANAGER_EXPORT void malicm_release_compilers(malicm_compiler** compilers, unsigned int number_of_compilers);

/**
 * \brief   Compile the code.
 * \details Use get_compilers to get valid compilers.
 *
 * \param outputs[out]      Outputs from the selected compiler.
 * \param code[in]          The source code to compile.
 * \param shader_type[in]   The type of shader to compile. Must be "vertex", "fragment", "compute", "geometry",
                            "tessellation_control", "tessellation_evaluation" or "kernel".
 * \param names[in]         Names of kernels to compile.
 * \param names_size[in]    The number of names in names.
 * \param binary_output[in] Whether you want binary output or not.
 * \param prerotate[in]     Whether you want prerotate enabled or not.
 * \param defines[in]       A list of defines to pass to the compiler. For example, "DEBUG=true"
 * \param defines_size[in]  The number of defines in defines.
 * \param compiler[in]      The compiler to use for compilation.
 *
 * \return Will return true if the compiler was run on the input code and false if not.
 *         A compiler can run and still produce errors, check the output to see if compiling the code
 *         produced any errors.
 */
MALIOC_COMPILER_MANAGER_EXPORT bool malicm_compile(malioc_outputs* outputs,
                                                   const char* code,
                                                   const char* shader_type,
                                                   const char* const* names,
                                                   int names_size,
                                                   bool binary_output,
                                                   bool prerotate,
                                                   const char* const* defines,
                                                   int defines_size,
                                                   malicm_compiler compiler);

}
#endif
