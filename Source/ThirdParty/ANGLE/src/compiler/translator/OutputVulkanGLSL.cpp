//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
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
#include "compiler/translator/ValidateVaryingLocations.h"
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
                                     bool forceHighp,
                                     bool enablePrecision,
                                     ShCompileOptions compileOptions)
    : TOutputGLSL(objSink,
                  clampingStrategy,
                  hashFunction,
                  nameMap,
                  symbolTable,
                  shaderType,
                  shaderVersion,
                  output,
                  compileOptions),
      mNextUnusedBinding(0),
      mNextUnusedInputLocation(0),
      mNextUnusedOutputLocation(0),
      mForceHighp(forceHighp),
      mEnablePrecision(enablePrecision)
{}

void TOutputVulkanGLSL::writeLayoutQualifier(TIntermTyped *variable)
{
    const TType &type = variable->getType();

    bool needsSetBinding =
        IsSampler(type.getBasicType()) || type.isInterfaceBlock() || IsImage(type.getBasicType());
    bool needsLocation = type.getQualifier() == EvqAttribute ||
                         type.getQualifier() == EvqVertexIn ||
                         type.getQualifier() == EvqFragmentOut || IsVarying(type.getQualifier());

    if (!NeedsToWriteLayoutQualifier(type) && !needsSetBinding && !needsLocation)
    {
        return;
    }

    TInfoSinkBase &out                      = objSink();
    const TLayoutQualifier &layoutQualifier = type.getLayoutQualifier();

    // This isn't super clean, but it gets the job done.
    // See corresponding code in glslang_wrapper_utils.cpp.
    TIntermSymbol *symbol = variable->getAsSymbolNode();
    ASSERT(symbol);

    ImmutableString name      = symbol->getName();
    const char *blockStorage  = nullptr;
    const char *matrixPacking = nullptr;

    // For interface blocks, use the block name instead.  When the layout qualifier is being
    // replaced in the backend, that would be the name that's available.
    if (type.isInterfaceBlock())
    {
        const TInterfaceBlock *interfaceBlock = type.getInterfaceBlock();
        name                                  = interfaceBlock->name();
        TLayoutBlockStorage storage           = interfaceBlock->blockStorage();

        // Make sure block storage format is specified.
        if (storage != EbsStd430)
        {
            // Change interface block layout qualifiers to std140 for any layout that is not
            // explicitly set to std430.  This is to comply with GL_KHR_vulkan_glsl where shared and
            // packed are not allowed (and std140 could be used instead) and unspecified layouts can
            // assume either std140 or std430 (and we choose std140 as std430 is not yet universally
            // supported).
            storage = EbsStd140;
        }

        if (interfaceBlock->blockStorage() != EbsUnspecified)
        {
            blockStorage = getBlockStorageString(storage);
        }
    }

    // Specify matrix packing if necessary.
    if (layoutQualifier.matrixPacking != EmpUnspecified)
    {
        matrixPacking = getMatrixPackingString(layoutQualifier.matrixPacking);
    }

    const char *separator = "";
    out << "layout(";

    // If the resource declaration requires set & binding layout qualifiers, specify arbitrary
    // ones.
    if (needsSetBinding)
    {
        out << "set=0, binding=" << nextUnusedBinding();
        separator = ", ";
    }

    if (needsLocation)
    {
        const unsigned int locationCount = CalculateVaryingLocationCount(symbol, getShaderType());
        uint32_t location                = IsShaderIn(type.getQualifier())
                                ? nextUnusedInputLocation(locationCount)
                                : nextUnusedOutputLocation(locationCount);

        out << "location=" << location;
        separator = ", ";
    }

    // Output the list of qualifiers already known at this stage, i.e. everything other than
    // `location` and `set`/`binding`.
    std::string otherQualifiers = getCommonLayoutQualifiers(variable);

    if (blockStorage)
    {
        out << separator << blockStorage;
        separator = ", ";
    }
    if (matrixPacking)
    {
        out << separator << matrixPacking;
        separator = ", ";
    }
    if (!otherQualifiers.empty())
    {
        out << separator << otherQualifiers;
    }

    out << ") ";
}

void TOutputVulkanGLSL::writeVariableType(const TType &type,
                                          const TSymbol *symbol,
                                          bool isFunctionArgument)
{
    TType overrideType(type);

    // External textures are treated as 2D textures in the vulkan back-end
    if (type.getBasicType() == EbtSamplerExternalOES)
    {
        overrideType.setBasicType(EbtSampler2D);
    }

    TOutputGLSL::writeVariableType(overrideType, symbol, isFunctionArgument);
}

void TOutputVulkanGLSL::writeStructType(const TStructure *structure)
{
    if (!structDeclared(structure))
    {
        declareStruct(structure);
        objSink() << ";\n";
    }
}

bool TOutputVulkanGLSL::writeVariablePrecision(TPrecision precision)
{
    if ((precision == EbpUndefined) || !mEnablePrecision)
        return false;

    TInfoSinkBase &out = objSink();
    if (mForceHighp)
        out << getPrecisionString(EbpHigh);
    else
        out << getPrecisionString(precision);
    return true;
}

}  // namespace sh
