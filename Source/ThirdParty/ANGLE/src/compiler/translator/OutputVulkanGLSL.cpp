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

namespace sh
{

TOutputVulkanGLSL::TOutputVulkanGLSL(TInfoSinkBase &objSink,
                                     ShArrayIndexClampingStrategy clampingStrategy,
                                     ShHashFunction64 hashFunction,
                                     NameMap &nameMap,
                                     TSymbolTable &symbolTable,
                                     sh::GLenum shaderType,
                                     int shaderVersion,
                                     ShShaderOutput output,
                                     ShCompileOptions compileOptions)
    : TOutputGLSLBase(objSink,
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
void TOutputVulkanGLSL::writeLayoutQualifier(const TType &type)
{
    TInfoSinkBase &out                      = objSink();
    const TLayoutQualifier &layoutQualifier = type.getLayoutQualifier();
    out << "layout(";

    if (type.getQualifier() == EvqAttribute || type.getQualifier() == EvqFragmentOut ||
        type.getQualifier() == EvqVertexIn)
    {
        // TODO(jmadill): Multiple output locations.
        out << "location = "
            << "0";
    }

    if (IsImage(type.getBasicType()) && layoutQualifier.imageInternalFormat != EiifUnspecified)
    {
        ASSERT(type.getQualifier() == EvqTemporary || type.getQualifier() == EvqUniform);
        out << getImageInternalFormatString(layoutQualifier.imageInternalFormat);
    }

    out << ") ";
}

bool TOutputVulkanGLSL::writeVariablePrecision(TPrecision precision)
{
    if (precision == EbpUndefined)
        return false;

    TInfoSinkBase &out = objSink();
    out << getPrecisionString(precision);
    return true;
}

void TOutputVulkanGLSL::visitSymbol(TIntermSymbol *node)
{
    TInfoSinkBase &out = objSink();

    const TString &symbol = node->getSymbol();
    if (symbol == "gl_FragColor")
    {
        out << "webgl_FragColor";
    }
    else if (symbol == "gl_FragData")
    {
        out << "webgl_FragData";
    }
    else
    {
        TOutputGLSLBase::visitSymbol(node);
    }
}

}  // namespace sh
