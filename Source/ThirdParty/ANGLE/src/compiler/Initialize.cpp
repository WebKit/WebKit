//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

//
// Create strings that declare built-in definitions, add built-ins that
// cannot be expressed in the files, and establish mappings between 
// built-in functions and operators.
//

#include "compiler/Initialize.h"

#include "compiler/intermediate.h"

//============================================================================
//
// Prototypes for built-in functions seen by both vertex and fragment shaders.
//
//============================================================================
static TString BuiltInFunctionsCommon()
{
    TString s;

    //
    // Angle and Trigonometric Functions.
    //
    s.append(TString("float radians(float degrees);"));
    s.append(TString("vec2  radians(vec2  degrees);"));
    s.append(TString("vec3  radians(vec3  degrees);"));
    s.append(TString("vec4  radians(vec4  degrees);"));

    s.append(TString("float degrees(float radians);"));
    s.append(TString("vec2  degrees(vec2  radians);"));
    s.append(TString("vec3  degrees(vec3  radians);"));
    s.append(TString("vec4  degrees(vec4  radians);"));

    s.append(TString("float sin(float angle);"));
    s.append(TString("vec2  sin(vec2  angle);"));
    s.append(TString("vec3  sin(vec3  angle);"));
    s.append(TString("vec4  sin(vec4  angle);"));

    s.append(TString("float cos(float angle);"));
    s.append(TString("vec2  cos(vec2  angle);"));
    s.append(TString("vec3  cos(vec3  angle);"));
    s.append(TString("vec4  cos(vec4  angle);"));

    s.append(TString("float tan(float angle);"));
    s.append(TString("vec2  tan(vec2  angle);"));
    s.append(TString("vec3  tan(vec3  angle);"));
    s.append(TString("vec4  tan(vec4  angle);"));

    s.append(TString("float asin(float x);"));
    s.append(TString("vec2  asin(vec2  x);"));
    s.append(TString("vec3  asin(vec3  x);"));
    s.append(TString("vec4  asin(vec4  x);"));

    s.append(TString("float acos(float x);"));
    s.append(TString("vec2  acos(vec2  x);"));
    s.append(TString("vec3  acos(vec3  x);"));
    s.append(TString("vec4  acos(vec4  x);"));

    s.append(TString("float atan(float y, float x);"));
    s.append(TString("vec2  atan(vec2  y, vec2  x);"));
    s.append(TString("vec3  atan(vec3  y, vec3  x);"));
    s.append(TString("vec4  atan(vec4  y, vec4  x);"));

    s.append(TString("float atan(float y_over_x);"));
    s.append(TString("vec2  atan(vec2  y_over_x);"));
    s.append(TString("vec3  atan(vec3  y_over_x);"));
    s.append(TString("vec4  atan(vec4  y_over_x);"));

    //
    // Exponential Functions.
    //
    s.append(TString("float pow(float x, float y);"));
    s.append(TString("vec2  pow(vec2  x, vec2  y);"));
    s.append(TString("vec3  pow(vec3  x, vec3  y);"));
    s.append(TString("vec4  pow(vec4  x, vec4  y);"));

    s.append(TString("float exp(float x);"));
    s.append(TString("vec2  exp(vec2  x);"));
    s.append(TString("vec3  exp(vec3  x);"));
    s.append(TString("vec4  exp(vec4  x);"));

    s.append(TString("float log(float x);"));
    s.append(TString("vec2  log(vec2  x);"));
    s.append(TString("vec3  log(vec3  x);"));
    s.append(TString("vec4  log(vec4  x);"));

    s.append(TString("float exp2(float x);"));
    s.append(TString("vec2  exp2(vec2  x);"));
    s.append(TString("vec3  exp2(vec3  x);"));
    s.append(TString("vec4  exp2(vec4  x);"));

    s.append(TString("float log2(float x);"));
    s.append(TString("vec2  log2(vec2  x);"));
    s.append(TString("vec3  log2(vec3  x);"));
    s.append(TString("vec4  log2(vec4  x);"));

    s.append(TString("float sqrt(float x);"));
    s.append(TString("vec2  sqrt(vec2  x);"));
    s.append(TString("vec3  sqrt(vec3  x);"));
    s.append(TString("vec4  sqrt(vec4  x);"));

    s.append(TString("float inversesqrt(float x);"));
    s.append(TString("vec2  inversesqrt(vec2  x);"));
    s.append(TString("vec3  inversesqrt(vec3  x);"));
    s.append(TString("vec4  inversesqrt(vec4  x);"));

    //
    // Common Functions.
    //
    s.append(TString("float abs(float x);"));
    s.append(TString("vec2  abs(vec2  x);"));
    s.append(TString("vec3  abs(vec3  x);"));
    s.append(TString("vec4  abs(vec4  x);"));

    s.append(TString("float sign(float x);"));
    s.append(TString("vec2  sign(vec2  x);"));
    s.append(TString("vec3  sign(vec3  x);"));
    s.append(TString("vec4  sign(vec4  x);"));

    s.append(TString("float floor(float x);"));
    s.append(TString("vec2  floor(vec2  x);"));
    s.append(TString("vec3  floor(vec3  x);"));
    s.append(TString("vec4  floor(vec4  x);"));

    s.append(TString("float ceil(float x);"));
    s.append(TString("vec2  ceil(vec2  x);"));
    s.append(TString("vec3  ceil(vec3  x);"));
    s.append(TString("vec4  ceil(vec4  x);"));

    s.append(TString("float fract(float x);"));
    s.append(TString("vec2  fract(vec2  x);"));
    s.append(TString("vec3  fract(vec3  x);"));
    s.append(TString("vec4  fract(vec4  x);"));

    s.append(TString("float mod(float x, float y);"));
    s.append(TString("vec2  mod(vec2  x, float y);"));
    s.append(TString("vec3  mod(vec3  x, float y);"));
    s.append(TString("vec4  mod(vec4  x, float y);"));
    s.append(TString("vec2  mod(vec2  x, vec2  y);"));
    s.append(TString("vec3  mod(vec3  x, vec3  y);"));
    s.append(TString("vec4  mod(vec4  x, vec4  y);"));

    s.append(TString("float min(float x, float y);"));
    s.append(TString("vec2  min(vec2  x, float y);"));
    s.append(TString("vec3  min(vec3  x, float y);"));
    s.append(TString("vec4  min(vec4  x, float y);"));
    s.append(TString("vec2  min(vec2  x, vec2  y);"));
    s.append(TString("vec3  min(vec3  x, vec3  y);"));
    s.append(TString("vec4  min(vec4  x, vec4  y);"));

    s.append(TString("float max(float x, float y);"));
    s.append(TString("vec2  max(vec2  x, float y);"));
    s.append(TString("vec3  max(vec3  x, float y);"));
    s.append(TString("vec4  max(vec4  x, float y);"));
    s.append(TString("vec2  max(vec2  x, vec2  y);"));
    s.append(TString("vec3  max(vec3  x, vec3  y);"));
    s.append(TString("vec4  max(vec4  x, vec4  y);"));

    s.append(TString("float clamp(float x, float minVal, float maxVal);"));
    s.append(TString("vec2  clamp(vec2  x, float minVal, float maxVal);"));
    s.append(TString("vec3  clamp(vec3  x, float minVal, float maxVal);"));
    s.append(TString("vec4  clamp(vec4  x, float minVal, float maxVal);"));
    s.append(TString("vec2  clamp(vec2  x, vec2  minVal, vec2  maxVal);"));
    s.append(TString("vec3  clamp(vec3  x, vec3  minVal, vec3  maxVal);"));
    s.append(TString("vec4  clamp(vec4  x, vec4  minVal, vec4  maxVal);"));

    s.append(TString("float mix(float x, float y, float a);"));
    s.append(TString("vec2  mix(vec2  x, vec2  y, float a);"));
    s.append(TString("vec3  mix(vec3  x, vec3  y, float a);"));
    s.append(TString("vec4  mix(vec4  x, vec4  y, float a);"));
    s.append(TString("vec2  mix(vec2  x, vec2  y, vec2  a);"));
    s.append(TString("vec3  mix(vec3  x, vec3  y, vec3  a);"));
    s.append(TString("vec4  mix(vec4  x, vec4  y, vec4  a);"));

    s.append(TString("float step(float edge, float x);"));
    s.append(TString("vec2  step(vec2  edge, vec2  x);"));
    s.append(TString("vec3  step(vec3  edge, vec3  x);"));
    s.append(TString("vec4  step(vec4  edge, vec4  x);"));
    s.append(TString("vec2  step(float edge, vec2  x);"));
    s.append(TString("vec3  step(float edge, vec3  x);"));
    s.append(TString("vec4  step(float edge, vec4  x);"));

    s.append(TString("float smoothstep(float edge0, float edge1, float x);"));
    s.append(TString("vec2  smoothstep(vec2  edge0, vec2  edge1, vec2  x);"));
    s.append(TString("vec3  smoothstep(vec3  edge0, vec3  edge1, vec3  x);"));
    s.append(TString("vec4  smoothstep(vec4  edge0, vec4  edge1, vec4  x);"));
    s.append(TString("vec2  smoothstep(float edge0, float edge1, vec2  x);"));
    s.append(TString("vec3  smoothstep(float edge0, float edge1, vec3  x);"));
    s.append(TString("vec4  smoothstep(float edge0, float edge1, vec4  x);"));

    //
    // Geometric Functions.
    //
    s.append(TString("float length(float x);"));
    s.append(TString("float length(vec2  x);"));
    s.append(TString("float length(vec3  x);"));
    s.append(TString("float length(vec4  x);"));

    s.append(TString("float distance(float p0, float p1);"));
    s.append(TString("float distance(vec2  p0, vec2  p1);"));
    s.append(TString("float distance(vec3  p0, vec3  p1);"));
    s.append(TString("float distance(vec4  p0, vec4  p1);"));

    s.append(TString("float dot(float x, float y);"));
    s.append(TString("float dot(vec2  x, vec2  y);"));
    s.append(TString("float dot(vec3  x, vec3  y);"));
    s.append(TString("float dot(vec4  x, vec4  y);"));

    s.append(TString("vec3 cross(vec3 x, vec3 y);"));
    s.append(TString("float normalize(float x);"));
    s.append(TString("vec2  normalize(vec2  x);"));
    s.append(TString("vec3  normalize(vec3  x);"));
    s.append(TString("vec4  normalize(vec4  x);"));

    s.append(TString("float faceforward(float N, float I, float Nref);"));
    s.append(TString("vec2  faceforward(vec2  N, vec2  I, vec2  Nref);"));
    s.append(TString("vec3  faceforward(vec3  N, vec3  I, vec3  Nref);"));
    s.append(TString("vec4  faceforward(vec4  N, vec4  I, vec4  Nref);"));

    s.append(TString("float reflect(float I, float N);"));
    s.append(TString("vec2  reflect(vec2  I, vec2  N);"));
    s.append(TString("vec3  reflect(vec3  I, vec3  N);"));
    s.append(TString("vec4  reflect(vec4  I, vec4  N);"));

    s.append(TString("float refract(float I, float N, float eta);"));
    s.append(TString("vec2  refract(vec2  I, vec2  N, float eta);"));
    s.append(TString("vec3  refract(vec3  I, vec3  N, float eta);"));
    s.append(TString("vec4  refract(vec4  I, vec4  N, float eta);"));

    //
    // Matrix Functions.
    //
    s.append(TString("mat2 matrixCompMult(mat2 x, mat2 y);"));
    s.append(TString("mat3 matrixCompMult(mat3 x, mat3 y);"));
    s.append(TString("mat4 matrixCompMult(mat4 x, mat4 y);"));

    //
    // Vector relational functions.
    //
    s.append(TString("bvec2 lessThan(vec2 x, vec2 y);"));
    s.append(TString("bvec3 lessThan(vec3 x, vec3 y);"));
    s.append(TString("bvec4 lessThan(vec4 x, vec4 y);"));

    s.append(TString("bvec2 lessThan(ivec2 x, ivec2 y);"));
    s.append(TString("bvec3 lessThan(ivec3 x, ivec3 y);"));
    s.append(TString("bvec4 lessThan(ivec4 x, ivec4 y);"));

    s.append(TString("bvec2 lessThanEqual(vec2 x, vec2 y);"));
    s.append(TString("bvec3 lessThanEqual(vec3 x, vec3 y);"));
    s.append(TString("bvec4 lessThanEqual(vec4 x, vec4 y);"));

    s.append(TString("bvec2 lessThanEqual(ivec2 x, ivec2 y);"));
    s.append(TString("bvec3 lessThanEqual(ivec3 x, ivec3 y);"));
    s.append(TString("bvec4 lessThanEqual(ivec4 x, ivec4 y);"));

    s.append(TString("bvec2 greaterThan(vec2 x, vec2 y);"));
    s.append(TString("bvec3 greaterThan(vec3 x, vec3 y);"));
    s.append(TString("bvec4 greaterThan(vec4 x, vec4 y);"));

    s.append(TString("bvec2 greaterThan(ivec2 x, ivec2 y);"));
    s.append(TString("bvec3 greaterThan(ivec3 x, ivec3 y);"));
    s.append(TString("bvec4 greaterThan(ivec4 x, ivec4 y);"));

    s.append(TString("bvec2 greaterThanEqual(vec2 x, vec2 y);"));
    s.append(TString("bvec3 greaterThanEqual(vec3 x, vec3 y);"));
    s.append(TString("bvec4 greaterThanEqual(vec4 x, vec4 y);"));

    s.append(TString("bvec2 greaterThanEqual(ivec2 x, ivec2 y);"));
    s.append(TString("bvec3 greaterThanEqual(ivec3 x, ivec3 y);"));
    s.append(TString("bvec4 greaterThanEqual(ivec4 x, ivec4 y);"));

    s.append(TString("bvec2 equal(vec2 x, vec2 y);"));
    s.append(TString("bvec3 equal(vec3 x, vec3 y);"));
    s.append(TString("bvec4 equal(vec4 x, vec4 y);"));

    s.append(TString("bvec2 equal(ivec2 x, ivec2 y);"));
    s.append(TString("bvec3 equal(ivec3 x, ivec3 y);"));
    s.append(TString("bvec4 equal(ivec4 x, ivec4 y);"));

    s.append(TString("bvec2 equal(bvec2 x, bvec2 y);"));
    s.append(TString("bvec3 equal(bvec3 x, bvec3 y);"));
    s.append(TString("bvec4 equal(bvec4 x, bvec4 y);"));

    s.append(TString("bvec2 notEqual(vec2 x, vec2 y);"));
    s.append(TString("bvec3 notEqual(vec3 x, vec3 y);"));
    s.append(TString("bvec4 notEqual(vec4 x, vec4 y);"));

    s.append(TString("bvec2 notEqual(ivec2 x, ivec2 y);"));
    s.append(TString("bvec3 notEqual(ivec3 x, ivec3 y);"));
    s.append(TString("bvec4 notEqual(ivec4 x, ivec4 y);"));

    s.append(TString("bvec2 notEqual(bvec2 x, bvec2 y);"));
    s.append(TString("bvec3 notEqual(bvec3 x, bvec3 y);"));
    s.append(TString("bvec4 notEqual(bvec4 x, bvec4 y);"));

    s.append(TString("bool any(bvec2 x);"));
    s.append(TString("bool any(bvec3 x);"));
    s.append(TString("bool any(bvec4 x);"));

    s.append(TString("bool all(bvec2 x);"));
    s.append(TString("bool all(bvec3 x);"));
    s.append(TString("bool all(bvec4 x);"));

    s.append(TString("bvec2 not(bvec2 x);"));
    s.append(TString("bvec3 not(bvec3 x);"));
    s.append(TString("bvec4 not(bvec4 x);"));

    //
    // Noise functions.
    //
    //s.append(TString("float noise1(float x);"));
    //s.append(TString("float noise1(vec2  x);"));
    //s.append(TString("float noise1(vec3  x);"));
    //s.append(TString("float noise1(vec4  x);"));

    //s.append(TString("vec2 noise2(float x);"));
    //s.append(TString("vec2 noise2(vec2  x);"));
    //s.append(TString("vec2 noise2(vec3  x);"));
    //s.append(TString("vec2 noise2(vec4  x);"));

    //s.append(TString("vec3 noise3(float x);"));
    //s.append(TString("vec3 noise3(vec2  x);"));
    //s.append(TString("vec3 noise3(vec3  x);"));
    //s.append(TString("vec3 noise3(vec4  x);"));

    //s.append(TString("vec4 noise4(float x);"));
    //s.append(TString("vec4 noise4(vec2  x);"));
    //s.append(TString("vec4 noise4(vec3  x);"));
    //s.append(TString("vec4 noise4(vec4  x);"));

    return s;
}

