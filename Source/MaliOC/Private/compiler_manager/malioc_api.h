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

#ifndef MALIOC_API_H
#define MALIOC_API_H

/**
 * \file malioc_api.h
 * \brief API for using an offline compiler library.
 * \details The offline compilers from Mali-DDKs should expose the following
 *          APIs when built for offline use. This API is versioned
 *          ( \see malioc_get_API_version). An implementing library must
 *          implement only one version of the API.
 * \version 1.0.0
 */

/*
 * If building an offline compiler library, define MALIOC_LIBRARY
 * to export the correct functions.
 */
#if defined MALIOC_LIBRARY
#if defined _WIN32
#define MALIOC_EXPORT __declspec(dllexport)
#else
#define MALIOC_EXPORT __attribute__((visibility("default")))
#endif
#else
#define MALIOC_EXPORT
#endif

/**
 * \brief The version of the API.
 */
typedef struct
{
    /**
     * \brief Major version of the API
     * \details Changes if a non-backwards compatible change of the API takes
     *          place. For example, removing functions or changing function
     *          prototypes.
     */
    unsigned int major;

    /**
     * \brief Minor version of the API
     * \details Changes if a backwards compatible change of the API takes
     *          place. For example, adding new functions or new cores.
     */
    unsigned int minor;

    /**
     * \brief Patch version of the API
     * \details Currently unused.
     */
    unsigned int patch;
}malioc_api_version;

/**
 * \brief A boolean type.
 */
typedef enum
{
    MALIOC_FALSE = 0,
    MALIOC_TRUE
}malioc_bool;

/**
 * \brief Key-Value pairs.
 * \details Used to pass information that will likely change change in the
 *          future. Used as a:
 *           - capability output (flexible capabilities)
 *           - input (flexible inputs)
 *           - output (flexible outputs)
 */
typedef struct
{
    /**
     * \brief Number of entries in the list.
     * \details The TOTAL number of entries, not the number of pairs.
     */
    unsigned int number_of_entries;

    /**
     * \brief A list of Key-Value pairs.
     * \details A single value must always follow a single key. All keys must
     *          be documented. The number of a times a key can appear and the
     *          format of the value must also be documented. Each value and key
     *          is a NULL terminated string.
     */
    char** list;
}malioc_key_value_pairs;

/**
 * \brief Errors that can be returned from compiler operations.
 * \details Definitions of the errors can be found in the documentation for
 *          each function.
 */
typedef enum
{
    MALIOC_SUCCESS = 0,
    MALIOC_CORE_NOT_SUPPORTED,
    MALIOC_INVALID_PARAMETERS,
    MALIOC_BINARY_OUTPUT_NOT_SUPPORTED,
    MALIOC_MEMORY_ERROR
}malioc_compiler_error;

/**
 * \brief Definition of a hardware core.
 * \details Used as both a:
 *           - capability output (which cores are supported)
 *           - input (which core to compile for)
 */
typedef struct
{
    /**
     * \brief Name of the hardware core.
     * \details For example, "Mali-400". A NULL terminated string.
     */
    const char* core_name;

    /**
     * \brief Hardware version of the core
     * \details In the form: "r3p0-15dev0". A NULL terminated string.
     */
    const char* core_version;
}malioc_hardware_core;

/**
 * \brief Capabilities of an offline compiler library.
 */
typedef struct
{
    /**
     * \brief The number of hardware cores returned.
     * \details Must not be 0.
     */
    unsigned int number_of_supported_cores;

    /**
     * \brief A list of the hardware cores supported by this library.
     * \details Must not be NULL. The length of the list is
     *          number_of_supported_cores.
     */
    malioc_hardware_core* supported_hardware_cores;

    /**
     * \brief Does this library have the ability to return binaries.
     */
    malioc_bool is_binary_output_supported;

    /**
     * \brief An array of key-value pairs lists that can be used to output
     *        capabilities not defined by this API.
     * \details For example, which languages/APIs the compiler accepts.
     */
    malioc_key_value_pairs flexible_capabilities;
}malioc_capabilities;

/**
 * \brief Compiler inputs.
 * \details Accepted by all compilation functions, regardless of architecture
 *          and source type.
 */
typedef struct
{
    /**
     * \brief Input source code.
     * \details NULL terminated string.
     */
    const char* source;

    /**
     * \brief An array of key-value pairs lists that can be used to input items
     *        not defined by this API.
     * \details For example, additional optimisation flags and defines.
     */
    malioc_key_value_pairs flexible_inputs;

    /**
     * \brief The core to compile the code for.
     * \details If either core_name or core_version are NULL, the behaviour is
     *          undefined.
     */
    malioc_hardware_core required_hardware_core;

    /**
     * \brief Flag to choose if a binary should be produced.
     */
    malioc_bool is_binary_output_required;
}malioc_inputs;

