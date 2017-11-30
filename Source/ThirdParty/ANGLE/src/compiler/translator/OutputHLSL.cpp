//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/OutputHLSL.h"

#include <algorithm>
#include <cfloat>
#include <stdio.h>

#include "common/angleutils.h"
#include "common/debug.h"
#include "common/utilities.h"
#include "compiler/translator/BuiltInFunctionEmulator.h"
#include "compiler/translator/BuiltInFunctionEmulatorHLSL.h"
#include "compiler/translator/ImageFunctionHLSL.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/NodeSearch.h"
#include "compiler/translator/RemoveSwitchFallThrough.h"
#include "compiler/translator/SearchSymbol.h"
#include "compiler/translator/StructureHLSL.h"
#include "compiler/translator/TextureFunctionHLSL.h"
#include "compiler/translator/TranslatorHLSL.h"
#include "compiler/translator/UniformHLSL.h"
#include "compiler/translator/UtilsHLSL.h"
#include "compiler/translator/blocklayout.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{

TString ArrayHelperFunctionName(const char *prefix, const TType &type)
{
    TStringStream fnName;
    fnName << prefix << "_";
    if (type.isArray())
    {
        for (unsigned int arraySize : *type.getArraySizes())
        {
            fnName << arraySize << "_";
        }
    }
    fnName << TypeString(type);
    return fnName.str();
}

bool IsDeclarationWrittenOut(TIntermDeclaration *node)
{
    TIntermSequence *sequence = node->getSequence();
    TIntermTyped *variable    = (*sequence)[0]->getAsTyped();
    ASSERT(sequence->size() == 1);
    ASSERT(variable);
    return (variable->getQualifier() == EvqTemporary || variable->getQualifier() == EvqGlobal ||
            variable->getQualifier() == EvqConst);
}

bool IsInStd140InterfaceBlock(TIntermTyped *node)
{
    TIntermBinary *binaryNode = node->getAsBinaryNode();

    if (binaryNode)
    {
        return IsInStd140InterfaceBlock(binaryNode->getLeft());
    }

    const TType &type = node->getType();

    // determine if we are in the standard layout
    const TInterfaceBlock *interfaceBlock = type.getInterfaceBlock();
    if (interfaceBlock)
    {
        return (interfaceBlock->blockStorage() == EbsStd140);
    }

    return false;
}

}  // anonymous namespace

void OutputHLSL::writeFloat(TInfoSinkBase &out, float f)
{
    // This is known not to work for NaN on all drivers but make the best effort to output NaNs
    // regardless.
    if ((gl::isInf(f) || gl::isNaN(f)) && mShaderVersion >= 300 &&
        mOutputType == SH_HLSL_4_1_OUTPUT)
    {
        out << "asfloat(" << gl::bitCast<uint32_t>(f) << "u)";
    }
    else
    {
        out << std::min(FLT_MAX, std::max(-FLT_MAX, f));
    }
}

void OutputHLSL::writeSingleConstant(TInfoSinkBase &out, const TConstantUnion *const constUnion)
{
    ASSERT(constUnion != nullptr);
    switch (constUnion->getType())
    {
        case EbtFloat:
            writeFloat(out, constUnion->getFConst());
            break;
        case EbtInt:
            out << constUnion->getIConst();
            break;
        case EbtUInt:
            out << constUnion->getUConst();
            break;
        case EbtBool:
            out << constUnion->getBConst();
            break;
        default:
            UNREACHABLE();
    }
}

const TConstantUnion *OutputHLSL::writeConstantUnionArray(TInfoSinkBase &out,
                                                          const TConstantUnion *const constUnion,
                                                          const size_t size)
{
    const TConstantUnion *constUnionIterated = constUnion;
    for (size_t i = 0; i < size; i++, constUnionIterated++)
    {
        writeSingleConstant(out, constUnionIterated);

        if (i != size - 1)
        {
            out << ", ";
        }
    }
    return constUnionIterated;
}

OutputHLSL::OutputHLSL(sh::GLenum shaderType,
                       int shaderVersion,
                       const TExtensionBehavior &extensionBehavior,
                       const char *sourcePath,
                       ShShaderOutput outputType,
                       int numRenderTargets,
                       const std::vector<Uniform> &uniforms,
                       ShCompileOptions compileOptions,
                       TSymbolTable *symbolTable,
                       PerformanceDiagnostics *perfDiagnostics)
    : TIntermTraverser(true, true, true, symbolTable),
      mShaderType(shaderType),
      mShaderVersion(shaderVersion),
      mExtensionBehavior(extensionBehavior),
      mSourcePath(sourcePath),
      mOutputType(outputType),
      mCompileOptions(compileOptions),
      mNumRenderTargets(numRenderTargets),
      mCurrentFunctionMetadata(nullptr),
      mPerfDiagnostics(perfDiagnostics)
{
    mInsideFunction = false;

    mUsesFragColor               = false;
    mUsesFragData                = false;
    mUsesDepthRange              = false;
    mUsesFragCoord               = false;
    mUsesPointCoord              = false;
    mUsesFrontFacing             = false;
    mUsesPointSize               = false;
    mUsesInstanceID              = false;
    mHasMultiviewExtensionEnabled =
        IsExtensionEnabled(mExtensionBehavior, TExtension::OVR_multiview);
    mUsesViewID                  = false;
    mUsesVertexID                = false;
    mUsesFragDepth               = false;
    mUsesNumWorkGroups           = false;
    mUsesWorkGroupID             = false;
    mUsesLocalInvocationID       = false;
    mUsesGlobalInvocationID      = false;
    mUsesLocalInvocationIndex    = false;
    mUsesXor                     = false;
    mUsesDiscardRewriting        = false;
    mUsesNestedBreak             = false;
    mRequiresIEEEStrictCompiling = false;

    mUniqueIndex = 0;

    mOutputLod0Function      = false;
    mInsideDiscontinuousLoop = false;
    mNestedLoopDepth         = 0;

    mExcessiveLoopIndex = nullptr;

    mStructureHLSL       = new StructureHLSL;
    mUniformHLSL         = new UniformHLSL(shaderType, mStructureHLSL, outputType, uniforms);
    mTextureFunctionHLSL = new TextureFunctionHLSL;
    mImageFunctionHLSL   = new ImageFunctionHLSL;

    if (mOutputType == SH_HLSL_3_0_OUTPUT)
    {
        // Fragment shaders need dx_DepthRange, dx_ViewCoords and dx_DepthFront.
        // Vertex shaders need a slightly different set: dx_DepthRange, dx_ViewCoords and
        // dx_ViewAdjust.
        // In both cases total 3 uniform registers need to be reserved.
        mUniformHLSL->reserveUniformRegisters(3);
    }

    // Reserve registers for the default uniform block and driver constants
    mUniformHLSL->reserveUniformBlockRegisters(2);
}

OutputHLSL::~OutputHLSL()
{
    SafeDelete(mStructureHLSL);
    SafeDelete(mUniformHLSL);
    SafeDelete(mTextureFunctionHLSL);
    SafeDelete(mImageFunctionHLSL);
    for (auto &eqFunction : mStructEqualityFunctions)
    {
        SafeDelete(eqFunction);
    }
    for (auto &eqFunction : mArrayEqualityFunctions)
    {
        SafeDelete(eqFunction);
    }
}

void OutputHLSL::output(TIntermNode *treeRoot, TInfoSinkBase &objSink)
{
    BuiltInFunctionEmulator builtInFunctionEmulator;
    InitBuiltInFunctionEmulatorForHLSL(&builtInFunctionEmulator);
    if ((mCompileOptions & SH_EMULATE_ISNAN_FLOAT_FUNCTION) != 0)
    {
        InitBuiltInIsnanFunctionEmulatorForHLSLWorkarounds(&builtInFunctionEmulator,
                                                           mShaderVersion);
    }

    builtInFunctionEmulator.markBuiltInFunctionsForEmulation(treeRoot);

    // Now that we are done changing the AST, do the analyses need for HLSL generation
    CallDAG::InitResult success = mCallDag.init(treeRoot, nullptr);
    ASSERT(success == CallDAG::INITDAG_SUCCESS);
    mASTMetadataList = CreateASTMetadataHLSL(treeRoot, mCallDag);

    const std::vector<MappedStruct> std140Structs = FlagStd140Structs(treeRoot);
    // TODO(oetuaho): The std140Structs could be filtered based on which ones actually get used in
    // the shader code. When we add shader storage blocks we might also consider an alternative
    // solution, since the struct mapping won't work very well for shader storage blocks.

    // Output the body and footer first to determine what has to go in the header
    mInfoSinkStack.push(&mBody);
    treeRoot->traverse(this);
    mInfoSinkStack.pop();

    mInfoSinkStack.push(&mFooter);
    mInfoSinkStack.pop();

    mInfoSinkStack.push(&mHeader);
    header(mHeader, std140Structs, &builtInFunctionEmulator);
    mInfoSinkStack.pop();

    objSink << mHeader.c_str();
    objSink << mBody.c_str();
    objSink << mFooter.c_str();

    builtInFunctionEmulator.cleanup();
}

const std::map<std::string, unsigned int> &OutputHLSL::getUniformBlockRegisterMap() const
{
    return mUniformHLSL->getUniformBlockRegisterMap();
}

const std::map<std::string, unsigned int> &OutputHLSL::getUniformRegisterMap() const
{
    return mUniformHLSL->getUniformRegisterMap();
}

TString OutputHLSL::structInitializerString(int indent,
                                            const TType &type,
                                            const TString &name) const
{
    TString init;

    TString indentString;
    for (int spaces = 0; spaces < indent; spaces++)
    {
        indentString += "    ";
    }

    if (type.isArray())
    {
        init += indentString + "{\n";
        for (unsigned int arrayIndex = 0u; arrayIndex < type.getOutermostArraySize(); ++arrayIndex)
        {
            TStringStream indexedString;
            indexedString << name << "[" << arrayIndex << "]";
            TType elementType = type;
            elementType.toArrayElementType();
            init += structInitializerString(indent + 1, elementType, indexedString.str());
            if (arrayIndex < type.getOutermostArraySize() - 1)
            {
                init += ",";
            }
            init += "\n";
        }
        init += indentString + "}";
    }
    else if (type.getBasicType() == EbtStruct)
    {
        init += indentString + "{\n";
        const TStructure &structure = *type.getStruct();
        const TFieldList &fields    = structure.fields();
        for (unsigned int fieldIndex = 0; fieldIndex < fields.size(); fieldIndex++)
        {
            const TField &field      = *fields[fieldIndex];
            const TString &fieldName = name + "." + Decorate(field.name());
            const TType &fieldType   = *field.type();

            init += structInitializerString(indent + 1, fieldType, fieldName);
            if (fieldIndex < fields.size() - 1)
            {
                init += ",";
            }
            init += "\n";
        }
        init += indentString + "}";
    }
    else
    {
        init += indentString + name;
    }

    return init;
}

