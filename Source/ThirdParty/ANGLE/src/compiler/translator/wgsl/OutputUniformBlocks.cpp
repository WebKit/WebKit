//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/wgsl/OutputUniformBlocks.h"

#include <iostream>

#include "GLSLANG/ShaderVars.h"
#include "angle_gl.h"
#include "common/mathutil.h"
#include "common/utilities.h"
#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/Common.h"
#include "compiler/translator/Compiler.h"
#include "compiler/translator/ImmutableString.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/SymbolUniqueId.h"
#include "compiler/translator/tree_ops/GatherDefaultUniforms.h"
#include "compiler/translator/tree_util/DriverUniform.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/util.h"
#include "compiler/translator/wgsl/Utils.h"

namespace sh
{

namespace
{

// Traverses the AST and finds all structs that are used in the uniform address space (see the
// UniformBlockMetadata struct).
class FindUniformAddressSpaceStructs : public TIntermTraverser
{
  public:
    FindUniformAddressSpaceStructs(UniformBlockMetadata *uniformBlockMetadata)
        : TIntermTraverser(true, false, false), mUniformBlockMetadata(uniformBlockMetadata)
    {}

    ~FindUniformAddressSpaceStructs() override = default;

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override
    {
        const TIntermSequence &sequence = *(node->getSequence());

        TIntermTyped *variable = sequence.front()->getAsTyped();
        const TType &type      = variable->getType();

        // Note: EvqBuffer is the qualifier given to structs created by ReduceInterfaceBlocks.
        if (type.getQualifier() == EvqUniform || type.getQualifier() == EvqBuffer)
        {
#if defined(ANGLE_ENABLE_ASSERTS)
            if (IsDefaultUniform(type))
            {
                FATAL()
                    << "Found default uniform still in AST, by now all default uniforms should be "
                       "moved into an interface block or deleted if inactive. Uniform name = "
                    << variable->getAsSymbolNode()->getName();
            }
#endif

            recordTypesUsedInUniformAddressSpace(&type);
        }

        return true;
    }

  private:
    // Recurses through the tree of types referred to be `type` (which is used in the uniform
    // address space) and fills in the `mUniformBlockMetadata` struct appropriately.
    void recordTypesUsedInUniformAddressSpace(const TType *type)
    {
        if (type->isArray())
        {
            TType innerType = *type;
            innerType.toArrayBaseType();
            recordTypesUsedInUniformAddressSpace(&innerType);
        }
        else if (type->getStruct() != nullptr)
        {
            mUniformBlockMetadata->structsInUniformAddressSpace.insert(
                type->getStruct()->uniqueId().get());
            // Recurse into the types of the fields of this struct type.
            for (TField *const field : type->getStruct()->fields())
            {
                recordTypesUsedInUniformAddressSpace(field->type());
            }
        }
    }