//============================================================================
//
// Prototypes for built-in functions seen by vertex shaders only.
//
//============================================================================
static TString BuiltInFunctionsVertex(const ShBuiltInResources& resources)
{
    TString s;

    //
    // Geometric Functions.
    //
    //s.append(TString("vec4 ftransform();"));

    //
    // Texture Functions.
    //
    if (resources.MaxVertexTextureImageUnits > 0) {
        s.append(TString("vec4 texture2D(sampler2D sampler, vec2 coord);"));
        s.append(TString("vec4 texture2DProj(sampler2D sampler, vec3 coord);"));
        s.append(TString("vec4 texture2DProj(sampler2D sampler, vec4 coord);"));
        s.append(TString("vec4 textureCube(samplerCube sampler, vec3 coord);"));

        s.append(TString("vec4 texture2DLod(sampler2D sampler, vec2 coord, float lod);"));
        s.append(TString("vec4 texture2DProjLod(sampler2D sampler, vec3 coord, float lod);"));
        s.append(TString("vec4 texture2DProjLod(sampler2D sampler, vec4 coord, float lod);"));
        s.append(TString("vec4 textureCubeLod(samplerCube sampler, vec3 coord, float lod);"));
    }

    return s;
}

//============================================================================
//
// Prototypes for built-in functions seen by fragment shaders only.
//
//============================================================================
static TString BuiltInFunctionsFragment(const ShBuiltInResources& resources)
{
    TString s;

    //
    // Texture Functions.
    //
    s.append(TString("vec4 texture2D(sampler2D sampler, vec2 coord);"));
    s.append(TString("vec4 texture2DProj(sampler2D sampler, vec3 coord);"));
    s.append(TString("vec4 texture2DProj(sampler2D sampler, vec4 coord);"));
    s.append(TString("vec4 textureCube(samplerCube sampler, vec3 coord);"));

    s.append(TString("vec4 texture2D(sampler2D sampler, vec2 coord, float bias);"));
    s.append(TString("vec4 texture2DProj(sampler2D sampler, vec3 coord, float bias);"));
    s.append(TString("vec4 texture2DProj(sampler2D sampler, vec4 coord, float bias);"));
    s.append(TString("vec4 textureCube(samplerCube sampler, vec3 coord, float bias);"));

    if (resources.OES_standard_derivatives) {
        s.append(TString("float dFdx(float p);"));
        s.append(TString("vec2  dFdx(vec2  p);"));
        s.append(TString("vec3  dFdx(vec3  p);"));
        s.append(TString("vec4  dFdx(vec4  p);"));

        s.append(TString("float dFdy(float p);"));
        s.append(TString("vec2  dFdy(vec2  p);"));
        s.append(TString("vec3  dFdy(vec3  p);"));
        s.append(TString("vec4  dFdy(vec4  p);"));

        s.append(TString("float fwidth(float p);"));
        s.append(TString("vec2  fwidth(vec2  p);"));
        s.append(TString("vec3  fwidth(vec3  p);"));
        s.append(TString("vec4  fwidth(vec4  p);"));
    }

    return s;
}

