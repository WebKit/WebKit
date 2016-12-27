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
#include "compiler/translator/FlagStd140Structs.h"
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

namespace
{

void WriteSingleConstant(TInfoSinkBase &out, const TConstantUnion *const constUnion)
{
    ASSERT(constUnion != nullptr);
    switch (constUnion->getType())
    {
        case EbtFloat:
            out << std::min(FLT_MAX, std::max(-FLT_MAX, constUnion->getFConst()));
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

const TConstantUnion *WriteConstantUnionArray(TInfoSinkBase &out,
                                              const TConstantUnion *const constUnion,
                                              const size_t size)
{
    const TConstantUnion *constUnionIterated = constUnion;
    for (size_t i = 0; i < size; i++, constUnionIterated++)
    {
        WriteSingleConstant(out, constUnionIterated);

        if (i != size - 1)
        {
            out << ", ";
        }
    }
    return constUnionIterated;
}

} // namespace

namespace sh
{

OutputHLSL::OutputHLSL(sh::GLenum shaderType,
                       int shaderVersion,
                       const TExtensionBehavior &extensionBehavior,
                       const char *sourcePath,
                       ShShaderOutput outputType,
                       int numRenderTargets,
                       const std::vector<Uniform> &uniforms,
                       ShCompileOptions compileOptions)
    : TIntermTraverser(true, true, true),
      mShaderType(shaderType),
      mShaderVersion(shaderVersion),
      mExtensionBehavior(extensionBehavior),
      mSourcePath(sourcePath),
      mOutputType(outputType),
      mCompileOptions(compileOptions),
      mNumRenderTargets(numRenderTargets),
      mCurrentFunctionMetadata(nullptr)
{
    mInsideFunction = false;

    mUsesFragColor = false;
    mUsesFragData = false;
    mUsesDepthRange = false;
    mUsesFragCoord = false;
    mUsesPointCoord = false;
    mUsesFrontFacing = false;
    mUsesPointSize = false;
    mUsesInstanceID = false;
    mUsesVertexID                = false;
    mUsesFragDepth = false;
    mUsesXor = false;
    mUsesDiscardRewriting = false;
    mUsesNestedBreak = false;
    mRequiresIEEEStrictCompiling = false;

    mUniqueIndex = 0;

    mOutputLod0Function = false;
    mInsideDiscontinuousLoop = false;
    mNestedLoopDepth = 0;

    mExcessiveLoopIndex = NULL;

    mStructureHLSL = new StructureHLSL;
    mUniformHLSL = new UniformHLSL(mStructureHLSL, outputType, uniforms);
    mTextureFunctionHLSL = new TextureFunctionHLSL;

    if (mOutputType == SH_HLSL_3_0_OUTPUT)
    {
        // Fragment shaders need dx_DepthRange, dx_ViewCoords and dx_DepthFront.
        // Vertex shaders need a slightly different set: dx_DepthRange, dx_ViewCoords and dx_ViewAdjust.
        // In both cases total 3 uniform registers need to be reserved.
        mUniformHLSL->reserveUniformRegisters(3);
    }

    // Reserve registers for the default uniform block and driver constants
    mUniformHLSL->reserveInterfaceBlockRegisters(2);
}

OutputHLSL::~OutputHLSL()
{
    SafeDelete(mStructureHLSL);
    SafeDelete(mUniformHLSL);
    SafeDelete(mTextureFunctionHLSL);
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
    const std::vector<TIntermTyped*> &flaggedStructs = FlagStd140ValueStructs(treeRoot);
    makeFlaggedStructMaps(flaggedStructs);

    BuiltInFunctionEmulator builtInFunctionEmulator;
    InitBuiltInFunctionEmulatorForHLSL(&builtInFunctionEmulator);
    if ((mCompileOptions & SH_EMULATE_ISNAN_FLOAT_FUNCTION) != 0)
    {
        InitBuiltInIsnanFunctionEmulatorForHLSLWorkarounds(&builtInFunctionEmulator,
                                                           mShaderVersion);
    }

    builtInFunctionEmulator.MarkBuiltInFunctionsForEmulation(treeRoot);

    // Now that we are done changing the AST, do the analyses need for HLSL generation
    CallDAG::InitResult success = mCallDag.init(treeRoot, &objSink);
    ASSERT(success == CallDAG::INITDAG_SUCCESS);
    UNUSED_ASSERTION_VARIABLE(success);
    mASTMetadataList = CreateASTMetadataHLSL(treeRoot, mCallDag);

    // Output the body and footer first to determine what has to go in the header
    mInfoSinkStack.push(&mBody);
    treeRoot->traverse(this);
    mInfoSinkStack.pop();

    mInfoSinkStack.push(&mFooter);
    mInfoSinkStack.pop();

    mInfoSinkStack.push(&mHeader);
    header(mHeader, &builtInFunctionEmulator);
    mInfoSinkStack.pop();

    objSink << mHeader.c_str();
    objSink << mBody.c_str();
    objSink << mFooter.c_str();

    builtInFunctionEmulator.Cleanup();
}

void OutputHLSL::makeFlaggedStructMaps(const std::vector<TIntermTyped *> &flaggedStructs)
{
    for (unsigned int structIndex = 0; structIndex < flaggedStructs.size(); structIndex++)
    {
        TIntermTyped *flaggedNode = flaggedStructs[structIndex];

        TInfoSinkBase structInfoSink;
        mInfoSinkStack.push(&structInfoSink);

        // This will mark the necessary block elements as referenced
        flaggedNode->traverse(this);

        TString structName(structInfoSink.c_str());
        mInfoSinkStack.pop();

        mFlaggedStructOriginalNames[flaggedNode] = structName;

        for (size_t pos = structName.find('.'); pos != std::string::npos; pos = structName.find('.'))
        {
            structName.erase(pos, 1);
        }

        mFlaggedStructMappedNames[flaggedNode] = "map" + structName;
    }
}

const std::map<std::string, unsigned int> &OutputHLSL::getInterfaceBlockRegisterMap() const
{
    return mUniformHLSL->getInterfaceBlockRegisterMap();
}

const std::map<std::string, unsigned int> &OutputHLSL::getUniformRegisterMap() const
{
    return mUniformHLSL->getUniformRegisterMap();
}

int OutputHLSL::vectorSize(const TType &type) const
{
    int elementSize = type.isMatrix() ? type.getCols() : 1;
    unsigned int arraySize = type.isArray() ? type.getArraySize() : 1u;

    return elementSize * arraySize;
}

TString OutputHLSL::structInitializerString(int indent, const TStructure &structure, const TString &rhsStructName)
{
    TString init;

    TString preIndentString;
    TString fullIndentString;

    for (int spaces = 0; spaces < (indent * 4); spaces++)
    {
        preIndentString += ' ';
    }

    for (int spaces = 0; spaces < ((indent+1) * 4); spaces++)
    {
        fullIndentString += ' ';
    }

    init += preIndentString + "{\n";

    const TFieldList &fields = structure.fields();
    for (unsigned int fieldIndex = 0; fieldIndex < fields.size(); fieldIndex++)
    {
        const TField &field = *fields[fieldIndex];
        const TString &fieldName = rhsStructName + "." + Decorate(field.name());
        const TType &fieldType = *field.type();

        if (fieldType.getStruct())
        {
            init += structInitializerString(indent + 1, *fieldType.getStruct(), fieldName);
        }
        else
        {
            init += fullIndentString + fieldName + ",\n";
        }
    }

    init += preIndentString + "}" + (indent == 0 ? ";" : ",") + "\n";

    return init;
}

void OutputHLSL::header(TInfoSinkBase &out, const BuiltInFunctionEmulator *builtInFunctionEmulator)
{
    TString varyings;
    TString attributes;
    TString flaggedStructs;

    for (std::map<TIntermTyped*, TString>::const_iterator flaggedStructIt = mFlaggedStructMappedNames.begin(); flaggedStructIt != mFlaggedStructMappedNames.end(); flaggedStructIt++)
    {
        TIntermTyped *structNode = flaggedStructIt->first;
        const TString &mappedName = flaggedStructIt->second;
        const TStructure &structure = *structNode->getType().getStruct();
        const TString &originalName = mFlaggedStructOriginalNames[structNode];

        flaggedStructs += "static " + Decorate(structure.name()) + " " + mappedName + " =\n";
        flaggedStructs += structInitializerString(0, structure, originalName);
        flaggedStructs += "\n";
    }

    for (ReferencedSymbols::const_iterator varying = mReferencedVaryings.begin(); varying != mReferencedVaryings.end(); varying++)
    {
        const TType &type = varying->second->getType();
        const TString &name = varying->second->getSymbol();

        // Program linking depends on this exact format
        varyings += "static " + InterpolationString(type.getQualifier()) + " " + TypeString(type) + " " +
                    Decorate(name) + ArrayString(type) + " = " + initializer(type) + ";\n";
    }

    for (ReferencedSymbols::const_iterator attribute = mReferencedAttributes.begin(); attribute != mReferencedAttributes.end(); attribute++)
    {
        const TType &type = attribute->second->getType();
        const TString &name = attribute->second->getSymbol();

        attributes += "static " + TypeString(type) + " " + Decorate(name) + ArrayString(type) + " = " + initializer(type) + ";\n";
    }

    out << mStructureHLSL->structsHeader();

    mUniformHLSL->uniformsHeader(out, mOutputType, mReferencedUniforms);
    out << mUniformHLSL->interfaceBlocksHeader(mReferencedInterfaceBlocks);

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
        TExtensionBehavior::const_iterator iter = mExtensionBehavior.find("GL_EXT_draw_buffers");
        const bool usingMRTExtension = (iter != mExtensionBehavior.end() && (iter->second == EBhEnable || iter->second == EBhRequire));

        out << "// Varyings\n";
        out <<  varyings;
        out << "\n";

        if (mShaderVersion >= 300)
        {
            for (ReferencedSymbols::const_iterator outputVariableIt = mReferencedOutputVariables.begin(); outputVariableIt != mReferencedOutputVariables.end(); outputVariableIt++)
            {
                const TString &variableName = outputVariableIt->first;
                const TType &variableType = outputVariableIt->second->getType();

                out << "static " + TypeString(variableType) + " out_" + variableName + ArrayString(variableType) +
                       " = " + initializer(variableType) + ";\n";
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
            out << "static gl_DepthRangeParameters gl_DepthRange = {dx_DepthRange.x, dx_DepthRange.y, dx_DepthRange.z};\n"
                   "\n";
        }

        if (!flaggedStructs.empty())
        {
            out << "// Std140 Structures accessed by value\n";
            out << "\n";
            out << flaggedStructs;
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
    else   // Vertex shader
    {
        out << "// Attributes\n";
        out <<  attributes;
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
        out <<  varyings;
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
            out << "static gl_DepthRangeParameters gl_DepthRange = {dx_DepthRange.x, dx_DepthRange.y, dx_DepthRange.z};\n"
                   "\n";
        }

        if (!flaggedStructs.empty())
        {
            out << "// Std140 Structures accessed by value\n";
            out << "\n";
            out << flaggedStructs;
            out << "\n";
        }
    }

    bool getDimensionsIgnoresBaseLevel =
        (mCompileOptions & SH_HLSL_GET_DIMENSIONS_IGNORES_BASE_LEVEL) != 0;
    mTextureFunctionHLSL->textureFunctionHeader(out, mOutputType, getDimensionsIgnoresBaseLevel);

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

    if (mUsesFragDepth)
    {
        out << "#define GL_USES_FRAG_DEPTH\n";
    }

    if (mUsesDepthRange)
    {
        out << "#define GL_USES_DEPTH_RANGE\n";
    }

    if (mUsesXor)
    {
        out << "bool xor(bool p, bool q)\n"
               "{\n"
               "    return (p || q) && !(p && q);\n"
               "}\n"
               "\n";
    }

    builtInFunctionEmulator->OutputEmulatedFunctions(out);
}

void OutputHLSL::visitSymbol(TIntermSymbol *node)
{
    TInfoSinkBase &out = getInfoSink();

    // Handle accessing std140 structs by value
    if (mFlaggedStructMappedNames.count(node) > 0)
    {
        out << mFlaggedStructMappedNames[node];
        return;
    }

    TString name = node->getSymbol();

    if (name == "gl_DepthRange")
    {
        mUsesDepthRange = true;
        out << name;
    }
    else
    {
        TQualifier qualifier = node->getQualifier();

        if (qualifier == EvqUniform)
        {
            const TType &nodeType = node->getType();
            const TInterfaceBlock *interfaceBlock = nodeType.getInterfaceBlock();

            if (interfaceBlock)
            {
                mReferencedInterfaceBlocks[interfaceBlock->name()] = node;
            }
            else
            {
                mReferencedUniforms[name] = node;
            }

            ensureStructDefined(nodeType);

            const TName &nameWithMetadata = node->getName();
            out << DecorateUniform(nameWithMetadata, nodeType);
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
        else
        {
            out << DecorateIfNeeded(node->getName());
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

bool OutputHLSL::ancestorEvaluatesToSamplerInStruct(Visit visit)
{
    // Inside InVisit the current node is already in the path.
    const unsigned int initialN = visit == InVisit ? 1u : 0u;
    for (unsigned int n = initialN; getAncestorNode(n) != nullptr; ++n)
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

    // Handle accessing std140 structs by value
    if (mFlaggedStructMappedNames.count(node) > 0)
    {
        out << mFlaggedStructMappedNames[node];
        return false;
    }

    switch (node->getOp())
    {
        case EOpComma:
            outputTriplet(out, visit, "(", ", ", ")");
            break;
        case EOpAssign:
            if (node->getLeft()->isArray())
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
                ASSERT(rightAgg == nullptr || rightAgg->getOp() != EOpFunctionCall);

                const TString &functionName = addArrayAssignmentFunction(node->getType());
                outputTriplet(out, visit, (functionName + "(").c_str(), ", ", ")");
            }
            else
            {
                outputTriplet(out, visit, "(", " = ", ")");
            }
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
            const TType& leftType = node->getLeft()->getType();
            if (leftType.isInterfaceBlock())
            {
                if (visit == PreVisit)
                {
                    TInterfaceBlock* interfaceBlock = leftType.getInterfaceBlock();
                    const int arrayIndex = node->getRight()->getAsConstantUnion()->getIConst(0);
                    mReferencedInterfaceBlocks[interfaceBlock->instanceName()] = node->getLeft()->getAsSymbolNode();
                    out << mUniformHLSL->interfaceBlockInstanceString(*interfaceBlock, arrayIndex);
                    return false;
                }
            }
            else if (ancestorEvaluatesToSamplerInStruct(visit))
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
            const TStructure* structure = node->getLeft()->getType().getStruct();
            const TIntermConstantUnion* index = node->getRight()->getAsConstantUnion();
            const TField* field = structure->fields()[index->getIConst(0)];

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
                indexingReturnsSampler = ancestorEvaluatesToSamplerInStruct(visit);
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
        if (visit == InVisit)
        {
            const TInterfaceBlock* interfaceBlock = node->getLeft()->getType().getInterfaceBlock();
            const TIntermConstantUnion* index = node->getRight()->getAsConstantUnion();
            const TField* field = interfaceBlock->fields()[index->getIConst(0)];
            out << "." + Decorate(field->name());

            return false;
        }
        break;
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
        // HLSL doesn't short-circuit ||, so we assume that || affected by short-circuiting have been unfolded.
        ASSERT(!node->getRight()->hasSideEffects());
        outputTriplet(out, visit, "(", " || ", ")");
        return true;
      case EOpLogicalXor:
        mUsesXor = true;
        outputTriplet(out, visit, "xor(", ", ", ")");
        break;
      case EOpLogicalAnd:
        // HLSL doesn't short-circuit &&, so we assume that && affected by short-circuiting have been unfolded.
        ASSERT(!node->getRight()->hasSideEffects());
        outputTriplet(out, visit, "(", " && ", ")");
        return true;
      default: UNREACHABLE();
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
        case EOpVectorLogicalNot:
            outputTriplet(out, visit, "(!", "", ")");
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
        ASSERT(node->getUseEmulatedFunction());
        writeEmulatedFunctionTriplet(out, visit, "asinh(");
        break;
      case EOpAcosh:
        ASSERT(node->getUseEmulatedFunction());
        writeEmulatedFunctionTriplet(out, visit, "acosh(");
        break;
      case EOpAtanh:
        ASSERT(node->getUseEmulatedFunction());
        writeEmulatedFunctionTriplet(out, visit, "atanh(");
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
        writeEmulatedFunctionTriplet(out, visit, "roundEven(");
        break;
      case EOpCeil:
          outputTriplet(out, visit, "ceil(", "", ")");
          break;
      case EOpFract:
          outputTriplet(out, visit, "frac(", "", ")");
          break;
      case EOpIsNan:
          if (node->getUseEmulatedFunction())
              writeEmulatedFunctionTriplet(out, visit, "isnan(");
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
        ASSERT(node->getUseEmulatedFunction());
        writeEmulatedFunctionTriplet(out, visit, "packSnorm2x16(");
        break;
      case EOpPackUnorm2x16:
        ASSERT(node->getUseEmulatedFunction());
        writeEmulatedFunctionTriplet(out, visit, "packUnorm2x16(");
        break;
      case EOpPackHalf2x16:
        ASSERT(node->getUseEmulatedFunction());
        writeEmulatedFunctionTriplet(out, visit, "packHalf2x16(");
        break;
      case EOpUnpackSnorm2x16:
        ASSERT(node->getUseEmulatedFunction());
        writeEmulatedFunctionTriplet(out, visit, "unpackSnorm2x16(");
        break;
      case EOpUnpackUnorm2x16:
        ASSERT(node->getUseEmulatedFunction());
        writeEmulatedFunctionTriplet(out, visit, "unpackUnorm2x16(");
        break;
      case EOpUnpackHalf2x16:
        ASSERT(node->getUseEmulatedFunction());
        writeEmulatedFunctionTriplet(out, visit, "unpackHalf2x16(");
        break;
      case EOpLength:
          outputTriplet(out, visit, "length(", "", ")");
          break;
      case EOpNormalize:
          outputTriplet(out, visit, "normalize(", "", ")");
          break;
      case EOpDFdx:
        if(mInsideDiscontinuousLoop || mOutputLod0Function)
        {
            outputTriplet(out, visit, "(", "", ", 0.0)");
        }
        else
        {
            outputTriplet(out, visit, "ddx(", "", ")");
        }
        break;
      case EOpDFdy:
        if(mInsideDiscontinuousLoop || mOutputLod0Function)
        {
            outputTriplet(out, visit, "(", "", ", 0.0)");
        }
        else
        {
            outputTriplet(out, visit, "ddy(", "", ")");
        }
        break;
      case EOpFwidth:
        if(mInsideDiscontinuousLoop || mOutputLod0Function)
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
        writeEmulatedFunctionTriplet(out, visit, "inverse(");
        break;

      case EOpAny:
          outputTriplet(out, visit, "any(", "", ")");
          break;
      case EOpAll:
          outputTriplet(out, visit, "all(", "", ")");
          break;
      default: UNREACHABLE();
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
            TStructure *s       = nodeBinary->getLeft()->getAsTyped()->getType().getStruct();
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

    for (TIntermSequence::iterator sit = node->getSequence()->begin();
         sit != node->getSequence()->end(); sit++)
    {
        outputLineDirective(out, (*sit)->getLine().first_line);

        (*sit)->traverse(this);

        // Don't output ; after case labels, they're terminated by :
        // This is needed especially since outputting a ; after a case statement would turn empty
        // case statements into non-empty case statements, disallowing fall-through from them.
        // Also no need to output ; after if statements or sequences. This is done just for
        // code clarity.
        if ((*sit)->getAsCaseNode() == nullptr && (*sit)->getAsIfElseNode() == nullptr &&
            (*sit)->getAsBlock() == nullptr)
            out << ";\n";
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

    out << TypeString(node->getType()) << " ";

    TIntermSequence *parameters = node->getFunctionParameters()->getSequence();

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

bool OutputHLSL::visitAggregate(Visit visit, TIntermAggregate *node)
{
    TInfoSinkBase &out = getInfoSink();

    switch (node->getOp())
    {
        case EOpDeclaration:
            if (visit == PreVisit)
            {
                TIntermSequence *sequence = node->getSequence();
                TIntermTyped *variable    = (*sequence)[0]->getAsTyped();
                ASSERT(sequence->size() == 1);

                if (variable &&
                    (variable->getQualifier() == EvqTemporary ||
                     variable->getQualifier() == EvqGlobal || variable->getQualifier() == EvqConst))
                {
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
                             variable->getAsSymbolNode()->getSymbol() ==
                                 "")  // Type (struct) declaration
                    {
                        // Already added to constructor map
                    }
                    else
                        UNREACHABLE();
                }
                else if (variable && IsVaryingOut(variable->getQualifier()))
                {
                    for (TIntermSequence::iterator sit = sequence->begin(); sit != sequence->end();
                         sit++)
                    {
                        TIntermSymbol *symbol = (*sit)->getAsSymbolNode();

                        if (symbol)
                        {
                            // Vertex (output) varyings which are declared but not written to should
                            // still be declared to allow successful linking
                            mReferencedVaryings[symbol->getSymbol()] = symbol;
                        }
                        else
                        {
                            (*sit)->traverse(this);
                        }
                    }
                }

                return false;
            }
            else if (visit == InVisit)
            {
                out << ", ";
            }
            break;
        case EOpInvariantDeclaration:
            // Do not do any translation
            return false;
        case EOpPrototype:
            if (visit == PreVisit)
            {
                size_t index = mCallDag.findIndex(node->getFunctionSymbolInfo());
                // Skip the prototype if it is not implemented (and thus not used)
                if (index == CallDAG::InvalidIndex)
                {
                    return false;
                }

                TIntermSequence *arguments = node->getSequence();

                TString name =
                    DecorateFunctionIfNeeded(node->getFunctionSymbolInfo()->getNameObj());
                out << TypeString(node->getType()) << " " << name
                    << DisambiguateFunctionName(arguments) << (mOutputLod0Function ? "Lod0(" : "(");

                for (unsigned int i = 0; i < arguments->size(); i++)
                {
                    TIntermSymbol *symbol = (*arguments)[i]->getAsSymbolNode();

                    if (symbol)
                    {
                        out << argumentString(symbol);

                        if (i < arguments->size() - 1)
                        {
                            out << ", ";
                        }
                    }
                    else
                        UNREACHABLE();
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
            break;
        case EOpFunctionCall:
        {
            TIntermSequence *arguments = node->getSequence();

            bool lod0 = mInsideDiscontinuousLoop || mOutputLod0Function;
            if (node->isUserDefined())
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
            else if (node->getFunctionSymbolInfo()->getNameObj().isInternal())
            {
                // This path is used for internal functions that don't have their definitions in the
                // AST, such as precision emulation functions.
                out << DecorateFunctionIfNeeded(node->getFunctionSymbolInfo()->getNameObj()) << "(";
            }
            else
            {
                TString name = TFunction::unmangleName(node->getFunctionSymbolInfo()->getName());
                TBasicType samplerType = (*arguments)[0]->getAsTyped()->getType().getBasicType();
                int coords                  = (*arguments)[1]->getAsTyped()->getNominalSize();
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
                    argType.createSamplerSymbols("angle_" + structName, "",
                                                 argType.isArray() ? argType.getArraySize() : 0u,
                                                 &samplerSymbols, nullptr);
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
        case EOpParameters:
            outputTriplet(out, visit, "(", ", ", ")\n{\n");
            break;
        case EOpConstructFloat:
            outputConstructor(out, visit, node->getType(), "vec1", node->getSequence());
            break;
        case EOpConstructVec2:
            outputConstructor(out, visit, node->getType(), "vec2", node->getSequence());
            break;
        case EOpConstructVec3:
            outputConstructor(out, visit, node->getType(), "vec3", node->getSequence());
            break;
        case EOpConstructVec4:
            outputConstructor(out, visit, node->getType(), "vec4", node->getSequence());
            break;
        case EOpConstructBool:
            outputConstructor(out, visit, node->getType(), "bvec1", node->getSequence());
            break;
        case EOpConstructBVec2:
            outputConstructor(out, visit, node->getType(), "bvec2", node->getSequence());
            break;
        case EOpConstructBVec3:
            outputConstructor(out, visit, node->getType(), "bvec3", node->getSequence());
            break;
        case EOpConstructBVec4:
            outputConstructor(out, visit, node->getType(), "bvec4", node->getSequence());
            break;
        case EOpConstructInt:
            outputConstructor(out, visit, node->getType(), "ivec1", node->getSequence());
            break;
        case EOpConstructIVec2:
            outputConstructor(out, visit, node->getType(), "ivec2", node->getSequence());
            break;
        case EOpConstructIVec3:
            outputConstructor(out, visit, node->getType(), "ivec3", node->getSequence());
            break;
        case EOpConstructIVec4:
            outputConstructor(out, visit, node->getType(), "ivec4", node->getSequence());
            break;
        case EOpConstructUInt:
            outputConstructor(out, visit, node->getType(), "uvec1", node->getSequence());
            break;
        case EOpConstructUVec2:
            outputConstructor(out, visit, node->getType(), "uvec2", node->getSequence());
            break;
        case EOpConstructUVec3:
            outputConstructor(out, visit, node->getType(), "uvec3", node->getSequence());
            break;
        case EOpConstructUVec4:
            outputConstructor(out, visit, node->getType(), "uvec4", node->getSequence());
            break;
        case EOpConstructMat2:
            outputConstructor(out, visit, node->getType(), "mat2", node->getSequence());
            break;
        case EOpConstructMat2x3:
            outputConstructor(out, visit, node->getType(), "mat2x3", node->getSequence());
            break;
        case EOpConstructMat2x4:
            outputConstructor(out, visit, node->getType(), "mat2x4", node->getSequence());
            break;
        case EOpConstructMat3x2:
            outputConstructor(out, visit, node->getType(), "mat3x2", node->getSequence());
            break;
        case EOpConstructMat3:
            outputConstructor(out, visit, node->getType(), "mat3", node->getSequence());
            break;
        case EOpConstructMat3x4:
            outputConstructor(out, visit, node->getType(), "mat3x4", node->getSequence());
            break;
        case EOpConstructMat4x2:
            outputConstructor(out, visit, node->getType(), "mat4x2", node->getSequence());
            break;
        case EOpConstructMat4x3:
            outputConstructor(out, visit, node->getType(), "mat4x3", node->getSequence());
            break;
        case EOpConstructMat4:
            outputConstructor(out, visit, node->getType(), "mat4", node->getSequence());
            break;
        case EOpConstructStruct:
        {
            if (node->getType().isArray())
            {
                UNIMPLEMENTED();
            }
            const TString &structName = StructNameString(*node->getType().getStruct());
            mStructureHLSL->addConstructor(node->getType(), structName, node->getSequence());
            outputTriplet(out, visit, (structName + "_ctor(").c_str(), ", ", ")");
        }
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
        case EOpVectorEqual:
            outputTriplet(out, visit, "(", " == ", ")");
            break;
        case EOpVectorNotEqual:
            outputTriplet(out, visit, "(", " != ", ")");
            break;
        case EOpMod:
            ASSERT(node->getUseEmulatedFunction());
            writeEmulatedFunctionTriplet(out, visit, "mod(");
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
            writeEmulatedFunctionTriplet(out, visit, "atan(");
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
                // There is no HLSL equivalent for ESSL3 built-in "genType mix (genType x, genType y, genBType a)",
                // so use emulated version.
                ASSERT(node->getUseEmulatedFunction());
                writeEmulatedFunctionTriplet(out, visit, "mix(");
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
        case EOpDistance:
            outputTriplet(out, visit, "distance(", ", ", ")");
            break;
        case EOpDot:
            outputTriplet(out, visit, "dot(", ", ", ")");
            break;
        case EOpCross:
            outputTriplet(out, visit, "cross(", ", ", ")");
            break;
        case EOpFaceForward:
            ASSERT(node->getUseEmulatedFunction());
            writeEmulatedFunctionTriplet(out, visit, "faceforward(");
            break;
        case EOpReflect:
            outputTriplet(out, visit, "reflect(", ", ", ")");
            break;
        case EOpRefract:
            outputTriplet(out, visit, "refract(", ", ", ")");
            break;
        case EOpOuterProduct:
            ASSERT(node->getUseEmulatedFunction());
            writeEmulatedFunctionTriplet(out, visit, "outerProduct(");
            break;
        case EOpMul:
            outputTriplet(out, visit, "(", " * ", ")");
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

    if (node->getStatementList())
    {
        node->setStatementList(RemoveSwitchFallThrough::removeFallThrough(node->getStatementList()));
        outputTriplet(out, visit, "switch (", ") ", "");
        // The curly braces get written when visiting the statementList aggregate
    }
    else
    {
        // No statementList, so it won't output curly braces
        outputTriplet(out, visit, "switch (", ") {", "}\n");
    }
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
    mInsideDiscontinuousLoop = mInsideDiscontinuousLoop ||
    mCurrentFunctionMetadata->mDiscontinuousLoops.count(node) > 0;

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
        out << "while(\n";

        node->getCondition()->traverse(this);

        out << ");";
    }

    out << "}\n";

    mInsideDiscontinuousLoop = wasDiscontinuous;
    mNestedLoopDepth--;

    return false;
}

bool OutputHLSL::visitBranch(Visit visit, TIntermBranch *node)
{
    TInfoSinkBase &out = getInfoSink();

    switch (node->getFlowOp())
    {
      case EOpKill:
          outputTriplet(out, visit, "discard;\n", "", "");
        break;
      case EOpBreak:
        if (visit == PreVisit)
        {
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
                out << "break;\n";
            }
        }
        break;
      case EOpContinue:
          outputTriplet(out, visit, "continue;\n", "", "");
          break;
      case EOpReturn:
        if (visit == PreVisit)
        {
            if (node->getExpression())
            {
                out << "return ";
            }
            else
            {
                out << "return;\n";
            }
        }
        else if (visit == PostVisit)
        {
            if (node->getExpression())
            {
                out << ";\n";
            }
        }
        break;
      default: UNREACHABLE();
    }

    return true;
}

bool OutputHLSL::isSingleStatement(TIntermNode *node)
{
    if (node->getAsBlock())
    {
        return false;
    }

    TIntermAggregate *aggregate = node->getAsAggregate();
    if (aggregate)
    {
        if (aggregate->getOp() == EOpDeclaration)
        {
            // Declaring multiple comma-separated variables must be considered multiple statements
            // because each individual declaration has side effects which are visible in the next.
            return false;
        }
        else
        {
            for (TIntermSequence::iterator sit = aggregate->getSequence()->begin(); sit != aggregate->getSequence()->end(); sit++)
            {
                if (!isSingleStatement(*sit))
                {
                    return false;
                }
            }

            return true;
        }
    }

    return true;
}

// Handle loops with more than 254 iterations (unsupported by D3D9) by splitting them
// (The D3D documentation says 255 iterations, but the compiler complains at anything more than 254).
bool OutputHLSL::handleExcessiveLoop(TInfoSinkBase &out, TIntermLoop *node)
{
    const int MAX_LOOP_ITERATIONS = 254;

    // Parse loops of the form:
    // for(int index = initial; index [comparator] limit; index += increment)
    TIntermSymbol *index = NULL;
    TOperator comparator = EOpNull;
    int initial = 0;
    int limit = 0;
    int increment = 0;

    // Parse index name and intial value
    if (node->getInit())
    {
        TIntermAggregate *init = node->getInit()->getAsAggregate();

        if (init)
        {
            TIntermSequence *sequence = init->getSequence();
            TIntermTyped *variable = (*sequence)[0]->getAsTyped();

            if (variable && variable->getQualifier() == EvqTemporary)
            {
                TIntermBinary *assign = variable->getAsBinaryNode();

                if (assign->getOp() == EOpInitialize)
                {
                    TIntermSymbol *symbol = assign->getLeft()->getAsSymbolNode();
                    TIntermConstantUnion *constant = assign->getRight()->getAsConstantUnion();

                    if (symbol && constant)
                    {
                        if (constant->getBasicType() == EbtInt && constant->isScalar())
                        {
                            index = symbol;
                            initial = constant->getIConst(0);
                        }
                    }
                }
            }
        }
    }

    // Parse comparator and limit value
    if (index != NULL && node->getCondition())
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
                    limit = constant->getIConst(0);
                }
            }
        }
    }

    // Parse increment
    if (index != NULL && comparator != EOpNull && node->getExpression())
    {
        TIntermBinary *binaryTerminal = node->getExpression()->getAsBinaryNode();
        TIntermUnary *unaryTerminal = node->getExpression()->getAsUnaryNode();

        if (binaryTerminal)
        {
            TOperator op = binaryTerminal->getOp();
            TIntermConstantUnion *constant = binaryTerminal->getRight()->getAsConstantUnion();

            if (constant)
            {
                if (constant->getBasicType() == EbtInt && constant->isScalar())
                {
                    int value = constant->getIConst(0);

                    switch (op)
                    {
                      case EOpAddAssign: increment = value;  break;
                      case EOpSubAssign: increment = -value; break;
                      default: UNIMPLEMENTED();
                    }
                }
            }
        }
        else if (unaryTerminal)
        {
            TOperator op = unaryTerminal->getOp();

            switch (op)
            {
              case EOpPostIncrement: increment = 1;  break;
              case EOpPostDecrement: increment = -1; break;
              case EOpPreIncrement:  increment = 1;  break;
              case EOpPreDecrement:  increment = -1; break;
              default: UNIMPLEMENTED();
            }
        }
    }

    if (index != NULL && comparator != EOpNull && increment != 0)
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
                return false;   // Not an excessive loop
            }

            TIntermSymbol *restoreIndex = mExcessiveLoopIndex;
            mExcessiveLoopIndex = index;

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

                if (iterations <= MAX_LOOP_ITERATIONS)   // Last loop fragment
                {
                    mExcessiveLoopIndex = NULL;   // Stops setting the Break flag
                }

                // for(int index = initial; index < clampedLimit; index += increment)
                const char *unroll = mCurrentFunctionMetadata->hasGradientInCallGraph(node) ? "LOOP" : "";

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
        else UNIMPLEMENTED();
    }

    return false;   // Not handled as an excessive loop
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
        nameStr = DecorateIfNeeded(name);
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
        type.createSamplerSymbols("angle" + nameStr, "", 0u, &samplerSymbols, nullptr);
        for (const TIntermSymbol *sampler : samplerSymbols)
        {
            if (mOutputType == SH_HLSL_4_1_OUTPUT)
            {
                argString << ", const uint " << sampler->getSymbol() << ArrayString(type);
            }
            else if (mOutputType == SH_HLSL_4_0_FL9_3_OUTPUT)
            {
                const TType &samplerType = sampler->getType();
                ASSERT((!type.isArray() && !samplerType.isArray()) ||
                       type.getArraySize() == samplerType.getArraySize());
                ASSERT(IsSampler(samplerType.getBasicType()));
                argString << ", " << QualifierString(qualifier) << " "
                          << TextureString(samplerType.getBasicType()) << " texture_"
                          << sampler->getSymbol() << ArrayString(type) << ", "
                          << QualifierString(qualifier) << " "
                          << SamplerString(samplerType.getBasicType()) << " sampler_"
                          << sampler->getSymbol() << ArrayString(type);
            }
            else
            {
                const TType &samplerType = sampler->getType();
                ASSERT((!type.isArray() && !samplerType.isArray()) ||
                       type.getArraySize() == samplerType.getArraySize());
                ASSERT(IsSampler(samplerType.getBasicType()));
                argString << ", " << QualifierString(qualifier) << " " << TypeString(samplerType)
                          << " " << sampler->getSymbol() << ArrayString(type);
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

void OutputHLSL::outputConstructor(TInfoSinkBase &out,
                                   Visit visit,
                                   const TType &type,
                                   const char *name,
                                   const TIntermSequence *parameters)
{
    if (type.isArray())
    {
        UNIMPLEMENTED();
    }

    if (visit == PreVisit)
    {
        TString constructorName = mStructureHLSL->addConstructor(type, name, parameters);

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

    const TStructure* structure = type.getStruct();
    if (structure)
    {
        out << StructNameString(*structure) + "_ctor(";

        const TFieldList& fields = structure->fields();

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
        size_t size = type.getObjectSize();
        bool writeType = size > 1;

        if (writeType)
        {
            out << TypeString(type) << "(";
        }
        constUnionIterated = WriteConstantUnionArray(out, constUnionIterated, size);
        if (writeType)
        {
            out << ")";
        }
    }

    return constUnionIterated;
}

void OutputHLSL::writeEmulatedFunctionTriplet(TInfoSinkBase &out, Visit visit, const char *preStr)
{
    TString preString = BuiltInFunctionEmulator::GetEmulatedFunctionName(preStr);
    outputTriplet(out, visit, preString.c_str(), ", ", ")");
}

bool OutputHLSL::writeSameSymbolInitializer(TInfoSinkBase &out, TIntermSymbol *symbolNode, TIntermTyped *expression)
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
    return expression->getAsConstantUnion() ||
           expression->isConstructorWithOnlyConstantUnionParameters();
}

bool OutputHLSL::writeConstantInitialization(TInfoSinkBase &out,
                                             TIntermSymbol *symbolNode,
                                             TIntermTyped *expression)
{
    if (canWriteAsHLSLLiteral(expression))
    {
        symbolNode->traverse(this);
        if (expression->getType().isArray())
        {
            out << "[" << expression->getType().getArraySize() << "]";
        }
        out << " = {";
        if (expression->getAsConstantUnion())
        {
            TIntermConstantUnion *nodeConst  = expression->getAsConstantUnion();
            const TConstantUnion *constUnion = nodeConst->getUnionArrayPointer();
            WriteConstantUnionArray(out, constUnion, nodeConst->getType().getObjectSize());
        }
        else
        {
            TIntermAggregate *constructor = expression->getAsAggregate();
            ASSERT(constructor != nullptr);
            for (TIntermNode *&node : *constructor->getSequence())
            {
                TIntermConstantUnion *nodeConst = node->getAsConstantUnion();
                ASSERT(nodeConst);
                const TConstantUnion *constUnion = nodeConst->getUnionArrayPointer();
                WriteConstantUnionArray(out, constUnion, nodeConst->getType().getObjectSize());
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
    function->structure = &structure;
    function->functionName = "angle_eq_" + structNameString;

    TInfoSinkBase fnOut;

    fnOut << "bool " << function->functionName << "(" << structNameString << " a, " << structNameString + " b)\n"
          << "{\n"
             "    return ";

    for (size_t i = 0; i < fields.size(); i++)
    {
        const TField *field = fields[i];
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

    fnOut << ";\n" << "}\n";

    function->functionDefinition = fnOut.c_str();

    mStructEqualityFunctions.push_back(function);
    mEqualityFunctions.push_back(function);

    return function->functionName;
}

TString OutputHLSL::addArrayEqualityFunction(const TType& type)
{
    for (const auto &eqFunction : mArrayEqualityFunctions)
    {
        if (eqFunction->type == type)
        {
            return eqFunction->functionName;
        }
    }

    const TString &typeName = TypeString(type);

    ArrayHelperFunction *function = new ArrayHelperFunction();
    function->type = type;

    TInfoSinkBase fnNameOut;
    fnNameOut << "angle_eq_" << type.getArraySize() << "_" << typeName;
    function->functionName = fnNameOut.c_str();

    TType nonArrayType = type;
    nonArrayType.clearArrayness();

    TInfoSinkBase fnOut;

    fnOut << "bool " << function->functionName << "("
          << typeName << " a[" << type.getArraySize() << "], "
          << typeName << " b[" << type.getArraySize() << "])\n"
          << "{\n"
             "    for (int i = 0; i < " << type.getArraySize() << "; ++i)\n"
             "    {\n"
             "        if (";

    outputEqual(PreVisit, nonArrayType, EOpNotEqual, fnOut);
    fnOut << "a[i]";
    outputEqual(InVisit, nonArrayType, EOpNotEqual, fnOut);
    fnOut << "b[i]";
    outputEqual(PostVisit, nonArrayType, EOpNotEqual, fnOut);

    fnOut << ") { return false; }\n"
             "    }\n"
             "    return true;\n"
             "}\n";

    function->functionDefinition = fnOut.c_str();

    mArrayEqualityFunctions.push_back(function);
    mEqualityFunctions.push_back(function);

    return function->functionName;
}

TString OutputHLSL::addArrayAssignmentFunction(const TType& type)
{
    for (const auto &assignFunction : mArrayAssignmentFunctions)
    {
        if (assignFunction.type == type)
        {
            return assignFunction.functionName;
        }
    }

    const TString &typeName = TypeString(type);

    ArrayHelperFunction function;
    function.type = type;

    TInfoSinkBase fnNameOut;
    fnNameOut << "angle_assign_" << type.getArraySize() << "_" << typeName;
    function.functionName = fnNameOut.c_str();

    TInfoSinkBase fnOut;

    fnOut << "void " << function.functionName << "(out "
        << typeName << " a[" << type.getArraySize() << "], "
        << typeName << " b[" << type.getArraySize() << "])\n"
        << "{\n"
           "    for (int i = 0; i < " << type.getArraySize() << "; ++i)\n"
           "    {\n"
           "        a[i] = b[i];\n"
           "    }\n"
           "}\n";

    function.functionDefinition = fnOut.c_str();

    mArrayAssignmentFunctions.push_back(function);

    return function.functionName;
}

TString OutputHLSL::addArrayConstructIntoFunction(const TType& type)
{
    for (const auto &constructIntoFunction : mArrayConstructIntoFunctions)
    {
        if (constructIntoFunction.type == type)
        {
            return constructIntoFunction.functionName;
        }
    }

    const TString &typeName = TypeString(type);

    ArrayHelperFunction function;
    function.type = type;

    TInfoSinkBase fnNameOut;
    fnNameOut << "angle_construct_into_" << type.getArraySize() << "_" << typeName;
    function.functionName = fnNameOut.c_str();

    TInfoSinkBase fnOut;

    fnOut << "void " << function.functionName << "(out "
          << typeName << " a[" << type.getArraySize() << "]";
    for (unsigned int i = 0u; i < type.getArraySize(); ++i)
    {
        fnOut << ", " << typeName << " b" << i;
    }
    fnOut << ")\n"
             "{\n";

    for (unsigned int i = 0u; i < type.getArraySize(); ++i)
    {
        fnOut << "    a[" << i << "] = b" << i << ";\n";
    }
    fnOut << "}\n";

    function.functionDefinition = fnOut.c_str();

    mArrayConstructIntoFunctions.push_back(function);

    return function.functionName;
}

void OutputHLSL::ensureStructDefined(const TType &type)
{
    TStructure *structure = type.getStruct();

    if (structure)
    {
        mStructureHLSL->addConstructor(type, StructNameString(*structure), nullptr);
    }
}



}