TString OutputHLSL::generateStructMapping(const std::vector<MappedStruct> &std140Structs) const
{
    TString mappedStructs;

    for (auto &mappedStruct : std140Structs)
    {
        TInterfaceBlock *interfaceBlock =
            mappedStruct.blockDeclarator->getType().getInterfaceBlock();
        const TString &interfaceBlockName = interfaceBlock->name();
        const TName &instanceName         = mappedStruct.blockDeclarator->getName();
        if (mReferencedUniformBlocks.count(interfaceBlockName) == 0 &&
            (instanceName.getString() == "" ||
             mReferencedUniformBlocks.count(instanceName.getString()) == 0))
        {
            continue;
        }

        unsigned int instanceCount = 1u;
        bool isInstanceArray       = mappedStruct.blockDeclarator->isArray();
        if (isInstanceArray)
        {
            instanceCount = mappedStruct.blockDeclarator->getOutermostArraySize();
        }

        for (unsigned int instanceArrayIndex = 0; instanceArrayIndex < instanceCount;
             ++instanceArrayIndex)
        {
            TString originalName;
            TString mappedName("map");

            if (instanceName.getString() != "")
            {
                unsigned int instanceStringArrayIndex = GL_INVALID_INDEX;
                if (isInstanceArray)
                    instanceStringArrayIndex = instanceArrayIndex;
                TString instanceString = mUniformHLSL->uniformBlockInstanceString(
                    *interfaceBlock, instanceStringArrayIndex);
                originalName += instanceString;
                mappedName += instanceString;
                originalName += ".";
                mappedName += "_";
            }

            TString fieldName = Decorate(mappedStruct.field->name());
            originalName += fieldName;
            mappedName += fieldName;

            TType *structType = mappedStruct.field->type();
            mappedStructs +=
                "static " + Decorate(structType->getStruct()->name()) + " " + mappedName;

            if (structType->isArray())
            {
                mappedStructs += ArrayString(*mappedStruct.field->type());
            }

            mappedStructs += " =\n";
            mappedStructs += structInitializerString(0, *structType, originalName);
            mappedStructs += ";\n";
        }
    }
    return mappedStructs;
}

void OutputHLSL::header(TInfoSinkBase &out,
                        const std::vector<MappedStruct> &std140Structs,
                        const BuiltInFunctionEmulator *builtInFunctionEmulator) const
{
    TString varyings;
    TString attributes;
    TString mappedStructs = generateStructMapping(std140Structs);

    for (ReferencedSymbols::const_iterator varying = mReferencedVaryings.begin();
         varying != mReferencedVaryings.end(); varying++)
    {
        const TType &type   = varying->second->getType();
        const TString &name = varying->second->getSymbol();

        // Program linking depends on this exact format
        varyings += "static " + InterpolationString(type.getQualifier()) + " " + TypeString(type) +
                    " " + Decorate(name) + ArrayString(type) + " = " + initializer(type) + ";\n";
    }

    for (ReferencedSymbols::const_iterator attribute = mReferencedAttributes.begin();
         attribute != mReferencedAttributes.end(); attribute++)
    {
        const TType &type   = attribute->second->getType();
        const TString &name = attribute->second->getSymbol();

        attributes += "static " + TypeString(type) + " " + Decorate(name) + ArrayString(type) +
                      " = " + initializer(type) + ";\n";
    }

    out << mStructureHLSL->structsHeader();

    mUniformHLSL->uniformsHeader(out, mOutputType, mReferencedUniforms, mSymbolTable);
    out << mUniformHLSL->uniformBlocksHeader(mReferencedUniformBlocks);

    if (!mEqualityFunctions.empty())
    {
        out << "\n// Equality functions\n\n";
        for (const auto &eqFunction : mEqualityFunctions)
        {
            out << eqFunction->functionDefinition << "\n";
        }
    }
    if (!mArrayAssignmentFunctions.empty())
    {
        out << "\n// Assignment functions\n\n";
        for (const auto &assignmentFunction : mArrayAssignmentFunctions)
        {
            out << assignmentFunction.functionDefinition << "\n";
        }
    }
    if (!mArrayConstructIntoFunctions.empty())
    {
        out << "\n// Array constructor functions\n\n";
        for (const auto &constructIntoFunction : mArrayConstructIntoFunctions)
        {
            out << constructIntoFunction.functionDefinition << "\n";
        }
    }

    if (mUsesDiscardRewriting)
    {
        out << "#define ANGLE_USES_DISCARD_REWRITING\n";
    }

    if (mUsesNestedBreak)
    {
        out << "#define ANGLE_USES_NESTED_BREAK\n";
    }

    if (mRequiresIEEEStrictCompiling)
    {
        out << "#define ANGLE_REQUIRES_IEEE_STRICT_COMPILING\n";
    }

    out << "#ifdef ANGLE_ENABLE_LOOP_FLATTEN\n"
           "#define LOOP [loop]\n"
           "#define FLATTEN [flatten]\n"
           "#else\n"
           "#define LOOP\n"
           "#define FLATTEN\n"
           "#endif\n";

    if (mShaderType == GL_FRAGMENT_SHADER)
    {
        const bool usingMRTExtension =
            IsExtensionEnabled(mExtensionBehavior, TExtension::EXT_draw_buffers);

        out << "// Varyings\n";
        out << varyings;
        out << "\n";

        if (mShaderVersion >= 300)
        {
            for (ReferencedSymbols::const_iterator outputVariableIt =
                     mReferencedOutputVariables.begin();
                 outputVariableIt != mReferencedOutputVariables.end(); outputVariableIt++)
            {
                const TString &variableName = outputVariableIt->first;
                const TType &variableType   = outputVariableIt->second->getType();

                out << "static " + TypeString(variableType) + " out_" + variableName +
                           ArrayString(variableType) + " = " + initializer(variableType) + ";\n";
            }
        }
        else
        {
            const unsigned int numColorValues = usingMRTExtension ? mNumRenderTargets : 1;

            out << "static float4 gl_Color[" << numColorValues << "] =\n"
                                                                  "{\n";
            for (unsigned int i = 0; i < numColorValues; i++)
            {
                out << "    float4(0, 0, 0, 0)";
                if (i + 1 != numColorValues)
                {
                    out << ",";
                }
                out << "\n";
            }

            out << "};\n";
        }

        if (mUsesFragDepth)
        {
            out << "static float gl_Depth = 0.0;\n";
        }

        if (mUsesFragCoord)
        {
            out << "static float4 gl_FragCoord = float4(0, 0, 0, 0);\n";
        }

        if (mUsesPointCoord)
        {
            out << "static float2 gl_PointCoord = float2(0.5, 0.5);\n";
        }

        if (mUsesFrontFacing)
        {
            out << "static bool gl_FrontFacing = false;\n";
        }

        out << "\n";

        if (mUsesDepthRange)
        {
            out << "struct gl_DepthRangeParameters\n"
                   "{\n"
                   "    float near;\n"
                   "    float far;\n"
                   "    float diff;\n"
                   "};\n"
                   "\n";
        }

        if (mOutputType == SH_HLSL_4_1_OUTPUT || mOutputType == SH_HLSL_4_0_FL9_3_OUTPUT)
        {
            out << "cbuffer DriverConstants : register(b1)\n"
                   "{\n";

            if (mUsesDepthRange)
            {
                out << "    float3 dx_DepthRange : packoffset(c0);\n";
            }

            if (mUsesFragCoord)
            {
                out << "    float4 dx_ViewCoords : packoffset(c1);\n";
            }

            if (mUsesFragCoord || mUsesFrontFacing)
            {
                out << "    float3 dx_DepthFront : packoffset(c2);\n";
            }

            if (mUsesFragCoord)
            {
                // dx_ViewScale is only used in the fragment shader to correct
                // the value for glFragCoord if necessary
                out << "    float2 dx_ViewScale : packoffset(c3);\n";
            }

            if (mHasMultiviewExtensionEnabled)
            {
                // We have to add a value which we can use to keep track of which multi-view code
                // path is to be selected in the GS.
                out << "    float multiviewSelectViewportIndex : packoffset(c3.z);\n";
            }

            if (mOutputType == SH_HLSL_4_1_OUTPUT)
            {
                mUniformHLSL->samplerMetadataUniforms(out, "c4");
            }

            out << "};\n";
        }
        else
        {
            if (mUsesDepthRange)
            {
                out << "uniform float3 dx_DepthRange : register(c0);";
            }

            if (mUsesFragCoord)
            {
                out << "uniform float4 dx_ViewCoords : register(c1);\n";
            }

            if (mUsesFragCoord || mUsesFrontFacing)
            {
                out << "uniform float3 dx_DepthFront : register(c2);\n";
            }
        }

        out << "\n";

        if (mUsesDepthRange)
        {
            out << "static gl_DepthRangeParameters gl_DepthRange = {dx_DepthRange.x, "
                   "dx_DepthRange.y, dx_DepthRange.z};\n"
                   "\n";
        }

        if (!mappedStructs.empty())
        {
            out << "// Structures from std140 blocks with padding removed\n";
            out << "\n";
            out << mappedStructs;
            out << "\n";
        }

        if (usingMRTExtension && mNumRenderTargets > 1)
        {
            out << "#define GL_USES_MRT\n";
        }

        if (mUsesFragColor)
        {
            out << "#define GL_USES_FRAG_COLOR\n";
        }

        if (mUsesFragData)
        {
            out << "#define GL_USES_FRAG_DATA\n";
        }
    }
    else if (mShaderType == GL_VERTEX_SHADER)
    {
        out << "// Attributes\n";
        out << attributes;
        out << "\n"
               "static float4 gl_Position = float4(0, 0, 0, 0);\n";

        if (mUsesPointSize)
        {
            out << "static float gl_PointSize = float(1);\n";
        }

        if (mUsesInstanceID)
        {
            out << "static int gl_InstanceID;";
        }

        if (mUsesVertexID)
        {
            out << "static int gl_VertexID;";
        }

        out << "\n"
               "// Varyings\n";
        out << varyings;
        out << "\n";

        if (mUsesDepthRange)
        {
            out << "struct gl_DepthRangeParameters\n"
                   "{\n"
                   "    float near;\n"
                   "    float far;\n"
                   "    float diff;\n"
                   "};\n"
                   "\n";
        }

        if (mOutputType == SH_HLSL_4_1_OUTPUT || mOutputType == SH_HLSL_4_0_FL9_3_OUTPUT)
        {
            out << "cbuffer DriverConstants : register(b1)\n"
                   "{\n";

            if (mUsesDepthRange)
            {
                out << "    float3 dx_DepthRange : packoffset(c0);\n";
            }

            // dx_ViewAdjust and dx_ViewCoords will only be used in Feature Level 9
            // shaders. However, we declare it for all shaders (including Feature Level 10+).
            // The bytecode is the same whether we declare it or not, since D3DCompiler removes it
            // if it's unused.
            out << "    float4 dx_ViewAdjust : packoffset(c1);\n";
            out << "    float2 dx_ViewCoords : packoffset(c2);\n";
            out << "    float2 dx_ViewScale  : packoffset(c3);\n";

            if (mHasMultiviewExtensionEnabled)
            {
                // We have to add a value which we can use to keep track of which multi-view code
                // path is to be selected in the GS.
                out << "    float multiviewSelectViewportIndex : packoffset(c3.z);\n";
            }

            if (mOutputType == SH_HLSL_4_1_OUTPUT)
            {
                mUniformHLSL->samplerMetadataUniforms(out, "c4");
            }

            out << "};\n"
                   "\n";
        }
        else
        {
            if (mUsesDepthRange)
            {
                out << "uniform float3 dx_DepthRange : register(c0);\n";
            }

            out << "uniform float4 dx_ViewAdjust : register(c1);\n";
            out << "uniform float2 dx_ViewCoords : register(c2);\n"
                   "\n";
        }

        if (mUsesDepthRange)
        {
            out << "static gl_DepthRangeParameters gl_DepthRange = {dx_DepthRange.x, "
                   "dx_DepthRange.y, dx_DepthRange.z};\n"
                   "\n";
        }

        if (!mappedStructs.empty())
        {
            out << "// Structures from std140 blocks with padding removed\n";
            out << "\n";
            out << mappedStructs;
            out << "\n";
        }
    }
    else  // Compute shader
    {
        ASSERT(mShaderType == GL_COMPUTE_SHADER);

        out << "cbuffer DriverConstants : register(b1)\n"
               "{\n";
        if (mUsesNumWorkGroups)
        {
            out << "    uint3 gl_NumWorkGroups : packoffset(c0);\n";
        }
        ASSERT(mOutputType == SH_HLSL_4_1_OUTPUT);
        mUniformHLSL->samplerMetadataUniforms(out, "c1");
        out << "};\n";

        // Follow built-in variables would be initialized in
        // DynamicHLSL::generateComputeShaderLinkHLSL, if they
        // are used in compute shader.
        if (mUsesWorkGroupID)
        {
            out << "static uint3 gl_WorkGroupID = uint3(0, 0, 0);\n";
        }

        if (mUsesLocalInvocationID)
        {
            out << "static uint3 gl_LocalInvocationID = uint3(0, 0, 0);\n";
        }

        if (mUsesGlobalInvocationID)
        {
            out << "static uint3 gl_GlobalInvocationID = uint3(0, 0, 0);\n";
        }

        if (mUsesLocalInvocationIndex)
        {
            out << "static uint gl_LocalInvocationIndex = uint(0);\n";
        }
    }

    bool getDimensionsIgnoresBaseLevel =
        (mCompileOptions & SH_HLSL_GET_DIMENSIONS_IGNORES_BASE_LEVEL) != 0;
    mTextureFunctionHLSL->textureFunctionHeader(out, mOutputType, getDimensionsIgnoresBaseLevel);
    mImageFunctionHLSL->imageFunctionHeader(out);

    if (mUsesFragCoord)
    {
        out << "#define GL_USES_FRAG_COORD\n";
    }

    if (mUsesPointCoord)
    {
        out << "#define GL_USES_POINT_COORD\n";
    }

    if (mUsesFrontFacing)
    {
        out << "#define GL_USES_FRONT_FACING\n";
    }

    if (mUsesPointSize)
    {
        out << "#define GL_USES_POINT_SIZE\n";
    }

    if (mHasMultiviewExtensionEnabled)
    {
        out << "#define GL_ANGLE_MULTIVIEW_ENABLED\n";
    }

    if (mUsesViewID)
    {
        out << "#define GL_USES_VIEW_ID\n";
    }

    if (mUsesFragDepth)
    {
        out << "#define GL_USES_FRAG_DEPTH\n";
    }

    if (mUsesDepthRange)
    {
        out << "#define GL_USES_DEPTH_RANGE\n";
    }

    if (mUsesNumWorkGroups)
    {
        out << "#define GL_USES_NUM_WORK_GROUPS\n";
    }

    if (mUsesWorkGroupID)
    {
        out << "#define GL_USES_WORK_GROUP_ID\n";
    }

    if (mUsesLocalInvocationID)
    {
        out << "#define GL_USES_LOCAL_INVOCATION_ID\n";
    }

    if (mUsesGlobalInvocationID)
    {
        out << "#define GL_USES_GLOBAL_INVOCATION_ID\n";
    }

    if (mUsesLocalInvocationIndex)
    {
        out << "#define GL_USES_LOCAL_INVOCATION_INDEX\n";
    }

    if (mUsesXor)
    {
        out << "bool xor(bool p, bool q)\n"
               "{\n"
               "    return (p || q) && !(p && q);\n"
               "}\n"
               "\n";
    }

    builtInFunctionEmulator->outputEmulatedFunctions(out);
}

