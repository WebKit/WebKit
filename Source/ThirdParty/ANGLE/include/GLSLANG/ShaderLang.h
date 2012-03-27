//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef _COMPILER_INTERFACE_INCLUDED_
#define _COMPILER_INTERFACE_INCLUDED_

#if defined(COMPONENT_BUILD)
#if defined(_WIN32) || defined(_WIN64)

#if defined(COMPILER_IMPLEMENTATION)
#define COMPILER_EXPORT __declspec(dllexport)
#else
#define COMPILER_EXPORT __declspec(dllimport)
#endif  // defined(COMPILER_IMPLEMENTATION)

#else  // defined(WIN32)
#define COMPILER_EXPORT __attribute__((visibility("default")))
#endif

#else  // defined(COMPONENT_BUILD)
#define COMPILER_EXPORT
#endif

//
// This is the platform independent interface between an OGL driver
// and the shading language compiler.
//

#ifdef __cplusplus
extern "C" {
#endif

// Version number for shader translation API.
// It is incremented everytime the API changes.
#define SH_VERSION 105

//
// The names of the following enums have been derived by replacing GL prefix
// with SH. For example, SH_INFO_LOG_LENGTH is equivalent to GL_INFO_LOG_LENGTH.
// The enum values are also equal to the values of their GL counterpart. This
// is done to make it easier for applications to use the shader library.
//
typedef enum {
  SH_FRAGMENT_SHADER = 0x8B30,
  SH_VERTEX_SHADER   = 0x8B31
} ShShaderType;

typedef enum {
  SH_GLES2_SPEC = 0x8B40,
  SH_WEBGL_SPEC = 0x8B41
} ShShaderSpec;

typedef enum {
  SH_ESSL_OUTPUT = 0x8B45,
  SH_GLSL_OUTPUT = 0x8B46,
  SH_HLSL_OUTPUT = 0x8B47
} ShShaderOutput;

typedef enum {
  SH_NONE           = 0,
  SH_INT            = 0x1404,
  SH_FLOAT          = 0x1406,
  SH_FLOAT_VEC2     = 0x8B50,
  SH_FLOAT_VEC3     = 0x8B51,
  SH_FLOAT_VEC4     = 0x8B52,
  SH_INT_VEC2       = 0x8B53,
  SH_INT_VEC3       = 0x8B54,
  SH_INT_VEC4       = 0x8B55,
  SH_BOOL           = 0x8B56,
  SH_BOOL_VEC2      = 0x8B57,
  SH_BOOL_VEC3      = 0x8B58,
  SH_BOOL_VEC4      = 0x8B59,
  SH_FLOAT_MAT2     = 0x8B5A,
  SH_FLOAT_MAT3     = 0x8B5B,
  SH_FLOAT_MAT4     = 0x8B5C,
  SH_SAMPLER_2D     = 0x8B5E,
  SH_SAMPLER_CUBE   = 0x8B60,
  SH_SAMPLER_2D_RECT_ARB = 0x8B63,
  SH_SAMPLER_EXTERNAL_OES = 0x8D66
} ShDataType;

typedef enum {
  SH_INFO_LOG_LENGTH             =  0x8B84,
  SH_OBJECT_CODE_LENGTH          =  0x8B88,  // GL_SHADER_SOURCE_LENGTH
  SH_ACTIVE_UNIFORMS             =  0x8B86,
  SH_ACTIVE_UNIFORM_MAX_LENGTH   =  0x8B87,
  SH_ACTIVE_ATTRIBUTES           =  0x8B89,
  SH_ACTIVE_ATTRIBUTE_MAX_LENGTH =  0x8B8A,
  SH_MAPPED_NAME_MAX_LENGTH      =  0x8B8B
} ShShaderInfo;

// Compile options.
typedef enum {
  SH_VALIDATE                = 0,
  SH_VALIDATE_LOOP_INDEXING  = 0x0001,
  SH_INTERMEDIATE_TREE       = 0x0002,
  SH_OBJECT_CODE             = 0x0004,
  SH_ATTRIBUTES_UNIFORMS     = 0x0008,
  SH_LINE_DIRECTIVES         = 0x0010,
  SH_SOURCE_PATH             = 0x0020,
  SH_MAP_LONG_VARIABLE_NAMES = 0x0040,
  SH_UNROLL_FOR_LOOP_WITH_INTEGER_INDEX = 0x0080,

  // This is needed only as a workaround for certain OpenGL driver bugs.
  SH_EMULATE_BUILT_IN_FUNCTIONS = 0x0100
} ShCompileOptions;

//
// Driver must call this first, once, before doing any other
// compiler operations.
// If the function succeeds, the return value is nonzero, else zero.
//
COMPILER_EXPORT int ShInitialize();
//
// Driver should call this at shutdown.
// If the function succeeds, the return value is nonzero, else zero.
//
COMPILER_EXPORT int ShFinalize();

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
    int ARB_texture_rectangle;
} ShBuiltInResources;

//
// Initialize built-in resources with minimum expected values.
//
COMPILER_EXPORT void ShInitBuiltInResources(ShBuiltInResources* resources);

//
// ShHandle held by but opaque to the driver.  It is allocated,
// managed, and de-allocated by the compiler. It's contents 
// are defined by and used by the compiler.
//
// If handle creation fails, 0 will be returned.
//
typedef void* ShHandle;

