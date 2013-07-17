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
// Prototypes for built-in functions seen by fragment shaders only.
//
//============================================================================
static TString BuiltInFunctionsFragment(const ShBuiltInResources& resources)
{
    TString s;

    //
    // Texture Functions.
    //
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
static TString BuiltInConstants(ShShaderSpec spec, const ShBuiltInResources &resources)
{
    TStringStream s;

    s << "const int gl_MaxVertexAttribs = " << resources.MaxVertexAttribs << ";";
    s << "const int gl_MaxVertexUniformVectors = " << resources.MaxVertexUniformVectors << ";";

    s << "const int gl_MaxVaryingVectors = " << resources.MaxVaryingVectors << ";";
    s << "const int gl_MaxVertexTextureImageUnits = " << resources.MaxVertexTextureImageUnits << ";";
    s << "const int gl_MaxCombinedTextureImageUnits = " << resources.MaxCombinedTextureImageUnits << ";";
    s << "const int gl_MaxTextureImageUnits = " << resources.MaxTextureImageUnits << ";";
    s << "const int gl_MaxFragmentUniformVectors = " << resources.MaxFragmentUniformVectors << ";";

    if (spec != SH_CSS_SHADERS_SPEC)
        s << "const int gl_MaxDrawBuffers = " << resources.MaxDrawBuffers << ";";

    return s.str();
}

void TBuiltIns::initialize(ShShaderType type, ShShaderSpec spec,
                           const ShBuiltInResources& resources)
{
    switch (type) {
    case SH_FRAGMENT_SHADER:
        builtInStrings.push_back(DefaultPrecisionFragment());
        builtInStrings.push_back(BuiltInFunctionsFragment(resources));
        builtInStrings.push_back(StandardUniforms());
        break;

    case SH_VERTEX_SHADER:
        builtInStrings.push_back(DefaultPrecisionVertex());
        builtInStrings.push_back(StandardUniforms());
        break;

    default: assert(false && "Language not supported");
    }

    builtInStrings.push_back(BuiltInConstants(spec, resources));
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
        symbolTable.insert(*new TVariable(NewPoolTString("gl_FragCoord"),                       TType(EbtFloat, EbpMedium, EvqFragCoord,   4)));
        symbolTable.insert(*new TVariable(NewPoolTString("gl_FrontFacing"),                     TType(EbtBool,  EbpUndefined, EvqFrontFacing, 1)));
        symbolTable.insert(*new TVariable(NewPoolTString("gl_PointCoord"),                      TType(EbtFloat, EbpMedium, EvqPointCoord,  2)));

        //
        // In CSS Shaders, gl_FragColor, gl_FragData, and gl_MaxDrawBuffers are not available.
        // Instead, css_MixColor and css_ColorMatrix are available.
        //
        if (spec != SH_CSS_SHADERS_SPEC) {
            symbolTable.insert(*new TVariable(NewPoolTString("gl_FragColor"),                   TType(EbtFloat, EbpMedium, EvqFragColor,   4)));
            symbolTable.insert(*new TVariable(NewPoolTString("gl_FragData[gl_MaxDrawBuffers]"), TType(EbtFloat, EbpMedium, EvqFragData,    4)));
            if (resources.EXT_frag_depth) {
                symbolTable.insert(*new TVariable(NewPoolTString("gl_FragDepthEXT"),            TType(EbtFloat, resources.FragmentPrecisionHigh ? EbpHigh : EbpMedium, EvqFragDepth, 1)));
                symbolTable.relateToExtension("gl_FragDepthEXT", "GL_EXT_frag_depth");
            }
        } else {
            symbolTable.insert(*new TVariable(NewPoolTString("css_MixColor"),                   TType(EbtFloat, EbpMedium, EvqGlobal,      4)));
            symbolTable.insert(*new TVariable(NewPoolTString("css_ColorMatrix"),                TType(EbtFloat, EbpMedium, EvqGlobal,      4, true)));
        }

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
    case SH_FRAGMENT_SHADER:
        if (spec != SH_CSS_SHADERS_SPEC) {
            // Set up gl_FragData.  The array size.
            TType fragData(EbtFloat, EbpMedium, EvqFragData, 4, false, true);
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
        extBehavior["GL_OES_standard_derivatives"] = EBhUndefined;
    if (resources.OES_EGL_image_external)
        extBehavior["GL_OES_EGL_image_external"] = EBhUndefined;
    if (resources.ARB_texture_rectangle)
        extBehavior["GL_ARB_texture_rectangle"] = EBhUndefined;
    if (resources.EXT_draw_buffers)
        extBehavior["GL_EXT_draw_buffers"] = EBhUndefined;
    if (resources.EXT_frag_depth)
        extBehavior["GL_EXT_frag_depth"] = EBhUndefined;
}