    UniformBlockMetadata *const mUniformBlockMetadata;
};

}  // namespace

bool RecordUniformBlockMetadata(TIntermBlock *root, UniformBlockMetadata &outMetadata)
{
    FindUniformAddressSpaceStructs traverser(&outMetadata);
    root->traverse(&traverser);
    return true;
}

bool OutputUniformBoolOrBvecConversion(TInfoSinkBase &output, const TType &type)
{
    ASSERT(type.getBasicType() == EbtBool);
    // Bools are represented by u32s in the uniform address space, and so the u32s need to
    // be casted.
    if (type.isVector())
    {
        // Compare != to a vector of 0s, in WGSL this is componentwise and returns a bvec.
        output << "(vec" << static_cast<unsigned int>(type.getNominalSize()) << "<u32>(0u) != ";
    }
    else
    {
        output << "bool(";
    }

    return true;
}

bool OutputUniformWrapperStructsAndConversions(
    TInfoSinkBase &output,
    const WGSLGenerationMetadataForUniforms &wgslGenerationMetadataForUniforms)
{

    auto generate16AlignedWrapperStruct = [&output](const TType &type) {
        output << "struct " << MakeUniformWrapperStructName(&type) << "\n{\n";
        output << "  @align(16) " << kWrappedStructFieldName << " : ";
        WriteWgslType(output, type, {WgslAddressSpace::Uniform});
        output << "\n};\n";
    };

    bool generatedVec2WrapperStruct = false;

    for (const TType &type : wgslGenerationMetadataForUniforms.arrayElementTypesInUniforms)
    {
        // Structs don't need wrapper structs.
        ASSERT(type.getStruct() == nullptr);
        // Multidimensional arrays not currently supported in uniforms
        ASSERT(!type.isArray());

        if (type.isVector() && type.getNominalSize() == 2)
        {
            generatedVec2WrapperStruct = true;
        }
        generate16AlignedWrapperStruct(type);
    }

    // matCx2 is represented as array<ANGLE_wrapped_vec2, C> so if there are matCx2s we need to
    // generate an ANGLE_wrapped_vec2 struct.
    if (!wgslGenerationMetadataForUniforms.outputMatCx2Conversion.empty() &&
        !generatedVec2WrapperStruct)
    {
        generate16AlignedWrapperStruct(*new TType(TBasicType::EbtFloat, 2));
    }

    for (const TType &type :
         wgslGenerationMetadataForUniforms.arrayElementTypesThatNeedUnwrappingConversions)
    {
        // Should be a subset of the types that have had wrapper structs generated above, otherwise
        // it's impossible to unwrap them!
        TType innerType = type;
        innerType.toArrayElementType();
        ASSERT(wgslGenerationMetadataForUniforms.arrayElementTypesInUniforms.count(innerType) != 0);

        // This could take ptr<uniform, typeName>, with the unrestricted_pointer_parameters
        // extension. This is probably fine.
        output << "fn " << MakeUnwrappingArrayConversionFunctionName(&type) << "(wrappedArr : ";
        WriteWgslType(output, type, {WgslAddressSpace::Uniform});
        output << ") -> ";
        WriteWgslType(output, type, {WgslAddressSpace::NonUniform});
        output << "\n{\n";
        output << "  var retVal : ";
        WriteWgslType(output, type, {WgslAddressSpace::NonUniform});
        output << ";\n";
        output << "  for (var i : u32 = 0; i < " << type.getOutermostArraySize() << "; i++) {;\n";
        output << "    retVal[i] = ";
        if (type.getBasicType() == EbtBool)
        {
            OutputUniformBoolOrBvecConversion(output, type);
        }
        output << "wrappedArr[i]." << kWrappedStructFieldName;
        if (type.getBasicType() == EbtBool)
        {
            output << ")";
        }
        output << ";\n";
        output << "  }\n";
        output << "  return retVal;\n";
        output << "}\n";
    }

    for (const TType &type : wgslGenerationMetadataForUniforms.outputMatCx2Conversion)
    {
        ASSERT(type.isMatrix() && type.getRows() == 2);
        output << "fn " << MakeMatCx2ConversionFunctionName(&type) << "(mangledMatrix : ";

        WriteWgslType(output, type, {WgslAddressSpace::Uniform});
        output << ") -> ";
        WriteWgslType(output, type, {WgslAddressSpace::NonUniform});
        output << "\n{\n";
        output << "  var retVal : ";
        WriteWgslType(output, type, {WgslAddressSpace::NonUniform});
        output << ";\n";

        if (type.isArray())
        {
            output << "  for (var i : u32 = 0; i < " << type.getOutermostArraySize()
                   << "; i++) {;\n";
            output << "    retVal[i] = ";
        }
        else
        {
            output << "  retVal = ";
        }

        TType baseType = type;
        baseType.toArrayBaseType();
        WriteWgslType(output, baseType, {WgslAddressSpace::NonUniform});
        output << "(";
        for (uint8_t i = 0; i < type.getCols(); i++)
        {
            if (i != 0)
            {
                output << ", ";
            }
            // The mangled matrix is an array and the elements are wrapped vec2s, which can be
            // passed directly to the matCx2 constructor.
            output << "mangledMatrix" << (type.isArray() ? "[i]" : "") << "[" << static_cast<int>(i)
                   << "]." << kWrappedStructFieldName;
        }
        output << ");\n";

        if (type.isArray())
        {
            // Close the for loop.
            output << "  }\n";
        }
        output << "  return retVal;\n";
        output << "}\n";
    }

    return true;
}

ImmutableString MakeUnwrappingArrayConversionFunctionName(const TType *type)
{
    ASSERT(type->getNumArraySizes() <= 1);
    ImmutableString arrStr = type->isArray() ? BuildConcatenatedImmutableString(
                                                   "Array", type->getOutermostArraySize(), "_")
                                             : kEmptyImmutableString;
    return BuildConcatenatedImmutableString("ANGLE_Convert_", arrStr,
                                            MakeUniformWrapperStructName(type), "_ElementsTo_",
                                            type->getBuiltInTypeNameString(), "_Elements");
}

bool IsMatCx2(const TType *type)
{
    return type->isMatrix() && type->getRows() == 2;
}

ImmutableString MakeMatCx2ConversionFunctionName(const TType *type)
{
    ASSERT(type->getNumArraySizes() <= 1);
    ImmutableString arrStr = type->isArray() ? BuildConcatenatedImmutableString(
                                                   "Array", type->getOutermostArraySize(), "_")
                                             : kEmptyImmutableString;
    return BuildConcatenatedImmutableString("ANGLE_Convert_", arrStr, "Mat", type->getCols(), "x2");
}

bool OutputUniformBlocksAndSamplers(TCompiler *compiler,
                                    TIntermBlock *root,
                                    const TVariable *defaultUniformBlock)
{
    TInfoSinkBase &output                            = compiler->getInfoSink().obj;
    GlobalVars globalVars                            = FindGlobalVars(root);

#if defined(ANGLE_ENABLE_ASSERTS)
    // Only output a struct at all if there are going to be members.
    bool outputDefaultUniformBlockVar = false;
    if (defaultUniformBlock)
    {
        const std::vector<ShaderVariable> &basicUniforms = compiler->getUniforms();
        for (const ShaderVariable &shaderVar : basicUniforms)
        {
            if (gl::IsOpaqueType(shaderVar.type) || !shaderVar.active)
            {
                continue;
            }
            if (shaderVar.isBuiltIn())
            {
                // gl_DepthRange and also the GLSL 4.2 gl_NumSamples are uniforms.
                // TODO(anglebug.com/42267100): put gl_DepthRange into default uniform block.
                continue;
            }

            // Some uniform variables might have been deleted, for example if they were structs that
            // only contained samplers (which are pulled into separate default uniforms).
            ASSERT(defaultUniformBlock->getType().getInterfaceBlock());
            for (const TField *field : defaultUniformBlock->getType().getInterfaceBlock()->fields())
            {
                if (field->name() == shaderVar.name)
                {
                    outputDefaultUniformBlockVar = true;
                    break;
                }
            }
        }
    }

    ASSERT(outputDefaultUniformBlockVar == (defaultUniformBlock != nullptr));
#else   // defined(ANGLE_ENABLE_ASSERTS)
    bool outputDefaultUniformBlockVar = (defaultUniformBlock != nullptr);
#endif  // defined(ANGLE_ENABLE_ASSERTS)

    if (outputDefaultUniformBlockVar)
    {
        ASSERT(compiler->getShaderType() == GL_VERTEX_SHADER ||
               compiler->getShaderType() == GL_FRAGMENT_SHADER);
        const uint32_t bindingIndex = compiler->getShaderType() == GL_VERTEX_SHADER
                                          ? kDefaultVertexUniformBlockBinding
                                          : kDefaultFragmentUniformBlockBinding;
        output << "@group(" << kDefaultUniformBlockBindGroup << ") @binding(" << bindingIndex
               << ") var<uniform> " << kDefaultUniformBlockVarName << " : "
               << kDefaultUniformBlockVarType << ";\n";
    }

    // Output interface blocks. Start with driver uniforms in their own bind group.
    output << "@group(" << kDriverUniformBindGroup << ") @binding(" << kDriverUniformBlockBinding
           << ") var<uniform> " << kDriverUniformsVarName << " : " << kDriverUniformsBlockName
           << ";\n";
    // TODO(anglebug.com/376553328): now output the UBOs in `compiler->getUniformBlocks()` in its
    // own bind group.

    // Output split texture/sampler variables.
    for (const auto &globalVarIter : globalVars)
    {
        TIntermDeclaration *declNode = globalVarIter.second;
        ASSERT(declNode);

        const TIntermSymbol *declSymbol = &ViewDeclaration(*declNode).symbol;
        const TType &declType           = declSymbol->getType();
        if (!declType.isSampler())
        {
            continue;
        }

        // Note that this may output ignored symbols.
        output << kTextureSamplerBindingMarker << kAngleSamplerPrefix << declSymbol->getName()
               << " : ";
        WriteWgslSamplerType(output, declType, WgslSamplerTypeConfig::Sampler);
        output << ";\n";

        output << kTextureSamplerBindingMarker << kAngleTexturePrefix << declSymbol->getName()
               << " : ";
        WriteWgslSamplerType(output, declType, WgslSamplerTypeConfig::Texture);
        output << ";\n";
    }

    return true;
}

std::string WGSLGetMappedSamplerName(const std::string &originalName)
{
    std::string samplerName = originalName;

    // Samplers in structs are extracted.
    std::replace(samplerName.begin(), samplerName.end(), '.', '_');

    // Remove array elements
    auto out = samplerName.begin();
    for (auto in = samplerName.begin(); in != samplerName.end(); in++)
    {
        if (*in == '[')
        {
            while (*in != ']')
            {
                in++;
                ASSERT(in != samplerName.end());
            }
        }
        else
        {
            *out++ = *in;
        }
    }

    samplerName.erase(out, samplerName.end());

    return samplerName;
}

}  // namespace sh
