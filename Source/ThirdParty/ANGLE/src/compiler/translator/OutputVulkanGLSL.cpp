//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// OutputVulkanGLSL:
//   Code that outputs shaders that fit GL_KHR_vulkan_glsl.
//   The shaders are then fed into glslang to spit out SPIR-V (libANGLE-side).
//   See: https://www.khronos.org/registry/vulkan/specs/misc/GL_KHR_vulkan_glsl.txt
//

#include "compiler/translator/OutputVulkanGLSL.h"

#include "compiler/translator/util.h"

namespace sh
{

TOutputVulkanGLSL::TOutputVulkanGLSL(TInfoSinkBase &objSink,
                                     ShArrayIndexClampingStrategy clampingStrategy,
                                     ShHashFunction64 hashFunction,
                                     NameMap &nameMap,
                                     TSymbolTable *symbolTable,
                                     sh::GLenum shaderType,
                                     int shaderVersion,
                                     ShShaderOutput output,
                                     ShCompileOptions compileOptions)
    : TOutputGLSL(objSink,
                  clampingStrategy,
                  hashFunction,
                  nameMap,
                  symbolTable,
                  shaderType,
                  shaderVersion,
                  output,
                  compileOptions)
{
}

// TODO(jmadill): This is not complete.
void TOutputVulkanGLSL::writeLayoutQualifier(TIntermTyped *variable)
{
    const TType &type = variable->getType();

    bool needsCustomLayout =
        (type.getQualifier() == EvqAttribute || type.getQualifier() == EvqFragmentOut ||
         type.getQualifier() == EvqVertexIn || IsVarying(type.getQualifier()) ||
         IsSampler(type.getBasicType()));

    if (!NeedsToWriteLayoutQualifier(type) && !needsCustomLayout)
    {
        return;
    }

    TInfoSinkBase &out                      = objSink();
    const TLayoutQualifier &layoutQualifier = type.getLayoutQualifier();
    out << "layout(";

    // This isn't super clean, but it gets the job done.
    // See corresponding code in GlslangWrapper.cpp.
    // TODO(jmadill): Ensure declarations are separated.

    TIntermSymbol *symbol = variable->getAsSymbolNode();
    ASSERT(symbol);

    if (needsCustomLayout)
    {
        out << "@@ LAYOUT-" << symbol->getName().getString() << " @@";
    }

    if (IsImage(type.getBasicType()) && layoutQualifier.imageInternalFormat != EiifUnspecified)
    {
        ASSERT(type.getQualifier() == EvqTemporary || type.getQualifier() == EvqUniform);
        out << getImageInternalFormatString(layoutQualifier.imageInternalFormat);
    }

    out << ") ";
}

}  // namespace sh
