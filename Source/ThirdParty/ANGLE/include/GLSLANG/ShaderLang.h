//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef GLSLANG_SHADERLANG_H_
#define GLSLANG_SHADERLANG_H_

#if defined(COMPONENT_BUILD) && !defined(ANGLE_TRANSLATOR_STATIC)
#if defined(_WIN32) || defined(_WIN64)

#if defined(ANGLE_TRANSLATOR_IMPLEMENTATION)
#define COMPILER_EXPORT __declspec(dllexport)
#else
#define COMPILER_EXPORT __declspec(dllimport)
#endif  // defined(ANGLE_TRANSLATOR_IMPLEMENTATION)

#else  // defined(_WIN32) || defined(_WIN64)
#define COMPILER_EXPORT __attribute__((visibility("default")))
#endif

#else  // defined(COMPONENT_BUILD) && !defined(ANGLE_TRANSLATOR_STATIC)
#define COMPILER_EXPORT
#endif

#include <stddef.h>

#include "khrplatform.h"

#include <array>
#include <map>
#include <string>
#include <vector>

//
// This is the platform independent interface between an OGL driver
// and the shading language compiler.
//

namespace sh
{
// GLenum alias
typedef unsigned int GLenum;
}

// Must be included after GLenum proxy typedef
// Note: make sure to increment ANGLE_SH_VERSION when changing ShaderVars.h
#include "ShaderVars.h"

// Version number for shader translation API.
// It is incremented every time the API changes.
#define ANGLE_SH_VERSION 163

typedef enum {
    SH_GLES2_SPEC,
    SH_WEBGL_SPEC,

    SH_GLES3_SPEC,
    SH_WEBGL2_SPEC,

    SH_GLES3_1_SPEC,
    SH_WEBGL3_SPEC,

} ShShaderSpec;

typedef enum
{
    // ESSL output only supported in some configurations.
    SH_ESSL_OUTPUT = 0x8B45,

    // GLSL output only supported in some configurations.
    SH_GLSL_COMPATIBILITY_OUTPUT = 0x8B46,
    // Note: GL introduced core profiles in 1.5.
    SH_GLSL_130_OUTPUT      = 0x8B47,
    SH_GLSL_140_OUTPUT      = 0x8B80,
    SH_GLSL_150_CORE_OUTPUT = 0x8B81,
    SH_GLSL_330_CORE_OUTPUT = 0x8B82,
    SH_GLSL_400_CORE_OUTPUT = 0x8B83,
    SH_GLSL_410_CORE_OUTPUT = 0x8B84,
    SH_GLSL_420_CORE_OUTPUT = 0x8B85,
    SH_GLSL_430_CORE_OUTPUT = 0x8B86,
    SH_GLSL_440_CORE_OUTPUT = 0x8B87,
    SH_GLSL_450_CORE_OUTPUT = 0x8B88,

    // HLSL output only supported in some configurations.
    // Deprecated:
    SH_HLSL_OUTPUT   = 0x8B48,
    SH_HLSL9_OUTPUT  = 0x8B48,
    SH_HLSL11_OUTPUT = 0x8B49,

    // Prefer using these to specify HLSL output type:
    SH_HLSL_3_0_OUTPUT       = 0x8B48,  // D3D 9
    SH_HLSL_4_1_OUTPUT       = 0x8B49,  // D3D 11
    SH_HLSL_4_0_FL9_3_OUTPUT = 0x8B4A   // D3D 11 feature level 9_3
} ShShaderOutput;

// Compile options.

typedef uint64_t ShCompileOptions;

