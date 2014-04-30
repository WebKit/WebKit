//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/OutputHLSL.h"

#include "common/angleutils.h"
#include "common/utilities.h"
#include "common/blocklayout.h"
#include "compiler/translator/compilerdebug.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/DetectDiscontinuity.h"
#include "compiler/translator/SearchSymbol.h"
#include "compiler/translator/UnfoldShortCircuit.h"
#include "compiler/translator/FlagStd140Structs.h"
#include "compiler/translator/NodeSearch.h"
#include "compiler/translator/RewriteElseBlocks.h"

#include <algorithm>
#include <cfloat>
#include <stdio.h>

namespace sh
{

TString OutputHLSL::TextureFunction::name() const
{
    TString name = "gl_texture";

    if (IsSampler2D(sampler))
    {
        name += "2D";
    }
    else if (IsSampler3D(sampler))
    {
        name += "3D";
    }
    else if (IsSamplerCube(sampler))
    {
        name += "Cube";
    }
    else UNREACHABLE();

    if (proj)
    {
        name += "Proj";
    }

    if (offset)
    {
        name += "Offset";
    }

    switch(method)
    {
      case IMPLICIT:                  break;
      case BIAS:                      break;   // Extra parameter makes the signature unique
      case LOD:      name += "Lod";   break;
      case LOD0:     name += "Lod0";  break;
      case LOD0BIAS: name += "Lod0";  break;   // Extra parameter makes the signature unique
      case SIZE:     name += "Size";  break;
      case FETCH:    name += "Fetch"; break;
      case GRAD:     name += "Grad";  break;
      default: UNREACHABLE();
    }

    return name + "(";
}

const char *RegisterPrefix(const TType &type)
{
    if (IsSampler(type.getBasicType()))
    {
        return "s";
    }
    else
    {
        return "c";
    }
}

bool OutputHLSL::TextureFunction::operator<(const TextureFunction &rhs) const
{
    if (sampler < rhs.sampler) return true;
    if (sampler > rhs.sampler) return false;

    if (coords < rhs.coords)   return true;
    if (coords > rhs.coords)   return false;

    if (!proj && rhs.proj)     return true;
    if (proj && !rhs.proj)     return false;

    if (!offset && rhs.offset) return true;
    if (offset && !rhs.offset) return false;

    if (method < rhs.method)   return true;
    if (method > rhs.method)   return false;

    return false;
}

OutputHLSL::OutputHLSL(TParseContext &context, const ShBuiltInResources& resources, ShShaderOutput outputType)
    : TIntermTraverser(true, true, true), mContext(context), mOutputType(outputType)
{
    mUnfoldShortCircuit = new UnfoldShortCircuit(context, this);
    mInsideFunction = false;

    mUsesFragColor = false;
    mUsesFragData = false;
    mUsesDepthRange = false;
    mUsesFragCoord = false;
    mUsesPointCoord = false;
    mUsesFrontFacing = false;
    mUsesPointSize = false;
    mUsesFragDepth = false;
    mUsesXor = false;
    mUsesMod1 = false;
    mUsesMod2v = false;
    mUsesMod2f = false;
    mUsesMod3v = false;
    mUsesMod3f = false;
    mUsesMod4v = false;
    mUsesMod4f = false;
    mUsesFaceforward1 = false;
    mUsesFaceforward2 = false;
    mUsesFaceforward3 = false;
    mUsesFaceforward4 = false;
    mUsesAtan2_1 = false;
    mUsesAtan2_2 = false;
    mUsesAtan2_3 = false;
    mUsesAtan2_4 = false;
    mUsesDiscardRewriting = false;
    mUsesNestedBreak = false;

    mNumRenderTargets = resources.EXT_draw_buffers ? resources.MaxDrawBuffers : 1;

    mScopeDepth = 0;

    mUniqueIndex = 0;

    mContainsLoopDiscontinuity = false;
    mOutputLod0Function = false;
    mInsideDiscontinuousLoop = false;
    mNestedLoopDepth = 0;

    mExcessiveLoopIndex = NULL;

    if (mOutputType == SH_HLSL9_OUTPUT)
    {
        if (mContext.shaderType == SH_FRAGMENT_SHADER)
        {
            mUniformRegister = 3;   // Reserve registers for dx_DepthRange, dx_ViewCoords and dx_DepthFront
        }
        else
        {
            mUniformRegister = 2;   // Reserve registers for dx_DepthRange and dx_ViewAdjust
        }
    }
    else
    {
        mUniformRegister = 0;
    }

    mSamplerRegister = 0;
    mInterfaceBlockRegister = 2; // Reserve registers for the default uniform block and driver constants
    mPaddingCounter = 0;
}

OutputHLSL::~OutputHLSL()
{
    delete mUnfoldShortCircuit;
}

void OutputHLSL::output()
{
    mContainsLoopDiscontinuity = mContext.shaderType == SH_FRAGMENT_SHADER && containsLoopDiscontinuity(mContext.treeRoot);
    const std::vector<TIntermTyped*> &flaggedStructs = FlagStd140ValueStructs(mContext.treeRoot);
    makeFlaggedStructMaps(flaggedStructs);

    // Work around D3D9 bug that would manifest in vertex shaders with selection blocks which
    // use a vertex attribute as a condition, and some related computation in the else block.
    if (mOutputType == SH_HLSL9_OUTPUT && mContext.shaderType == SH_VERTEX_SHADER)
    {
        RewriteElseBlocks(mContext.treeRoot);
    }

    mContext.treeRoot->traverse(this);   // Output the body first to determine what has to go in the header
    header();

    mContext.infoSink().obj << mHeader.c_str();
    mContext.infoSink().obj << mBody.c_str();
}

void OutputHLSL::makeFlaggedStructMaps(const std::vector<TIntermTyped *> &flaggedStructs)
{
    for (unsigned int structIndex = 0; structIndex < flaggedStructs.size(); structIndex++)
    {
        TIntermTyped *flaggedNode = flaggedStructs[structIndex];

        // This will mark the necessary block elements as referenced
        flaggedNode->traverse(this);
        TString structName(mBody.c_str());
        mBody.erase();

        mFlaggedStructOriginalNames[flaggedNode] = structName;

        for (size_t pos = structName.find('.'); pos != std::string::npos; pos = structName.find('.'))
        {
            structName.erase(pos, 1);
        }

        mFlaggedStructMappedNames[flaggedNode] = "map" + structName;
    }
}

TInfoSinkBase &OutputHLSL::getBodyStream()
{
    return mBody;
}

const std::vector<gl::Uniform> &OutputHLSL::getUniforms()
{
    return mActiveUniforms;
}

const std::vector<gl::InterfaceBlock> &OutputHLSL::getInterfaceBlocks() const
{
    return mActiveInterfaceBlocks;
}

const std::vector<gl::Attribute> &OutputHLSL::getOutputVariables() const
{
    return mActiveOutputVariables;
}

const std::vector<gl::Attribute> &OutputHLSL::getAttributes() const
{
    return mActiveAttributes;
}

const std::vector<gl::Varying> &OutputHLSL::getVaryings() const
{
    return mActiveVaryings;
}

int OutputHLSL::vectorSize(const TType &type) const
{
    int elementSize = type.isMatrix() ? type.getCols() : 1;
    int arraySize = type.isArray() ? type.getArraySize() : 1;

    return elementSize * arraySize;
}

TString OutputHLSL::interfaceBlockFieldString(const TInterfaceBlock &interfaceBlock, const TField &field)
{
    if (interfaceBlock.hasInstanceName())
    {
        return interfaceBlock.name() + "." + field.name();
    }
    else
    {
        return field.name();
    }
}

TString OutputHLSL::decoratePrivate(const TString &privateText)
{
    return "dx_" + privateText;
}

TString OutputHLSL::interfaceBlockStructNameString(const TInterfaceBlock &interfaceBlock)
{
    return decoratePrivate(interfaceBlock.name()) + "_type";
}

TString OutputHLSL::interfaceBlockInstanceString(const TInterfaceBlock& interfaceBlock, unsigned int arrayIndex)
{
    if (!interfaceBlock.hasInstanceName())
    {
        return "";
    }
    else if (interfaceBlock.isArray())
    {
        return decoratePrivate(interfaceBlock.instanceName()) + "_" + str(arrayIndex);
    }
    else
    {
        return decorate(interfaceBlock.instanceName());
    }
}

TString OutputHLSL::interfaceBlockFieldTypeString(const TField &field, TLayoutBlockStorage blockStorage)
{
    const TType &fieldType = *field.type();
    const TLayoutMatrixPacking matrixPacking = fieldType.getLayoutQualifier().matrixPacking;
    ASSERT(matrixPacking != EmpUnspecified);

    if (fieldType.isMatrix())
    {
        // Use HLSL row-major packing for GLSL column-major matrices
        const TString &matrixPackString = (matrixPacking == EmpRowMajor ? "column_major" : "row_major");
        return matrixPackString + " " + typeString(fieldType);
    }
    else if (fieldType.getStruct())
    {
        // Use HLSL row-major packing for GLSL column-major matrices
        return structureTypeName(*fieldType.getStruct(), matrixPacking == EmpColumnMajor, blockStorage == EbsStd140);
    }
    else
    {
        return typeString(fieldType);
    }
}

TString OutputHLSL::interfaceBlockFieldString(const TInterfaceBlock &interfaceBlock, TLayoutBlockStorage blockStorage)
{
    TString hlsl;

    int elementIndex = 0;

    for (unsigned int typeIndex = 0; typeIndex < interfaceBlock.fields().size(); typeIndex++)
    {
        const TField &field = *interfaceBlock.fields()[typeIndex];
        const TType &fieldType = *field.type();

        if (blockStorage == EbsStd140)
        {
            // 2 and 3 component vector types in some cases need pre-padding
            hlsl += std140PrePaddingString(fieldType, &elementIndex);
        }

        hlsl += "    " + interfaceBlockFieldTypeString(field, blockStorage) +
                " " + decorate(field.name()) + arrayString(fieldType) + ";\n";

        // must pad out after matrices and arrays, where HLSL usually allows itself room to pack stuff
        if (blockStorage == EbsStd140)
        {
            const bool useHLSLRowMajorPacking = (fieldType.getLayoutQualifier().matrixPacking == EmpColumnMajor);
            hlsl += std140PostPaddingString(fieldType, useHLSLRowMajorPacking);
        }
    }

    return hlsl;
}

TString OutputHLSL::interfaceBlockStructString(const TInterfaceBlock &interfaceBlock)
{
    const TLayoutBlockStorage blockStorage = interfaceBlock.blockStorage();

    return "struct " + interfaceBlockStructNameString(interfaceBlock) + "\n"
           "{\n" +
           interfaceBlockFieldString(interfaceBlock, blockStorage) +
           "};\n\n";
}

TString OutputHLSL::interfaceBlockString(const TInterfaceBlock &interfaceBlock, unsigned int registerIndex, unsigned int arrayIndex)
{
    const TString &arrayIndexString =  (arrayIndex != GL_INVALID_INDEX ? decorate(str(arrayIndex)) : "");
    const TString &blockName = interfaceBlock.name() + arrayIndexString;
    TString hlsl;

    hlsl += "cbuffer " + blockName + " : register(b" + str(registerIndex) + ")\n"
            "{\n";

    if (interfaceBlock.hasInstanceName())
    {
        hlsl += "    " + interfaceBlockStructNameString(interfaceBlock) + " " + interfaceBlockInstanceString(interfaceBlock, arrayIndex) + ";\n";
    }
    else
    {
        const TLayoutBlockStorage blockStorage = interfaceBlock.blockStorage();
        hlsl += interfaceBlockFieldString(interfaceBlock, blockStorage);
    }

    hlsl += "};\n\n";

    return hlsl;
}

TString OutputHLSL::std140PrePaddingString(const TType &type, int *elementIndex)
{
    if (type.getBasicType() == EbtStruct || type.isMatrix() || type.isArray())
    {
        // no padding needed, HLSL will align the field to a new register
        *elementIndex = 0;
        return "";
    }

    const GLenum glType = glVariableType(type);
    const int numComponents = gl::UniformComponentCount(glType);

    if (numComponents >= 4)
    {
        // no padding needed, HLSL will align the field to a new register
        *elementIndex = 0;
        return "";
    }

    if (*elementIndex + numComponents > 4)
    {
        // no padding needed, HLSL will align the field to a new register
        *elementIndex = numComponents;
        return "";
    }

    TString padding;

    const int alignment = numComponents == 3 ? 4 : numComponents;
    const int paddingOffset = (*elementIndex % alignment);

    if (paddingOffset != 0)
    {
        // padding is neccessary
        for (int paddingIndex = paddingOffset; paddingIndex < alignment; paddingIndex++)
        {
            padding += "    float pad_" + str(mPaddingCounter++) + ";\n";
        }

        *elementIndex += (alignment - paddingOffset);
    }

    *elementIndex += numComponents;
    *elementIndex %= 4;

    return padding;
}

TString OutputHLSL::std140PostPaddingString(const TType &type, bool useHLSLRowMajorPacking)
{
    if (!type.isMatrix() && !type.isArray() && type.getBasicType() != EbtStruct)
    {
        return "";
    }

    int numComponents = 0;

    if (type.isMatrix())
    {
        // This method can also be called from structureString, which does not use layout qualifiers.
        // Thus, use the method parameter for determining the matrix packing.
        //
        // Note HLSL row major packing corresponds to GL API column-major, and vice-versa, since we
        // wish to always transpose GL matrices to play well with HLSL's matrix array indexing.
        //
        const bool isRowMajorMatrix = !useHLSLRowMajorPacking;
        const GLenum glType = glVariableType(type);
        numComponents = gl::MatrixComponentCount(glType, isRowMajorMatrix);
    }
    else if (type.getStruct())
    {
        const TString &structName = structureTypeName(*type.getStruct(), useHLSLRowMajorPacking, true);
        numComponents = mStd140StructElementIndexes[structName];

        if (numComponents == 0)
        {
            return "";
        }
    }
    else
    {
        const GLenum glType = glVariableType(type);
        numComponents = gl::UniformComponentCount(glType);
    }

    TString padding;
    for (int paddingOffset = numComponents; paddingOffset < 4; paddingOffset++)
    {
        padding += "    float pad_" + str(mPaddingCounter++) + ";\n";
    }
    return padding;
}

// Use the same layout for packed and shared
void setBlockLayout(gl::InterfaceBlock *interfaceBlock, gl::BlockLayoutType newLayout)
{
    interfaceBlock->layout = newLayout;
    interfaceBlock->blockInfo.clear();

    switch (newLayout)
    {
      case gl::BLOCKLAYOUT_SHARED:
      case gl::BLOCKLAYOUT_PACKED:
        {
            gl::HLSLBlockEncoder hlslEncoder(&interfaceBlock->blockInfo);
            hlslEncoder.encodeInterfaceBlockFields(interfaceBlock->fields);
            interfaceBlock->dataSize = hlslEncoder.getBlockSize();
        }
        break;

      case gl::BLOCKLAYOUT_STANDARD:
        {
            gl::Std140BlockEncoder stdEncoder(&interfaceBlock->blockInfo);
            stdEncoder.encodeInterfaceBlockFields(interfaceBlock->fields);
            interfaceBlock->dataSize = stdEncoder.getBlockSize();
        }
        break;

      default:
        UNREACHABLE();
        break;
    }
}

gl::BlockLayoutType convertBlockLayoutType(TLayoutBlockStorage blockStorage)
{
    switch (blockStorage)
    {
      case EbsPacked: return gl::BLOCKLAYOUT_PACKED;
      case EbsShared: return gl::BLOCKLAYOUT_SHARED;
      case EbsStd140: return gl::BLOCKLAYOUT_STANDARD;
      default: UNREACHABLE(); return gl::BLOCKLAYOUT_SHARED;
    }
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
        const TString &fieldName = rhsStructName + "." + decorate(field.name());
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

void OutputHLSL::header()
{
    TInfoSinkBase &out = mHeader;

    TString uniforms;
    TString interfaceBlocks;
    TString varyings;
    TString attributes;
    TString flaggedStructs;

    for (ReferencedSymbols::const_iterator uniformIt = mReferencedUniforms.begin(); uniformIt != mReferencedUniforms.end(); uniformIt++)
    {
        const TIntermSymbol &uniform = *uniformIt->second;
        const TType &type = uniform.getType();
        const TString &name = uniform.getSymbol();

        int registerIndex = declareUniformAndAssignRegister(type, name);

        if (mOutputType == SH_HLSL11_OUTPUT && IsSampler(type.getBasicType()))   // Also declare the texture
        {
            uniforms += "uniform " + samplerString(type) + " sampler_" + decorateUniform(name, type) + arrayString(type) + 
                        " : register(s" + str(registerIndex) + ");\n";

            uniforms += "uniform " + textureString(type) + " texture_" + decorateUniform(name, type) + arrayString(type) +
                        " : register(t" + str(registerIndex) + ");\n";
        }
        else
        {
            const TStructure *structure = type.getStruct();
            const TString &typeName = (structure ? structureTypeName(*structure, false, false) : typeString(type));

            const TString &registerString = TString("register(") + RegisterPrefix(type) + str(registerIndex) + ")";

            uniforms += "uniform " + typeName + " " + decorateUniform(name, type) + arrayString(type) + " : " + registerString + ";\n";
        }
    }

    for (ReferencedSymbols::const_iterator interfaceBlockIt = mReferencedInterfaceBlocks.begin(); interfaceBlockIt != mReferencedInterfaceBlocks.end(); interfaceBlockIt++)
    {
        const TType &nodeType = interfaceBlockIt->second->getType();
        const TInterfaceBlock &interfaceBlock = *nodeType.getInterfaceBlock();
        const TFieldList &fieldList = interfaceBlock.fields();

        unsigned int arraySize = static_cast<unsigned int>(interfaceBlock.arraySize());
        gl::InterfaceBlock activeBlock(interfaceBlock.name().c_str(), arraySize, mInterfaceBlockRegister);
        for (unsigned int typeIndex = 0; typeIndex < fieldList.size(); typeIndex++)
        {
            const TField &field = *fieldList[typeIndex];
            const TString &fullUniformName = interfaceBlockFieldString(interfaceBlock, field);
            declareInterfaceBlockField(*field.type(), fullUniformName, activeBlock.fields);
        }

        mInterfaceBlockRegister += std::max(1u, arraySize);

        gl::BlockLayoutType blockLayoutType = convertBlockLayoutType(interfaceBlock.blockStorage());
        setBlockLayout(&activeBlock, blockLayoutType);

        if (interfaceBlock.matrixPacking() == EmpRowMajor)
        {
            activeBlock.isRowMajorLayout = true;
        }

        mActiveInterfaceBlocks.push_back(activeBlock);

        if (interfaceBlock.hasInstanceName())
        {
            interfaceBlocks += interfaceBlockStructString(interfaceBlock);
        }

        if (arraySize > 0)
        {
            for (unsigned int arrayIndex = 0; arrayIndex < arraySize; arrayIndex++)
            {
                interfaceBlocks += interfaceBlockString(interfaceBlock, activeBlock.registerIndex + arrayIndex, arrayIndex);
            }
        }
        else
        {
            interfaceBlocks += interfaceBlockString(interfaceBlock, activeBlock.registerIndex, GL_INVALID_INDEX);
        }
    }

    for (std::map<TIntermTyped*, TString>::const_iterator flaggedStructIt = mFlaggedStructMappedNames.begin(); flaggedStructIt != mFlaggedStructMappedNames.end(); flaggedStructIt++)
    {
        TIntermTyped *structNode = flaggedStructIt->first;
        const TString &mappedName = flaggedStructIt->second;
        const TStructure &structure = *structNode->getType().getStruct();
        const TString &originalName = mFlaggedStructOriginalNames[structNode];

        flaggedStructs += "static " + decorate(structure.name()) + " " + mappedName + " =\n";
        flaggedStructs += structInitializerString(0, structure, originalName);
        flaggedStructs += "\n";
    }

    for (ReferencedSymbols::const_iterator varying = mReferencedVaryings.begin(); varying != mReferencedVaryings.end(); varying++)
    {
        const TType &type = varying->second->getType();
        const TString &name = varying->second->getSymbol();

        // Program linking depends on this exact format
        varyings += "static " + interpolationString(type.getQualifier()) + " " + typeString(type) + " " +
                    decorate(name) + arrayString(type) + " = " + initializer(type) + ";\n";

        declareVaryingToList(type, type.getQualifier(), name, mActiveVaryings);
    }

    for (ReferencedSymbols::const_iterator attribute = mReferencedAttributes.begin(); attribute != mReferencedAttributes.end(); attribute++)
    {
        const TType &type = attribute->second->getType();
        const TString &name = attribute->second->getSymbol();

        attributes += "static " + typeString(type) + " " + decorate(name) + arrayString(type) + " = " + initializer(type) + ";\n";

        gl::Attribute attributeVar(glVariableType(type), glVariablePrecision(type), name.c_str(),
                               (unsigned int)type.getArraySize(), type.getLayoutQualifier().location);
        mActiveAttributes.push_back(attributeVar);
    }

    for (StructDeclarations::iterator structDeclaration = mStructDeclarations.begin(); structDeclaration != mStructDeclarations.end(); structDeclaration++)
    {
        out << *structDeclaration;
    }

    for (Constructors::iterator constructor = mConstructors.begin(); constructor != mConstructors.end(); constructor++)
    {
        out << *constructor;
    }

    if (mUsesDiscardRewriting)
    {
        out << "#define ANGLE_USES_DISCARD_REWRITING" << "\n";
    }

    if (mUsesNestedBreak)
    {
        out << "#define ANGLE_USES_NESTED_BREAK" << "\n";
    }

    if (mContext.shaderType == SH_FRAGMENT_SHADER)
    {
        TExtensionBehavior::const_iterator iter = mContext.extensionBehavior().find("GL_EXT_draw_buffers");
        const bool usingMRTExtension = (iter != mContext.extensionBehavior().end() && (iter->second == EBhEnable || iter->second == EBhRequire));

        out << "// Varyings\n";
        out <<  varyings;
        out << "\n";

        if (mContext.getShaderVersion() >= 300)
        {
            for (ReferencedSymbols::const_iterator outputVariableIt = mReferencedOutputVariables.begin(); outputVariableIt != mReferencedOutputVariables.end(); outputVariableIt++)
            {
                const TString &variableName = outputVariableIt->first;
                const TType &variableType = outputVariableIt->second->getType();
                const TLayoutQualifier &layoutQualifier = variableType.getLayoutQualifier();

                out << "static " + typeString(variableType) + " out_" + variableName + arrayString(variableType) +
                       " = " + initializer(variableType) + ";\n";

                gl::Attribute outputVar(glVariableType(variableType), glVariablePrecision(variableType), variableName.c_str(),
                                    (unsigned int)variableType.getArraySize(), layoutQualifier.location);
                mActiveOutputVariables.push_back(outputVar);
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

        if (mOutputType == SH_HLSL11_OUTPUT)
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
        
        out <<  uniforms;
        out << "\n";

        if (!interfaceBlocks.empty())
        {
            out << interfaceBlocks;
            out << "\n";

            if (!flaggedStructs.empty())
            {
                out << "// Std140 Structures accessed by value\n";
                out << "\n";
                out << flaggedStructs;
                out << "\n";
            }
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

        if (mOutputType == SH_HLSL11_OUTPUT)
        {
            if (mUsesDepthRange)
            {
                out << "cbuffer DriverConstants : register(b1)\n"
                       "{\n"
                       "    float3 dx_DepthRange : packoffset(c0);\n"
                       "};\n"
                       "\n";
            }
        }
        else
        {
            if (mUsesDepthRange)
            {
                out << "uniform float3 dx_DepthRange : register(c0);\n";
            }

            out << "uniform float4 dx_ViewAdjust : register(c1);\n"
                   "\n";
        }

        if (mUsesDepthRange)
        {
            out << "static gl_DepthRangeParameters gl_DepthRange = {dx_DepthRange.x, dx_DepthRange.y, dx_DepthRange.z};\n"
                   "\n";
        }

        out << uniforms;
        out << "\n";
        
        if (!interfaceBlocks.empty())
        {
            out << interfaceBlocks;
            out << "\n";

            if (!flaggedStructs.empty())
            {
                out << "// Std140 Structures accessed by value\n";
                out << "\n";
                out << flaggedStructs;
                out << "\n";
            }
        }
    }

    for (TextureFunctionSet::const_iterator textureFunction = mUsesTexture.begin(); textureFunction != mUsesTexture.end(); textureFunction++)
    {
        // Return type
        if (textureFunction->method == TextureFunction::SIZE)
        {
            switch(textureFunction->sampler)
            {
              case EbtSampler2D:            out << "int2 "; break;
              case EbtSampler3D:            out << "int3 "; break;
              case EbtSamplerCube:          out << "int2 "; break;
              case EbtSampler2DArray:       out << "int3 "; break;
              case EbtISampler2D:           out << "int2 "; break;
              case EbtISampler3D:           out << "int3 "; break;
              case EbtISamplerCube:         out << "int2 "; break;
              case EbtISampler2DArray:      out << "int3 "; break;
              case EbtUSampler2D:           out << "int2 "; break;
              case EbtUSampler3D:           out << "int3 "; break;
              case EbtUSamplerCube:         out << "int2 "; break;
              case EbtUSampler2DArray:      out << "int3 "; break;
              case EbtSampler2DShadow:      out << "int2 "; break;
              case EbtSamplerCubeShadow:    out << "int2 "; break;
              case EbtSampler2DArrayShadow: out << "int3 "; break;
              default: UNREACHABLE();
            }
        }
        else   // Sampling function
        {
            switch(textureFunction->sampler)
            {
              case EbtSampler2D:            out << "float4 "; break;
              case EbtSampler3D:            out << "float4 "; break;
              case EbtSamplerCube:          out << "float4 "; break;
              case EbtSampler2DArray:       out << "float4 "; break;
              case EbtISampler2D:           out << "int4 ";   break;
              case EbtISampler3D:           out << "int4 ";   break;
              case EbtISamplerCube:         out << "int4 ";   break;
              case EbtISampler2DArray:      out << "int4 ";   break;
              case EbtUSampler2D:           out << "uint4 ";  break;
              case EbtUSampler3D:           out << "uint4 ";  break;
              case EbtUSamplerCube:         out << "uint4 ";  break;
              case EbtUSampler2DArray:      out << "uint4 ";  break;
              case EbtSampler2DShadow:      out << "float ";  break;
              case EbtSamplerCubeShadow:    out << "float ";  break;
              case EbtSampler2DArrayShadow: out << "float ";  break;
              default: UNREACHABLE();
            }
        }

        // Function name
        out << textureFunction->name();

        // Argument list
        int hlslCoords = 4;

        if (mOutputType == SH_HLSL9_OUTPUT)
        {
            switch(textureFunction->sampler)
            {
              case EbtSampler2D:   out << "sampler2D s";   hlslCoords = 2; break;
              case EbtSamplerCube: out << "samplerCUBE s"; hlslCoords = 3; break;
              default: UNREACHABLE();
            }

            switch(textureFunction->method)
            {
              case TextureFunction::IMPLICIT:                 break;
              case TextureFunction::BIAS:     hlslCoords = 4; break;
              case TextureFunction::LOD:      hlslCoords = 4; break;
              case TextureFunction::LOD0:     hlslCoords = 4; break;
              case TextureFunction::LOD0BIAS: hlslCoords = 4; break;
              default: UNREACHABLE();
            }
        }
        else if (mOutputType == SH_HLSL11_OUTPUT)
        {
            switch(textureFunction->sampler)
            {
              case EbtSampler2D:            out << "Texture2D x, SamplerState s";                hlslCoords = 2; break;
              case EbtSampler3D:            out << "Texture3D x, SamplerState s";                hlslCoords = 3; break;
              case EbtSamplerCube:          out << "TextureCube x, SamplerState s";              hlslCoords = 3; break;
              case EbtSampler2DArray:       out << "Texture2DArray x, SamplerState s";           hlslCoords = 3; break;
              case EbtISampler2D:           out << "Texture2D<int4> x, SamplerState s";          hlslCoords = 2; break;
              case EbtISampler3D:           out << "Texture3D<int4> x, SamplerState s";          hlslCoords = 3; break;
              case EbtISamplerCube:         out << "Texture2DArray<int4> x, SamplerState s";     hlslCoords = 3; break;
              case EbtISampler2DArray:      out << "Texture2DArray<int4> x, SamplerState s";     hlslCoords = 3; break;
              case EbtUSampler2D:           out << "Texture2D<uint4> x, SamplerState s";         hlslCoords = 2; break;
              case EbtUSampler3D:           out << "Texture3D<uint4> x, SamplerState s";         hlslCoords = 3; break;
              case EbtUSamplerCube:         out << "Texture2DArray<uint4> x, SamplerState s";    hlslCoords = 3; break;
              case EbtUSampler2DArray:      out << "Texture2DArray<uint4> x, SamplerState s";    hlslCoords = 3; break;
              case EbtSampler2DShadow:      out << "Texture2D x, SamplerComparisonState s";      hlslCoords = 2; break;
              case EbtSamplerCubeShadow:    out << "TextureCube x, SamplerComparisonState s";    hlslCoords = 3; break;
              case EbtSampler2DArrayShadow: out << "Texture2DArray x, SamplerComparisonState s"; hlslCoords = 3; break;
              default: UNREACHABLE();
            }
        }
        else UNREACHABLE();

        if (textureFunction->method == TextureFunction::FETCH)   // Integer coordinates
        {
            switch(textureFunction->coords)
            {
              case 2: out << ", int2 t"; break;
              case 3: out << ", int3 t"; break;
              default: UNREACHABLE();
            }
        }
        else   // Floating-point coordinates (except textureSize)
        {
            switch(textureFunction->coords)
            {
              case 1: out << ", int lod";  break;   // textureSize()
              case 2: out << ", float2 t"; break;
              case 3: out << ", float3 t"; break;
              case 4: out << ", float4 t"; break;
              default: UNREACHABLE();
            }
        }

        if (textureFunction->method == TextureFunction::GRAD)
        {
            switch(textureFunction->sampler)
            {
              case EbtSampler2D:
              case EbtISampler2D:
              case EbtUSampler2D:
              case EbtSampler2DArray:
              case EbtISampler2DArray:
              case EbtUSampler2DArray:
              case EbtSampler2DShadow:
              case EbtSampler2DArrayShadow:
                out << ", float2 ddx, float2 ddy";
                break;
              case EbtSampler3D:
              case EbtISampler3D:
              case EbtUSampler3D:
              case EbtSamplerCube:
              case EbtISamplerCube:
              case EbtUSamplerCube:
              case EbtSamplerCubeShadow:
                out << ", float3 ddx, float3 ddy";
                break;
              default: UNREACHABLE();
            }
        }

        switch(textureFunction->method)
        {
          case TextureFunction::IMPLICIT:                        break;
          case TextureFunction::BIAS:                            break;   // Comes after the offset parameter
          case TextureFunction::LOD:      out << ", float lod";  break;
          case TextureFunction::LOD0:                            break;
          case TextureFunction::LOD0BIAS:                        break;   // Comes after the offset parameter
          case TextureFunction::SIZE:                            break;
          case TextureFunction::FETCH:    out << ", int mip";    break;
          case TextureFunction::GRAD:                            break;
          default: UNREACHABLE();
        }

        if (textureFunction->offset)
        {
            switch(textureFunction->sampler)
            {
              case EbtSampler2D:            out << ", int2 offset"; break;
              case EbtSampler3D:            out << ", int3 offset"; break;
              case EbtSampler2DArray:       out << ", int2 offset"; break;
              case EbtISampler2D:           out << ", int2 offset"; break;
              case EbtISampler3D:           out << ", int3 offset"; break;
              case EbtISampler2DArray:      out << ", int2 offset"; break;
              case EbtUSampler2D:           out << ", int2 offset"; break;
              case EbtUSampler3D:           out << ", int3 offset"; break;
              case EbtUSampler2DArray:      out << ", int2 offset"; break;
              case EbtSampler2DShadow:      out << ", int2 offset"; break;
              case EbtSampler2DArrayShadow: out << ", int2 offset"; break;
              default: UNREACHABLE();
            }
        }

        if (textureFunction->method == TextureFunction::BIAS ||
            textureFunction->method == TextureFunction::LOD0BIAS)
        {
            out << ", float bias";
        }

        out << ")\n"
               "{\n";

        if (textureFunction->method == TextureFunction::SIZE)
        {
            if (IsSampler2D(textureFunction->sampler) || IsSamplerCube(textureFunction->sampler))
            {
                if (IsSamplerArray(textureFunction->sampler))
                {
                    out << "    uint width; uint height; uint layers; uint numberOfLevels;\n"
                           "    x.GetDimensions(lod, width, height, layers, numberOfLevels);\n";
                }
                else
                {
                    out << "    uint width; uint height; uint numberOfLevels;\n"
                           "    x.GetDimensions(lod, width, height, numberOfLevels);\n";
                }
            }
            else if (IsSampler3D(textureFunction->sampler))
            {
                out << "    uint width; uint height; uint depth; uint numberOfLevels;\n"
                       "    x.GetDimensions(lod, width, height, depth, numberOfLevels);\n";
            }
            else UNREACHABLE();

            switch(textureFunction->sampler)
            {
              case EbtSampler2D:            out << "    return int2(width, height);";         break;
              case EbtSampler3D:            out << "    return int3(width, height, depth);";  break;
              case EbtSamplerCube:          out << "    return int2(width, height);";         break;
              case EbtSampler2DArray:       out << "    return int3(width, height, layers);"; break;
              case EbtISampler2D:           out << "    return int2(width, height);";         break;
              case EbtISampler3D:           out << "    return int3(width, height, depth);";  break;
              case EbtISamplerCube:         out << "    return int2(width, height);";         break;
              case EbtISampler2DArray:      out << "    return int3(width, height, layers);"; break;
              case EbtUSampler2D:           out << "    return int2(width, height);";         break;
              case EbtUSampler3D:           out << "    return int3(width, height, depth);";  break;
              case EbtUSamplerCube:         out << "    return int2(width, height);";         break;
              case EbtUSampler2DArray:      out << "    return int3(width, height, layers);"; break;
              case EbtSampler2DShadow:      out << "    return int2(width, height);";         break;
              case EbtSamplerCubeShadow:    out << "    return int2(width, height);";         break;
              case EbtSampler2DArrayShadow: out << "    return int3(width, height, layers);"; break;
              default: UNREACHABLE();
            }
        }
        else
        {
            if (IsIntegerSampler(textureFunction->sampler) && IsSamplerCube(textureFunction->sampler))
            {
                out << "    float width; float height; float layers; float levels;\n";

                out << "    uint mip = 0;\n";

                out << "    x.GetDimensions(mip, width, height, layers, levels);\n";

                out << "    bool xMajor = abs(t.x) > abs(t.y) && abs(t.x) > abs(t.z);\n";
                out << "    bool yMajor = abs(t.y) > abs(t.z) && abs(t.y) > abs(t.x);\n";
                out << "    bool zMajor = abs(t.z) > abs(t.x) && abs(t.z) > abs(t.y);\n";
                out << "    bool negative = (xMajor && t.x < 0.0f) || (yMajor && t.y < 0.0f) || (zMajor && t.z < 0.0f);\n";

                // FACE_POSITIVE_X = 000b
                // FACE_NEGATIVE_X = 001b
                // FACE_POSITIVE_Y = 010b
                // FACE_NEGATIVE_Y = 011b
                // FACE_POSITIVE_Z = 100b
                // FACE_NEGATIVE_Z = 101b
                out << "    int face = (int)negative + (int)yMajor * 2 + (int)zMajor * 4;\n";

                out << "    float u = xMajor ? -t.z : (yMajor && t.y < 0.0f ? -t.x : t.x);\n";
                out << "    float v = yMajor ? t.z : (negative ? t.y : -t.y);\n";
                out << "    float m = xMajor ? t.x : (yMajor ? t.y : t.z);\n";

                out << "    t.x = (u * 0.5f / m) + 0.5f;\n";
                out << "    t.y = (v * 0.5f / m) + 0.5f;\n";
            }
            else if (IsIntegerSampler(textureFunction->sampler) &&
                     textureFunction->method != TextureFunction::FETCH)
            {
                if (IsSampler2D(textureFunction->sampler))
                {
                    if (IsSamplerArray(textureFunction->sampler))
                    {
                        out << "    float width; float height; float layers; float levels;\n";

                        if (textureFunction->method == TextureFunction::LOD0)
                        {
                            out << "    uint mip = 0;\n";
                        }
                        else if (textureFunction->method == TextureFunction::LOD0BIAS)
                        {
                            out << "    uint mip = bias;\n";
                        }
                        else
                        {
                            if (textureFunction->method == TextureFunction::IMPLICIT ||
                                textureFunction->method == TextureFunction::BIAS)
                            {
                                out << "    x.GetDimensions(0, width, height, layers, levels);\n"
                                       "    float2 tSized = float2(t.x * width, t.y * height);\n"
                                       "    float dx = length(ddx(tSized));\n"
                                       "    float dy = length(ddy(tSized));\n"
                                       "    float lod = log2(max(dx, dy));\n";

                                if (textureFunction->method == TextureFunction::BIAS)
                                {
                                    out << "    lod += bias;\n";
                                }
                            }
                            else if (textureFunction->method == TextureFunction::GRAD)
                            {
                                out << "    x.GetDimensions(0, width, height, layers, levels);\n"
                                       "    float lod = log2(max(length(ddx), length(ddy)));\n";
                            }

                            out << "    uint mip = uint(min(max(round(lod), 0), levels - 1));\n";
                        }

                        out << "    x.GetDimensions(mip, width, height, layers, levels);\n";
                    }
                    else
                    {
                        out << "    float width; float height; float levels;\n";

                        if (textureFunction->method == TextureFunction::LOD0)
                        {
                            out << "    uint mip = 0;\n";
                        }
                        else if (textureFunction->method == TextureFunction::LOD0BIAS)
                        {
                            out << "    uint mip = bias;\n";
                        }
                        else
                        {
                            if (textureFunction->method == TextureFunction::IMPLICIT ||
                                textureFunction->method == TextureFunction::BIAS)
                            {
                                out << "    x.GetDimensions(0, width, height, levels);\n"
                                       "    float2 tSized = float2(t.x * width, t.y * height);\n"
                                       "    float dx = length(ddx(tSized));\n"
                                       "    float dy = length(ddy(tSized));\n"
                                       "    float lod = log2(max(dx, dy));\n";

                                if (textureFunction->method == TextureFunction::BIAS)
                                {
                                    out << "    lod += bias;\n";
                                }
                            }
                            else if (textureFunction->method == TextureFunction::LOD)
                            {
                                out << "    x.GetDimensions(0, width, height, levels);\n";
                            }
                            else if (textureFunction->method == TextureFunction::GRAD)
                            {
                                out << "    x.GetDimensions(0, width, height, levels);\n"
                                       "    float lod = log2(max(length(ddx), length(ddy)));\n";
                            }

                            out << "    uint mip = uint(min(max(round(lod), 0), levels - 1));\n";
                        }

                        out << "    x.GetDimensions(mip, width, height, levels);\n";
                    }
                }
                else if (IsSampler3D(textureFunction->sampler))
                {
                    out << "    float width; float height; float depth; float levels;\n";

                    if (textureFunction->method == TextureFunction::LOD0)
                    {
                        out << "    uint mip = 0;\n";
                    }
                    else if (textureFunction->method == TextureFunction::LOD0BIAS)
                    {
                        out << "    uint mip = bias;\n";
                    }
                    else
                    {
                        if (textureFunction->method == TextureFunction::IMPLICIT ||
                            textureFunction->method == TextureFunction::BIAS)
                        {
                            out << "    x.GetDimensions(0, width, height, depth, levels);\n"
                                   "    float3 tSized = float3(t.x * width, t.y * height, t.z * depth);\n"
                                   "    float dx = length(ddx(tSized));\n"
                                   "    float dy = length(ddy(tSized));\n"
                                   "    float lod = log2(max(dx, dy));\n";

                            if (textureFunction->method == TextureFunction::BIAS)
                            {
                                out << "    lod += bias;\n";
                            }
                        }
                        else if (textureFunction->method == TextureFunction::GRAD)
                        {
                            out << "    x.GetDimensions(0, width, height, depth, levels);\n"
                                   "    float lod = log2(max(length(ddx), length(ddy)));\n";
                        }

                        out << "    uint mip = uint(min(max(round(lod), 0), levels - 1));\n";
                    }

                    out << "    x.GetDimensions(mip, width, height, depth, levels);\n";
                }
                else UNREACHABLE();
            }

            out << "    return ";

            // HLSL intrinsic
            if (mOutputType == SH_HLSL9_OUTPUT)
            {
                switch(textureFunction->sampler)
                {
                  case EbtSampler2D:   out << "tex2D";   break;
                  case EbtSamplerCube: out << "texCUBE"; break;
                  default: UNREACHABLE();
                }

                switch(textureFunction->method)
                {
                  case TextureFunction::IMPLICIT: out << "(s, ";     break;
                  case TextureFunction::BIAS:     out << "bias(s, "; break;
                  case TextureFunction::LOD:      out << "lod(s, ";  break;
                  case TextureFunction::LOD0:     out << "lod(s, ";  break;
                  case TextureFunction::LOD0BIAS: out << "lod(s, ";  break;
                  default: UNREACHABLE();
                }
            }
            else if (mOutputType == SH_HLSL11_OUTPUT)
            {
                if (textureFunction->method == TextureFunction::GRAD)
                {
                    if (IsIntegerSampler(textureFunction->sampler))
                    {
                        out << "x.Load(";
                    }
                    else if (IsShadowSampler(textureFunction->sampler))
                    {
                        out << "x.SampleCmpLevelZero(s, ";
                    }
                    else
                    {
                        out << "x.SampleGrad(s, ";
                    }
                }
                else if (IsIntegerSampler(textureFunction->sampler) ||
                         textureFunction->method == TextureFunction::FETCH)
                {
                    out << "x.Load(";
                }
                else if (IsShadowSampler(textureFunction->sampler))
                {
                    out << "x.SampleCmp(s, ";
                }
                else
                {
                    switch(textureFunction->method)
                    {
                      case TextureFunction::IMPLICIT: out << "x.Sample(s, ";      break;
                      case TextureFunction::BIAS:     out << "x.SampleBias(s, ";  break;
                      case TextureFunction::LOD:      out << "x.SampleLevel(s, "; break;
                      case TextureFunction::LOD0:     out << "x.SampleLevel(s, "; break;
                      case TextureFunction::LOD0BIAS: out << "x.SampleLevel(s, "; break;
                      default: UNREACHABLE();
                    }
                }
            }
            else UNREACHABLE();

            // Integer sampling requires integer addresses
            TString addressx = "";
            TString addressy = "";
            TString addressz = "";
            TString close = "";

            if (IsIntegerSampler(textureFunction->sampler) ||
                textureFunction->method == TextureFunction::FETCH)
            {
                switch(hlslCoords)
                {
                  case 2: out << "int3("; break;
                  case 3: out << "int4("; break;
                  default: UNREACHABLE();
                }
            
                // Convert from normalized floating-point to integer
                if (textureFunction->method != TextureFunction::FETCH)
                {
                    addressx = "int(floor(width * frac((";
                    addressy = "int(floor(height * frac((";

                    if (IsSamplerArray(textureFunction->sampler))
                    {
                        addressz = "int(max(0, min(layers - 1, floor(0.5 + ";
                    }
                    else if (IsSamplerCube(textureFunction->sampler))
                    {
                        addressz = "((((";
                    }
                    else
                    {
                        addressz = "int(floor(depth * frac((";
                    }

                    close = "))))";
                }
            }
            else
            {
                switch(hlslCoords)
                {
                  case 2: out << "float2("; break;
                  case 3: out << "float3("; break;
                  case 4: out << "float4("; break;
                  default: UNREACHABLE();
                }
            }

            TString proj = "";   // Only used for projected textures
        
            if (textureFunction->proj)
            {
                switch(textureFunction->coords)
                {
                  case 3: proj = " / t.z"; break;
                  case 4: proj = " / t.w"; break;
                  default: UNREACHABLE();
                }
            }

            out << addressx + ("t.x" + proj) + close + ", " + addressy + ("t.y" + proj) + close;

            if (mOutputType == SH_HLSL9_OUTPUT)
            {
                if (hlslCoords >= 3)
                {
                    if (textureFunction->coords < 3)
                    {
                        out << ", 0";
                    }
                    else
                    {
                        out << ", t.z" + proj;
                    }
                }

                if (hlslCoords == 4)
                {
                    switch(textureFunction->method)
                    {
                      case TextureFunction::BIAS:     out << ", bias"; break;
                      case TextureFunction::LOD:      out << ", lod";  break;
                      case TextureFunction::LOD0:     out << ", 0";    break;
                      case TextureFunction::LOD0BIAS: out << ", bias"; break;
                      default: UNREACHABLE();
                    }
                }

                out << "));\n";
            }
            else if (mOutputType == SH_HLSL11_OUTPUT)
            {
                if (hlslCoords >= 3)
                {
                    if (IsIntegerSampler(textureFunction->sampler) && IsSamplerCube(textureFunction->sampler))
                    {
                        out << ", face";
                    }
                    else
                    {
                        out << ", " + addressz + ("t.z" + proj) + close;
                    }
                }

                if (textureFunction->method == TextureFunction::GRAD)
                {
                    if (IsIntegerSampler(textureFunction->sampler))
                    {
                        out << ", mip)";
                    }
                    else if (IsShadowSampler(textureFunction->sampler))
                    {
                        // Compare value
                        switch(textureFunction->coords)
                        {
                          case 3: out << "), t.z"; break;
                          case 4: out << "), t.w"; break;
                          default: UNREACHABLE();
                        }
                    }
                    else
                    {
                        out << "), ddx, ddy";
                    }
                }
                else if (IsIntegerSampler(textureFunction->sampler) ||
                         textureFunction->method == TextureFunction::FETCH)
                {
                    out << ", mip)";
                }
                else if (IsShadowSampler(textureFunction->sampler))
                {
                    // Compare value
                    switch(textureFunction->coords)
                    {
                      case 3: out << "), t.z"; break;
                      case 4: out << "), t.w"; break;
                      default: UNREACHABLE();
                    }
                }
                else
                {
                    switch(textureFunction->method)
                    {
                      case TextureFunction::IMPLICIT: out << ")";       break;
                      case TextureFunction::BIAS:     out << "), bias"; break;
                      case TextureFunction::LOD:      out << "), lod";  break;
                      case TextureFunction::LOD0:     out << "), 0";    break;
                      case TextureFunction::LOD0BIAS: out << "), bias"; break;
                      default: UNREACHABLE();
                    }
                }

                if (textureFunction->offset)
                {
                    out << ", offset";
                }

                out << ");";
            }
            else UNREACHABLE();
        }

        out << "\n"
               "}\n"
               "\n";
    }

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

    if (mUsesMod1)
    {
        out << "float mod(float x, float y)\n"
               "{\n"
               "    return x - y * floor(x / y);\n"
               "}\n"
               "\n";
    }

    if (mUsesMod2v)
    {
        out << "float2 mod(float2 x, float2 y)\n"
               "{\n"
               "    return x - y * floor(x / y);\n"
               "}\n"
               "\n";
    }

    if (mUsesMod2f)
    {
        out << "float2 mod(float2 x, float y)\n"
               "{\n"
               "    return x - y * floor(x / y);\n"
               "}\n"
               "\n";
    }
    
    if (mUsesMod3v)
    {
        out << "float3 mod(float3 x, float3 y)\n"
               "{\n"
               "    return x - y * floor(x / y);\n"
               "}\n"
               "\n";
    }

    if (mUsesMod3f)
    {
        out << "float3 mod(float3 x, float y)\n"
               "{\n"
               "    return x - y * floor(x / y);\n"
               "}\n"
               "\n";
    }

    if (mUsesMod4v)
    {
        out << "float4 mod(float4 x, float4 y)\n"
               "{\n"
               "    return x - y * floor(x / y);\n"
               "}\n"
               "\n";
    }

    if (mUsesMod4f)
    {
        out << "float4 mod(float4 x, float y)\n"
               "{\n"
               "    return x - y * floor(x / y);\n"
               "}\n"
               "\n";
    }

    if (mUsesFaceforward1)
    {
        out << "float faceforward(float N, float I, float Nref)\n"
               "{\n"
               "    if(dot(Nref, I) >= 0)\n"
               "    {\n"
               "        return -N;\n"
               "    }\n"
               "    else\n"
               "    {\n"
               "        return N;\n"
               "    }\n"
               "}\n"
               "\n";
    }

    if (mUsesFaceforward2)
    {
        out << "float2 faceforward(float2 N, float2 I, float2 Nref)\n"
               "{\n"
               "    if(dot(Nref, I) >= 0)\n"
               "    {\n"
               "        return -N;\n"
               "    }\n"
               "    else\n"
               "    {\n"
               "        return N;\n"
               "    }\n"
               "}\n"
               "\n";
    }

    if (mUsesFaceforward3)
    {
        out << "float3 faceforward(float3 N, float3 I, float3 Nref)\n"
               "{\n"
               "    if(dot(Nref, I) >= 0)\n"
               "    {\n"
               "        return -N;\n"
               "    }\n"
               "    else\n"
               "    {\n"
               "        return N;\n"
               "    }\n"
               "}\n"
               "\n";
    }

    if (mUsesFaceforward4)
    {
        out << "float4 faceforward(float4 N, float4 I, float4 Nref)\n"
               "{\n"
               "    if(dot(Nref, I) >= 0)\n"
               "    {\n"
               "        return -N;\n"
               "    }\n"
               "    else\n"
               "    {\n"
               "        return N;\n"
               "    }\n"
               "}\n"
               "\n";
    }

    if (mUsesAtan2_1)
    {
        out << "float atanyx(float y, float x)\n"
               "{\n"
               "    if(x == 0 && y == 0) x = 1;\n"   // Avoid producing a NaN
               "    return atan2(y, x);\n"
               "}\n";
    }

    if (mUsesAtan2_2)
    {
        out << "float2 atanyx(float2 y, float2 x)\n"
               "{\n"
               "    if(x[0] == 0 && y[0] == 0) x[0] = 1;\n"
               "    if(x[1] == 0 && y[1] == 0) x[1] = 1;\n"
               "    return float2(atan2(y[0], x[0]), atan2(y[1], x[1]));\n"
               "}\n";
    }

    if (mUsesAtan2_3)
    {
        out << "float3 atanyx(float3 y, float3 x)\n"
               "{\n"
               "    if(x[0] == 0 && y[0] == 0) x[0] = 1;\n"
               "    if(x[1] == 0 && y[1] == 0) x[1] = 1;\n"
               "    if(x[2] == 0 && y[2] == 0) x[2] = 1;\n"
               "    return float3(atan2(y[0], x[0]), atan2(y[1], x[1]), atan2(y[2], x[2]));\n"
               "}\n";
    }

    if (mUsesAtan2_4)
    {
        out << "float4 atanyx(float4 y, float4 x)\n"
               "{\n"
               "    if(x[0] == 0 && y[0] == 0) x[0] = 1;\n"
               "    if(x[1] == 0 && y[1] == 0) x[1] = 1;\n"
               "    if(x[2] == 0 && y[2] == 0) x[2] = 1;\n"
               "    if(x[3] == 0 && y[3] == 0) x[3] = 1;\n"
               "    return float4(atan2(y[0], x[0]), atan2(y[1], x[1]), atan2(y[2], x[2]), atan2(y[3], x[3]));\n"
               "}\n";
    }
}

void OutputHLSL::visitSymbol(TIntermSymbol *node)
{
    TInfoSinkBase &out = mBody;

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
            const TType& nodeType = node->getType();
            const TInterfaceBlock* interfaceBlock = nodeType.getInterfaceBlock();

            if (interfaceBlock)
            {
                mReferencedInterfaceBlocks[interfaceBlock->name()] = node;
            }
            else
            {
                mReferencedUniforms[name] = node;
            }

            out << decorateUniform(name, nodeType);
        }
        else if (qualifier == EvqAttribute || qualifier == EvqVertexIn)
        {
            mReferencedAttributes[name] = node;
            out << decorate(name);
        }
        else if (isVarying(qualifier))
        {
            mReferencedVaryings[name] = node;
            out << decorate(name);
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
        else if (name == "gl_FragDepthEXT")
        {
            mUsesFragDepth = true;
            out << "gl_Depth";
        }
        else if (qualifier == EvqInternal)
        {
            out << name;
        }
        else
        {
            out << decorate(name);
        }
    }
}

bool OutputHLSL::visitBinary(Visit visit, TIntermBinary *node)
{
    TInfoSinkBase &out = mBody;

    // Handle accessing std140 structs by value
    if (mFlaggedStructMappedNames.count(node) > 0)
    {
        out << mFlaggedStructMappedNames[node];
        return false;
    }

    switch (node->getOp())
    {
      case EOpAssign:                  outputTriplet(visit, "(", " = ", ")");           break;
      case EOpInitialize:
        if (visit == PreVisit)
        {
            // GLSL allows to write things like "float x = x;" where a new variable x is defined
            // and the value of an existing variable x is assigned. HLSL uses C semantics (the
            // new variable is created before the assignment is evaluated), so we need to convert
            // this to "float t = x, x = t;".

            TIntermSymbol *symbolNode = node->getLeft()->getAsSymbolNode();
            TIntermTyped *expression = node->getRight();

            sh::SearchSymbol searchSymbol(symbolNode->getSymbol());
            expression->traverse(&searchSymbol);
            bool sameSymbol = searchSymbol.foundMatch();

            if (sameSymbol)
            {
                // Type already printed
                out << "t" + str(mUniqueIndex) + " = ";
                expression->traverse(this);
                out << ", ";
                symbolNode->traverse(this);
                out << " = t" + str(mUniqueIndex);

                mUniqueIndex++;
                return false;
            }
        }
        else if (visit == InVisit)
        {
            out << " = ";
        }
        break;
      case EOpAddAssign:               outputTriplet(visit, "(", " += ", ")");          break;
      case EOpSubAssign:               outputTriplet(visit, "(", " -= ", ")");          break;
      case EOpMulAssign:               outputTriplet(visit, "(", " *= ", ")");          break;
      case EOpVectorTimesScalarAssign: outputTriplet(visit, "(", " *= ", ")");          break;
      case EOpMatrixTimesScalarAssign: outputTriplet(visit, "(", " *= ", ")");          break;
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
            out << " = mul(";
            node->getLeft()->traverse(this);
            out << ", ";   
        }
        else
        {
            out << "))";
        }
        break;
      case EOpDivAssign:               outputTriplet(visit, "(", " /= ", ")");          break;
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
                    out << interfaceBlockInstanceString(*interfaceBlock, arrayIndex);

                    return false;
                }
            }
            else
            {
                outputTriplet(visit, "", "[", "]");
            }
        }
        break;
      case EOpIndexIndirect:
        // We do not currently support indirect references to interface blocks
        ASSERT(node->getLeft()->getBasicType() != EbtInterfaceBlock);
        outputTriplet(visit, "", "[", "]");
        break;
      case EOpIndexDirectStruct:
        if (visit == InVisit)
        {
            const TStructure* structure = node->getLeft()->getType().getStruct();
            const TIntermConstantUnion* index = node->getRight()->getAsConstantUnion();
            const TField* field = structure->fields()[index->getIConst(0)];
            out << "." + decorateField(field->name(), *structure);

            return false;
        }
        break;
      case EOpIndexDirectInterfaceBlock:
        if (visit == InVisit)
        {
            const TInterfaceBlock* interfaceBlock = node->getLeft()->getType().getInterfaceBlock();
            const TIntermConstantUnion* index = node->getRight()->getAsConstantUnion();
            const TField* field = interfaceBlock->fields()[index->getIConst(0)];
            out << "." + decorate(field->name());

            return false;
        }
        break;
      case EOpVectorSwizzle:
        if (visit == InVisit)
        {
            out << ".";

            TIntermAggregate *swizzle = node->getRight()->getAsAggregate();

            if (swizzle)
            {
                TIntermSequence &sequence = swizzle->getSequence();

                for (TIntermSequence::iterator sit = sequence.begin(); sit != sequence.end(); sit++)
                {
                    TIntermConstantUnion *element = (*sit)->getAsConstantUnion();

                    if (element)
                    {
                        int i = element->getIConst(0);

                        switch (i)
                        {
                        case 0: out << "x"; break;
                        case 1: out << "y"; break;
                        case 2: out << "z"; break;
                        case 3: out << "w"; break;
                        default: UNREACHABLE();
                        }
                    }
                    else UNREACHABLE();
                }
            }
            else UNREACHABLE();

            return false;   // Fully processed
        }
        break;
      case EOpAdd:               outputTriplet(visit, "(", " + ", ")"); break;
      case EOpSub:               outputTriplet(visit, "(", " - ", ")"); break;
      case EOpMul:               outputTriplet(visit, "(", " * ", ")"); break;
      case EOpDiv:               outputTriplet(visit, "(", " / ", ")"); break;
      case EOpEqual:
      case EOpNotEqual:
        if (node->getLeft()->isScalar())
        {
            if (node->getOp() == EOpEqual)
            {
                outputTriplet(visit, "(", " == ", ")");
            }
            else
            {
                outputTriplet(visit, "(", " != ", ")");
            }
        }
        else if (node->getLeft()->getBasicType() == EbtStruct)
        {
            if (node->getOp() == EOpEqual)
            {
                out << "(";
            }
            else
            {
                out << "!(";
            }

            const TStructure &structure = *node->getLeft()->getType().getStruct();
            const TFieldList &fields = structure.fields();

            for (size_t i = 0; i < fields.size(); i++)
            {
                const TField *field = fields[i];

                node->getLeft()->traverse(this);
                out << "." + decorateField(field->name(), structure) + " == ";
                node->getRight()->traverse(this);
                out << "." + decorateField(field->name(), structure);

                if (i < fields.size() - 1)
                {
                    out << " && ";
                }
            }

            out << ")";

            return false;
        }
        else
        {
            ASSERT(node->getLeft()->isMatrix() || node->getLeft()->isVector());

            if (node->getOp() == EOpEqual)
            {
                outputTriplet(visit, "all(", " == ", ")");
            }
            else
            {
                outputTriplet(visit, "!all(", " == ", ")");
            }
        }
        break;
      case EOpLessThan:          outputTriplet(visit, "(", " < ", ")");   break;
      case EOpGreaterThan:       outputTriplet(visit, "(", " > ", ")");   break;
      case EOpLessThanEqual:     outputTriplet(visit, "(", " <= ", ")");  break;
      case EOpGreaterThanEqual:  outputTriplet(visit, "(", " >= ", ")");  break;
      case EOpVectorTimesScalar: outputTriplet(visit, "(", " * ", ")");   break;
      case EOpMatrixTimesScalar: outputTriplet(visit, "(", " * ", ")");   break;
      case EOpVectorTimesMatrix: outputTriplet(visit, "mul(", ", transpose(", "))"); break;
      case EOpMatrixTimesVector: outputTriplet(visit, "mul(transpose(", "), ", ")"); break;
      case EOpMatrixTimesMatrix: outputTriplet(visit, "transpose(mul(transpose(", "), transpose(", ")))"); break;
      case EOpLogicalOr:
        if (node->getRight()->hasSideEffects())
        {
            out << "s" << mUnfoldShortCircuit->getNextTemporaryIndex();
            return false;
        }
        else
        {
           outputTriplet(visit, "(", " || ", ")");
           return true;
        }
      case EOpLogicalXor:
        mUsesXor = true;
        outputTriplet(visit, "xor(", ", ", ")");
        break;
      case EOpLogicalAnd:
        if (node->getRight()->hasSideEffects())
        {
            out << "s" << mUnfoldShortCircuit->getNextTemporaryIndex();
            return false;
        }
        else
        {
           outputTriplet(visit, "(", " && ", ")");
           return true;
        }
      default: UNREACHABLE();
    }