void OutputHLSL::visitSymbol(TIntermSymbol *node)
{
    TInfoSinkBase &out = getInfoSink();

    // Handle accessing std140 structs by value
    if (IsInStd140InterfaceBlock(node) && node->getBasicType() == EbtStruct)
    {
        out << "map";
    }

    TString name = node->getSymbol();

    if (name == "gl_DepthRange")
    {
        mUsesDepthRange = true;
        out << name;
    }
    else
    {
        const TType &nodeType = node->getType();
        TQualifier qualifier = node->getQualifier();

        ensureStructDefined(nodeType);

        if (qualifier == EvqUniform)
        {
            const TInterfaceBlock *interfaceBlock = nodeType.getInterfaceBlock();

            if (interfaceBlock)
            {
                mReferencedUniformBlocks[interfaceBlock->name()] = node;
            }
            else
            {
                mReferencedUniforms[name] = node;
            }

            out << DecorateVariableIfNeeded(node->getName());
        }
        else if (qualifier == EvqAttribute || qualifier == EvqVertexIn)
        {
            mReferencedAttributes[name] = node;
            out << Decorate(name);
        }
        else if (IsVarying(qualifier))
        {
            mReferencedVaryings[name] = node;
            out << Decorate(name);
            if (name == "ViewID_OVR")
            {
                mUsesViewID = true;
            }
        }
        else if (qualifier == EvqFragmentOut)
        {
            mReferencedOutputVariables[name] = node;
            out << "out_" << name;
        }
        else if (qualifier == EvqFragColor)
        {
            out << "gl_Color[0]";
            mUsesFragColor = true;
        }
        else if (qualifier == EvqFragData)
        {
            out << "gl_Color";
            mUsesFragData = true;
        }
        else if (qualifier == EvqFragCoord)
        {
            mUsesFragCoord = true;
            out << name;
        }
        else if (qualifier == EvqPointCoord)
        {
            mUsesPointCoord = true;
            out << name;
        }
        else if (qualifier == EvqFrontFacing)
        {
            mUsesFrontFacing = true;
            out << name;
        }
        else if (qualifier == EvqPointSize)
        {
            mUsesPointSize = true;
            out << name;
        }
        else if (qualifier == EvqInstanceID)
        {
            mUsesInstanceID = true;
            out << name;
        }
        else if (qualifier == EvqVertexID)
        {
            mUsesVertexID = true;
            out << name;
        }
        else if (name == "gl_FragDepthEXT" || name == "gl_FragDepth")
        {
            mUsesFragDepth = true;
            out << "gl_Depth";
        }
        else if (qualifier == EvqNumWorkGroups)
        {
            mUsesNumWorkGroups = true;
            out << name;
        }
        else if (qualifier == EvqWorkGroupID)
        {
            mUsesWorkGroupID = true;
            out << name;
        }
        else if (qualifier == EvqLocalInvocationID)
        {
            mUsesLocalInvocationID = true;
            out << name;
        }
        else if (qualifier == EvqGlobalInvocationID)
        {
            mUsesGlobalInvocationID = true;
            out << name;
        }
        else if (qualifier == EvqLocalInvocationIndex)
        {
            mUsesLocalInvocationIndex = true;
            out << name;
        }
        else
        {
            out << DecorateVariableIfNeeded(node->getName());
        }
    }
}

void OutputHLSL::visitRaw(TIntermRaw *node)
{
    getInfoSink() << node->getRawText();
}

void OutputHLSL::outputEqual(Visit visit, const TType &type, TOperator op, TInfoSinkBase &out)
{
    if (type.isScalar() && !type.isArray())
    {
        if (op == EOpEqual)
        {
            outputTriplet(out, visit, "(", " == ", ")");
        }
        else
        {
            outputTriplet(out, visit, "(", " != ", ")");
        }
    }
    else
    {
        if (visit == PreVisit && op == EOpNotEqual)
        {
            out << "!";
        }

        if (type.isArray())
        {
            const TString &functionName = addArrayEqualityFunction(type);
            outputTriplet(out, visit, (functionName + "(").c_str(), ", ", ")");
        }
        else if (type.getBasicType() == EbtStruct)
        {
            const TStructure &structure = *type.getStruct();
            const TString &functionName = addStructEqualityFunction(structure);
            outputTriplet(out, visit, (functionName + "(").c_str(), ", ", ")");
        }
        else
        {
            ASSERT(type.isMatrix() || type.isVector());
            outputTriplet(out, visit, "all(", " == ", ")");
        }
    }
}

void OutputHLSL::outputAssign(Visit visit, const TType &type, TInfoSinkBase &out)
{
    if (type.isArray())
    {
        const TString &functionName = addArrayAssignmentFunction(type);
        outputTriplet(out, visit, (functionName + "(").c_str(), ", ", ")");
    }
    else
    {
        outputTriplet(out, visit, "(", " = ", ")");
    }
}

bool OutputHLSL::ancestorEvaluatesToSamplerInStruct()
{
    for (unsigned int n = 0u; getAncestorNode(n) != nullptr; ++n)
    {
        TIntermNode *ancestor               = getAncestorNode(n);
        const TIntermBinary *ancestorBinary = ancestor->getAsBinaryNode();
        if (ancestorBinary == nullptr)
        {
            return false;
        }
        switch (ancestorBinary->getOp())
        {
            case EOpIndexDirectStruct:
            {
                const TStructure *structure = ancestorBinary->getLeft()->getType().getStruct();
                const TIntermConstantUnion *index =
                    ancestorBinary->getRight()->getAsConstantUnion();
                const TField *field = structure->fields()[index->getIConst(0)];
                if (IsSampler(field->type()->getBasicType()))
                {
                    return true;
                }
                break;
            }
            case EOpIndexDirect:
                break;
            default:
                // Returning a sampler from indirect indexing is not supported.
                return false;
        }
    }
    return false;
}

bool OutputHLSL::visitSwizzle(Visit visit, TIntermSwizzle *node)
{
    TInfoSinkBase &out = getInfoSink();
    if (visit == PostVisit)
    {
        out << ".";
        node->writeOffsetsAsXYZW(&out);
    }
    return true;
}