const ShCompileOptions SH_VALIDATE                           = 0;
const ShCompileOptions SH_VALIDATE_LOOP_INDEXING             = UINT64_C(1) << 0;
const ShCompileOptions SH_INTERMEDIATE_TREE                  = UINT64_C(1) << 1;
const ShCompileOptions SH_OBJECT_CODE                        = UINT64_C(1) << 2;
const ShCompileOptions SH_VARIABLES                          = UINT64_C(1) << 3;
const ShCompileOptions SH_LINE_DIRECTIVES                    = UINT64_C(1) << 4;
const ShCompileOptions SH_SOURCE_PATH                        = UINT64_C(1) << 5;
const ShCompileOptions SH_UNROLL_FOR_LOOP_WITH_INTEGER_INDEX = UINT64_C(1) << 6;
// If a sampler array index happens to be a loop index,
//   1) if its type is integer, unroll the loop.
//   2) if its type is float, fail the shader compile.
// This is to work around a mac driver bug.
const ShCompileOptions SH_UNROLL_FOR_LOOP_WITH_SAMPLER_ARRAY_INDEX = UINT64_C(1) << 7;

// This flag works around bug in Intel Mac drivers related to abs(i) where
// i is an integer.
const ShCompileOptions SH_EMULATE_ABS_INT_FUNCTION = UINT64_C(1) << 8;

// Enforce the GLSL 1.017 Appendix A section 7 packing restrictions.
// This flag only enforces (and can only enforce) the packing
// restrictions for uniform variables in both vertex and fragment
// shaders. ShCheckVariablesWithinPackingLimits() lets embedders
// enforce the packing restrictions for varying variables during
// program link time.
const ShCompileOptions SH_ENFORCE_PACKING_RESTRICTIONS = UINT64_C(1) << 9;

// This flag ensures all indirect (expression-based) array indexing
// is clamped to the bounds of the array. This ensures, for example,
// that you cannot read off the end of a uniform, whether an array
// vec234, or mat234 type. The ShArrayIndexClampingStrategy enum,
// specified in the ShBuiltInResources when constructing the
// compiler, selects the strategy for the clamping implementation.
const ShCompileOptions SH_CLAMP_INDIRECT_ARRAY_BOUNDS = UINT64_C(1) << 10;

// This flag limits the complexity of an expression.
const ShCompileOptions SH_LIMIT_EXPRESSION_COMPLEXITY = UINT64_C(1) << 11;

// This flag limits the depth of the call stack.
const ShCompileOptions SH_LIMIT_CALL_STACK_DEPTH = UINT64_C(1) << 12;

// This flag initializes gl_Position to vec4(0,0,0,0) at the
// beginning of the vertex shader's main(), and has no effect in the
// fragment shader. It is intended as a workaround for drivers which
// incorrectly fail to link programs if gl_Position is not written.
const ShCompileOptions SH_INIT_GL_POSITION = UINT64_C(1) << 13;

// This flag replaces
//   "a && b" with "a ? b : false",
//   "a || b" with "a ? true : b".
// This is to work around a MacOSX driver bug that |b| is executed
// independent of |a|'s value.
const ShCompileOptions SH_UNFOLD_SHORT_CIRCUIT = UINT64_C(1) << 14;

// This flag initializes output variables to 0 at the beginning of main().
// It is to avoid undefined behaviors.
const ShCompileOptions SH_INIT_OUTPUT_VARIABLES = UINT64_C(1) << 15;

// This flag scalarizes vec/ivec/bvec/mat constructor args.
// It is intended as a workaround for Linux/Mac driver bugs.
const ShCompileOptions SH_SCALARIZE_VEC_AND_MAT_CONSTRUCTOR_ARGS = UINT64_C(1) << 16;

// This flag overwrites a struct name with a unique prefix.
// It is intended as a workaround for drivers that do not handle
// struct scopes correctly, including all Mac drivers and Linux AMD.
const ShCompileOptions SH_REGENERATE_STRUCT_NAMES = UINT64_C(1) << 17;

// This flag makes the compiler not prune unused function early in the
// compilation process. Pruning coupled with SH_LIMIT_CALL_STACK_DEPTH
// helps avoid bad shaders causing stack overflows.
const ShCompileOptions SH_DONT_PRUNE_UNUSED_FUNCTIONS = UINT64_C(1) << 18;