//============================================================================
//
// Standard uniforms.
//
//============================================================================
static TString StandardUniforms()
{
    TString s;

    //
    // Depth range in window coordinates
    //
    s.append(TString("struct gl_DepthRangeParameters {"));
    s.append(TString("    highp float near;"));        // n
    s.append(TString("    highp float far;"));         // f
    s.append(TString("    highp float diff;"));        // f - n
    s.append(TString("};"));
    s.append(TString("uniform gl_DepthRangeParameters gl_DepthRange;"));

    return s;
}

//============================================================================
//
// Default precision for vertex shaders.
//
//============================================================================
static TString DefaultPrecisionVertex()
{
    TString s;

    s.append(TString("precision highp int;"));
    s.append(TString("precision highp float;"));

    return s;
}

//============================================================================
//
// Default precision for fragment shaders.
//
//============================================================================
static TString DefaultPrecisionFragment()
{
    TString s;

    s.append(TString("precision mediump int;"));
    // No default precision for float in fragment shaders

    return s;
}

//============================================================================
//
// Implementation dependent built-in constants.
//
//============================================================================
static TString BuiltInConstants(const ShBuiltInResources &resources)
{
    TStringStream s;

    s << "const int gl_MaxVertexAttribs = " << resources.MaxVertexAttribs << ";";
    s << "const int gl_MaxVertexUniformVectors = " << resources.MaxVertexUniformVectors << ";";

    s << "const int gl_MaxVaryingVectors = " << resources.MaxVaryingVectors << ";";
    s << "const int gl_MaxVertexTextureImageUnits = " << resources.MaxVertexTextureImageUnits << ";";
    s << "const int gl_MaxCombinedTextureImageUnits = " << resources.MaxCombinedTextureImageUnits << ";";
    s << "const int gl_MaxTextureImageUnits = " << resources.MaxTextureImageUnits << ";";
    s << "const int gl_MaxFragmentUniformVectors = " << resources.MaxFragmentUniformVectors << ";";
    s << "const int gl_MaxDrawBuffers = " << resources.MaxDrawBuffers << ";";

    return s.str();
}