    return true;
}

bool OutputHLSL::visitUnary(Visit visit, TIntermUnary *node)
{
    switch (node->getOp())
    {
      case EOpNegative:         outputTriplet(visit, "(-", "", ")");  break;
      case EOpVectorLogicalNot: outputTriplet(visit, "(!", "", ")");  break;
      case EOpLogicalNot:       outputTriplet(visit, "(!", "", ")");  break;
      case EOpPostIncrement:    outputTriplet(visit, "(", "", "++)"); break;
      case EOpPostDecrement:    outputTriplet(visit, "(", "", "--)"); break;
      case EOpPreIncrement:     outputTriplet(visit, "(++", "", ")"); break;
      case EOpPreDecrement:     outputTriplet(visit, "(--", "", ")"); break;
      case EOpConvIntToBool:
      case EOpConvUIntToBool:
      case EOpConvFloatToBool:
        switch (node->getOperand()->getType().getNominalSize())
        {
          case 1:    outputTriplet(visit, "bool(", "", ")");  break;
          case 2:    outputTriplet(visit, "bool2(", "", ")"); break;
          case 3:    outputTriplet(visit, "bool3(", "", ")"); break;
          case 4:    outputTriplet(visit, "bool4(", "", ")"); break;
          default: UNREACHABLE();
        }
        break;
      case EOpConvBoolToFloat:
      case EOpConvIntToFloat:
      case EOpConvUIntToFloat:
        switch (node->getOperand()->getType().getNominalSize())
        {
          case 1:    outputTriplet(visit, "float(", "", ")");  break;
          case 2:    outputTriplet(visit, "float2(", "", ")"); break;
          case 3:    outputTriplet(visit, "float3(", "", ")"); break;
          case 4:    outputTriplet(visit, "float4(", "", ")"); break;
          default: UNREACHABLE();
        }
        break;
      case EOpConvFloatToInt:
      case EOpConvBoolToInt:
      case EOpConvUIntToInt:
        switch (node->getOperand()->getType().getNominalSize())
        {
          case 1:    outputTriplet(visit, "int(", "", ")");  break;
          case 2:    outputTriplet(visit, "int2(", "", ")"); break;
          case 3:    outputTriplet(visit, "int3(", "", ")"); break;
          case 4:    outputTriplet(visit, "int4(", "", ")"); break;
          default: UNREACHABLE();
        }
        break;
      case EOpConvFloatToUInt:
      case EOpConvBoolToUInt:
      case EOpConvIntToUInt:
        switch (node->getOperand()->getType().getNominalSize())
        {
          case 1:    outputTriplet(visit, "uint(", "", ")");  break;
          case 2:    outputTriplet(visit, "uint2(", "", ")");  break;
          case 3:    outputTriplet(visit, "uint3(", "", ")");  break;
          case 4:    outputTriplet(visit, "uint4(", "", ")");  break;
          default: UNREACHABLE();
        }
        break;
      case EOpRadians:          outputTriplet(visit, "radians(", "", ")");   break;
      case EOpDegrees:          outputTriplet(visit, "degrees(", "", ")");   break;
      case EOpSin:              outputTriplet(visit, "sin(", "", ")");       break;
      case EOpCos:              outputTriplet(visit, "cos(", "", ")");       break;
      case EOpTan:              outputTriplet(visit, "tan(", "", ")");       break;
      case EOpAsin:             outputTriplet(visit, "asin(", "", ")");      break;
      case EOpAcos:             outputTriplet(visit, "acos(", "", ")");      break;
      case EOpAtan:             outputTriplet(visit, "atan(", "", ")");      break;
      case EOpExp:              outputTriplet(visit, "exp(", "", ")");       break;
      case EOpLog:              outputTriplet(visit, "log(", "", ")");       break;
      case EOpExp2:             outputTriplet(visit, "exp2(", "", ")");      break;
      case EOpLog2:             outputTriplet(visit, "log2(", "", ")");      break;
      case EOpSqrt:             outputTriplet(visit, "sqrt(", "", ")");      break;
      case EOpInverseSqrt:      outputTriplet(visit, "rsqrt(", "", ")");     break;
      case EOpAbs:              outputTriplet(visit, "abs(", "", ")");       break;
      case EOpSign:             outputTriplet(visit, "sign(", "", ")");      break;
      case EOpFloor:            outputTriplet(visit, "floor(", "", ")");     break;
      case EOpCeil:             outputTriplet(visit, "ceil(", "", ")");      break;
      case EOpFract:            outputTriplet(visit, "frac(", "", ")");      break;
      case EOpLength:           outputTriplet(visit, "length(", "", ")");    break;
      case EOpNormalize:        outputTriplet(visit, "normalize(", "", ")"); break;
      case EOpDFdx:
        if(mInsideDiscontinuousLoop || mOutputLod0Function)
        {
            outputTriplet(visit, "(", "", ", 0.0)");
        }
        else
        {
            outputTriplet(visit, "ddx(", "", ")");
        }
        break;
      case EOpDFdy:
        if(mInsideDiscontinuousLoop || mOutputLod0Function)
        {
            outputTriplet(visit, "(", "", ", 0.0)");
        }
        else
        {
           outputTriplet(visit, "ddy(", "", ")");
        }
        break;
      case EOpFwidth:
        if(mInsideDiscontinuousLoop || mOutputLod0Function)
        {
            outputTriplet(visit, "(", "", ", 0.0)");
        }
        else
        {
            outputTriplet(visit, "fwidth(", "", ")");
        }
        break;
      case EOpAny:              outputTriplet(visit, "any(", "", ")");       break;
      case EOpAll:              outputTriplet(visit, "all(", "", ")");       break;
      default: UNREACHABLE();
    }

    return true;
}