// This flag works around a bug in NVIDIA 331 series drivers related
// to pow(x, y) where y is a constant vector.
const ShCompileOptions SH_REMOVE_POW_WITH_CONSTANT_EXPONENT = UINT64_C(1) << 19;

// This flag works around bugs in Mac drivers related to do-while by
// transforming them into an other construct.
const ShCompileOptions SH_REWRITE_DO_WHILE_LOOPS = UINT64_C(1) << 20;

// This flag works around a bug in the HLSL compiler optimizer that folds certain
// constant pow expressions incorrectly. Only applies to the HLSL back-end. It works
// by expanding the integer pow expressions into a series of multiplies.
const ShCompileOptions SH_EXPAND_SELECT_HLSL_INTEGER_POW_EXPRESSIONS = UINT64_C(1) << 21;

// Flatten "#pragma STDGL invariant(all)" into the declarations of
// varying variables and built-in GLSL variables. This compiler
// option is enabled automatically when needed.
const ShCompileOptions SH_FLATTEN_PRAGMA_STDGL_INVARIANT_ALL = UINT64_C(1) << 22;

// Some drivers do not take into account the base level of the texture in the results of the
// HLSL GetDimensions builtin.  This flag instructs the compiler to manually add the base level
// offsetting.
const ShCompileOptions SH_HLSL_GET_DIMENSIONS_IGNORES_BASE_LEVEL = UINT64_C(1) << 23;

// This flag works around an issue in translating GLSL function texelFetchOffset on
// INTEL drivers. It works by translating texelFetchOffset into texelFetch.
const ShCompileOptions SH_REWRITE_TEXELFETCHOFFSET_TO_TEXELFETCH = UINT64_C(1) << 24;

// This flag works around condition bug of for and while loops in Intel Mac OSX drivers.
// Condition calculation is not correct. Rewrite it from "CONDITION" to "CONDITION && true".
const ShCompileOptions SH_ADD_AND_TRUE_TO_LOOP_CONDITION = UINT64_C(1) << 25;

// This flag works around a bug in evaluating unary minus operator on integer on some INTEL
// drivers. It works by translating -(int) into ~(int) + 1.
const ShCompileOptions SH_REWRITE_INTEGER_UNARY_MINUS_OPERATOR = UINT64_C(1) << 26;

// This flag works around a bug in evaluating isnan() on some INTEL D3D and Mac OSX drivers.
// It works by using an expression to emulate this function.
const ShCompileOptions SH_EMULATE_ISNAN_FLOAT_FUNCTION = UINT64_C(1) << 27;

// This flag will use all uniforms of unused std140 and shared uniform blocks at the
// beginning of the vertex/fragment shader's main(). It is intended as a workaround for Mac
// drivers with shader version 4.10. In those drivers, they will treat unused
// std140 and shared uniform blocks' members as inactive. However, WebGL2.0 based on
// OpenGL ES3.0.4 requires all members of a named uniform block declared with a shared or std140
// layout qualifier to be considered active. The uniform block itself is also considered active.
const ShCompileOptions SH_USE_UNUSED_STANDARD_SHARED_BLOCKS = UINT64_C(1) << 28;

// Defines alternate strategies for implementing array index clamping.
typedef enum {
  // Use the clamp intrinsic for array index clamping.
  SH_CLAMP_WITH_CLAMP_INTRINSIC = 1,

  // Use a user-defined function for array index clamping.
  SH_CLAMP_WITH_USER_DEFINED_INT_CLAMP_FUNCTION
} ShArrayIndexClampingStrategy;

//
// Driver must call this first, once, before doing any other
// compiler operations.
// If the function succeeds, the return value is true, else false.
//
COMPILER_EXPORT bool ShInitialize();
//
// Driver should call this at shutdown.
// If the function succeeds, the return value is true, else false.
//
COMPILER_EXPORT bool ShFinalize();