bool OutputHLSL::visitBinary(Visit visit, TIntermBinary *node)
{
    TInfoSinkBase &out = getInfoSink();

    switch (node->getOp())
    {
        case EOpComma:
            outputTriplet(out, visit, "(", ", ", ")");
            break;
        case EOpAssign:
            if (node->isArray())
            {
                TIntermAggregate *rightAgg = node->getRight()->getAsAggregate();
                if (rightAgg != nullptr && rightAgg->isConstructor())
                {
                    const TString &functionName = addArrayConstructIntoFunction(node->getType());
                    out << functionName << "(";
                    node->getLeft()->traverse(this);
                    TIntermSequence *seq = rightAgg->getSequence();
                    for (auto &arrayElement : *seq)
                    {
                        out << ", ";
                        arrayElement->traverse(this);
                    }
                    out << ")";
                    return false;
                }
                // ArrayReturnValueToOutParameter should have eliminated expressions where a
                // function call is assigned.
                ASSERT(rightAgg == nullptr);
            }
            outputAssign(visit, node->getType(), out);
            break;
        case EOpInitialize:
            if (visit == PreVisit)
            {
                TIntermSymbol *symbolNode = node->getLeft()->getAsSymbolNode();
                ASSERT(symbolNode);
                TIntermTyped *expression = node->getRight();

                // Global initializers must be constant at this point.
                ASSERT(symbolNode->getQualifier() != EvqGlobal ||
                       canWriteAsHLSLLiteral(expression));

                // GLSL allows to write things like "float x = x;" where a new variable x is defined
                // and the value of an existing variable x is assigned. HLSL uses C semantics (the
                // new variable is created before the assignment is evaluated), so we need to
                // convert
                // this to "float t = x, x = t;".
                if (writeSameSymbolInitializer(out, symbolNode, expression))
                {
                    // Skip initializing the rest of the expression
                    return false;
                }
                else if (writeConstantInitialization(out, symbolNode, expression))
                {
                    return false;
                }
            }
            else if (visit == InVisit)
            {
                out << " = ";
            }
            break;
        case EOpAddAssign:
            outputTriplet(out, visit, "(", " += ", ")");
            break;
        case EOpSubAssign:
            outputTriplet(out, visit, "(", " -= ", ")");
            break;
        case EOpMulAssign:
            outputTriplet(out, visit, "(", " *= ", ")");
            break;
        case EOpVectorTimesScalarAssign:
            outputTriplet(out, visit, "(", " *= ", ")");
            break;
        case EOpMatrixTimesScalarAssign:
            outputTriplet(out, visit, "(", " *= ", ")");
            break;
        case EOpVectorTimesMatrixAssign:
            if (visit == PreVisit)
            {
                out << "(";
            }
            else if (visit == InVisit)
            {
                out << " = mul(";
                node->getLeft()->traverse(this);
                out << ", transpose(";
            }
            else
            {
                out << ")))";
            }
            break;
        case EOpMatrixTimesMatrixAssign:
            if (visit == PreVisit)
            {
                out << "(";
            }
            else if (visit == InVisit)
            {
                out << " = transpose(mul(transpose(";
                node->getLeft()->traverse(this);
                out << "), transpose(";
            }
            else
            {
                out << "))))";
            }
            break;
        case EOpDivAssign:
            outputTriplet(out, visit, "(", " /= ", ")");
            break;
        case EOpIModAssign:
            outputTriplet(out, visit, "(", " %= ", ")");
            break;
        case EOpBitShiftLeftAssign:
            outputTriplet(out, visit, "(", " <<= ", ")");
            break;
        case EOpBitShiftRightAssign:
            outputTriplet(out, visit, "(", " >>= ", ")");
            break;
        case EOpBitwiseAndAssign:
            outputTriplet(out, visit, "(", " &= ", ")");
            break;
        case EOpBitwiseXorAssign:
            outputTriplet(out, visit, "(", " ^= ", ")");
            break;
        case EOpBitwiseOrAssign:
            outputTriplet(out, visit, "(", " |= ", ")");
            break;
        case EOpIndexDirect:
        {
            const TType &leftType = node->getLeft()->getType();
            if (leftType.isInterfaceBlock())
            {
                if (visit == PreVisit)
                {
                    TInterfaceBlock *interfaceBlock = leftType.getInterfaceBlock();
                    const int arrayIndex = node->getRight()->getAsConstantUnion()->getIConst(0);
                    mReferencedUniformBlocks[interfaceBlock->instanceName()] =
                        node->getLeft()->getAsSymbolNode();
                    out << mUniformHLSL->uniformBlockInstanceString(*interfaceBlock, arrayIndex);
                    return false;
                }
            }
            else if (ancestorEvaluatesToSamplerInStruct())
            {
                // All parts of an expression that access a sampler in a struct need to use _ as
                // separator to access the sampler variable that has been moved out of the struct.
                outputTriplet(out, visit, "", "_", "");
            }
            else
            {
                outputTriplet(out, visit, "", "[", "]");
            }
        }
        break;
        case EOpIndexIndirect:
            // We do not currently support indirect references to interface blocks
            ASSERT(node->getLeft()->getBasicType() != EbtInterfaceBlock);
            outputTriplet(out, visit, "", "[", "]");
            break;
        case EOpIndexDirectStruct:
        {
            const TStructure *structure       = node->getLeft()->getType().getStruct();
            const TIntermConstantUnion *index = node->getRight()->getAsConstantUnion();
            const TField *field               = structure->fields()[index->getIConst(0)];

            // In cases where indexing returns a sampler, we need to access the sampler variable
            // that has been moved out of the struct.
            bool indexingReturnsSampler = IsSampler(field->type()->getBasicType());
            if (visit == PreVisit && indexingReturnsSampler)
            {
                // Samplers extracted from structs have "angle" prefix to avoid name conflicts.
                // This prefix is only output at the beginning of the indexing expression, which
                // may have multiple parts.
                out << "angle";
            }
            if (!indexingReturnsSampler)
            {
                // All parts of an expression that access a sampler in a struct need to use _ as
                // separator to access the sampler variable that has been moved out of the struct.
                indexingReturnsSampler = ancestorEvaluatesToSamplerInStruct();
            }
            if (visit == InVisit)
            {
                if (indexingReturnsSampler)
                {
                    out << "_" + field->name();
                }
                else
                {
                    out << "." + DecorateField(field->name(), *structure);
                }

                return false;
            }
        }
        break;
        case EOpIndexDirectInterfaceBlock:
        {
            bool structInStd140Block =
                node->getBasicType() == EbtStruct && IsInStd140InterfaceBlock(node->getLeft());
            if (visit == PreVisit && structInStd140Block)
            {
                out << "map";
            }
            if (visit == InVisit)
            {
                const TInterfaceBlock *interfaceBlock =
                    node->getLeft()->getType().getInterfaceBlock();
                const TIntermConstantUnion *index = node->getRight()->getAsConstantUnion();
                const TField *field               = interfaceBlock->fields()[index->getIConst(0)];
                if (structInStd140Block)
                {
                    out << "_";
                }
                else
                {
                    out << ".";
                }
                out << Decorate(field->name());

                return false;
            }
            break;
        }
        case EOpAdd:
            outputTriplet(out, visit, "(", " + ", ")");
            break;
        case EOpSub:
            outputTriplet(out, visit, "(", " - ", ")");
            break;
        case EOpMul:
            outputTriplet(out, visit, "(", " * ", ")");
            break;
        case EOpDiv:
            outputTriplet(out, visit, "(", " / ", ")");
            break;
        case EOpIMod:
            outputTriplet(out, visit, "(", " % ", ")");
            break;
        case EOpBitShiftLeft:
            outputTriplet(out, visit, "(", " << ", ")");
            break;
        case EOpBitShiftRight:
            outputTriplet(out, visit, "(", " >> ", ")");
            break;
        case EOpBitwiseAnd:
            outputTriplet(out, visit, "(", " & ", ")");
            break;
        case EOpBitwiseXor:
            outputTriplet(out, visit, "(", " ^ ", ")");
            break;
        case EOpBitwiseOr:
            outputTriplet(out, visit, "(", " | ", ")");
            break;
        case EOpEqual:
        case EOpNotEqual:
            outputEqual(visit, node->getLeft()->getType(), node->getOp(), out);
            break;
        case EOpLessThan:
            outputTriplet(out, visit, "(", " < ", ")");
            break;
        case EOpGreaterThan:
            outputTriplet(out, visit, "(", " > ", ")");
            break;
        case EOpLessThanEqual:
            outputTriplet(out, visit, "(", " <= ", ")");
            break;
        case EOpGreaterThanEqual:
            outputTriplet(out, visit, "(", " >= ", ")");
            break;
        case EOpVectorTimesScalar:
            outputTriplet(out, visit, "(", " * ", ")");
            break;
        case EOpMatrixTimesScalar:
            outputTriplet(out, visit, "(", " * ", ")");
            break;
        case EOpVectorTimesMatrix:
            outputTriplet(out, visit, "mul(", ", transpose(", "))");
            break;
        case EOpMatrixTimesVector:
            outputTriplet(out, visit, "mul(transpose(", "), ", ")");
            break;
        case EOpMatrixTimesMatrix:
            outputTriplet(out, visit, "transpose(mul(transpose(", "), transpose(", ")))");
            break;
        case EOpLogicalOr:
            // HLSL doesn't short-circuit ||, so we assume that || affected by short-circuiting have
            // been unfolded.
            ASSERT(!node->getRight()->hasSideEffects());
            outputTriplet(out, visit, "(", " || ", ")");
            return true;
        case EOpLogicalXor:
            mUsesXor = true;
            outputTriplet(out, visit, "xor(", ", ", ")");
            break;
        case EOpLogicalAnd:
            // HLSL doesn't short-circuit &&, so we assume that && affected by short-circuiting have
            // been unfolded.
            ASSERT(!node->getRight()->hasSideEffects());
            outputTriplet(out, visit, "(", " && ", ")");
            return true;
        default:
            UNREACHABLE();
    }

    return true;
}

