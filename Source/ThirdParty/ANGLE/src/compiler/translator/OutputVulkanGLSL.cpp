//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// OutputVulkanGLSL:
//   Code that outputs shaders that fit GL_KHR_vulkan_glsl, to be fed to glslang to generate
//   SPIR-V.
//   See: https://www.khronos.org/registry/vulkan/specs/misc/GL_KHR_vulkan_glsl.txt
//

#include "compiler/translator/OutputVulkanGLSL.h"

#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/Symbol.h"
#include "compiler/translator/ValidateVaryingLocations.h"
#include "compiler/translator/util.h"

namespace sh
{

TOutputVulkanGLSL::TOutputVulkanGLSL(TCompiler *compiler,
                                     TInfoSinkBase &objSink,
                                     bool enablePrecision,
                                     ShCompileOptions compileOptions)
    : TOutputGLSL(compiler, objSink, compileOptions),
      mNextUnusedBinding(0),
      mNextUnusedInputLocation(0),
      mNextUnusedOutputLocation(0),
      mEnablePrecision(enablePrecision)
{}

void TOutputVulkanGLSL::writeLayoutQualifier(TIntermSymbol *symbol)
{
    const TType &type = symbol->getType();

    bool needsSetBinding = IsSampler(type.getBasicType()) ||
                           (type.isInterfaceBlock() && (type.getQualifier() == EvqUniform ||
                                                        type.getQualifier() == EvqBuffer)) ||
                           IsImage(type.getBasicType()) || IsSubpassInputType(type.getBasicType());
    bool needsLocation = type.getQualifier() == EvqAttribute ||
                         type.getQualifier() == EvqVertexIn ||
                         type.getQualifier() == EvqFragmentOut || IsVarying(type.getQualifier());
    bool needsInputAttachmentIndex = IsSubpassInputType(type.getBasicType());
    bool needsSpecConstId          = type.getQualifier() == EvqSpecConst;

    if (!NeedsToWriteLayoutQualifier(type) && !needsSetBinding && !needsLocation &&
        !needsInputAttachmentIndex && !needsSpecConstId)
    {
        return;
    }

    TInfoSinkBase &out                      = objSink();
    const TLayoutQualifier &layoutQualifier = type.getLayoutQualifier();

    // This isn't super clean, but it gets the job done.
    // See corresponding code in glslang_wrapper_utils.cpp.
    const char *blockStorage  = nullptr;
    const char *matrixPacking = nullptr;

    if (type.isInterfaceBlock())
    {
        const TInterfaceBlock *interfaceBlock = type.getInterfaceBlock();
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
    const char *kCommaSeparator = ", ";
    const char *separator       = "";
    out << "layout(";

    // If the resource declaration is about input attachment, need to specify input_attachment_index
    if (needsInputAttachmentIndex)
    {
        out << "input_attachment_index=" << layoutQualifier.inputAttachmentIndex;
        separator = kCommaSeparator;
    }

    // If it's a specialization constant, add that constant_id qualifier.
    if (needsSpecConstId)
    {
        out << separator << "constant_id=" << layoutQualifier.location;
    }

    // If the resource declaration requires set & binding layout qualifiers, specify arbitrary
    // ones.
    if (needsSetBinding)
    {
        out << separator << "set=0, binding=" << nextUnusedBinding();
        separator = kCommaSeparator;
    }

    if (needsLocation)
    {
        uint32_t location = 0;
        if (layoutQualifier.index <= 0)
        {
            // Note: for index == 1 (dual source blending), don't count locations as they are
            // expected to alias the color output locations.  Only one dual-source output is
            // supported, so location will be always 0.
            const unsigned int locationCount =
                CalculateVaryingLocationCount(symbol->getType(), getShaderType());
            location = IsShaderIn(type.getQualifier()) ? nextUnusedInputLocation(locationCount)
                                                       : nextUnusedOutputLocation(locationCount);
        }

        out << separator << "location=" << location;
        separator = kCommaSeparator;
    }

    // Output the list of qualifiers already known at this stage, i.e. everything other than
    // `location` and `set`/`binding`.
    std::string otherQualifiers = getCommonLayoutQualifiers(symbol);

    if (blockStorage)
    {
        out << separator << blockStorage;
        separator = kCommaSeparator;
    }
    if (matrixPacking)
    {
        out << separator << matrixPacking;
        separator = kCommaSeparator;
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

bool TOutputVulkanGLSL::writeVariablePrecision(TPrecision precision)
{
    if ((precision == EbpUndefined) || !mEnablePrecision)
        return false;

    TInfoSinkBase &out = objSink();
    out << getPrecisionString(precision);
    return true;
}

}  // namespace sh
