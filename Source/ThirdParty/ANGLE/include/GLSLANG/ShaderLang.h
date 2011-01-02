//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef _COMPILER_INTERFACE_INCLUDED_
#define _COMPILER_INTERFACE_INCLUDED_

//
// This is the platform independent interface between an OGL driver
// and the shading language compiler.
//

#ifdef __cplusplus
extern "C" {
#endif
//
// Driver must call this first, once, before doing any other
// compiler operations.
// If the function succeeds, the return value is nonzero, else zero.
//
int ShInitialize();
//
// Driver should call this at shutdown.
// If the function succeeds, the return value is nonzero, else zero.
//
int ShFinalize();
//
// Types of languages the compiler can consume.
//
typedef enum {
    EShLangVertex,
    EShLangFragment,
    EShLangCount,
} EShLanguage;

//
// The language specification compiler conforms to.
// It currently supports OpenGL ES and WebGL specifications.
//
typedef enum {
    EShSpecGLES2,
    EShSpecWebGL,
} EShSpec;

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
} TBuiltInResource;

//
// Initialize built-in resources with minimum expected values.
//
void ShInitBuiltInResource(TBuiltInResource* resources);

//
// Optimization level for the compiler.
//
typedef enum {
    EShOptNoGeneration,
    EShOptNone,
    EShOptSimple,       // Optimizations that can be done quickly
    EShOptFull,         // Optimizations that will take more time
} EShOptimizationLevel;

enum TDebugOptions {
    EDebugOpNone               = 0x000,
    EDebugOpIntermediate       = 0x001,  // Writes intermediate tree into info-log.
};

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
ShHandle ShConstructCompiler(EShLanguage, EShSpec, const TBuiltInResource*);
void ShDestruct(ShHandle);

//
// The return value of ShCompile is boolean, indicating
// success or failure.
//
// The info-log should be written by ShCompile into 
// ShHandle, so it can answer future queries.
//
int ShCompile(
    const ShHandle,
    const char* const shaderStrings[],
    const int numStrings,
    const EShOptimizationLevel,
    int debugOptions
    );

//
// All the following return 0 if the information is not
// available in the object passed down, or the object is bad.
//
const char* ShGetInfoLog(const ShHandle);
const char* ShGetObjectCode(const ShHandle);

#ifdef __cplusplus
}
#endif

#endif // _COMPILER_INTERFACE_INCLUDED_