bool OutputHLSL::visitAggregate(Visit visit, TIntermAggregate *node)
{
    TInfoSinkBase &out = mBody;

    switch (node->getOp())
    {
      case EOpSequence:
        {
            if (mInsideFunction)
            {
                outputLineDirective(node->getLine().first_line);
                out << "{\n";

                mScopeDepth++;

                if (mScopeBracket.size() < mScopeDepth)
                {
                    mScopeBracket.push_back(0);   // New scope level
                }
                else
                {
                    mScopeBracket[mScopeDepth - 1]++;   // New scope at existing level
                }
            }

            for (TIntermSequence::iterator sit = node->getSequence().begin(); sit != node->getSequence().end(); sit++)
            {
                outputLineDirective((*sit)->getLine().first_line);

                traverseStatements(*sit);

                out << ";\n";
            }

            if (mInsideFunction)
            {
                outputLineDirective(node->getLine().last_line);
                out << "}\n";

                mScopeDepth--;
            }

            return false;
        }
      case EOpDeclaration:
        if (visit == PreVisit)
        {
            TIntermSequence &sequence = node->getSequence();
            TIntermTyped *variable = sequence[0]->getAsTyped();

            if (variable && (variable->getQualifier() == EvqTemporary || variable->getQualifier() == EvqGlobal))
            {
                if (variable->getType().getStruct())
                {
                    addConstructor(variable->getType(), scopedStruct(variable->getType().getStruct()->name()), NULL);
                }

                if (!variable->getAsSymbolNode() || variable->getAsSymbolNode()->getSymbol() != "")   // Variable declaration
                {
                    if (!mInsideFunction)
                    {
                        out << "static ";
                    }

                    out << typeString(variable->getType()) + " ";

                    for (TIntermSequence::iterator sit = sequence.begin(); sit != sequence.end(); sit++)
                    {
                        TIntermSymbol *symbol = (*sit)->getAsSymbolNode();

                        if (symbol)
                        {
                            symbol->traverse(this);
                            out << arrayString(symbol->getType());
                            out << " = " + initializer(symbol->getType());
                        }
                        else
                        {
                            (*sit)->traverse(this);
                        }

                        if (*sit != sequence.back())
                        {
                            out << ", ";
                        }
                    }
                }
                else if (variable->getAsSymbolNode() && variable->getAsSymbolNode()->getSymbol() == "")   // Type (struct) declaration
                {
                    // Already added to constructor map
                }
                else UNREACHABLE();
            }
            else if (variable && isVaryingOut(variable->getQualifier()))
            {
                for (TIntermSequence::iterator sit = sequence.begin(); sit != sequence.end(); sit++)
                {
                    TIntermSymbol *symbol = (*sit)->getAsSymbolNode();

                    if (symbol)
                    {
                        // Vertex (output) varyings which are declared but not written to should still be declared to allow successful linking
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
      case EOpPrototype:
        if (visit == PreVisit)
        {
            out << typeString(node->getType()) << " " << decorate(node->getName()) << (mOutputLod0Function ? "Lod0(" : "(");

            TIntermSequence &arguments = node->getSequence();

            for (unsigned int i = 0; i < arguments.size(); i++)
            {
                TIntermSymbol *symbol = arguments[i]->getAsSymbolNode();

                if (symbol)
                {
                    out << argumentString(symbol);

                    if (i < arguments.size() - 1)
                    {
                        out << ", ";
                    }
                }
                else UNREACHABLE();
            }

            out << ");\n";

            // Also prototype the Lod0 variant if needed
            if (mContainsLoopDiscontinuity && !mOutputLod0Function)
            {
                mOutputLod0Function = true;
                node->traverse(this);
                mOutputLod0Function = false;
            }

            return false;
        }
        break;
      case EOpComma:            outputTriplet(visit, "(", ", ", ")");                break;
      case EOpFunction:
        {
            TString name = TFunction::unmangleName(node->getName());

            out << typeString(node->getType()) << " ";

            if (name == "main")
            {
                out << "gl_main(";
            }
            else
            {
                out << decorate(name) << (mOutputLod0Function ? "Lod0(" : "(");
            }

            TIntermSequence &sequence = node->getSequence();
            TIntermSequence &arguments = sequence[0]->getAsAggregate()->getSequence();

            for (unsigned int i = 0; i < arguments.size(); i++)
            {
                TIntermSymbol *symbol = arguments[i]->getAsSymbolNode();

                if (symbol)
                {
                    if (symbol->getType().getStruct())
                    {
                        addConstructor(symbol->getType(), scopedStruct(symbol->getType().getStruct()->name()), NULL);
                    }

                    out << argumentString(symbol);

                    if (i < arguments.size() - 1)
                    {
                        out << ", ";
                    }
                }
                else UNREACHABLE();
            }

            out << ")\n"
                "{\n";
            
            if (sequence.size() > 1)
            {
                mInsideFunction = true;
                sequence[1]->traverse(this);
                mInsideFunction = false;
            }
            
            out << "}\n";

            if (mContainsLoopDiscontinuity && !mOutputLod0Function)
            {
                if (name != "main")
                {
                    mOutputLod0Function = true;
                    node->traverse(this);
                    mOutputLod0Function = false;
                }
            }

            return false;
        }
        break;
      case EOpFunctionCall:
        {
            TString name = TFunction::unmangleName(node->getName());
            bool lod0 = mInsideDiscontinuousLoop || mOutputLod0Function;
            TIntermSequence &arguments = node->getSequence();

            if (node->isUserDefined())
            {
                out << decorate(name) << (lod0 ? "Lod0(" : "(");
            }
            else
            {
                TBasicType samplerType = arguments[0]->getAsTyped()->getType().getBasicType();

                TextureFunction textureFunction;
                textureFunction.sampler = samplerType;
                textureFunction.coords = arguments[1]->getAsTyped()->getNominalSize();
                textureFunction.method = TextureFunction::IMPLICIT;
                textureFunction.proj = false;
                textureFunction.offset = false;

                if (name == "texture2D" || name == "textureCube" || name == "texture")
                {
                    textureFunction.method = TextureFunction::IMPLICIT;
                }
                else if (name == "texture2DProj" || name == "textureProj")
                {
                    textureFunction.method = TextureFunction::IMPLICIT;
                    textureFunction.proj = true;
                }
                else if (name == "texture2DLod" || name == "textureCubeLod" || name == "textureLod" ||
                         name == "texture2DLodEXT" || name == "textureCubeLodEXT")
                {
                    textureFunction.method = TextureFunction::LOD;
                }
                else if (name == "texture2DProjLod" || name == "textureProjLod" || name == "texture2DProjLodEXT")
                {
                    textureFunction.method = TextureFunction::LOD;
                    textureFunction.proj = true;
                }
                else if (name == "textureSize")
                {
                    textureFunction.method = TextureFunction::SIZE;
                }
                else if (name == "textureOffset")
                {
                    textureFunction.method = TextureFunction::IMPLICIT;
                    textureFunction.offset = true;
                }
                else if (name == "textureProjOffset")
                {
                    textureFunction.method = TextureFunction::IMPLICIT;
                    textureFunction.offset = true;
                    textureFunction.proj = true;
                }
                else if (name == "textureLodOffset")
                {
                    textureFunction.method = TextureFunction::LOD;
                    textureFunction.offset = true;
                }
                else if (name == "textureProjLodOffset")
                {
                    textureFunction.method = TextureFunction::LOD;
                    textureFunction.proj = true;
                    textureFunction.offset = true;
                }
                else if (name == "texelFetch")
                {
                    textureFunction.method = TextureFunction::FETCH;
                }
                else if (name == "texelFetchOffset")
                {
                    textureFunction.method = TextureFunction::FETCH;
                    textureFunction.offset = true;
                }
                else if (name == "textureGrad" || name == "texture2DGradEXT")
                {
                    textureFunction.method = TextureFunction::GRAD;
                }
                else if (name == "textureGradOffset")
                {
                    textureFunction.method = TextureFunction::GRAD;
                    textureFunction.offset = true;
                }
                else if (name == "textureProjGrad" || name == "texture2DProjGradEXT" || name == "textureCubeGradEXT")
                {
                    textureFunction.method = TextureFunction::GRAD;
                    textureFunction.proj = true;
                }
                else if (name == "textureProjGradOffset")
                {
                    textureFunction.method = TextureFunction::GRAD;
                    textureFunction.proj = true;
                    textureFunction.offset = true;
                }
                else UNREACHABLE();

                if (textureFunction.method == TextureFunction::IMPLICIT)   // Could require lod 0 or have a bias argument
                {
                    unsigned int mandatoryArgumentCount = 2;   // All functions have sampler and coordinate arguments

                    if (textureFunction.offset)
                    {
                        mandatoryArgumentCount++;
                    }

                    bool bias = (arguments.size() > mandatoryArgumentCount);   // Bias argument is optional

                    if (lod0 || mContext.shaderType == SH_VERTEX_SHADER)
                    {
                        if (bias)
                        {
                            textureFunction.method = TextureFunction::LOD0BIAS;
                        }
                        else
                        {
                            textureFunction.method = TextureFunction::LOD0;
                        }
                    }
                    else if (bias)
                    {
                        textureFunction.method = TextureFunction::BIAS;
                    }
                }

                mUsesTexture.insert(textureFunction);

                out << textureFunction.name();
            }

            for (TIntermSequence::iterator arg = arguments.begin(); arg != arguments.end(); arg++)
            {
                if (mOutputType == SH_HLSL11_OUTPUT && IsSampler((*arg)->getAsTyped()->getBasicType()))
                {
                    out << "texture_";
                    (*arg)->traverse(this);
                    out << ", sampler_";
                }

                (*arg)->traverse(this);

                if (arg < arguments.end() - 1)
                {
                    out << ", ";
                }
            }

            out << ")";

            return false;
        }
        break;
      case EOpParameters:       outputTriplet(visit, "(", ", ", ")\n{\n");             break;
      case EOpConstructFloat:
        addConstructor(node->getType(), "vec1", &node->getSequence());
        outputTriplet(visit, "vec1(", "", ")");
        break;
      case EOpConstructVec2:
        addConstructor(node->getType(), "vec2", &node->getSequence());
        outputTriplet(visit, "vec2(", ", ", ")");
        break;
      case EOpConstructVec3:
        addConstructor(node->getType(), "vec3", &node->getSequence());
        outputTriplet(visit, "vec3(", ", ", ")");
        break;
      case EOpConstructVec4:
        addConstructor(node->getType(), "vec4", &node->getSequence());
        outputTriplet(visit, "vec4(", ", ", ")");
        break;
      case EOpConstructBool:
        addConstructor(node->getType(), "bvec1", &node->getSequence());
        outputTriplet(visit, "bvec1(", "", ")");
        break;
      case EOpConstructBVec2:
        addConstructor(node->getType(), "bvec2", &node->getSequence());
        outputTriplet(visit, "bvec2(", ", ", ")");
        break;
      case EOpConstructBVec3:
        addConstructor(node->getType(), "bvec3", &node->getSequence());
        outputTriplet(visit, "bvec3(", ", ", ")");
        break;
      case EOpConstructBVec4:
        addConstructor(node->getType(), "bvec4", &node->getSequence());
        outputTriplet(visit, "bvec4(", ", ", ")");
        break;
      case EOpConstructInt:
        addConstructor(node->getType(), "ivec1", &node->getSequence());
        outputTriplet(visit, "ivec1(", "", ")");
        break;
      case EOpConstructIVec2:
        addConstructor(node->getType(), "ivec2", &node->getSequence());
        outputTriplet(visit, "ivec2(", ", ", ")");
        break;
      case EOpConstructIVec3:
        addConstructor(node->getType(), "ivec3", &node->getSequence());
        outputTriplet(visit, "ivec3(", ", ", ")");
        break;
      case EOpConstructIVec4:
        addConstructor(node->getType(), "ivec4", &node->getSequence());
        outputTriplet(visit, "ivec4(", ", ", ")");
        break;
      case EOpConstructUInt:
        addConstructor(node->getType(), "uvec1", &node->getSequence());
        outputTriplet(visit, "uvec1(", "", ")");
        break;
      case EOpConstructUVec2:
        addConstructor(node->getType(), "uvec2", &node->getSequence());
        outputTriplet(visit, "uvec2(", ", ", ")");
        break;
      case EOpConstructUVec3:
        addConstructor(node->getType(), "uvec3", &node->getSequence());
        outputTriplet(visit, "uvec3(", ", ", ")");
        break;
      case EOpConstructUVec4:
        addConstructor(node->getType(), "uvec4", &node->getSequence());
        outputTriplet(visit, "uvec4(", ", ", ")");
        break;
      case EOpConstructMat2:
        addConstructor(node->getType(), "mat2", &node->getSequence());
        outputTriplet(visit, "mat2(", ", ", ")");
        break;
      case EOpConstructMat3:
        addConstructor(node->getType(), "mat3", &node->getSequence());
        outputTriplet(visit, "mat3(", ", ", ")");
        break;
      case EOpConstructMat4: 
        addConstructor(node->getType(), "mat4", &node->getSequence());
        outputTriplet(visit, "mat4(", ", ", ")");
        break;
      case EOpConstructStruct:
        addConstructor(node->getType(), scopedStruct(node->getType().getStruct()->name()), &node->getSequence());
        outputTriplet(visit, structLookup(node->getType().getStruct()->name()) + "_ctor(", ", ", ")");
        break;
      case EOpLessThan:         outputTriplet(visit, "(", " < ", ")");                 break;
      case EOpGreaterThan:      outputTriplet(visit, "(", " > ", ")");                 break;
      case EOpLessThanEqual:    outputTriplet(visit, "(", " <= ", ")");                break;
      case EOpGreaterThanEqual: outputTriplet(visit, "(", " >= ", ")");                break;
      case EOpVectorEqual:      outputTriplet(visit, "(", " == ", ")");                break;
      case EOpVectorNotEqual:   outputTriplet(visit, "(", " != ", ")");                break;
      case EOpMod:
        {
            // We need to look at the number of components in both arguments
            const int modValue = node->getSequence()[0]->getAsTyped()->getNominalSize() * 10
                               + node->getSequence()[1]->getAsTyped()->getNominalSize();
            switch (modValue)
            {
              case 11: mUsesMod1 = true; break;
              case 22: mUsesMod2v = true; break;
              case 21: mUsesMod2f = true; break;
              case 33: mUsesMod3v = true; break;
              case 31: mUsesMod3f = true; break;
              case 44: mUsesMod4v = true; break;
              case 41: mUsesMod4f = true; break;
              default: UNREACHABLE();
            }

            outputTriplet(visit, "mod(", ", ", ")");
        }
        break;
      case EOpPow:              outputTriplet(visit, "pow(", ", ", ")");               break;
      case EOpAtan:
        ASSERT(node->getSequence().size() == 2);   // atan(x) is a unary operator
        switch (node->getSequence()[0]->getAsTyped()->getNominalSize())
        {
          case 1: mUsesAtan2_1 = true; break;
          case 2: mUsesAtan2_2 = true; break;
          case 3: mUsesAtan2_3 = true; break;
          case 4: mUsesAtan2_4 = true; break;
          default: UNREACHABLE();
        }
        outputTriplet(visit, "atanyx(", ", ", ")");
        break;
      case EOpMin:           outputTriplet(visit, "min(", ", ", ")");           break;
      case EOpMax:           outputTriplet(visit, "max(", ", ", ")");           break;
      case EOpClamp:         outputTriplet(visit, "clamp(", ", ", ")");         break;
      case EOpMix:           outputTriplet(visit, "lerp(", ", ", ")");          break;
      case EOpStep:          outputTriplet(visit, "step(", ", ", ")");          break;
      case EOpSmoothStep:    outputTriplet(visit, "smoothstep(", ", ", ")");    break;
      case EOpDistance:      outputTriplet(visit, "distance(", ", ", ")");      break;
      case EOpDot:           outputTriplet(visit, "dot(", ", ", ")");           break;
      case EOpCross:         outputTriplet(visit, "cross(", ", ", ")");         break;
      case EOpFaceForward:
        {
            switch (node->getSequence()[0]->getAsTyped()->getNominalSize())   // Number of components in the first argument
            {
            case 1: mUsesFaceforward1 = true; break;
            case 2: mUsesFaceforward2 = true; break;
            case 3: mUsesFaceforward3 = true; break;
            case 4: mUsesFaceforward4 = true; break;
            default: UNREACHABLE();
            }
            
            outputTriplet(visit, "faceforward(", ", ", ")");
        }
        break;
      case EOpReflect:       outputTriplet(visit, "reflect(", ", ", ")");       break;
      case EOpRefract:       outputTriplet(visit, "refract(", ", ", ")");       break;
      case EOpMul:           outputTriplet(visit, "(", " * ", ")");             break;
      default: UNREACHABLE();
    }

    return true;
}

bool OutputHLSL::visitSelection(Visit visit, TIntermSelection *node)
{
    TInfoSinkBase &out = mBody;

    if (node->usesTernaryOperator())
    {
        out << "s" << mUnfoldShortCircuit->getNextTemporaryIndex();
    }
    else  // if/else statement
    {
        mUnfoldShortCircuit->traverse(node->getCondition());

        out << "if (";

        node->getCondition()->traverse(this);

        out << ")\n";
        
        outputLineDirective(node->getLine().first_line);
        out << "{\n";

        bool discard = false;

        if (node->getTrueBlock())
        {
            traverseStatements(node->getTrueBlock());

            // Detect true discard
            discard = (discard || FindDiscard::search(node->getTrueBlock()));
        }

        outputLineDirective(node->getLine().first_line);
        out << ";\n}\n";

        if (node->getFalseBlock())
        {
            out << "else\n";

            outputLineDirective(node->getFalseBlock()->getLine().first_line);
            out << "{\n";

            outputLineDirective(node->getFalseBlock()->getLine().first_line);
            traverseStatements(node->getFalseBlock());

            outputLineDirective(node->getFalseBlock()->getLine().first_line);
            out << ";\n}\n";

            // Detect false discard
            discard = (discard || FindDiscard::search(node->getFalseBlock()));
        }

        // ANGLE issue 486: Detect problematic conditional discard
        if (discard && FindSideEffectRewriting::search(node))
        {
            mUsesDiscardRewriting = true;
        }
    }

    return false;
}

void OutputHLSL::visitConstantUnion(TIntermConstantUnion *node)
{
    writeConstantUnion(node->getType(), node->getUnionArrayPointer());
}

bool OutputHLSL::visitLoop(Visit visit, TIntermLoop *node)
{
    mNestedLoopDepth++;

    bool wasDiscontinuous = mInsideDiscontinuousLoop;

    if (mContainsLoopDiscontinuity && !mInsideDiscontinuousLoop)
    {
        mInsideDiscontinuousLoop = containsLoopDiscontinuity(node);
    }

    if (mOutputType == SH_HLSL9_OUTPUT)
    {
        if (handleExcessiveLoop(node))
        {
            mInsideDiscontinuousLoop = wasDiscontinuous;
            mNestedLoopDepth--;

            return false;
        }
    }

    TInfoSinkBase &out = mBody;

    if (node->getType() == ELoopDoWhile)
    {
        out << "{do\n";

        outputLineDirective(node->getLine().first_line);
        out << "{\n";
    }
    else
    {
        out << "{for(";
        
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
        
        outputLineDirective(node->getLine().first_line);
        out << "{\n";
    }

    if (node->getBody())
    {
        traverseStatements(node->getBody());
    }

    outputLineDirective(node->getLine().first_line);
    out << ";}\n";

    if (node->getType() == ELoopDoWhile)
    {
        outputLineDirective(node->getCondition()->getLine().first_line);
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
    TInfoSinkBase &out = mBody;

    switch (node->getFlowOp())
    {
      case EOpKill:
        outputTriplet(visit, "discard;\n", "", "");
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
      case EOpContinue: outputTriplet(visit, "continue;\n", "", ""); break;
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

void OutputHLSL::traverseStatements(TIntermNode *node)
{
    if (isSingleStatement(node))
    {
        mUnfoldShortCircuit->traverse(node);
    }

    node->traverse(this);
}

bool OutputHLSL::isSingleStatement(TIntermNode *node)
{
    TIntermAggregate *aggregate = node->getAsAggregate();

    if (aggregate)
    {
        if (aggregate->getOp() == EOpSequence)
        {
            return false;
        }
        else
        {
            for (TIntermSequence::iterator sit = aggregate->getSequence().begin(); sit != aggregate->getSequence().end(); sit++)
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
bool OutputHLSL::handleExcessiveLoop(TIntermLoop *node)
{
    const int MAX_LOOP_ITERATIONS = 254;
    TInfoSinkBase &out = mBody;

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
            TIntermSequence &sequence = init->getSequence();
            TIntermTyped *variable = sequence[0]->getAsTyped();

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

                out << "for(";
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
                
                outputLineDirective(node->getLine().first_line);
                out << "{\n";

                if (node->getBody())
                {
                    node->getBody()->traverse(this);
                }

                outputLineDirective(node->getLine().first_line);
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

void OutputHLSL::outputTriplet(Visit visit, const TString &preString, const TString &inString, const TString &postString)
{
    TInfoSinkBase &out = mBody;

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

void OutputHLSL::outputLineDirective(int line)
{
    if ((mContext.compileOptions & SH_LINE_DIRECTIVES) && (line > 0))
    {
        mBody << "\n";
        mBody << "#line " << line;

        if (mContext.sourcePath)
        {
            mBody << " \"" << mContext.sourcePath << "\"";
        }
        
        mBody << "\n";
    }
}

TString OutputHLSL::argumentString(const TIntermSymbol *symbol)
{
    TQualifier qualifier = symbol->getQualifier();
    const TType &type = symbol->getType();
    TString name = symbol->getSymbol();

    if (name.empty())   // HLSL demands named arguments, also for prototypes
    {
        name = "x" + str(mUniqueIndex++);
    }
    else
    {
        name = decorate(name);
    }

    if (mOutputType == SH_HLSL11_OUTPUT && IsSampler(type.getBasicType()))
    {
        return qualifierString(qualifier) + " " + textureString(type) + " texture_" + name + arrayString(type) + ", " +
               qualifierString(qualifier) + " " + samplerString(type) + " sampler_" + name + arrayString(type);        
    }

    return qualifierString(qualifier) + " " + typeString(type) + " " + name + arrayString(type);
}

TString OutputHLSL::interpolationString(TQualifier qualifier)
{
    switch(qualifier)
    {
      case EvqVaryingIn:           return "";
      case EvqFragmentIn:          return "";
      case EvqInvariantVaryingIn:  return "";
      case EvqSmoothIn:            return "linear";
      case EvqFlatIn:              return "nointerpolation";
      case EvqCentroidIn:          return "centroid";
      case EvqVaryingOut:          return "";
      case EvqVertexOut:           return "";
      case EvqInvariantVaryingOut: return "";
      case EvqSmoothOut:           return "linear";
      case EvqFlatOut:             return "nointerpolation";
      case EvqCentroidOut:         return "centroid";
      default: UNREACHABLE();
    }

    return "";
}

TString OutputHLSL::qualifierString(TQualifier qualifier)
{
    switch(qualifier)
    {
      case EvqIn:            return "in";
      case EvqOut:           return "out";
      case EvqInOut:         return "inout";
      case EvqConstReadOnly: return "const";
      default: UNREACHABLE();
    }

    return "";
}

TString OutputHLSL::typeString(const TType &type)
{
    const TStructure* structure = type.getStruct();
    if (structure)
    {
        const TString& typeName = structure->name();
        if (typeName != "")
        {
            return structLookup(typeName);
        }
        else   // Nameless structure, define in place
        {
            return structureString(*structure, false, false);
        }
    }
    else if (type.isMatrix())
    {
        int cols = type.getCols();
        int rows = type.getRows();
        return "float" + str(cols) + "x" + str(rows);
    }
    else
    {
        switch (type.getBasicType())
        {
          case EbtFloat:
            switch (type.getNominalSize())
            {
              case 1: return "float";
              case 2: return "float2";
              case 3: return "float3";
              case 4: return "float4";
            }
          case EbtInt:
            switch (type.getNominalSize())
            {
              case 1: return "int";
              case 2: return "int2";
              case 3: return "int3";
              case 4: return "int4";
            }
          case EbtUInt:
            switch (type.getNominalSize())
            {
              case 1: return "uint";
              case 2: return "uint2";
              case 3: return "uint3";
              case 4: return "uint4";
            }
          case EbtBool:
            switch (type.getNominalSize())
            {
              case 1: return "bool";
              case 2: return "bool2";
              case 3: return "bool3";
              case 4: return "bool4";
            }
          case EbtVoid:
            return "void";
          case EbtSampler2D:
          case EbtISampler2D:
          case EbtUSampler2D:
          case EbtSampler2DArray:
          case EbtISampler2DArray:
          case EbtUSampler2DArray:
            return "sampler2D";
          case EbtSamplerCube:
          case EbtISamplerCube:
          case EbtUSamplerCube:
            return "samplerCUBE";
          case EbtSamplerExternalOES:
            return "sampler2D";
          default:
            break;
        }
    }

    UNREACHABLE();
    return "<unknown type>";
}

TString OutputHLSL::textureString(const TType &type)
{
    switch (type.getBasicType())
    {
      case EbtSampler2D:            return "Texture2D";
      case EbtSamplerCube:          return "TextureCube";
      case EbtSamplerExternalOES:   return "Texture2D";
      case EbtSampler2DArray:       return "Texture2DArray";
      case EbtSampler3D:            return "Texture3D";
      case EbtISampler2D:           return "Texture2D<int4>";
      case EbtISampler3D:           return "Texture3D<int4>";
      case EbtISamplerCube:         return "Texture2DArray<int4>";
      case EbtISampler2DArray:      return "Texture2DArray<int4>";
      case EbtUSampler2D:           return "Texture2D<uint4>";
      case EbtUSampler3D:           return "Texture3D<uint4>";
      case EbtUSamplerCube:         return "Texture2DArray<uint4>";
      case EbtUSampler2DArray:      return "Texture2DArray<uint4>";
      case EbtSampler2DShadow:      return "Texture2D";
      case EbtSamplerCubeShadow:    return "TextureCube";
      case EbtSampler2DArrayShadow: return "Texture2DArray";
      default: UNREACHABLE();
    }
    
    return "<unknown texture type>";
}

TString OutputHLSL::samplerString(const TType &type)
{
    if (IsShadowSampler(type.getBasicType()))
    {
        return "SamplerComparisonState";
    }
    else
    {
        return "SamplerState";
    }
}

TString OutputHLSL::arrayString(const TType &type)
{
    if (!type.isArray())
    {
        return "";
    }

    return "[" + str(type.getArraySize()) + "]";
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

TString OutputHLSL::structureString(const TStructure &structure, bool useHLSLRowMajorPacking, bool useStd140Packing)
{
    const TFieldList &fields = structure.fields();
    const bool isNameless = (structure.name() == "");
    const TString &structName = structureTypeName(structure, useHLSLRowMajorPacking, useStd140Packing);
    const TString declareString = (isNameless ? "struct" : "struct " + structName);

    TString string;
    string += declareString + "\n"
              "{\n";

    int elementIndex = 0;

    for (unsigned int i = 0; i < fields.size(); i++)
    {
        const TField &field = *fields[i];
        const TType &fieldType = *field.type();
        const TStructure *fieldStruct = fieldType.getStruct();
        const TString &fieldTypeString = fieldStruct ? structureTypeName(*fieldStruct, useHLSLRowMajorPacking, useStd140Packing) : typeString(fieldType);

        if (useStd140Packing)
        {
            string += std140PrePaddingString(*field.type(), &elementIndex);
        }

        string += "    " + fieldTypeString + " " + decorateField(field.name(), structure) + arrayString(fieldType) + ";\n";

        if (useStd140Packing)
        {
            string += std140PostPaddingString(*field.type(), useHLSLRowMajorPacking);
        }
    }

    // Nameless structs do not finish with a semicolon and newline, to leave room for an instance variable
    string += (isNameless ? "} " : "};\n");

    // Add remaining element index to the global map, for use with nested structs in standard layouts
    if (useStd140Packing)
    {
        mStd140StructElementIndexes[structName] = elementIndex;
    }

    return string;
}

TString OutputHLSL::structureTypeName(const TStructure &structure, bool useHLSLRowMajorPacking, bool useStd140Packing)
{
    if (structure.name() == "")
    {
        return "";
    }

    TString prefix = "";

    // Structs packed with row-major matrices in HLSL are prefixed with "rm"
    // GLSL column-major maps to HLSL row-major, and the converse is true

    if (useStd140Packing)
    {
        prefix += "std";
    }

    if (useHLSLRowMajorPacking)
    {
        if (prefix != "") prefix += "_";
        prefix += "rm";
    }

    return prefix + structLookup(structure.name());
}

void OutputHLSL::addConstructor(const TType &type, const TString &name, const TIntermSequence *parameters)
{
    if (name == "")
    {
        return;   // Nameless structures don't have constructors
    }

    if (type.getStruct() && mStructNames.find(decorate(name)) != mStructNames.end())
    {
        return;   // Already added
    }

    TType ctorType = type;
    ctorType.clearArrayness();
    ctorType.setPrecision(EbpHigh);
    ctorType.setQualifier(EvqTemporary);

    TString ctorName = type.getStruct() ? decorate(name) : name;

    typedef std::vector<TType> ParameterArray;
    ParameterArray ctorParameters;

    const TStructure* structure = type.getStruct();
    if (structure)
    {
        mStructNames.insert(decorate(name));

        const TString &structString = structureString(*structure, false, false);

        if (std::find(mStructDeclarations.begin(), mStructDeclarations.end(), structString) == mStructDeclarations.end())
        {
            // Add row-major packed struct for interface blocks
            TString rowMajorString = "#pragma pack_matrix(row_major)\n" +
                                     structureString(*structure, true, false) +
                                     "#pragma pack_matrix(column_major)\n";

            TString std140String = structureString(*structure, false, true);
            TString std140RowMajorString = "#pragma pack_matrix(row_major)\n" +
                                           structureString(*structure, true, true) +
                                           "#pragma pack_matrix(column_major)\n";

            mStructDeclarations.push_back(structString);
            mStructDeclarations.push_back(rowMajorString);
            mStructDeclarations.push_back(std140String);
            mStructDeclarations.push_back(std140RowMajorString);
        }

        const TFieldList &fields = structure->fields();
        for (unsigned int i = 0; i < fields.size(); i++)
        {
            ctorParameters.push_back(*fields[i]->type());
        }
    }
    else if (parameters)
    {
        for (TIntermSequence::const_iterator parameter = parameters->begin(); parameter != parameters->end(); parameter++)
        {
            ctorParameters.push_back((*parameter)->getAsTyped()->getType());
        }
    }
    else UNREACHABLE();

    TString constructor;

    if (ctorType.getStruct())
    {
        constructor += ctorName + " " + ctorName + "_ctor(";
    }
    else   // Built-in type
    {
        constructor += typeString(ctorType) + " " + ctorName + "(";
    }

    for (unsigned int parameter = 0; parameter < ctorParameters.size(); parameter++)
    {
        const TType &type = ctorParameters[parameter];

        constructor += typeString(type) + " x" + str(parameter) + arrayString(type);

        if (parameter < ctorParameters.size() - 1)
        {
            constructor += ", ";
        }
    }

    constructor += ")\n"
                   "{\n";

    if (ctorType.getStruct())
    {
        constructor += "    " + ctorName + " structure = {";
    }
    else
    {
        constructor += "    return " + typeString(ctorType) + "(";
    }

    if (ctorType.isMatrix() && ctorParameters.size() == 1)
    {
        int rows = ctorType.getRows();
        int cols = ctorType.getCols();
        const TType &parameter = ctorParameters[0];

        if (parameter.isScalar())
        {
            for (int row = 0; row < rows; row++)
            {
                for (int col = 0; col < cols; col++)
                {
                    constructor += TString((row == col) ? "x0" : "0.0");
                    
                    if (row < rows - 1 || col < cols - 1)
                    {
                        constructor += ", ";
                    }
                }
            }
        }
        else if (parameter.isMatrix())
        {
            for (int row = 0; row < rows; row++)
            {
                for (int col = 0; col < cols; col++)
                {
                    if (row < parameter.getRows() && col < parameter.getCols())
                    {
                        constructor += TString("x0") + "[" + str(row) + "]" + "[" + str(col) + "]";
                    }
                    else
                    {
                        constructor += TString((row == col) ? "1.0" : "0.0");
                    }

                    if (row < rows - 1 || col < cols - 1)
                    {
                        constructor += ", ";
                    }
                }
            }
        }
        else UNREACHABLE();
    }
    else
    {
        size_t remainingComponents = ctorType.getObjectSize();
        size_t parameterIndex = 0;

        while (remainingComponents > 0)
        {
            const TType &parameter = ctorParameters[parameterIndex];
            const size_t parameterSize = parameter.getObjectSize();
            bool moreParameters = parameterIndex + 1 < ctorParameters.size();

            constructor += "x" + str(parameterIndex);

            if (parameter.isScalar())
            {
                remainingComponents -= parameter.getObjectSize();
            }
            else if (parameter.isVector())
            {
                if (remainingComponents == parameterSize || moreParameters)
                {
                    ASSERT(parameterSize <= remainingComponents);
                    remainingComponents -= parameterSize;
                }
                else if (remainingComponents < static_cast<size_t>(parameter.getNominalSize()))
                {
                    switch (remainingComponents)
                    {
                      case 1: constructor += ".x";    break;
                      case 2: constructor += ".xy";   break;
                      case 3: constructor += ".xyz";  break;
                      case 4: constructor += ".xyzw"; break;
                      default: UNREACHABLE();
                    }

                    remainingComponents = 0;
                }
                else UNREACHABLE();
            }
            else if (parameter.isMatrix() || parameter.getStruct())
            {
                ASSERT(remainingComponents == parameterSize || moreParameters);
                ASSERT(parameterSize <= remainingComponents);
                
                remainingComponents -= parameterSize;
            }
            else UNREACHABLE();

            if (moreParameters)
            {
                parameterIndex++;
            }

            if (remainingComponents)
            {
                constructor += ", ";
            }
        }
    }

    if (ctorType.getStruct())
    {
        constructor += "};\n"
                       "    return structure;\n"
                       "}\n";
    }
    else
    {
        constructor += ");\n"
                       "}\n";
    }

    mConstructors.insert(constructor);
}

const ConstantUnion *OutputHLSL::writeConstantUnion(const TType &type, const ConstantUnion *constUnion)
{
    TInfoSinkBase &out = mBody;

    const TStructure* structure = type.getStruct();
    if (structure)
    {
        out << structLookup(structure->name()) + "_ctor(";
        
        const TFieldList& fields = structure->fields();

        for (size_t i = 0; i < fields.size(); i++)
        {
            const TType *fieldType = fields[i]->type();

            constUnion = writeConstantUnion(*fieldType, constUnion);

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
            out << typeString(type) << "(";
        }

        for (size_t i = 0; i < size; i++, constUnion++)
        {
            switch (constUnion->getType())
            {
              case EbtFloat: out << std::min(FLT_MAX, std::max(-FLT_MAX, constUnion->getFConst())); break;
              case EbtInt:   out << constUnion->getIConst(); break;
              case EbtUInt:  out << constUnion->getUConst(); break;
              case EbtBool:  out << constUnion->getBConst(); break;
              default: UNREACHABLE();
            }

            if (i != size - 1)
            {
                out << ", ";
            }
        }

        if (writeType)
        {
            out << ")";
        }
    }

    return constUnion;
}

TString OutputHLSL::scopeString(unsigned int depthLimit)
{
    TString string;

    for (unsigned int i = 0; i < mScopeBracket.size() && i < depthLimit; i++)
    {
        string += "_" + str(i);
    }

    return string;
}

TString OutputHLSL::scopedStruct(const TString &typeName)
{
    if (typeName == "")
    {
        return typeName;
    }

    return typeName + scopeString(mScopeDepth);
}

TString OutputHLSL::structLookup(const TString &typeName)
{
    for (int depth = mScopeDepth; depth >= 0; depth--)
    {
        TString scopedName = decorate(typeName + scopeString(depth));

        for (StructNames::iterator structName = mStructNames.begin(); structName != mStructNames.end(); structName++)
        {
            if (*structName == scopedName)
            {
                return scopedName;
            }
        }
    }

    UNREACHABLE();   // Should have found a matching constructor

    return typeName;
}

TString OutputHLSL::decorate(const TString &string)
{
    if (string.compare(0, 3, "gl_") != 0 && string.compare(0, 3, "dx_") != 0)
    {
        return "_" + string;
    }
    
    return string;
}

TString OutputHLSL::decorateUniform(const TString &string, const TType &type)
{
    if (type.getBasicType() == EbtSamplerExternalOES)
    {
        return "ex_" + string;
    }
    
    return decorate(string);
}

TString OutputHLSL::decorateField(const TString &string, const TStructure &structure)
{
    if (structure.name().compare(0, 3, "gl_") != 0)
    {
        return decorate(string);
    }

    return string;
}

void OutputHLSL::declareInterfaceBlockField(const TType &type, const TString &name, std::vector<gl::InterfaceBlockField>& output)
{
    const TStructure *structure = type.getStruct();

    if (!structure)
    {
        const bool isRowMajorMatrix = (type.isMatrix() && type.getLayoutQualifier().matrixPacking == EmpRowMajor);
        gl::InterfaceBlockField field(glVariableType(type), glVariablePrecision(type), name.c_str(),
                                      (unsigned int)type.getArraySize(), isRowMajorMatrix);
        output.push_back(field);
   }
    else
    {
        gl::InterfaceBlockField structField(GL_STRUCT_ANGLEX, GL_NONE, name.c_str(), (unsigned int)type.getArraySize(), false);

        const TFieldList &fields = structure->fields();

        for (size_t fieldIndex = 0; fieldIndex < fields.size(); fieldIndex++)
        {
            TField *field = fields[fieldIndex];
            TType *fieldType = field->type();

            // make sure to copy matrix packing information
            fieldType->setLayoutQualifier(type.getLayoutQualifier());

            declareInterfaceBlockField(*fieldType, field->name(), structField.fields);
        }

        output.push_back(structField);
    }
}

gl::Uniform OutputHLSL::declareUniformToList(const TType &type, const TString &name, int registerIndex, std::vector<gl::Uniform>& output)
{
    const TStructure *structure = type.getStruct();

    if (!structure)
    {
        gl::Uniform uniform(glVariableType(type), glVariablePrecision(type), name.c_str(),
                            (unsigned int)type.getArraySize(), (unsigned int)registerIndex, 0);
        output.push_back(uniform);

        return uniform;
   }
    else
    {
        gl::Uniform structUniform(GL_STRUCT_ANGLEX, GL_NONE, name.c_str(), (unsigned int)type.getArraySize(),
                                  (unsigned int)registerIndex, GL_INVALID_INDEX);

        const TFieldList &fields = structure->fields();

        for (size_t fieldIndex = 0; fieldIndex < fields.size(); fieldIndex++)
        {
            TField *field = fields[fieldIndex];
            TType *fieldType = field->type();

            declareUniformToList(*fieldType, field->name(), GL_INVALID_INDEX, structUniform.fields);
        }

        // assign register offset information -- this will override the information in any sub-structures.
        HLSLVariableGetRegisterInfo(registerIndex, &structUniform);

        output.push_back(structUniform);

        return structUniform;
    }
}

gl::InterpolationType getInterpolationType(TQualifier qualifier)
{
    switch (qualifier)
    {
      case EvqFlatIn:
      case EvqFlatOut:
        return gl::INTERPOLATION_FLAT;

      case EvqSmoothIn:
      case EvqSmoothOut:
      case EvqVertexOut:
      case EvqFragmentIn:
      case EvqVaryingIn:
      case EvqVaryingOut:
        return gl::INTERPOLATION_SMOOTH;

      case EvqCentroidIn:
      case EvqCentroidOut:
        return gl::INTERPOLATION_CENTROID;

      default: UNREACHABLE();
        return gl::INTERPOLATION_SMOOTH;
    }
}

void OutputHLSL::declareVaryingToList(const TType &type, TQualifier baseTypeQualifier, const TString &name, std::vector<gl::Varying>& fieldsOut)
{
    const TStructure *structure = type.getStruct();

    gl::InterpolationType interpolation = getInterpolationType(baseTypeQualifier);
    if (!structure)
    {
        gl::Varying varying(glVariableType(type), glVariablePrecision(type), name.c_str(), (unsigned int)type.getArraySize(), interpolation);
        fieldsOut.push_back(varying);
    }
    else
    {
        gl::Varying structVarying(GL_STRUCT_ANGLEX, GL_NONE, name.c_str(), (unsigned int)type.getArraySize(), interpolation);
        const TFieldList &fields = structure->fields();

        structVarying.structName = structure->name().c_str();

        for (size_t fieldIndex = 0; fieldIndex < fields.size(); fieldIndex++)
        {
            const TField &field = *fields[fieldIndex];
            declareVaryingToList(*field.type(), baseTypeQualifier, field.name(), structVarying.fields);
        }

        fieldsOut.push_back(structVarying);
    }
}

int OutputHLSL::declareUniformAndAssignRegister(const TType &type, const TString &name)
{
    int registerIndex = (IsSampler(type.getBasicType()) ? mSamplerRegister : mUniformRegister);

    const gl::Uniform &uniform = declareUniformToList(type, name, registerIndex, mActiveUniforms);

    if (IsSampler(type.getBasicType()))
    {
        mSamplerRegister += gl::HLSLVariableRegisterCount(uniform);
    }
    else
    {
        mUniformRegister += gl::HLSLVariableRegisterCount(uniform);
    }

    return registerIndex;
}

GLenum OutputHLSL::glVariableType(const TType &type)
{
    if (type.getBasicType() == EbtFloat)
    {
        if (type.isScalar())
        {
            return GL_FLOAT;
        }
        else if (type.isVector())
        {
            switch(type.getNominalSize())
            {
              case 2: return GL_FLOAT_VEC2;
              case 3: return GL_FLOAT_VEC3;
              case 4: return GL_FLOAT_VEC4;
              default: UNREACHABLE();
            }
        }
        else if (type.isMatrix())
        {
            switch (type.getCols())
            {
              case 2:
                switch(type.getRows())
                {
                  case 2: return GL_FLOAT_MAT2;
                  case 3: return GL_FLOAT_MAT2x3;
                  case 4: return GL_FLOAT_MAT2x4;
                  default: UNREACHABLE();
                }

              case 3:
                switch(type.getRows())
                {
                  case 2: return GL_FLOAT_MAT3x2;
                  case 3: return GL_FLOAT_MAT3;
                  case 4: return GL_FLOAT_MAT3x4;
                  default: UNREACHABLE();
                }

              case 4:
                switch(type.getRows())
                {
                  case 2: return GL_FLOAT_MAT4x2;
                  case 3: return GL_FLOAT_MAT4x3;
                  case 4: return GL_FLOAT_MAT4;
                  default: UNREACHABLE();
                }

              default: UNREACHABLE();
            }
        }
        else UNREACHABLE();
    }
    else if (type.getBasicType() == EbtInt)
    {
        if (type.isScalar())
        {
            return GL_INT;
        }
        else if (type.isVector())
        {
            switch(type.getNominalSize())
            {
              case 2: return GL_INT_VEC2;
              case 3: return GL_INT_VEC3;
              case 4: return GL_INT_VEC4;
              default: UNREACHABLE();
            }
        }
        else UNREACHABLE();
    }
    else if (type.getBasicType() == EbtUInt)
    {
        if (type.isScalar())
        {
            return GL_UNSIGNED_INT;
        }
        else if (type.isVector())
        {
            switch(type.getNominalSize())
            {
              case 2: return GL_UNSIGNED_INT_VEC2;
              case 3: return GL_UNSIGNED_INT_VEC3;
              case 4: return GL_UNSIGNED_INT_VEC4;
              default: UNREACHABLE();
            }
        }
        else UNREACHABLE();
    }
    else if (type.getBasicType() == EbtBool)
    {
        if (type.isScalar())
        {
            return GL_BOOL;
        }
        else if (type.isVector())
        {
            switch(type.getNominalSize())
            {
              case 2: return GL_BOOL_VEC2;
              case 3: return GL_BOOL_VEC3;
              case 4: return GL_BOOL_VEC4;
              default: UNREACHABLE();
            }
        }
        else UNREACHABLE();
    }

    switch(type.getBasicType())
    {
      case EbtSampler2D:            return GL_SAMPLER_2D;
      case EbtSampler3D:            return GL_SAMPLER_3D;
      case EbtSamplerCube:          return GL_SAMPLER_CUBE;
      case EbtSampler2DArray:       return GL_SAMPLER_2D_ARRAY;
      case EbtISampler2D:           return GL_INT_SAMPLER_2D;
      case EbtISampler3D:           return GL_INT_SAMPLER_3D;
      case EbtISamplerCube:         return GL_INT_SAMPLER_CUBE;
      case EbtISampler2DArray:      return GL_INT_SAMPLER_2D_ARRAY;
      case EbtUSampler2D:           return GL_UNSIGNED_INT_SAMPLER_2D;
      case EbtUSampler3D:           return GL_UNSIGNED_INT_SAMPLER_3D;
      case EbtUSamplerCube:         return GL_UNSIGNED_INT_SAMPLER_CUBE;
      case EbtUSampler2DArray:      return GL_UNSIGNED_INT_SAMPLER_2D_ARRAY;
      case EbtSampler2DShadow:      return GL_SAMPLER_2D_SHADOW;
      case EbtSamplerCubeShadow:    return GL_SAMPLER_CUBE_SHADOW;
      case EbtSampler2DArrayShadow: return GL_SAMPLER_2D_ARRAY_SHADOW;
      default: UNREACHABLE();
    }

    return GL_NONE;
}

GLenum OutputHLSL::glVariablePrecision(const TType &type)
{
    if (type.getBasicType() == EbtFloat)
    {
        switch (type.getPrecision())
        {
          case EbpHigh:   return GL_HIGH_FLOAT;
          case EbpMedium: return GL_MEDIUM_FLOAT;
          case EbpLow:    return GL_LOW_FLOAT;
          case EbpUndefined:
            // Should be defined as the default precision by the parser
          default: UNREACHABLE();
        }
    }
    else if (type.getBasicType() == EbtInt || type.getBasicType() == EbtUInt)
    {
        switch (type.getPrecision())
        {
          case EbpHigh:   return GL_HIGH_INT;
          case EbpMedium: return GL_MEDIUM_INT;
          case EbpLow:    return GL_LOW_INT;
          case EbpUndefined:
            // Should be defined as the default precision by the parser
          default: UNREACHABLE();
        }
    }

    // Other types (boolean, sampler) don't have a precision
    return GL_NONE;
}

bool OutputHLSL::isVaryingOut(TQualifier qualifier)
{
    switch(qualifier)
    {
      case EvqVaryingOut:
      case EvqInvariantVaryingOut:
      case EvqSmoothOut:
      case EvqFlatOut:
      case EvqCentroidOut:
      case EvqVertexOut:
        return true;

      default: break;
    }

    return false;
}

bool OutputHLSL::isVaryingIn(TQualifier qualifier)
{
    switch(qualifier)
    {
      case EvqVaryingIn:
      case EvqInvariantVaryingIn:
      case EvqSmoothIn:
      case EvqFlatIn:
      case EvqCentroidIn:
      case EvqFragmentIn:
        return true;

      default: break;
    }

    return false;
}

bool OutputHLSL::isVarying(TQualifier qualifier)
{
    return isVaryingIn(qualifier) || isVaryingOut(qualifier);
}

}