bool OutputHLSL::visitUnary(Visit visit, TIntermUnary *node)
{
    TInfoSinkBase &out = getInfoSink();

    switch (node->getOp())
    {
        case EOpNegative:
            outputTriplet(out, visit, "(-", "", ")");
            break;
        case EOpPositive:
            outputTriplet(out, visit, "(+", "", ")");
            break;
        case EOpLogicalNot:
            outputTriplet(out, visit, "(!", "", ")");
            break;
        case EOpBitwiseNot:
            outputTriplet(out, visit, "(~", "", ")");
            break;
        case EOpPostIncrement:
            outputTriplet(out, visit, "(", "", "++)");
            break;
        case EOpPostDecrement:
            outputTriplet(out, visit, "(", "", "--)");
            break;
        case EOpPreIncrement:
            outputTriplet(out, visit, "(++", "", ")");
            break;
        case EOpPreDecrement:
            outputTriplet(out, visit, "(--", "", ")");
            break;
        case EOpRadians:
            outputTriplet(out, visit, "radians(", "", ")");
            break;
        case EOpDegrees:
            outputTriplet(out, visit, "degrees(", "", ")");
            break;
        case EOpSin:
            outputTriplet(out, visit, "sin(", "", ")");
            break;
        case EOpCos:
            outputTriplet(out, visit, "cos(", "", ")");
            break;
        case EOpTan:
            outputTriplet(out, visit, "tan(", "", ")");
            break;
        case EOpAsin:
            outputTriplet(out, visit, "asin(", "", ")");
            break;
        case EOpAcos:
            outputTriplet(out, visit, "acos(", "", ")");
            break;
        case EOpAtan:
            outputTriplet(out, visit, "atan(", "", ")");
            break;
        case EOpSinh:
            outputTriplet(out, visit, "sinh(", "", ")");
            break;
        case EOpCosh:
            outputTriplet(out, visit, "cosh(", "", ")");
            break;
        case EOpTanh:
            outputTriplet(out, visit, "tanh(", "", ")");
            break;
        case EOpAsinh:
        case EOpAcosh:
        case EOpAtanh:
            ASSERT(node->getUseEmulatedFunction());
            writeEmulatedFunctionTriplet(out, visit, node->getOp());
            break;
        case EOpExp:
            outputTriplet(out, visit, "exp(", "", ")");
            break;
        case EOpLog:
            outputTriplet(out, visit, "log(", "", ")");
            break;
        case EOpExp2:
            outputTriplet(out, visit, "exp2(", "", ")");
            break;
        case EOpLog2:
            outputTriplet(out, visit, "log2(", "", ")");
            break;
        case EOpSqrt:
            outputTriplet(out, visit, "sqrt(", "", ")");
            break;
        case EOpInverseSqrt:
            outputTriplet(out, visit, "rsqrt(", "", ")");
            break;
        case EOpAbs:
            outputTriplet(out, visit, "abs(", "", ")");
            break;
        case EOpSign:
            outputTriplet(out, visit, "sign(", "", ")");
            break;
        case EOpFloor:
            outputTriplet(out, visit, "floor(", "", ")");
            break;
        case EOpTrunc:
            outputTriplet(out, visit, "trunc(", "", ")");
            break;
        case EOpRound:
            outputTriplet(out, visit, "round(", "", ")");
            break;
        case EOpRoundEven:
            ASSERT(node->getUseEmulatedFunction());
            writeEmulatedFunctionTriplet(out, visit, node->getOp());
            break;
        case EOpCeil:
            outputTriplet(out, visit, "ceil(", "", ")");
            break;
        case EOpFract:
            outputTriplet(out, visit, "frac(", "", ")");
            break;
        case EOpIsNan:
            if (node->getUseEmulatedFunction())
                writeEmulatedFunctionTriplet(out, visit, node->getOp());
            else
                outputTriplet(out, visit, "isnan(", "", ")");
            mRequiresIEEEStrictCompiling = true;
            break;
        case EOpIsInf:
            outputTriplet(out, visit, "isinf(", "", ")");
            break;
        case EOpFloatBitsToInt:
            outputTriplet(out, visit, "asint(", "", ")");
            break;
        case EOpFloatBitsToUint:
            outputTriplet(out, visit, "asuint(", "", ")");
            break;
        case EOpIntBitsToFloat:
            outputTriplet(out, visit, "asfloat(", "", ")");
            break;
        case EOpUintBitsToFloat:
            outputTriplet(out, visit, "asfloat(", "", ")");
            break;
        case EOpPackSnorm2x16:
        case EOpPackUnorm2x16:
        case EOpPackHalf2x16:
        case EOpUnpackSnorm2x16:
        case EOpUnpackUnorm2x16:
        case EOpUnpackHalf2x16:
        case EOpPackUnorm4x8:
        case EOpPackSnorm4x8:
        case EOpUnpackUnorm4x8:
        case EOpUnpackSnorm4x8:
            ASSERT(node->getUseEmulatedFunction());
            writeEmulatedFunctionTriplet(out, visit, node->getOp());
            break;
        case EOpLength:
            outputTriplet(out, visit, "length(", "", ")");
            break;
        case EOpNormalize:
            outputTriplet(out, visit, "normalize(", "", ")");
            break;
        case EOpDFdx:
            if (mInsideDiscontinuousLoop || mOutputLod0Function)
            {
                outputTriplet(out, visit, "(", "", ", 0.0)");
            }
            else
            {
                outputTriplet(out, visit, "ddx(", "", ")");
            }
            break;
        case EOpDFdy:
            if (mInsideDiscontinuousLoop || mOutputLod0Function)
            {
                outputTriplet(out, visit, "(", "", ", 0.0)");
            }
            else
            {
                outputTriplet(out, visit, "ddy(", "", ")");
            }
            break;
        case EOpFwidth:
            if (mInsideDiscontinuousLoop || mOutputLod0Function)
            {
                outputTriplet(out, visit, "(", "", ", 0.0)");
            }
            else
            {
                outputTriplet(out, visit, "fwidth(", "", ")");
            }
            break;
        case EOpTranspose:
            outputTriplet(out, visit, "transpose(", "", ")");
            break;
        case EOpDeterminant:
            outputTriplet(out, visit, "determinant(transpose(", "", "))");
            break;
        case EOpInverse:
            ASSERT(node->getUseEmulatedFunction());
            writeEmulatedFunctionTriplet(out, visit, node->getOp());
            break;

        case EOpAny:
            outputTriplet(out, visit, "any(", "", ")");
            break;
        case EOpAll:
            outputTriplet(out, visit, "all(", "", ")");
            break;
        case EOpLogicalNotComponentWise:
            outputTriplet(out, visit, "(!", "", ")");
            break;
        case EOpBitfieldReverse:
            outputTriplet(out, visit, "reversebits(", "", ")");
            break;
        case EOpBitCount:
            outputTriplet(out, visit, "countbits(", "", ")");
            break;
        case EOpFindLSB:
            // Note that it's unclear from the HLSL docs what this returns for 0, but this is tested
            // in GLSLTest and results are consistent with GL.
            outputTriplet(out, visit, "firstbitlow(", "", ")");
            break;
        case EOpFindMSB:
            // Note that it's unclear from the HLSL docs what this returns for 0 or -1, but this is
            // tested in GLSLTest and results are consistent with GL.
            outputTriplet(out, visit, "firstbithigh(", "", ")");
            break;
        default:
            UNREACHABLE();
    }

    return true;
}

TString OutputHLSL::samplerNamePrefixFromStruct(TIntermTyped *node)
{
    if (node->getAsSymbolNode())
    {
        return node->getAsSymbolNode()->getSymbol();
    }
    TIntermBinary *nodeBinary = node->getAsBinaryNode();
    switch (nodeBinary->getOp())
    {
        case EOpIndexDirect:
        {
            int index = nodeBinary->getRight()->getAsConstantUnion()->getIConst(0);

            TInfoSinkBase prefixSink;
            prefixSink << samplerNamePrefixFromStruct(nodeBinary->getLeft()) << "_" << index;
            return TString(prefixSink.c_str());
        }
        case EOpIndexDirectStruct:
        {
            const TStructure *s = nodeBinary->getLeft()->getAsTyped()->getType().getStruct();
            int index           = nodeBinary->getRight()->getAsConstantUnion()->getIConst(0);
            const TField *field = s->fields()[index];

            TInfoSinkBase prefixSink;
            prefixSink << samplerNamePrefixFromStruct(nodeBinary->getLeft()) << "_"
                       << field->name();
            return TString(prefixSink.c_str());
        }
        default:
            UNREACHABLE();
            return TString("");
    }
}

bool OutputHLSL::visitBlock(Visit visit, TIntermBlock *node)
{
    TInfoSinkBase &out = getInfoSink();

    if (mInsideFunction)
    {
        outputLineDirective(out, node->getLine().first_line);
        out << "{\n";
    }

    for (TIntermNode *statement : *node->getSequence())
    {
        outputLineDirective(out, statement->getLine().first_line);

        statement->traverse(this);

        // Don't output ; after case labels, they're terminated by :
        // This is needed especially since outputting a ; after a case statement would turn empty
        // case statements into non-empty case statements, disallowing fall-through from them.
        // Also the output code is clearer if we don't output ; after statements where it is not
        // needed:
        //  * if statements
        //  * switch statements
        //  * blocks
        //  * function definitions
        //  * loops (do-while loops output the semicolon in VisitLoop)
        //  * declarations that don't generate output.
        if (statement->getAsCaseNode() == nullptr && statement->getAsIfElseNode() == nullptr &&
            statement->getAsBlock() == nullptr && statement->getAsLoopNode() == nullptr &&
            statement->getAsSwitchNode() == nullptr &&
            statement->getAsFunctionDefinition() == nullptr &&
            (statement->getAsDeclarationNode() == nullptr ||
             IsDeclarationWrittenOut(statement->getAsDeclarationNode())) &&
            statement->getAsInvariantDeclarationNode() == nullptr)
        {
            out << ";\n";
        }
    }

    if (mInsideFunction)
    {
        outputLineDirective(out, node->getLine().last_line);
        out << "}\n";
    }

    return false;
}

bool OutputHLSL::visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node)
{
    TInfoSinkBase &out = getInfoSink();

    ASSERT(mCurrentFunctionMetadata == nullptr);

    size_t index = mCallDag.findIndex(node->getFunctionSymbolInfo());
    ASSERT(index != CallDAG::InvalidIndex);
    mCurrentFunctionMetadata = &mASTMetadataList[index];

    out << TypeString(node->getFunctionPrototype()->getType()) << " ";

    TIntermSequence *parameters = node->getFunctionPrototype()->getSequence();

    if (node->getFunctionSymbolInfo()->isMain())
    {
        out << "gl_main(";
    }
    else
    {
        out << DecorateFunctionIfNeeded(node->getFunctionSymbolInfo()->getNameObj())
            << DisambiguateFunctionName(parameters) << (mOutputLod0Function ? "Lod0(" : "(");
    }

    for (unsigned int i = 0; i < parameters->size(); i++)
    {
        TIntermSymbol *symbol = (*parameters)[i]->getAsSymbolNode();

        if (symbol)
        {
            ensureStructDefined(symbol->getType());

            out << argumentString(symbol);

            if (i < parameters->size() - 1)
            {
                out << ", ";
            }
        }
        else
            UNREACHABLE();
    }

    out << ")\n";

    mInsideFunction = true;
    // The function body node will output braces.
    node->getBody()->traverse(this);
    mInsideFunction = false;

    mCurrentFunctionMetadata = nullptr;

    bool needsLod0 = mASTMetadataList[index].mNeedsLod0;
    if (needsLod0 && !mOutputLod0Function && mShaderType == GL_FRAGMENT_SHADER)
    {
        ASSERT(!node->getFunctionSymbolInfo()->isMain());
        mOutputLod0Function = true;
        node->traverse(this);
        mOutputLod0Function = false;
    }

    return false;
}

bool OutputHLSL::visitDeclaration(Visit visit, TIntermDeclaration *node)
{
    if (visit == PreVisit)
    {
        TIntermSequence *sequence = node->getSequence();
        TIntermTyped *variable    = (*sequence)[0]->getAsTyped();
        ASSERT(sequence->size() == 1);
        ASSERT(variable);

        if (IsDeclarationWrittenOut(node))
        {
            TInfoSinkBase &out = getInfoSink();
            ensureStructDefined(variable->getType());

            if (!variable->getAsSymbolNode() ||
                variable->getAsSymbolNode()->getSymbol() != "")  // Variable declaration
            {
                if (!mInsideFunction)
                {
                    out << "static ";
                }

                out << TypeString(variable->getType()) + " ";

                TIntermSymbol *symbol = variable->getAsSymbolNode();

                if (symbol)
                {
                    symbol->traverse(this);
                    out << ArrayString(symbol->getType());
                    out << " = " + initializer(symbol->getType());
                }
                else
                {
                    variable->traverse(this);
                }
            }
            else if (variable->getAsSymbolNode() &&
                     variable->getAsSymbolNode()->getSymbol() == "")  // Type (struct) declaration
            {
                ASSERT(variable->getBasicType() == EbtStruct);
                // ensureStructDefined has already been called.
            }
            else
                UNREACHABLE();
        }
        else if (IsVaryingOut(variable->getQualifier()))
        {
            TIntermSymbol *symbol = variable->getAsSymbolNode();
            ASSERT(symbol);  // Varying declarations can't have initializers.

            // Vertex outputs which are declared but not written to should still be declared to
            // allow successful linking.
            mReferencedVaryings[symbol->getSymbol()] = symbol;
        }
    }
    return false;
}

bool OutputHLSL::visitInvariantDeclaration(Visit visit, TIntermInvariantDeclaration *node)
{
    // Do not do any translation
    return false;
}

bool OutputHLSL::visitFunctionPrototype(Visit visit, TIntermFunctionPrototype *node)
{
    TInfoSinkBase &out = getInfoSink();

    ASSERT(visit == PreVisit);
    size_t index = mCallDag.findIndex(node->getFunctionSymbolInfo());
    // Skip the prototype if it is not implemented (and thus not used)
    if (index == CallDAG::InvalidIndex)
    {
        return false;
    }

    TIntermSequence *arguments = node->getSequence();

    TString name = DecorateFunctionIfNeeded(node->getFunctionSymbolInfo()->getNameObj());
    out << TypeString(node->getType()) << " " << name << DisambiguateFunctionName(arguments)
        << (mOutputLod0Function ? "Lod0(" : "(");

    for (unsigned int i = 0; i < arguments->size(); i++)
    {
        TIntermSymbol *symbol = (*arguments)[i]->getAsSymbolNode();
        ASSERT(symbol != nullptr);

        out << argumentString(symbol);

        if (i < arguments->size() - 1)
        {
            out << ", ";
        }
    }

    out << ");\n";

    // Also prototype the Lod0 variant if needed
    bool needsLod0 = mASTMetadataList[index].mNeedsLod0;
    if (needsLod0 && !mOutputLod0Function && mShaderType == GL_FRAGMENT_SHADER)
    {
        mOutputLod0Function = true;
        node->traverse(this);
        mOutputLod0Function = false;
    }

    return false;
}

