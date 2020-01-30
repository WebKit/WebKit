//
// Copyright (c) 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TOutputVulkanGLSLForMetal:
//    This is a special version Vulkan GLSL output that will make some special
//    considerations for Metal backend limitations.
//

#include "compiler/translator/OutputVulkanGLSLForMetal.h"

#include "common/apple_platform_utils.h"
#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/Symbol.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{
bool gOverrideRemoveInvariant = false;

bool ShoudRemoveInvariant(const TType &type)
{
    if (gOverrideRemoveInvariant)
    {
        return true;
    }

    if (type.getQualifier() != EvqPosition)
    {
        // Metal only supports invariant for gl_Position
        return true;
    }

    if (ANGLE_APPLE_AVAILABLE_XCI(10.14, 13.0, 12))
    {
        return false;
    }
    else
    {
        // Metal 2.1 is not available, so we need to remove "invariant" keyword
        return true;
    }
}

}

// static
void TOutputVulkanGLSLForMetal::RemoveInvariantForTest(bool remove)
{
    gOverrideRemoveInvariant = remove;
}

TOutputVulkanGLSLForMetal::TOutputVulkanGLSLForMetal(TInfoSinkBase &objSink,
                                                     ShArrayIndexClampingStrategy clampingStrategy,
                                                     ShHashFunction64 hashFunction,
                                                     NameMap &nameMap,
                                                     TSymbolTable *symbolTable,
                                                     sh::GLenum shaderType,
                                                     int shaderVersion,
                                                     ShShaderOutput output,
                                                     ShCompileOptions compileOptions)
    : TOutputVulkanGLSL(objSink,
                        clampingStrategy,
                        hashFunction,
                        nameMap,
                        symbolTable,
                        shaderType,
                        shaderVersion,
                        output,
                        compileOptions)
{}

void TOutputVulkanGLSLForMetal::writeVariableType(const TType &type, const TSymbol *symbol)
{
    TType overrideType(type);

    // Remove invariant keyword if required.
    if (type.isInvariant() && ShoudRemoveInvariant(type))
    {
        overrideType.setInvariant(false);
    }

    TOutputVulkanGLSL::writeVariableType(overrideType, symbol);
}

bool TOutputVulkanGLSLForMetal::visitInvariantDeclaration(Visit visit,
                                                          TIntermInvariantDeclaration *node)
{
    TInfoSinkBase &out = objSink();
    ASSERT(visit == PreVisit);
    const TIntermSymbol *symbol = node->getSymbol();
    if (!ShoudRemoveInvariant(symbol->getType()))
    {
        out << "invariant ";
    }
    out << hashName(&symbol->variable());
    return false;
}
}  // namespace sh