// The 64 bits hash function. The first parameter is the input string; the
// second parameter is the string length.
typedef khronos_uint64_t (*ShHashFunction64)(const char*, size_t);

//
// Implementation dependent built-in resources (constants and extensions).
// The names for these resources has been obtained by stripping gl_/GL_.
//
typedef struct
{
    // Constants.
    int MaxVertexAttribs;
    int MaxVertexUniformVectors;
    int MaxVaryingVectors;
    int MaxVertexTextureImageUnits;
    int MaxCombinedTextureImageUnits;
    int MaxTextureImageUnits;
    int MaxFragmentUniformVectors;
    int MaxDrawBuffers;

    // Extensions.
    // Set to 1 to enable the extension, else 0.
    int OES_standard_derivatives;
    int OES_EGL_image_external;
    int OES_EGL_image_external_essl3;
    int NV_EGL_stream_consumer_external;
    int ARB_texture_rectangle;
    int EXT_blend_func_extended;
    int EXT_draw_buffers;
    int EXT_frag_depth;
    int EXT_shader_texture_lod;
    int WEBGL_debug_shader_precision;
    int EXT_shader_framebuffer_fetch;
    int NV_shader_framebuffer_fetch;
    int ARM_shader_framebuffer_fetch;

    // Set to 1 to enable replacing GL_EXT_draw_buffers #extension directives
    // with GL_NV_draw_buffers in ESSL output. This flag can be used to emulate
    // EXT_draw_buffers by using it in combination with GLES3.0 glDrawBuffers
    // function. This applies to Tegra K1 devices.
    int NV_draw_buffers;

    // Set to 1 if highp precision is supported in the ESSL 1.00 version of the
    // fragment language. Does not affect versions of the language where highp
    // support is mandatory.
    // Default is 0.
    int FragmentPrecisionHigh;

    // GLSL ES 3.0 constants.
    int MaxVertexOutputVectors;
    int MaxFragmentInputVectors;
    int MinProgramTexelOffset;
    int MaxProgramTexelOffset;

    // Extension constants.

    // Value of GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT for OpenGL ES output context.
    // Value of GL_MAX_DUAL_SOURCE_DRAW_BUFFERS for OpenGL output context.
    // GLES SL version 100 gl_MaxDualSourceDrawBuffersEXT value for EXT_blend_func_extended.
    int MaxDualSourceDrawBuffers;

    // Name Hashing.
    // Set a 64 bit hash function to enable user-defined name hashing.
    // Default is NULL.
    ShHashFunction64 HashFunction;

    // Selects a strategy to use when implementing array index clamping.
    // Default is SH_CLAMP_WITH_CLAMP_INTRINSIC.
    ShArrayIndexClampingStrategy ArrayIndexClampingStrategy;

    // The maximum complexity an expression can be when SH_LIMIT_EXPRESSION_COMPLEXITY is turned on.
    int MaxExpressionComplexity;

    // The maximum depth a call stack can be.
    int MaxCallStackDepth;

    // The maximum number of parameters a function can have when SH_LIMIT_EXPRESSION_COMPLEXITY is
    // turned on.
    int MaxFunctionParameters;

    // GLES 3.1 constants

    // maximum number of available image units
    int MaxImageUnits;

    // maximum number of image uniforms in a vertex shader
    int MaxVertexImageUniforms;

    // maximum number of image uniforms in a fragment shader
    int MaxFragmentImageUniforms;

    // maximum number of image uniforms in a compute shader
    int MaxComputeImageUniforms;

    // maximum total number of image uniforms in a program
    int MaxCombinedImageUniforms;

    // maximum number of ssbos and images in a shader
    int MaxCombinedShaderOutputResources;

    // maximum number of groups in each dimension
    std::array<int, 3> MaxComputeWorkGroupCount;
    // maximum number of threads per work group in each dimension
    std::array<int, 3> MaxComputeWorkGroupSize;

    // maximum number of total uniform components
    int MaxComputeUniformComponents;

    // maximum number of texture image units in a compute shader
    int MaxComputeTextureImageUnits;

    // maximum number of atomic counters in a compute shader
    int MaxComputeAtomicCounters;

    // maximum number of atomic counter buffers in a compute shader
    int MaxComputeAtomicCounterBuffers;

    // maximum number of atomic counters in a vertex shader
    int MaxVertexAtomicCounters;

    // maximum number of atomic counters in a fragment shader
    int MaxFragmentAtomicCounters;

    // maximum number of atomic counters in a program
    int MaxCombinedAtomicCounters;

    // maximum binding for an atomic counter
    int MaxAtomicCounterBindings;

    // maximum number of atomic counter buffers in a vertex shader
    int MaxVertexAtomicCounterBuffers;

    // maximum number of atomic counter buffers in a fragment shader
    int MaxFragmentAtomicCounterBuffers;

    // maximum number of atomic counter buffers in a program
    int MaxCombinedAtomicCounterBuffers;

    // maximum number of buffer object storage in machine units
    int MaxAtomicCounterBufferSize;

} ShBuiltInResources;

