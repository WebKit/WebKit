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

#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/Symbol.h"
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
{}

// TODO(jmadill): This is not complete.
void TOutputVulkanGLSL::writeLayoutQualifier(TIntermTyped *variable)
{
    const TType &type = variable->getType();

    bool needsCustomLayout =
        (type.getQualifier() == EvqAttribute || type.getQualifier() == EvqFragmentOut ||
         type.getQualifier() == EvqVertexIn || IsVarying(type.getQualifier()) ||
         IsSampler(type.getBasicType()) || type.isInterfaceBlock());

    if (!NeedsToWriteLayoutQualifier(type) && !needsCustomLayout)
    {
        return;
    }

    TInfoSinkBase &out                      = objSink();
    const TLayoutQualifier &layoutQualifier = type.getLayoutQualifier();

    // This isn't super clean, but it gets the job done.
    // See corresponding code in GlslangWrapper.cpp.
    TIntermSymbol *symbol = variable->getAsSymbolNode();
    ASSERT(symbol);

    if (needsCustomLayout)
    {
        out << "@@ LAYOUT-" << symbol->getName() << " @@";
    }
    else
    {
        out << "layout(";
    }

    if (IsImage(type.getBasicType()) && layoutQualifier.imageInternalFormat != EiifUnspecified)
    {
        ASSERT(type.getQualifier() == EvqTemporary || type.getQualifier() == EvqUniform);
        out << getImageInternalFormatString(layoutQualifier.imageInternalFormat);
    }

    if (!needsCustomLayout)
    {
        out << ") ";
    }
}

void TOutputVulkanGLSL::writeQualifier(TQualifier qualifier, const TSymbol *symbol)
{
    if (qualifier != EvqUniform && qualifier != EvqVaryingIn && qualifier != EvqVaryingOut &&
        qualifier != EvqAttribute)
    {
        TOutputGLSLBase::writeQualifier(qualifier, symbol);
        return;
    }

    if (symbol == nullptr)
    {
        return;
    }

    TInfoSinkBase &out = objSink();
    out << "@@ QUALIFIER-" << symbol->name().data() << " @@ ";
}

void TOutputVulkanGLSL::writeVariableType(const TType &type, const TSymbol *symbol)
{
    TType overrideType(type);

    // External textures are treated as 2D textures in the vulkan back-end
    if (type.getBasicType() == EbtSamplerExternalOES)
    {
        overrideType.setBasicType(EbtSampler2D);
    }

    TOutputGLSL::writeVariableType(overrideType, symbol);
}

void TOutputVulkanGLSL::writeStructType(const TStructure *structure)
{
    if (!structDeclared(structure))
    {
        declareStruct(structure);
        objSink() << ";\n";
    }
}
}  // namespace sh