bool OutputHLSL::visitAggregate(Visit visit, TIntermAggregate *node)
{
    TInfoSinkBase &out = getInfoSink();

    switch (node->getOp())
    {
        case EOpCallBuiltInFunction:
        case EOpCallFunctionInAST:
        case EOpCallInternalRawFunction:
        {
            TIntermSequence *arguments = node->getSequence();

            bool lod0 = mInsideDiscontinuousLoop || mOutputLod0Function;
            if (node->getOp() == EOpCallFunctionInAST)
            {
                if (node->isArray())
                {
                    UNIMPLEMENTED();
                }
                size_t index = mCallDag.findIndex(node->getFunctionSymbolInfo());
                ASSERT(index != CallDAG::InvalidIndex);
                lod0 &= mASTMetadataList[index].mNeedsLod0;

                out << DecorateFunctionIfNeeded(node->getFunctionSymbolInfo()->getNameObj());
                out << DisambiguateFunctionName(node->getSequence());
                out << (lod0 ? "Lod0(" : "(");
            }
            else if (node->getOp() == EOpCallInternalRawFunction)
            {
                // This path is used for internal functions that don't have their definitions in the
                // AST, such as precision emulation functions.
                out << DecorateFunctionIfNeeded(node->getFunctionSymbolInfo()->getNameObj()) << "(";
            }
            else if (node->getFunctionSymbolInfo()->isImageFunction())
            {
                TString name              = node->getFunctionSymbolInfo()->getName();
                TType type                = (*arguments)[0]->getAsTyped()->getType();
                TString imageFunctionName = mImageFunctionHLSL->useImageFunction(
                    name, type.getBasicType(), type.getLayoutQualifier().imageInternalFormat,
                    type.getMemoryQualifier().readonly);
                out << imageFunctionName << "(";
            }
            else
            {
                const TString &name    = node->getFunctionSymbolInfo()->getName();
                TBasicType samplerType = (*arguments)[0]->getAsTyped()->getType().getBasicType();
                int coords = 0;  // textureSize(gsampler2DMS) doesn't have a second argument.
                if (arguments->size() > 1)
                {
                    coords = (*arguments)[1]->getAsTyped()->getNominalSize();
                }
                TString textureFunctionName = mTextureFunctionHLSL->useTextureFunction(
                    name, samplerType, coords, arguments->size(), lod0, mShaderType);
                out << textureFunctionName << "(";
            }

            for (TIntermSequence::iterator arg = arguments->begin(); arg != arguments->end(); arg++)
            {
                TIntermTyped *typedArg = (*arg)->getAsTyped();
                if (mOutputType == SH_HLSL_4_0_FL9_3_OUTPUT && IsSampler(typedArg->getBasicType()))
                {
                    out << "texture_";
                    (*arg)->traverse(this);
                    out << ", sampler_";
                }

                (*arg)->traverse(this);

                if (typedArg->getType().isStructureContainingSamplers())
                {
                    const TType &argType = typedArg->getType();
                    TVector<TIntermSymbol *> samplerSymbols;
                    TString structName = samplerNamePrefixFromStruct(typedArg);
                    argType.createSamplerSymbols("angle_" + structName, "", &samplerSymbols,
                                                 nullptr, mSymbolTable);
                    for (const TIntermSymbol *sampler : samplerSymbols)
                    {
                        if (mOutputType == SH_HLSL_4_0_FL9_3_OUTPUT)
                        {
                            out << ", texture_" << sampler->getSymbol();
                            out << ", sampler_" << sampler->getSymbol();
                        }
                        else
                        {
                            // In case of HLSL 4.1+, this symbol is the sampler index, and in case
                            // of D3D9, it's the sampler variable.
                            out << ", " + sampler->getSymbol();
                        }
                    }
                }

                if (arg < arguments->end() - 1)
                {
                    out << ", ";
                }
            }

            out << ")";

            return false;
        }
        case EOpConstruct:
            outputConstructor(out, visit, node);
            break;
        case EOpEqualComponentWise:
            outputTriplet(out, visit, "(", " == ", ")");
            break;
        case EOpNotEqualComponentWise:
            outputTriplet(out, visit, "(", " != ", ")");
            break;
        case EOpLessThanComponentWise:
            outputTriplet(out, visit, "(", " < ", ")");
            break;
        case EOpGreaterThanComponentWise:
            outputTriplet(out, visit, "(", " > ", ")");
            break;
        case EOpLessThanEqualComponentWise:
            outputTriplet(out, visit, "(", " <= ", ")");
            break;
        case EOpGreaterThanEqualComponentWise:
            outputTriplet(out, visit, "(", " >= ", ")");
            break;
        case EOpMod:
            ASSERT(node->getUseEmulatedFunction());
            writeEmulatedFunctionTriplet(out, visit, node->getOp());
            break;
        case EOpModf:
            outputTriplet(out, visit, "modf(", ", ", ")");
            break;
        case EOpPow:
            outputTriplet(out, visit, "pow(", ", ", ")");
            break;
        case EOpAtan:
            ASSERT(node->getSequence()->size() == 2);  // atan(x) is a unary operator
            ASSERT(node->getUseEmulatedFunction());
            writeEmulatedFunctionTriplet(out, visit, node->getOp());
            break;
        case EOpMin:
            outputTriplet(out, visit, "min(", ", ", ")");
            break;
        case EOpMax:
            outputTriplet(out, visit, "max(", ", ", ")");
            break;
        case EOpClamp:
            outputTriplet(out, visit, "clamp(", ", ", ")");
            break;
        case EOpMix:
        {
            TIntermTyped *lastParamNode = (*(node->getSequence()))[2]->getAsTyped();
            if (lastParamNode->getType().getBasicType() == EbtBool)
            {
                // There is no HLSL equivalent for ESSL3 built-in "genType mix (genType x, genType
                // y, genBType a)",
                // so use emulated version.
                ASSERT(node->getUseEmulatedFunction());
                writeEmulatedFunctionTriplet(out, visit, node->getOp());
            }
            else
            {
                outputTriplet(out, visit, "lerp(", ", ", ")");
            }
            break;
        }
        case EOpStep:
            outputTriplet(out, visit, "step(", ", ", ")");
            break;
        case EOpSmoothStep:
            outputTriplet(out, visit, "smoothstep(", ", ", ")");
            break;
        case EOpFrexp:
        case EOpLdexp:
            ASSERT(node->getUseEmulatedFunction());
            writeEmulatedFunctionTriplet(out, visit, node->getOp());
            break;
        case EOpDistance:
            outputTriplet(out, visit, "distance(", ", ", ")");
            break;
        case EOpDot:
            outputTriplet(out, visit, "dot(", ", ", ")");
            break;
        case EOpCross:
            outputTriplet(out, visit, "cross(", ", ", ")");
            break;
        case EOpFaceforward:
            ASSERT(node->getUseEmulatedFunction());
            writeEmulatedFunctionTriplet(out, visit, node->getOp());
            break;
        case EOpReflect:
            outputTriplet(out, visit, "reflect(", ", ", ")");
            break;
        case EOpRefract:
            outputTriplet(out, visit, "refract(", ", ", ")");
            break;
        case EOpOuterProduct:
            ASSERT(node->getUseEmulatedFunction());
            writeEmulatedFunctionTriplet(out, visit, node->getOp());
            break;
        case EOpMulMatrixComponentWise:
            outputTriplet(out, visit, "(", " * ", ")");
            break;
        case EOpBitfieldExtract:
        case EOpBitfieldInsert:
        case EOpUaddCarry:
        case EOpUsubBorrow:
        case EOpUmulExtended:
        case EOpImulExtended:
            ASSERT(node->getUseEmulatedFunction());
            writeEmulatedFunctionTriplet(out, visit, node->getOp());
            break;
        default:
            UNREACHABLE();
    }

    return true;
}

void OutputHLSL::writeIfElse(TInfoSinkBase &out, TIntermIfElse *node)
{
    out << "if (";

    node->getCondition()->traverse(this);

    out << ")\n";

    outputLineDirective(out, node->getLine().first_line);

    bool discard = false;

    if (node->getTrueBlock())
    {
        // The trueBlock child node will output braces.
        node->getTrueBlock()->traverse(this);

        // Detect true discard
        discard = (discard || FindDiscard::search(node->getTrueBlock()));
    }
    else
    {
        // TODO(oetuaho): Check if the semicolon inside is necessary.
        // It's there as a result of conservative refactoring of the output.
        out << "{;}\n";
    }

    outputLineDirective(out, node->getLine().first_line);

    if (node->getFalseBlock())
    {
        out << "else\n";

        outputLineDirective(out, node->getFalseBlock()->getLine().first_line);

        // The falseBlock child node will output braces.
        node->getFalseBlock()->traverse(this);

        outputLineDirective(out, node->getFalseBlock()->getLine().first_line);

        // Detect false discard
        discard = (discard || FindDiscard::search(node->getFalseBlock()));
    }

    // ANGLE issue 486: Detect problematic conditional discard
    if (discard)
    {
        mUsesDiscardRewriting = true;
    }
}

bool OutputHLSL::visitTernary(Visit, TIntermTernary *)
{
    // Ternary ops should have been already converted to something else in the AST. HLSL ternary
    // operator doesn't short-circuit, so it's not the same as the GLSL ternary operator.
    UNREACHABLE();
    return false;
}

bool OutputHLSL::visitIfElse(Visit visit, TIntermIfElse *node)
{
    TInfoSinkBase &out = getInfoSink();

    ASSERT(mInsideFunction);

    // D3D errors when there is a gradient operation in a loop in an unflattened if.
    if (mShaderType == GL_FRAGMENT_SHADER && mCurrentFunctionMetadata->hasGradientLoop(node))
    {
        out << "FLATTEN ";
    }

    writeIfElse(out, node);

    return false;
}

bool OutputHLSL::visitSwitch(Visit visit, TIntermSwitch *node)
{
    TInfoSinkBase &out = getInfoSink();

    ASSERT(node->getStatementList());
    if (visit == PreVisit)
    {
        node->setStatementList(RemoveSwitchFallThrough(node->getStatementList(), mPerfDiagnostics));
    }
    outputTriplet(out, visit, "switch (", ") ", "");
    // The curly braces get written when visiting the statementList block.
    return true;
}

bool OutputHLSL::visitCase(Visit visit, TIntermCase *node)
{
    TInfoSinkBase &out = getInfoSink();

    if (node->hasCondition())
    {
        outputTriplet(out, visit, "case (", "", "):\n");
        return true;
    }
    else
    {
        out << "default:\n";
        return false;
    }
}

void OutputHLSL::visitConstantUnion(TIntermConstantUnion *node)
{
    TInfoSinkBase &out = getInfoSink();
    writeConstantUnion(out, node->getType(), node->getUnionArrayPointer());
}