//
// Initialize built-in resources with minimum expected values.
// Parameters:
// resources: The object to initialize. Will be comparable with memcmp.
//
COMPILER_EXPORT void ShInitBuiltInResources(ShBuiltInResources *resources);

//
// ShHandle held by but opaque to the driver.  It is allocated,
// managed, and de-allocated by the compiler. Its contents
// are defined by and used by the compiler.
//
// If handle creation fails, 0 will be returned.
//
typedef void *ShHandle;

//
// Returns the a concatenated list of the items in ShBuiltInResources as a
// null-terminated string.
// This function must be updated whenever ShBuiltInResources is changed.
// Parameters:
// handle: Specifies the handle of the compiler to be used.
COMPILER_EXPORT const std::string &ShGetBuiltInResourcesString(const ShHandle handle);

//
// Driver calls these to create and destroy compiler objects.
//
// Returns the handle of constructed compiler, null if the requested compiler is
// not supported.
// Parameters:
// type: Specifies the type of shader - GL_FRAGMENT_SHADER or GL_VERTEX_SHADER.
// spec: Specifies the language spec the compiler must conform to -
//       SH_GLES2_SPEC or SH_WEBGL_SPEC.
// output: Specifies the output code type - for example SH_ESSL_OUTPUT, SH_GLSL_OUTPUT,
//         SH_HLSL_3_0_OUTPUT or SH_HLSL_4_1_OUTPUT. Note: Each output type may only
//         be supported in some configurations.
// resources: Specifies the built-in resources.
COMPILER_EXPORT ShHandle ShConstructCompiler(
    sh::GLenum type,
    ShShaderSpec spec,
    ShShaderOutput output,
    const ShBuiltInResources *resources);
COMPILER_EXPORT void ShDestruct(ShHandle handle);

//
// Compiles the given shader source.
// If the function succeeds, the return value is true, else false.
// Parameters:
// handle: Specifies the handle of compiler to be used.
// shaderStrings: Specifies an array of pointers to null-terminated strings
//                containing the shader source code.
// numStrings: Specifies the number of elements in shaderStrings array.
// compileOptions: A mask containing the following parameters:
// SH_VALIDATE: Validates shader to ensure that it conforms to the spec
//              specified during compiler construction.
// SH_VALIDATE_LOOP_INDEXING: Validates loop and indexing in the shader to
//                            ensure that they do not exceed the minimum
//                            functionality mandated in GLSL 1.0 spec,
//                            Appendix A, Section 4 and 5.
//                            There is no need to specify this parameter when
//                            compiling for WebGL - it is implied.
// SH_INTERMEDIATE_TREE: Writes intermediate tree to info log.
//                       Can be queried by calling ShGetInfoLog().
// SH_OBJECT_CODE: Translates intermediate tree to glsl or hlsl shader.
//                 Can be queried by calling ShGetObjectCode().
// SH_VARIABLES: Extracts attributes, uniforms, and varyings.
//               Can be queried by calling ShGetVariableInfo().
//
COMPILER_EXPORT bool ShCompile(const ShHandle handle,
                               const char *const shaderStrings[],
                               size_t numStrings,
                               ShCompileOptions compileOptions);