//
// Driver calls these to create and destroy compiler objects.
//
// Returns the handle of constructed compiler, null if the requested compiler is
// not supported.
// Parameters:
// type: Specifies the type of shader - SH_FRAGMENT_SHADER or SH_VERTEX_SHADER.
// spec: Specifies the language spec the compiler must conform to -
//       SH_GLES2_SPEC or SH_WEBGL_SPEC.
// output: Specifies the output code type - SH_ESSL_OUTPUT, SH_GLSL_OUTPUT,
//         or SH_HLSL_OUTPUT.
// resources: Specifies the built-in resources.
COMPILER_EXPORT ShHandle ShConstructCompiler(
    ShShaderType type,
    ShShaderSpec spec,
    ShShaderOutput output,
    const ShBuiltInResources* resources);
COMPILER_EXPORT void ShDestruct(ShHandle handle);

//
// Compiles the given shader source.
// If the function succeeds, the return value is nonzero, else zero.
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
// SH_ATTRIBUTES_UNIFORMS: Extracts attributes and uniforms.
//                         Can be queried by calling ShGetActiveAttrib() and
//                         ShGetActiveUniform().
//
COMPILER_EXPORT int ShCompile(
    const ShHandle handle,
    const char* const shaderStrings[],
    const int numStrings,
    int compileOptions
    );

// Returns a parameter from a compiled shader.
// Parameters:
// handle: Specifies the compiler
// pname: Specifies the parameter to query.
// The following parameters are defined:
// SH_INFO_LOG_LENGTH: the number of characters in the information log
//                     including the null termination character.
// SH_OBJECT_CODE_LENGTH: the number of characters in the object code
//                        including the null termination character.
// SH_ACTIVE_ATTRIBUTES: the number of active attribute variables.
// SH_ACTIVE_ATTRIBUTE_MAX_LENGTH: the length of the longest active attribute
//                                 variable name including the null
//                                 termination character.
// SH_ACTIVE_UNIFORMS: the number of active uniform variables.
// SH_ACTIVE_UNIFORM_MAX_LENGTH: the length of the longest active uniform
//                               variable name including the null
//                               termination character.
// SH_MAPPED_NAME_MAX_LENGTH: the length of the mapped variable name including
//                            the null termination character.
// 
// params: Requested parameter
COMPILER_EXPORT void ShGetInfo(const ShHandle handle,
                               ShShaderInfo pname,
                               int* params);

// Returns nul-terminated information log for a compiled shader.
// Parameters:
// handle: Specifies the compiler
// infoLog: Specifies an array of characters that is used to return
//          the information log. It is assumed that infoLog has enough memory
//          to accomodate the information log. The size of the buffer required
//          to store the returned information log can be obtained by calling
//          ShGetInfo with SH_INFO_LOG_LENGTH.
COMPILER_EXPORT void ShGetInfoLog(const ShHandle handle, char* infoLog);

// Returns null-terminated object code for a compiled shader.
// Parameters:
// handle: Specifies the compiler
// infoLog: Specifies an array of characters that is used to return
//          the object code. It is assumed that infoLog has enough memory to
//          accomodate the object code. The size of the buffer required to
//          store the returned object code can be obtained by calling
//          ShGetInfo with SH_OBJECT_CODE_LENGTH.
COMPILER_EXPORT void ShGetObjectCode(const ShHandle handle, char* objCode);

// Returns information about an active attribute variable.
// Parameters:
// handle: Specifies the compiler
// index: Specifies the index of the attribute variable to be queried.
// length: Returns the number of characters actually written in the string
//         indicated by name (excluding the null terminator) if a value other
//         than NULL is passed.
// size: Returns the size of the attribute variable.
// type: Returns the data type of the attribute variable.
// name: Returns a null terminated string containing the name of the
//       attribute variable. It is assumed that name has enough memory to
//       accomodate the attribute variable name. The size of the buffer
//       required to store the attribute variable name can be obtained by
//       calling ShGetInfo with SH_ACTIVE_ATTRIBUTE_MAX_LENGTH.
// mappedName: Returns a null terminated string containing the mapped name of
//             the attribute variable, It is assumed that mappedName has enough
//             memory (SH_MAPPED_NAME_MAX_LENGTH), or NULL if don't care
//             about the mapped name. If the name is not mapped, then name and
//             mappedName are the same.
COMPILER_EXPORT void ShGetActiveAttrib(const ShHandle handle,
                                       int index,
                                       int* length,
                                       int* size,
                                       ShDataType* type,
                                       char* name,
                                       char* mappedName);

// Returns information about an active uniform variable.
// Parameters:
// handle: Specifies the compiler
// index: Specifies the index of the uniform variable to be queried.
// length: Returns the number of characters actually written in the string
//         indicated by name (excluding the null terminator) if a value
//         other than NULL is passed.
// size: Returns the size of the uniform variable.
// type: Returns the data type of the uniform variable.
// name: Returns a null terminated string containing the name of the
//       uniform variable. It is assumed that name has enough memory to
//       accomodate the uniform variable name. The size of the buffer required
//       to store the uniform variable name can be obtained by calling
//       ShGetInfo with SH_ACTIVE_UNIFORMS_MAX_LENGTH.
// mappedName: Returns a null terminated string containing the mapped name of
//             the uniform variable, It is assumed that mappedName has enough
//             memory (SH_MAPPED_NAME_MAX_LENGTH), or NULL if don't care
//             about the mapped name. If the name is not mapped, then name and
//             mappedName are the same.
COMPILER_EXPORT void ShGetActiveUniform(const ShHandle handle,
                                        int index,
                                        int* length,
                                        int* size,
                                        ShDataType* type,
                                        char* name,
                                        char* mappedName);

#ifdef __cplusplus
}
#endif

#endif // _COMPILER_INTERFACE_INCLUDED_