/**
 * \brief Output structure for compilation.
 * \details Returned by all compilation functions, regardless of architecture
 *          and source type.
 */
typedef struct
{
    /**
     * \brief The number of flexible outputs returned.
     * \details Multiple outputs can be returned by one compilation, (multiple
     *          kernels in OpenCL, multiple render targets in OpenGL ES 3.0).
     */
    unsigned int number_of_flexible_outputs;

    /**
     * \brief An array of key-value pairs lists that can be used to return
     *        items not defined by this API.
     * \details For example, statistics about the compiled shaders would be
     *          placed in this list since they are architecture specific.
     *          There can be multiple lists since there can be multiple outputs
     *          per compilation.
     */
    malioc_key_value_pairs* flexible_outputs;

    /**
     * \brief The compiled binary size in bytes.
     * \details The size will be 0 if:
     *          - compilation failed
     *          - a binary is not requested
     *          - the backend of the compiler has not been included (this case
     *            can be detected when malioc_binary_output_not_supported is
     *            returned)
     */
    unsigned int binary_data_size;

    /**
     * \brief The compiled binary data.
     * \details The data will be NULL if:
     *          - compilation failed
     *          - a binary is not requested
     *          - the backend of the compiler has not been included (this case
     *            can be detected when MALIOC_BINARY_OUTPUT_NOT_SUPPORTED is
     *            returned)
     */
    void* binary_data;

    /**
     * \brief The number of errors produced while compiling.
     * \details Only errors produced by incorrect input code are reported here.
     *          The number of messages will be 0 if no error messages have been
     *          generated.
     */
    unsigned int number_of_errors;

    /**
     * \brief A list of the errors reported.
     * \details Only errors produced by incorrect input code are reported here.
     *          The list will be NULL if no error messages have been generated.
     *          Each message is a NULL terminated string. The length of the
     *          list is number_of_errors.
     */
    char** errors;

    /**
     * \brief The number of warnings produced while compiling.
     * \details Only warnings produced by incorrect input code are reported
     *          here. The number of messages will be 0 if no warning messages
     *          have been generated.
     */
    unsigned int number_of_warnings;

    /**
     * \brief A list of the warnings reported.
     * \details Only warnings produced by incorrect input code are reported
     *          here. The list will be NULL if no warning messages have been
     *          generated. Each message is a NULL terminated string. The length
     *          of the list is number_of_warnings.
     */
    char** warnings;
}malioc_outputs;

/**
 * \brief Get the version of the API which the library implements.
 * \note This function will never change.
 * \param version[out] The version of the API which the library implements.
 */
MALIOC_EXPORT void malioc_get_api_version(malioc_api_version* version);

/**
 * \brief Compiles some source code with the specified inputs.
 * \param inputs[in] Inputs and flags which affect compilation.
 * \param outputs[out] The compilation results which must be freed using
 *        malioc_release_outputs.
 * \return Error code defined in malioc_compiler_error.
 *         MALIOC_CORE_NOT_SUPPORTED if the library does not support the
 *         requested core.
 *         MALIOC_INVALID_PARAMETERS if parameters are NULL, or any of the
 *         flexible inputs are invalid.
 *         MALIOC_BINARY_OUTPUT_NOT_SUPPORTED if binary output is requested but
 *         the offline compiler library does not support it.
 *         MALIOC_MEMORY_ERROR if allocating memory for outputs fails.
 */
MALIOC_EXPORT malioc_compiler_error malioc_compile(malioc_inputs* inputs, malioc_outputs* outputs);

/**
 * \brief Frees any memory allocated in the library for inside the
 *        malioc_outputs structure.
 * \return Error code defined in malioc_compiler_error.
 *         MALIOC_INVALID_PARAMETERS if parameters are NULL.
 *         MALIOC_MEMORY_ERROR if freeing memory for outputs fails.
 */
MALIOC_EXPORT malioc_compiler_error malioc_release_outputs(malioc_outputs* outputs);

/**
 * \brief Query the offline compiler library for what it supports.
 * \param capabilities[out] The capabilities of the library. Must be freed
 *        using malioc_release_capabilities.
 * \return Error code defined in malioc_compiler_error.
 *         MALIOC_INVALID_PARAMETERS if parameters are NULL.
 *         MALIOC_MEMORY_ERROR if allocating memory for capabilities fails.
 */
MALIOC_EXPORT malioc_compiler_error malioc_get_capabilities(malioc_capabilities* capabilities);
/**
 * \brief Frees any memory allocated in the library for inside the
 *        malioc_capabilities structure.
 * \return Error code defined in malioc_compiler_error.
 *         MALIOC_INVALID_PARAMETERS if parameters are NULL.
 *         MALIOC_MEMORY_ERROR if freeing memory for capabilities fails.
 */
MALIOC_EXPORT malioc_compiler_error malioc_release_capabilities(malioc_capabilities* capabilities);

#endif