void TBuiltIns::initialize(ShShaderType type, ShShaderSpec spec,
                           const ShBuiltInResources& resources)
{
    switch (type) {
    case SH_FRAGMENT_SHADER:
        builtInStrings.push_back(DefaultPrecisionFragment());
        builtInStrings.push_back(BuiltInFunctionsCommon());
        builtInStrings.push_back(BuiltInFunctionsFragment(resources));
        builtInStrings.push_back(StandardUniforms());
        break;

    case SH_VERTEX_SHADER:
        builtInStrings.push_back(DefaultPrecisionVertex());
        builtInStrings.push_back(BuiltInFunctionsCommon());
        builtInStrings.push_back(BuiltInFunctionsVertex(resources));
        builtInStrings.push_back(StandardUniforms());
        break;

    default: assert(false && "Language not supported");
    }

    builtInStrings.push_back(BuiltInConstants(resources));
}

void IdentifyBuiltIns(ShShaderType type, ShShaderSpec spec,
                      const ShBuiltInResources& resources,
                      TSymbolTable& symbolTable)
{
    //
    // First, insert some special built-in variables that are not in 
    // the built-in header files.
    //
    switch(type) {
    case SH_FRAGMENT_SHADER:
        symbolTable.insert(*new TVariable(NewPoolTString("gl_FragCoord"),                   TType(EbtFloat, EbpMedium, EvqFragCoord,   4)));
        symbolTable.insert(*new TVariable(NewPoolTString("gl_FrontFacing"),                 TType(EbtBool,  EbpUndefined, EvqFrontFacing, 1)));
        symbolTable.insert(*new TVariable(NewPoolTString("gl_FragColor"),                   TType(EbtFloat, EbpMedium, EvqFragColor,   4)));
        symbolTable.insert(*new TVariable(NewPoolTString("gl_FragData[gl_MaxDrawBuffers]"), TType(EbtFloat, EbpMedium, EvqFragData,    4)));
        symbolTable.insert(*new TVariable(NewPoolTString("gl_PointCoord"),                  TType(EbtFloat, EbpMedium, EvqPointCoord,  2)));
        break;

    case SH_VERTEX_SHADER:
        symbolTable.insert(*new TVariable(NewPoolTString("gl_Position"),    TType(EbtFloat, EbpHigh, EvqPosition,    4)));
        symbolTable.insert(*new TVariable(NewPoolTString("gl_PointSize"),   TType(EbtFloat, EbpMedium, EvqPointSize,   1)));
        break;

    default: assert(false && "Language not supported");
    }

    //
    // Next, identify which built-ins from the already loaded headers have
    // a mapping to an operator.  Those that are not identified as such are
    // expected to be resolved through a library of functions, versus as
    // operations.
    //
    symbolTable.relateToOperator("not",              EOpVectorLogicalNot);

    symbolTable.relateToOperator("matrixCompMult",   EOpMul);

    symbolTable.relateToOperator("equal",            EOpVectorEqual);
    symbolTable.relateToOperator("notEqual",         EOpVectorNotEqual);
    symbolTable.relateToOperator("lessThan",         EOpLessThan);
    symbolTable.relateToOperator("greaterThan",      EOpGreaterThan);
    symbolTable.relateToOperator("lessThanEqual",    EOpLessThanEqual);
    symbolTable.relateToOperator("greaterThanEqual", EOpGreaterThanEqual);
    
    symbolTable.relateToOperator("radians",      EOpRadians);
    symbolTable.relateToOperator("degrees",      EOpDegrees);
    symbolTable.relateToOperator("sin",          EOpSin);
    symbolTable.relateToOperator("cos",          EOpCos);
    symbolTable.relateToOperator("tan",          EOpTan);
    symbolTable.relateToOperator("asin",         EOpAsin);
    symbolTable.relateToOperator("acos",         EOpAcos);
    symbolTable.relateToOperator("atan",         EOpAtan);

    symbolTable.relateToOperator("pow",          EOpPow);
    symbolTable.relateToOperator("exp2",         EOpExp2);
    symbolTable.relateToOperator("log",          EOpLog);
    symbolTable.relateToOperator("exp",          EOpExp);
    symbolTable.relateToOperator("log2",         EOpLog2);
    symbolTable.relateToOperator("sqrt",         EOpSqrt);
    symbolTable.relateToOperator("inversesqrt",  EOpInverseSqrt);

    symbolTable.relateToOperator("abs",          EOpAbs);
    symbolTable.relateToOperator("sign",         EOpSign);
    symbolTable.relateToOperator("floor",        EOpFloor);
    symbolTable.relateToOperator("ceil",         EOpCeil);
    symbolTable.relateToOperator("fract",        EOpFract);
    symbolTable.relateToOperator("mod",          EOpMod);
    symbolTable.relateToOperator("min",          EOpMin);
    symbolTable.relateToOperator("max",          EOpMax);
    symbolTable.relateToOperator("clamp",        EOpClamp);
    symbolTable.relateToOperator("mix",          EOpMix);
    symbolTable.relateToOperator("step",         EOpStep);
    symbolTable.relateToOperator("smoothstep",   EOpSmoothStep);

    symbolTable.relateToOperator("length",       EOpLength);
    symbolTable.relateToOperator("distance",     EOpDistance);
    symbolTable.relateToOperator("dot",          EOpDot);
    symbolTable.relateToOperator("cross",        EOpCross);
    symbolTable.relateToOperator("normalize",    EOpNormalize);
    symbolTable.relateToOperator("faceforward",  EOpFaceForward);
    symbolTable.relateToOperator("reflect",      EOpReflect);
    symbolTable.relateToOperator("refract",      EOpRefract);
    
    symbolTable.relateToOperator("any",          EOpAny);
    symbolTable.relateToOperator("all",          EOpAll);

    // Map language-specific operators.
    switch(type) {
    case SH_VERTEX_SHADER:
        break;
    case SH_FRAGMENT_SHADER:
        if (resources.OES_standard_derivatives) {
            symbolTable.relateToOperator("dFdx",   EOpDFdx);
            symbolTable.relateToOperator("dFdy",   EOpDFdy);
            symbolTable.relateToOperator("fwidth", EOpFwidth);

            symbolTable.relateToExtension("dFdx", "GL_OES_standard_derivatives");
            symbolTable.relateToExtension("dFdy", "GL_OES_standard_derivatives");
            symbolTable.relateToExtension("fwidth", "GL_OES_standard_derivatives");
        }
        break;
    default: break;
    }

    // Finally add resource-specific variables.
    switch(type) {
    case SH_FRAGMENT_SHADER: {
            // Set up gl_FragData.  The array size.
            TType fragData(EbtFloat, EbpMedium, EvqFragColor,   4, false, true);
            fragData.setArraySize(resources.MaxDrawBuffers);
            symbolTable.insert(*new TVariable(NewPoolTString("gl_FragData"),    fragData));
        }
        break;
    default: break;
    }
}

void InitExtensionBehavior(const ShBuiltInResources& resources,
                           TExtensionBehavior& extBehavior)
{
    if (resources.OES_standard_derivatives)
        extBehavior["GL_OES_standard_derivatives"] = EBhDisable;
}