bool OutputHLSL::visitLoop(Visit visit, TIntermLoop *node)
{
    mNestedLoopDepth++;

    bool wasDiscontinuous = mInsideDiscontinuousLoop;
    mInsideDiscontinuousLoop =
        mInsideDiscontinuousLoop || mCurrentFunctionMetadata->mDiscontinuousLoops.count(node) > 0;

    TInfoSinkBase &out = getInfoSink();

    if (mOutputType == SH_HLSL_3_0_OUTPUT)
    {
        if (handleExcessiveLoop(out, node))
        {
            mInsideDiscontinuousLoop = wasDiscontinuous;
            mNestedLoopDepth--;

            return false;
        }
    }

    const char *unroll = mCurrentFunctionMetadata->hasGradientInCallGraph(node) ? "LOOP" : "";
    if (node->getType() == ELoopDoWhile)
    {
        out << "{" << unroll << " do\n";

        outputLineDirective(out, node->getLine().first_line);
    }
    else
    {
        out << "{" << unroll << " for(";

        if (node->getInit())
        {
            node->getInit()->traverse(this);
        }

        out << "; ";

        if (node->getCondition())
        {
            node->getCondition()->traverse(this);
        }

        out << "; ";

        if (node->getExpression())
        {
            node->getExpression()->traverse(this);
        }

        out << ")\n";

        outputLineDirective(out, node->getLine().first_line);
    }

    if (node->getBody())
    {
        // The loop body node will output braces.
        node->getBody()->traverse(this);
    }
    else
    {
        // TODO(oetuaho): Check if the semicolon inside is necessary.
        // It's there as a result of conservative refactoring of the output.
        out << "{;}\n";
    }

    outputLineDirective(out, node->getLine().first_line);

    if (node->getType() == ELoopDoWhile)
    {
        outputLineDirective(out, node->getCondition()->getLine().first_line);
        out << "while (";

        node->getCondition()->traverse(this);

        out << ");\n";
    }

    out << "}\n";

    mInsideDiscontinuousLoop = wasDiscontinuous;
    mNestedLoopDepth--;

    return false;
}

bool OutputHLSL::visitBranch(Visit visit, TIntermBranch *node)
{
    if (visit == PreVisit)
    {
        TInfoSinkBase &out = getInfoSink();

        switch (node->getFlowOp())
        {
            case EOpKill:
                out << "discard";
                break;
            case EOpBreak:
                if (mNestedLoopDepth > 1)
                {
                    mUsesNestedBreak = true;
                }

                if (mExcessiveLoopIndex)
                {
                    out << "{Break";
                    mExcessiveLoopIndex->traverse(this);
                    out << " = true; break;}\n";
                }
                else
                {
                    out << "break";
                }
                break;
            case EOpContinue:
                out << "continue";
                break;
            case EOpReturn:
                if (node->getExpression())
                {
                    out << "return ";
                }
                else
                {
                    out << "return";
                }
                break;
            default:
                UNREACHABLE();
        }
    }

    return true;
}

// Handle loops with more than 254 iterations (unsupported by D3D9) by splitting them
// (The D3D documentation says 255 iterations, but the compiler complains at anything more than
// 254).
bool OutputHLSL::handleExcessiveLoop(TInfoSinkBase &out, TIntermLoop *node)
{
    const int MAX_LOOP_ITERATIONS = 254;

    // Parse loops of the form:
    // for(int index = initial; index [comparator] limit; index += increment)
    TIntermSymbol *index = nullptr;
    TOperator comparator = EOpNull;
    int initial          = 0;
    int limit            = 0;
    int increment        = 0;

    // Parse index name and intial value
    if (node->getInit())
    {
        TIntermDeclaration *init = node->getInit()->getAsDeclarationNode();

        if (init)
        {
            TIntermSequence *sequence = init->getSequence();
            TIntermTyped *variable    = (*sequence)[0]->getAsTyped();

            if (variable && variable->getQualifier() == EvqTemporary)
            {
                TIntermBinary *assign = variable->getAsBinaryNode();

                if (assign->getOp() == EOpInitialize)
                {
                    TIntermSymbol *symbol          = assign->getLeft()->getAsSymbolNode();
                    TIntermConstantUnion *constant = assign->getRight()->getAsConstantUnion();

                    if (symbol && constant)
                    {
                        if (constant->getBasicType() == EbtInt && constant->isScalar())
                        {
                            index   = symbol;
                            initial = constant->getIConst(0);
                        }
                    }
                }
            }
        }
    }

    // Parse comparator and limit value
    if (index != nullptr && node->getCondition())
    {
        TIntermBinary *test = node->getCondition()->getAsBinaryNode();

        if (test && test->getLeft()->getAsSymbolNode()->getId() == index->getId())
        {
            TIntermConstantUnion *constant = test->getRight()->getAsConstantUnion();

            if (constant)
            {
                if (constant->getBasicType() == EbtInt && constant->isScalar())
                {
                    comparator = test->getOp();
                    limit      = constant->getIConst(0);
                }
            }
        }
    }

    // Parse increment
    if (index != nullptr && comparator != EOpNull && node->getExpression())
    {
        TIntermBinary *binaryTerminal = node->getExpression()->getAsBinaryNode();
        TIntermUnary *unaryTerminal   = node->getExpression()->getAsUnaryNode();

        if (binaryTerminal)
        {
            TOperator op                   = binaryTerminal->getOp();
            TIntermConstantUnion *constant = binaryTerminal->getRight()->getAsConstantUnion();

            if (constant)
            {
                if (constant->getBasicType() == EbtInt && constant->isScalar())
                {
                    int value = constant->getIConst(0);

                    switch (op)
                    {
                        case EOpAddAssign:
                            increment = value;
                            break;
                        case EOpSubAssign:
                            increment = -value;
                            break;
                        default:
                            UNIMPLEMENTED();
                    }
                }
            }
        }
        else if (unaryTerminal)
        {
            TOperator op = unaryTerminal->getOp();

            switch (op)
            {
                case EOpPostIncrement:
                    increment = 1;
                    break;
                case EOpPostDecrement:
                    increment = -1;
                    break;
                case EOpPreIncrement:
                    increment = 1;
                    break;
                case EOpPreDecrement:
                    increment = -1;
                    break;
                default:
                    UNIMPLEMENTED();
            }
        }
    }

    if (index != nullptr && comparator != EOpNull && increment != 0)
    {
        if (comparator == EOpLessThanEqual)
        {
            comparator = EOpLessThan;
            limit += 1;
        }

        if (comparator == EOpLessThan)
        {
            int iterations = (limit - initial) / increment;

            if (iterations <= MAX_LOOP_ITERATIONS)
            {
                return false;  // Not an excessive loop
            }

            TIntermSymbol *restoreIndex = mExcessiveLoopIndex;
            mExcessiveLoopIndex         = index;

            out << "{int ";
            index->traverse(this);
            out << ";\n"
                   "bool Break";
            index->traverse(this);
            out << " = false;\n";

            bool firstLoopFragment = true;

            while (iterations > 0)
            {
                int clampedLimit = initial + increment * std::min(MAX_LOOP_ITERATIONS, iterations);

                if (!firstLoopFragment)
                {
                    out << "if (!Break";
                    index->traverse(this);
                    out << ") {\n";
                }

                if (iterations <= MAX_LOOP_ITERATIONS)  // Last loop fragment
                {
                    mExcessiveLoopIndex = nullptr;  // Stops setting the Break flag
                }

                // for(int index = initial; index < clampedLimit; index += increment)
                const char *unroll =
                    mCurrentFunctionMetadata->hasGradientInCallGraph(node) ? "LOOP" : "";

                out << unroll << " for(";
                index->traverse(this);
                out << " = ";
                out << initial;

                out << "; ";
                index->traverse(this);
                out << " < ";
                out << clampedLimit;

                out << "; ";
                index->traverse(this);
                out << " += ";
                out << increment;
                out << ")\n";

                outputLineDirective(out, node->getLine().first_line);
                out << "{\n";

                if (node->getBody())
                {
                    node->getBody()->traverse(this);
                }

                outputLineDirective(out, node->getLine().first_line);
                out << ";}\n";

                if (!firstLoopFragment)
                {
                    out << "}\n";
                }

                firstLoopFragment = false;

                initial += MAX_LOOP_ITERATIONS * increment;
                iterations -= MAX_LOOP_ITERATIONS;
            }

            out << "}";

            mExcessiveLoopIndex = restoreIndex;

            return true;
        }
        else
            UNIMPLEMENTED();
    }

    return false;  // Not handled as an excessive loop
}

void OutputHLSL::outputTriplet(TInfoSinkBase &out,
                               Visit visit,
                               const char *preString,
                               const char *inString,
                               const char *postString)
{
    if (visit == PreVisit)
    {
        out << preString;
    }
    else if (visit == InVisit)
    {
        out << inString;
    }
    else if (visit == PostVisit)
    {
        out << postString;
    }
}

void OutputHLSL::outputLineDirective(TInfoSinkBase &out, int line)
{
    if ((mCompileOptions & SH_LINE_DIRECTIVES) && (line > 0))
    {
        out << "\n";
        out << "#line " << line;

        if (mSourcePath)
        {
            out << " \"" << mSourcePath << "\"";
        }

        out << "\n";
    }
}

TString OutputHLSL::argumentString(const TIntermSymbol *symbol)
{
    TQualifier qualifier = symbol->getQualifier();
    const TType &type    = symbol->getType();
    const TName &name    = symbol->getName();
    TString nameStr;

    if (name.getString().empty())  // HLSL demands named arguments, also for prototypes
    {
        nameStr = "x" + str(mUniqueIndex++);
    }
    else
    {
        nameStr = DecorateVariableIfNeeded(name);
    }

    if (IsSampler(type.getBasicType()))
    {
        if (mOutputType == SH_HLSL_4_1_OUTPUT)
        {
            // Samplers are passed as indices to the sampler array.
            ASSERT(qualifier != EvqOut && qualifier != EvqInOut);
            return "const uint " + nameStr + ArrayString(type);
        }
        if (mOutputType == SH_HLSL_4_0_FL9_3_OUTPUT)
        {
            return QualifierString(qualifier) + " " + TextureString(type.getBasicType()) +
                   " texture_" + nameStr + ArrayString(type) + ", " + QualifierString(qualifier) +
                   " " + SamplerString(type.getBasicType()) + " sampler_" + nameStr +
                   ArrayString(type);
        }
    }

    TStringStream argString;
    argString << QualifierString(qualifier) << " " << TypeString(type) << " " << nameStr
              << ArrayString(type);

    // If the structure parameter contains samplers, they need to be passed into the function as
    // separate parameters. HLSL doesn't natively support samplers in structs.
    if (type.isStructureContainingSamplers())
    {
        ASSERT(qualifier != EvqOut && qualifier != EvqInOut);
        TVector<TIntermSymbol *> samplerSymbols;
        type.createSamplerSymbols("angle" + nameStr, "", &samplerSymbols, nullptr, mSymbolTable);
        for (const TIntermSymbol *sampler : samplerSymbols)
        {
            const TType &samplerType = sampler->getType();
            if (mOutputType == SH_HLSL_4_1_OUTPUT)
            {
                argString << ", const uint " << sampler->getSymbol() << ArrayString(samplerType);
            }
            else if (mOutputType == SH_HLSL_4_0_FL9_3_OUTPUT)
            {
                ASSERT(IsSampler(samplerType.getBasicType()));
                argString << ", " << QualifierString(qualifier) << " "
                          << TextureString(samplerType.getBasicType()) << " texture_"
                          << sampler->getSymbol() << ArrayString(samplerType) << ", "
                          << QualifierString(qualifier) << " "
                          << SamplerString(samplerType.getBasicType()) << " sampler_"
                          << sampler->getSymbol() << ArrayString(samplerType);
            }
            else
            {
                ASSERT(IsSampler(samplerType.getBasicType()));
                argString << ", " << QualifierString(qualifier) << " " << TypeString(samplerType)
                          << " " << sampler->getSymbol() << ArrayString(samplerType);
            }
        }
    }

    return argString.str();
}

