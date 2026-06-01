//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "compiler/translator/glsl/OutputGLSL.h"

#include "compiler/translator/Compiler.h"

namespace sh
{
bool RemoveInvariant(sh::GLenum shaderType,
                     int shaderVersion,
                     ShShaderOutput outputType,
                     const ShCompileOptions &compileOptions)
{
    return (shaderType == GL_FRAGMENT_SHADER && IsGLSL420OrNewer(outputType)) ||
           (shaderType == GL_VERTEX_SHADER && compileOptions.removeInvariantAndCentroidForESSL3 &&
            shaderVersion >= 300);
}

TOutputGLSL::TOutputGLSL(TCompiler *compiler,
                         TInfoSinkBase &objSink,
                         const ShCompileOptions &compileOptions)
    : TOutputGLSLBase(compiler,
                      objSink,
                      compileOptions,
                      RemoveInvariant(compiler->getShaderType(),
                                      compiler->getShaderVersion(),
                                      compiler->getOutputType(),
                                      compileOptions))
{}

bool TOutputGLSL::writeVariablePrecision(TPrecision)
{
    return false;
}

void TOutputGLSL::visitSymbol(TIntermSymbol *node)
{
    TInfoSinkBase &out = objSink();

    // All the special cases are built-ins, so if it's not a built-in we can return early.
    if (node->variable().symbolType() != SymbolType::BuiltIn)
    {
        TOutputGLSLBase::visitSymbol(node);
        return;
    }

    ASSERT(sh::IsGLSL150OrNewer(getShaderOutput()));

    // Some built-ins get a special translation.
    const ImmutableString &name = node->getName();
    if (name == "gl_FragDepthEXT")
    {
        out << "gl_FragDepth";
    }
    else if (name == "gl_FragColor")
    {
        out << "webgl_FragColor";
    }
    else if (name == "gl_FragData")
    {
        out << "webgl_FragData";
    }
    else if (name == "gl_SecondaryFragColorEXT")
    {
        out << "webgl_SecondaryFragColor";
    }
    else if (name == "gl_SecondaryFragDataEXT")
    {
        out << "webgl_SecondaryFragData";
    }
    else
    {
        TOutputGLSLBase::visitSymbol(node);
    }
}

ImmutableString TOutputGLSL::translateTextureFunction(const ImmutableString &name,
                                                      const ShCompileOptions &option)
{
    // Check WEBGL_video_texture invocation first.
    if (name == "textureVideoWEBGL")
    {
        if (option.takeVideoTextureAsExternalOES)
        {
            // TODO(http://anglebug.com/42262534): Implement external image situation.
            UNIMPLEMENTED();
            return ImmutableString("");
        }
        else
        {
            // Use "texture" instead of "texture2D" to match the translation
            // of samplerVideoWEBGL to sampler2D and the GLSL version's texture function naming.
            ASSERT(sh::IsGLSL150OrNewer(getShaderOutput()));
            return ImmutableString("texture");
        }
    }

    ASSERT(sh::IsGLSL150OrNewer(getShaderOutput()));
    static const char *legacyToCoreRename[] = {
        "texture2D", "texture", "texture2DProj", "textureProj", "texture2DLod", "textureLod",
        "texture2DProjLod", "textureProjLod", "texture2DRect", "texture", "texture2DRectProj",
        "textureProj", "textureCube", "texture", "textureCubeLod", "textureLod",
        // Extensions
        "texture2DLodEXT", "textureLod", "texture2DProjLodEXT", "textureProjLod",
        "textureCubeLodEXT", "textureLod", "texture2DGradEXT", "textureGrad",
        "texture2DProjGradEXT", "textureProjGrad", "textureCubeGradEXT", "textureGrad", "texture3D",
        "texture", "texture3DProj", "textureProj", "texture3DLod", "textureLod", "texture3DProjLod",
        "textureProjLod", "shadow2DEXT", "texture", "shadow2DProjEXT", "textureProj", nullptr,
        nullptr};

    for (int i = 0; legacyToCoreRename[i] != nullptr; i += 2)
    {
        if (name == legacyToCoreRename[i])
        {
            return ImmutableString(legacyToCoreRename[i + 1]);
        }
    }

    return name;
}

}  // namespace sh