// Clears the results from the previous compilation.
COMPILER_EXPORT void ShClearResults(const ShHandle handle);

// Return the version of the shader language.
COMPILER_EXPORT int ShGetShaderVersion(const ShHandle handle);

// Return the currently set language output type.
COMPILER_EXPORT ShShaderOutput ShGetShaderOutputType(
    const ShHandle handle);

// Returns null-terminated information log for a compiled shader.
// Parameters:
// handle: Specifies the compiler
COMPILER_EXPORT const std::string &ShGetInfoLog(const ShHandle handle);

// Returns null-terminated object code for a compiled shader.
// Parameters:
// handle: Specifies the compiler
COMPILER_EXPORT const std::string &ShGetObjectCode(const ShHandle handle);

// Returns a (original_name, hash) map containing all the user defined
// names in the shader, including variable names, function names, struct
// names, and struct field names.
// Parameters:
// handle: Specifies the compiler
COMPILER_EXPORT const std::map<std::string, std::string> *ShGetNameHashingMap(
    const ShHandle handle);

// Shader variable inspection.
// Returns a pointer to a list of variables of the designated type.
// (See ShaderVars.h for type definitions, included above)
// Returns NULL on failure.
// Parameters:
// handle: Specifies the compiler
COMPILER_EXPORT const std::vector<sh::Uniform> *ShGetUniforms(const ShHandle handle);
COMPILER_EXPORT const std::vector<sh::Varying> *ShGetVaryings(const ShHandle handle);
COMPILER_EXPORT const std::vector<sh::Attribute> *ShGetAttributes(const ShHandle handle);
COMPILER_EXPORT const std::vector<sh::OutputVariable> *ShGetOutputVariables(const ShHandle handle);
COMPILER_EXPORT const std::vector<sh::InterfaceBlock> *ShGetInterfaceBlocks(const ShHandle handle);
COMPILER_EXPORT sh::WorkGroupSize ShGetComputeShaderLocalGroupSize(const ShHandle handle);

typedef struct
{
    sh::GLenum type;
    int size;
} ShVariableInfo;

// Returns true if the passed in variables pack in maxVectors following
// the packing rules from the GLSL 1.017 spec, Appendix A, section 7.
// Returns false otherwise. Also look at the SH_ENFORCE_PACKING_RESTRICTIONS
// flag above.
// Parameters:
// maxVectors: the available rows of registers.
// variables: an array of variables.
COMPILER_EXPORT bool ShCheckVariablesWithinPackingLimits(
    int maxVectors,
    const std::vector<sh::ShaderVariable> &variables);

// Gives the compiler-assigned register for an interface block.
// The method writes the value to the output variable "indexOut".
// Returns true if it found a valid interface block, false otherwise.
// Parameters:
// handle: Specifies the compiler
// interfaceBlockName: Specifies the interface block
// indexOut: output variable that stores the assigned register
COMPILER_EXPORT bool ShGetInterfaceBlockRegister(const ShHandle handle,
                                                 const std::string &interfaceBlockName,
                                                 unsigned int *indexOut);

// Gives a map from uniform names to compiler-assigned registers in the default
// interface block. Note that the map contains also registers of samplers that
// have been extracted from structs.
COMPILER_EXPORT const std::map<std::string, unsigned int> *ShGetUniformRegisterMap(
    const ShHandle handle);

#endif // GLSLANG_SHADERLANG_H_