TString OutputHLSL::initializer(const TType &type)
{
    TString string;

    size_t size = type.getObjectSize();
    for (size_t component = 0; component < size; component++)
    {
        string += "0";

        if (component + 1 < size)
        {
            string += ", ";
        }
    }

    return "{" + string + "}";
}

void OutputHLSL::outputConstructor(TInfoSinkBase &out, Visit visit, TIntermAggregate *node)
{
    // Array constructors should have been already pruned from the code.
    ASSERT(!node->getType().isArray());

    if (visit == PreVisit)
    {
        TString constructorName;
        if (node->getBasicType() == EbtStruct)
        {
            constructorName = mStructureHLSL->addStructConstructor(*node->getType().getStruct());
        }
        else
        {
            constructorName =
                mStructureHLSL->addBuiltInConstructor(node->getType(), node->getSequence());
        }
        out << constructorName << "(";
    }
    else if (visit == InVisit)
    {
        out << ", ";
    }
    else if (visit == PostVisit)
    {
        out << ")";
    }
}

const TConstantUnion *OutputHLSL::writeConstantUnion(TInfoSinkBase &out,
                                                     const TType &type,
                                                     const TConstantUnion *const constUnion)
{
    const TConstantUnion *constUnionIterated = constUnion;

    const TStructure *structure = type.getStruct();
    if (structure)
    {
        out << mStructureHLSL->addStructConstructor(*structure) << "(";

        const TFieldList &fields = structure->fields();

        for (size_t i = 0; i < fields.size(); i++)
        {
            const TType *fieldType = fields[i]->type();
            constUnionIterated     = writeConstantUnion(out, *fieldType, constUnionIterated);

            if (i != fields.size() - 1)
            {
                out << ", ";
            }
        }

        out << ")";
    }
    else
    {
        size_t size    = type.getObjectSize();
        bool writeType = size > 1;

        if (writeType)
        {
            out << TypeString(type) << "(";
        }
        constUnionIterated = writeConstantUnionArray(out, constUnionIterated, size);
        if (writeType)
        {
            out << ")";
        }
    }

    return constUnionIterated;
}

void OutputHLSL::writeEmulatedFunctionTriplet(TInfoSinkBase &out, Visit visit, TOperator op)
{
    if (visit == PreVisit)
    {
        const char *opStr = GetOperatorString(op);
        BuiltInFunctionEmulator::WriteEmulatedFunctionName(out, opStr);
        out << "(";
    }
    else
    {
        outputTriplet(out, visit, nullptr, ", ", ")");
    }
}

bool OutputHLSL::writeSameSymbolInitializer(TInfoSinkBase &out,
                                            TIntermSymbol *symbolNode,
                                            TIntermTyped *expression)
{
    sh::SearchSymbol searchSymbol(symbolNode->getSymbol());
    expression->traverse(&searchSymbol);

    if (searchSymbol.foundMatch())
    {
        // Type already printed
        out << "t" + str(mUniqueIndex) + " = ";
        expression->traverse(this);
        out << ", ";
        symbolNode->traverse(this);
        out << " = t" + str(mUniqueIndex);

        mUniqueIndex++;
        return true;
    }

    return false;
}

bool OutputHLSL::canWriteAsHLSLLiteral(TIntermTyped *expression)
{
    // We support writing constant unions and constructors that only take constant unions as
    // parameters as HLSL literals.
    return !expression->getType().isArrayOfArrays() &&
           (expression->getAsConstantUnion() ||
            expression->isConstructorWithOnlyConstantUnionParameters());
}

bool OutputHLSL::writeConstantInitialization(TInfoSinkBase &out,
                                             TIntermSymbol *symbolNode,
                                             TIntermTyped *initializer)
{
    if (canWriteAsHLSLLiteral(initializer))
    {
        symbolNode->traverse(this);
        ASSERT(!symbolNode->getType().isArrayOfArrays());
        if (symbolNode->getType().isArray())
        {
            out << "[" << symbolNode->getType().getOutermostArraySize() << "]";
        }
        out << " = {";
        if (initializer->getAsConstantUnion())
        {
            TIntermConstantUnion *nodeConst  = initializer->getAsConstantUnion();
            const TConstantUnion *constUnion = nodeConst->getUnionArrayPointer();
            writeConstantUnionArray(out, constUnion, nodeConst->getType().getObjectSize());
        }
        else
        {
            TIntermAggregate *constructor = initializer->getAsAggregate();
            ASSERT(constructor != nullptr);
            for (TIntermNode *&node : *constructor->getSequence())
            {
                TIntermConstantUnion *nodeConst = node->getAsConstantUnion();
                ASSERT(nodeConst);
                const TConstantUnion *constUnion = nodeConst->getUnionArrayPointer();
                writeConstantUnionArray(out, constUnion, nodeConst->getType().getObjectSize());
                if (node != constructor->getSequence()->back())
                {
                    out << ", ";
                }
            }
        }
        out << "}";
        return true;
    }
    return false;
}

TString OutputHLSL::addStructEqualityFunction(const TStructure &structure)
{
    const TFieldList &fields = structure.fields();

    for (const auto &eqFunction : mStructEqualityFunctions)
    {
        if (eqFunction->structure == &structure)
        {
            return eqFunction->functionName;
        }
    }

    const TString &structNameString = StructNameString(structure);

    StructEqualityFunction *function = new StructEqualityFunction();
    function->structure              = &structure;
    function->functionName           = "angle_eq_" + structNameString;

    TInfoSinkBase fnOut;

    fnOut << "bool " << function->functionName << "(" << structNameString << " a, "
          << structNameString + " b)\n"
          << "{\n"
             "    return ";

    for (size_t i = 0; i < fields.size(); i++)
    {
        const TField *field    = fields[i];
        const TType *fieldType = field->type();

        const TString &fieldNameA = "a." + Decorate(field->name());
        const TString &fieldNameB = "b." + Decorate(field->name());

        if (i > 0)
        {
            fnOut << " && ";
        }

        fnOut << "(";
        outputEqual(PreVisit, *fieldType, EOpEqual, fnOut);
        fnOut << fieldNameA;
        outputEqual(InVisit, *fieldType, EOpEqual, fnOut);
        fnOut << fieldNameB;
        outputEqual(PostVisit, *fieldType, EOpEqual, fnOut);
        fnOut << ")";
    }

    fnOut << ";\n"
          << "}\n";

    function->functionDefinition = fnOut.c_str();

    mStructEqualityFunctions.push_back(function);
    mEqualityFunctions.push_back(function);

    return function->functionName;
}

TString OutputHLSL::addArrayEqualityFunction(const TType &type)
{
    for (const auto &eqFunction : mArrayEqualityFunctions)
    {
        if (eqFunction->type == type)
        {
            return eqFunction->functionName;
        }
    }

    TType elementType(type);
    elementType.toArrayElementType();

    ArrayHelperFunction *function = new ArrayHelperFunction();
    function->type                = type;

    function->functionName = ArrayHelperFunctionName("angle_eq", type);

    TInfoSinkBase fnOut;

    const TString &typeName = TypeString(type);
    fnOut << "bool " << function->functionName << "(" << typeName << " a" << ArrayString(type)
          << ", " << typeName << " b" << ArrayString(type) << ")\n"
          << "{\n"
             "    for (int i = 0; i < "
          << type.getOutermostArraySize()
          << "; ++i)\n"
             "    {\n"
             "        if (";

    outputEqual(PreVisit, elementType, EOpNotEqual, fnOut);
    fnOut << "a[i]";
    outputEqual(InVisit, elementType, EOpNotEqual, fnOut);
    fnOut << "b[i]";
    outputEqual(PostVisit, elementType, EOpNotEqual, fnOut);

    fnOut << ") { return false; }\n"
             "    }\n"
             "    return true;\n"
             "}\n";

    function->functionDefinition = fnOut.c_str();

    mArrayEqualityFunctions.push_back(function);
    mEqualityFunctions.push_back(function);

    return function->functionName;
}

TString OutputHLSL::addArrayAssignmentFunction(const TType &type)
{
    for (const auto &assignFunction : mArrayAssignmentFunctions)
    {
        if (assignFunction.type == type)
        {
            return assignFunction.functionName;
        }
    }

    TType elementType(type);
    elementType.toArrayElementType();

    ArrayHelperFunction function;
    function.type = type;

    function.functionName = ArrayHelperFunctionName("angle_assign", type);

    TInfoSinkBase fnOut;

    const TString &typeName = TypeString(type);
    fnOut << "void " << function.functionName << "(out " << typeName << " a" << ArrayString(type)
          << ", " << typeName << " b" << ArrayString(type) << ")\n"
          << "{\n"
             "    for (int i = 0; i < "
          << type.getOutermostArraySize()
          << "; ++i)\n"
             "    {\n"
             "        ";

    outputAssign(PreVisit, elementType, fnOut);
    fnOut << "a[i]";
    outputAssign(InVisit, elementType, fnOut);
    fnOut << "b[i]";
    outputAssign(PostVisit, elementType, fnOut);

    fnOut << ";\n"
             "    }\n"
             "}\n";

    function.functionDefinition = fnOut.c_str();

    mArrayAssignmentFunctions.push_back(function);

    return function.functionName;
}

TString OutputHLSL::addArrayConstructIntoFunction(const TType &type)
{
    for (const auto &constructIntoFunction : mArrayConstructIntoFunctions)
    {
        if (constructIntoFunction.type == type)
        {
            return constructIntoFunction.functionName;
        }
    }

    TType elementType(type);
    elementType.toArrayElementType();

    ArrayHelperFunction function;
    function.type = type;

    function.functionName = ArrayHelperFunctionName("angle_construct_into", type);

    TInfoSinkBase fnOut;

    const TString &typeName = TypeString(type);
    fnOut << "void " << function.functionName << "(out " << typeName << " a" << ArrayString(type);
    for (unsigned int i = 0u; i < type.getOutermostArraySize(); ++i)
    {
        fnOut << ", " << typeName << " b" << i << ArrayString(elementType);
    }
    fnOut << ")\n"
             "{\n";

    for (unsigned int i = 0u; i < type.getOutermostArraySize(); ++i)
    {
        fnOut << "    ";
        outputAssign(PreVisit, elementType, fnOut);
        fnOut << "a[" << i << "]";
        outputAssign(InVisit, elementType, fnOut);
        fnOut << "b" << i;
        outputAssign(PostVisit, elementType, fnOut);
        fnOut << ";\n";
    }
    fnOut << "}\n";

    function.functionDefinition = fnOut.c_str();

    mArrayConstructIntoFunctions.push_back(function);

    return function.functionName;
}

void OutputHLSL::ensureStructDefined(const TType &type)
{
    const TStructure *structure = type.getStruct();
    if (structure)
    {
        ASSERT(type.getBasicType() == EbtStruct);
        mStructureHLSL->ensureStructDefined(*structure);
    }
}

}  // namespace sh
