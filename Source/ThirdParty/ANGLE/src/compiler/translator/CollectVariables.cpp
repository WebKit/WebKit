//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CollectVariables.cpp: Collect lists of shader interface variables based on the AST.

#include "compiler/translator/CollectVariables.h"

#include "angle_gl.h"
#include "common/utilities.h"
#include "compiler/translator/HashNames.h"
#include "compiler/translator/IntermTraverse.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{

BlockLayoutType GetBlockLayoutType(TLayoutBlockStorage blockStorage)
{
    switch (blockStorage)
    {
        case EbsPacked:
            return BLOCKLAYOUT_PACKED;
        case EbsShared:
            return BLOCKLAYOUT_SHARED;
        case EbsStd140:
            return BLOCKLAYOUT_STD140;
        case EbsStd430:
            return BLOCKLAYOUT_STD430;
        default:
            UNREACHABLE();
            return BLOCKLAYOUT_SHARED;
    }
}

// TODO(jiawei.shao@intel.com): implement GL_OES_shader_io_blocks.
BlockType GetBlockType(TQualifier qualifier)
{
    switch (qualifier)
    {
        case EvqUniform:
            return BlockType::BLOCK_UNIFORM;
        case EvqBuffer:
            return BlockType::BLOCK_BUFFER;
        case EvqPerVertexIn:
            return BlockType::BLOCK_IN;
        default:
            UNREACHABLE();
            return BlockType::BLOCK_UNIFORM;
    }
}

template <class VarT>
VarT *FindVariable(const TString &name, std::vector<VarT> *infoList)
{
    // TODO(zmo): optimize this function.
    for (size_t ii = 0; ii < infoList->size(); ++ii)
    {
        if ((*infoList)[ii].name.c_str() == name)
            return &((*infoList)[ii]);
    }

    return nullptr;
}

// Note that this shouldn't be called for interface blocks - static use information is collected for
// individual fields in case of interface blocks.
void MarkStaticallyUsed(ShaderVariable *variable)
{
    if (!variable->staticUse)
    {
        if (variable->isStruct())
        {
            // Conservatively assume all fields are statically used as well.
            for (auto &field : variable->fields)
            {
                MarkStaticallyUsed(&field);
            }
        }
        variable->staticUse = true;
    }
}

ShaderVariable *FindVariableInInterfaceBlock(const TString &name,
                                             const TInterfaceBlock *interfaceBlock,
                                             std::vector<InterfaceBlock> *infoList)
{
    ASSERT(interfaceBlock);
    InterfaceBlock *namedBlock = FindVariable(interfaceBlock->name(), infoList);
    ASSERT(namedBlock);

    // Set static use on the parent interface block here
    namedBlock->staticUse = true;
    return FindVariable(name, &namedBlock->fields);
}

// Traverses the intermediate tree to collect all attributes, uniforms, varyings, fragment outputs,
// and interface blocks.
class CollectVariablesTraverser : public TIntermTraverser
{
  public:
    CollectVariablesTraverser(std::vector<Attribute> *attribs,
                              std::vector<OutputVariable> *outputVariables,
                              std::vector<Uniform> *uniforms,
                              std::vector<Varying> *inputVaryings,
                              std::vector<Varying> *outputVaryings,
                              std::vector<InterfaceBlock> *uniformBlocks,
                              std::vector<InterfaceBlock> *shaderStorageBlocks,
                              std::vector<InterfaceBlock> *inBlocks,
                              ShHashFunction64 hashFunction,
                              TSymbolTable *symbolTable,
                              int shaderVersion,
                              GLenum shaderType,
                              const TExtensionBehavior &extensionBehavior);

    void visitSymbol(TIntermSymbol *symbol) override;
    bool visitDeclaration(Visit, TIntermDeclaration *node) override;
    bool visitBinary(Visit visit, TIntermBinary *binaryNode) override;

  private:
    std::string getMappedName(const TName &name) const;

    void setCommonVariableProperties(const TType &type,
                                     const TName &name,
                                     ShaderVariable *variableOut) const;

    Attribute recordAttribute(const TIntermSymbol &variable) const;
    OutputVariable recordOutputVariable(const TIntermSymbol &variable) const;
    Varying recordVarying(const TIntermSymbol &variable) const;
    void recordInterfaceBlock(const TType &interfaceBlockType,
                              InterfaceBlock *interfaceBlock) const;
    Uniform recordUniform(const TIntermSymbol &variable) const;

    void setBuiltInInfoFromSymbolTable(const char *name, ShaderVariable *info);

    void recordBuiltInVaryingUsed(const char *name,
                                  bool *addedFlag,
                                  std::vector<Varying> *varyings);
    void recordBuiltInFragmentOutputUsed(const char *name, bool *addedFlag);
    void recordBuiltInAttributeUsed(const char *name, bool *addedFlag);
    InterfaceBlock *recordGLInUsed(const TType &glInType);
    InterfaceBlock *findNamedInterfaceBlock(const TString &name) const;

    std::vector<Attribute> *mAttribs;
    std::vector<OutputVariable> *mOutputVariables;
    std::vector<Uniform> *mUniforms;
    std::vector<Varying> *mInputVaryings;
    std::vector<Varying> *mOutputVaryings;
    std::vector<InterfaceBlock> *mUniformBlocks;
    std::vector<InterfaceBlock> *mShaderStorageBlocks;
    std::vector<InterfaceBlock> *mInBlocks;

    std::map<std::string, InterfaceBlockField *> mInterfaceBlockFields;

    // Shader uniforms
    bool mDepthRangeAdded;

    // Vertex Shader builtins
    bool mInstanceIDAdded;
    bool mVertexIDAdded;
    bool mPointSizeAdded;

    // Vertex Shader and Geometry Shader builtins
    bool mPositionAdded;

    // Fragment Shader builtins
    bool mPointCoordAdded;
    bool mFrontFacingAdded;
    bool mFragCoordAdded;
    bool mLastFragDataAdded;
    bool mFragColorAdded;
    bool mFragDataAdded;
    bool mFragDepthEXTAdded;
    bool mFragDepthAdded;
    bool mSecondaryFragColorEXTAdded;
    bool mSecondaryFragDataEXTAdded;

    // Geometry Shader builtins
    bool mPerVertexInAdded;
    bool mPrimitiveIDInAdded;
    bool mInvocationIDAdded;

    // Geometry Shader and Fragment Shader builtins
    bool mPrimitiveIDAdded;
    bool mLayerAdded;

    ShHashFunction64 mHashFunction;

    int mShaderVersion;
    GLenum mShaderType;
    const TExtensionBehavior &mExtensionBehavior;
};

CollectVariablesTraverser::CollectVariablesTraverser(
    std::vector<sh::Attribute> *attribs,
    std::vector<sh::OutputVariable> *outputVariables,
    std::vector<sh::Uniform> *uniforms,
    std::vector<sh::Varying> *inputVaryings,
    std::vector<sh::Varying> *outputVaryings,
    std::vector<sh::InterfaceBlock> *uniformBlocks,
    std::vector<sh::InterfaceBlock> *shaderStorageBlocks,
    std::vector<sh::InterfaceBlock> *inBlocks,
    ShHashFunction64 hashFunction,
    TSymbolTable *symbolTable,
    int shaderVersion,
    GLenum shaderType,
    const TExtensionBehavior &extensionBehavior)
    : TIntermTraverser(true, false, false, symbolTable),
      mAttribs(attribs),
      mOutputVariables(outputVariables),
      mUniforms(uniforms),
      mInputVaryings(inputVaryings),
      mOutputVaryings(outputVaryings),
      mUniformBlocks(uniformBlocks),
      mShaderStorageBlocks(shaderStorageBlocks),
      mInBlocks(inBlocks),
      mDepthRangeAdded(false),
      mInstanceIDAdded(false),
      mVertexIDAdded(false),
      mPointSizeAdded(false),
      mPositionAdded(false),
      mPointCoordAdded(false),
      mFrontFacingAdded(false),
      mFragCoordAdded(false),
      mLastFragDataAdded(false),
      mFragColorAdded(false),
      mFragDataAdded(false),
      mFragDepthEXTAdded(false),
      mFragDepthAdded(false),
      mSecondaryFragColorEXTAdded(false),
      mSecondaryFragDataEXTAdded(false),
      mPerVertexInAdded(false),
      mPrimitiveIDInAdded(false),
      mInvocationIDAdded(false),
      mPrimitiveIDAdded(false),
      mLayerAdded(false),
      mHashFunction(hashFunction),
      mShaderVersion(shaderVersion),
      mShaderType(shaderType),
      mExtensionBehavior(extensionBehavior)
{
}

std::string CollectVariablesTraverser::getMappedName(const TName &name) const
{
    return HashName(name, mHashFunction, nullptr).c_str();
}

void CollectVariablesTraverser::setBuiltInInfoFromSymbolTable(const char *name,
                                                              ShaderVariable *info)
{
    TVariable *symbolTableVar =
        reinterpret_cast<TVariable *>(mSymbolTable->findBuiltIn(name, mShaderVersion));
    ASSERT(symbolTableVar);
    const TType &type = symbolTableVar->getType();

    info->name       = name;
    info->mappedName = name;
    info->type       = GLVariableType(type);
    info->precision = GLVariablePrecision(type);
    if (auto *arraySizes = type.getArraySizes())
    {
        info->arraySizes.assign(arraySizes->begin(), arraySizes->end());
    }
}

void CollectVariablesTraverser::recordBuiltInVaryingUsed(const char *name,
                                                         bool *addedFlag,
                                                         std::vector<Varying> *varyings)
{
    ASSERT(varyings);
    if (!(*addedFlag))
    {
        Varying info;
        setBuiltInInfoFromSymbolTable(name, &info);
        info.staticUse   = true;
        info.isInvariant = mSymbolTable->isVaryingInvariant(name);
        varyings->push_back(info);
        (*addedFlag) = true;
    }
}

void CollectVariablesTraverser::recordBuiltInFragmentOutputUsed(const char *name, bool *addedFlag)
{
    if (!(*addedFlag))
    {
        OutputVariable info;
        setBuiltInInfoFromSymbolTable(name, &info);
        info.staticUse = true;
        mOutputVariables->push_back(info);
        (*addedFlag) = true;
    }
}

void CollectVariablesTraverser::recordBuiltInAttributeUsed(const char *name, bool *addedFlag)
{
    if (!(*addedFlag))
    {
        Attribute info;
        setBuiltInInfoFromSymbolTable(name, &info);
        info.staticUse = true;
        info.location  = -1;
        mAttribs->push_back(info);
        (*addedFlag) = true;
    }
}

InterfaceBlock *CollectVariablesTraverser::recordGLInUsed(const TType &glInType)
{
    if (!mPerVertexInAdded)
    {
        ASSERT(glInType.getQualifier() == EvqPerVertexIn);
        InterfaceBlock info;
        recordInterfaceBlock(glInType, &info);
        info.staticUse = true;

        mPerVertexInAdded = true;
        mInBlocks->push_back(info);
        return &mInBlocks->back();
    }
    else
    {
        return FindVariable("gl_PerVertex", mInBlocks);
    }
}

// We want to check whether a uniform/varying is statically used
// because we only count the used ones in packing computing.
// Also, gl_FragCoord, gl_PointCoord, and gl_FrontFacing count
// toward varying counting if they are statically used in a fragment
// shader.
void CollectVariablesTraverser::visitSymbol(TIntermSymbol *symbol)
{
    ASSERT(symbol != nullptr);

    if (symbol->getName().isInternal())
    {
        // Internal variables are not collected.
        return;
    }

    ShaderVariable *var       = nullptr;
    const TString &symbolName = symbol->getName().getString();

    if (IsVaryingIn(symbol->getQualifier()))
    {
        var = FindVariable(symbolName, mInputVaryings);
    }
    else if (IsVaryingOut(symbol->getQualifier()))
    {
        var = FindVariable(symbolName, mOutputVaryings);
    }
    else if (symbol->getType().getBasicType() == EbtInterfaceBlock)
    {
        UNREACHABLE();
    }
    else if (symbolName == "gl_DepthRange")
    {
        ASSERT(symbol->getQualifier() == EvqUniform);

        if (!mDepthRangeAdded)
        {
            Uniform info;
            const char kName[] = "gl_DepthRange";
            info.name          = kName;
            info.mappedName    = kName;
            info.type          = GL_NONE;
            info.precision     = GL_NONE;
            info.staticUse     = true;

            ShaderVariable nearInfo(GL_FLOAT);
            const char kNearName[] = "near";
            nearInfo.name          = kNearName;
            nearInfo.mappedName    = kNearName;
            nearInfo.precision     = GL_HIGH_FLOAT;
            nearInfo.staticUse     = true;

            ShaderVariable farInfo(GL_FLOAT);
            const char kFarName[] = "far";
            farInfo.name          = kFarName;
            farInfo.mappedName    = kFarName;
            farInfo.precision     = GL_HIGH_FLOAT;
            farInfo.staticUse     = true;

            ShaderVariable diffInfo(GL_FLOAT);
            const char kDiffName[] = "diff";
            diffInfo.name          = kDiffName;
            diffInfo.mappedName    = kDiffName;
            diffInfo.precision     = GL_HIGH_FLOAT;
            diffInfo.staticUse     = true;

            info.fields.push_back(nearInfo);
            info.fields.push_back(farInfo);
            info.fields.push_back(diffInfo);

            mUniforms->push_back(info);
            mDepthRangeAdded = true;
        }
    }
    else
    {
        switch (symbol->getQualifier())
        {
            case EvqAttribute:
            case EvqVertexIn:
                var = FindVariable(symbolName, mAttribs);
                break;
            case EvqFragmentOut:
                var = FindVariable(symbolName, mOutputVariables);
                break;
            case EvqUniform:
            {
                const TInterfaceBlock *interfaceBlock = symbol->getType().getInterfaceBlock();
                if (interfaceBlock)
                {
                    var = FindVariableInInterfaceBlock(symbolName, interfaceBlock, mUniformBlocks);
                }
                else
                {
                    var = FindVariable(symbolName, mUniforms);
                }

                // It's an internal error to reference an undefined user uniform
                ASSERT(symbolName.compare(0, 3, "gl_") != 0 || var);
            }
            break;
            case EvqBuffer:
            {
                const TInterfaceBlock *interfaceBlock = symbol->getType().getInterfaceBlock();
                var =
                    FindVariableInInterfaceBlock(symbolName, interfaceBlock, mShaderStorageBlocks);
            }
            break;
            case EvqFragCoord:
                recordBuiltInVaryingUsed("gl_FragCoord", &mFragCoordAdded, mInputVaryings);
                return;
            case EvqFrontFacing:
                recordBuiltInVaryingUsed("gl_FrontFacing", &mFrontFacingAdded, mInputVaryings);
                return;
            case EvqPointCoord:
                recordBuiltInVaryingUsed("gl_PointCoord", &mPointCoordAdded, mInputVaryings);
                return;
            case EvqInstanceID:
                // Whenever the SH_INITIALIZE_BUILTINS_FOR_INSTANCED_MULTIVIEW option is set,
                // gl_InstanceID is added inside expressions to initialize ViewID_OVR and
                // InstanceID. gl_InstanceID is not added to the symbol table for ESSL1 shaders
                // which makes it necessary to populate the type information explicitly instead of
                // extracting it from the symbol table.
                if (!mInstanceIDAdded)
                {
                    Attribute info;
                    const char kName[] = "gl_InstanceID";
                    info.name          = kName;
                    info.mappedName    = kName;
                    info.type          = GL_INT;
                    info.precision     = GL_HIGH_INT;  // Defined by spec.
                    info.staticUse     = true;
                    info.location      = -1;
                    mAttribs->push_back(info);
                    mInstanceIDAdded = true;
                }
                return;
            case EvqVertexID:
                recordBuiltInAttributeUsed("gl_VertexID", &mVertexIDAdded);
                return;
            case EvqPosition:
                recordBuiltInVaryingUsed("gl_Position", &mPositionAdded, mOutputVaryings);
                return;
            case EvqPointSize:
                recordBuiltInVaryingUsed("gl_PointSize", &mPointSizeAdded, mOutputVaryings);
                return;
            case EvqLastFragData:
                recordBuiltInVaryingUsed("gl_LastFragData", &mLastFragDataAdded, mInputVaryings);
                return;
            case EvqFragColor:
                recordBuiltInFragmentOutputUsed("gl_FragColor", &mFragColorAdded);
                return;
            case EvqFragData:
                if (!mFragDataAdded)
                {
                    OutputVariable info;
                    setBuiltInInfoFromSymbolTable("gl_FragData", &info);
                    if (!IsExtensionEnabled(mExtensionBehavior, TExtension::EXT_draw_buffers))
                    {
                        ASSERT(info.arraySizes.size() == 1u);
                        info.arraySizes.back() = 1u;
                    }
                    info.staticUse = true;
                    mOutputVariables->push_back(info);
                    mFragDataAdded = true;
                }
                return;
            case EvqFragDepthEXT:
                recordBuiltInFragmentOutputUsed("gl_FragDepthEXT", &mFragDepthEXTAdded);
                return;
            case EvqFragDepth:
                recordBuiltInFragmentOutputUsed("gl_FragDepth", &mFragDepthAdded);
                return;
            case EvqSecondaryFragColorEXT:
                recordBuiltInFragmentOutputUsed("gl_SecondaryFragColorEXT",
                                                &mSecondaryFragColorEXTAdded);
                return;
            case EvqSecondaryFragDataEXT:
                recordBuiltInFragmentOutputUsed("gl_SecondaryFragDataEXT",
                                                &mSecondaryFragDataEXTAdded);
                return;
            case EvqInvocationID:
                recordBuiltInVaryingUsed("gl_InvocationID", &mInvocationIDAdded, mInputVaryings);
                break;
            case EvqPrimitiveIDIn:
                recordBuiltInVaryingUsed("gl_PrimitiveIDIn", &mPrimitiveIDInAdded, mInputVaryings);
                break;
            case EvqPrimitiveID:
                if (mShaderType == GL_GEOMETRY_SHADER_OES)
                {
                    recordBuiltInVaryingUsed("gl_PrimitiveID", &mPrimitiveIDAdded, mOutputVaryings);
                }
                else
                {
                    ASSERT(mShaderType == GL_FRAGMENT_SHADER);
                    recordBuiltInVaryingUsed("gl_PrimitiveID", &mPrimitiveIDAdded, mInputVaryings);
                }
                break;
            case EvqLayer:
                if (mShaderType == GL_GEOMETRY_SHADER_OES)
                {
                    recordBuiltInVaryingUsed("gl_Layer", &mLayerAdded, mOutputVaryings);
                }
                else if (mShaderType == GL_FRAGMENT_SHADER)
                {
                    recordBuiltInVaryingUsed("gl_Layer", &mLayerAdded, mInputVaryings);
                }
                else
                {
                    ASSERT(mShaderType == GL_VERTEX_SHADER &&
                           IsExtensionEnabled(mExtensionBehavior, TExtension::OVR_multiview));
                }
                break;
            default:
                break;
        }
    }
    if (var)
    {
        MarkStaticallyUsed(var);
    }
}

void CollectVariablesTraverser::setCommonVariableProperties(const TType &type,
                                                            const TName &name,
                                                            ShaderVariable *variableOut) const
{
    ASSERT(variableOut);

    const TStructure *structure = type.getStruct();

    if (!structure)
    {
        variableOut->type      = GLVariableType(type);
        variableOut->precision = GLVariablePrecision(type);
    }
    else
    {
        // Structures use a NONE type that isn't exposed outside ANGLE.
        variableOut->type       = GL_NONE;
        variableOut->structName = structure->name().c_str();

        const TFieldList &fields = structure->fields();

        for (TField *field : fields)
        {
            // Regardless of the variable type (uniform, in/out etc.) its fields are always plain
            // ShaderVariable objects.
            ShaderVariable fieldVariable;
            setCommonVariableProperties(*field->type(), TName(field->name()), &fieldVariable);
            variableOut->fields.push_back(fieldVariable);
        }
    }
    variableOut->name       = name.getString().c_str();
    variableOut->mappedName = getMappedName(name);

    if (auto *arraySizes = type.getArraySizes())
    {
        variableOut->arraySizes.assign(arraySizes->begin(), arraySizes->end());
    }
}

Attribute CollectVariablesTraverser::recordAttribute(const TIntermSymbol &variable) const
{
    const TType &type = variable.getType();
    ASSERT(!type.getStruct());

    Attribute attribute;
    setCommonVariableProperties(type, variable.getName(), &attribute);

    attribute.location = type.getLayoutQualifier().location;
    return attribute;
}

OutputVariable CollectVariablesTraverser::recordOutputVariable(const TIntermSymbol &variable) const
{
    const TType &type = variable.getType();
    ASSERT(!type.getStruct());

    OutputVariable outputVariable;
    setCommonVariableProperties(type, variable.getName(), &outputVariable);

    outputVariable.location = type.getLayoutQualifier().location;
    return outputVariable;
}

Varying CollectVariablesTraverser::recordVarying(const TIntermSymbol &variable) const
{
    const TType &type = variable.getType();

    Varying varying;
    setCommonVariableProperties(type, variable.getName(), &varying);
    varying.location = type.getLayoutQualifier().location;

    switch (type.getQualifier())
    {
        case EvqVaryingIn:
        case EvqVaryingOut:
        case EvqVertexOut:
        case EvqSmoothOut:
        case EvqFlatOut:
        case EvqCentroidOut:
        case EvqGeometryOut:
            if (mSymbolTable->isVaryingInvariant(std::string(variable.getSymbol().c_str())) ||
                type.isInvariant())
            {
                varying.isInvariant = true;
            }
            break;
        default:
            break;
    }

    varying.interpolation = GetInterpolationType(type.getQualifier());
    return varying;
}

// TODO(jiawei.shao@intel.com): implement GL_OES_shader_io_blocks.
void CollectVariablesTraverser::recordInterfaceBlock(const TType &interfaceBlockType,
                                                     InterfaceBlock *interfaceBlock) const
{
    ASSERT(interfaceBlockType.getBasicType() == EbtInterfaceBlock);
    ASSERT(interfaceBlock);

    const TInterfaceBlock *blockType = interfaceBlockType.getInterfaceBlock();
    ASSERT(blockType);

    interfaceBlock->name       = blockType->name().c_str();
    interfaceBlock->mappedName = getMappedName(TName(blockType->name()));
    interfaceBlock->instanceName =
        (blockType->hasInstanceName() ? blockType->instanceName().c_str() : "");
    ASSERT(!interfaceBlockType.isArrayOfArrays());  // Disallowed by GLSL ES 3.10 section 4.3.9
    interfaceBlock->arraySize = interfaceBlockType.isArray() ? interfaceBlockType.getOutermostArraySize() : 0;

    interfaceBlock->blockType = GetBlockType(interfaceBlockType.getQualifier());
    if (interfaceBlock->blockType == BlockType::BLOCK_UNIFORM ||
        interfaceBlock->blockType == BlockType::BLOCK_BUFFER)
    {
        interfaceBlock->isRowMajorLayout = (blockType->matrixPacking() == EmpRowMajor);
        interfaceBlock->binding          = blockType->blockBinding();
        interfaceBlock->layout           = GetBlockLayoutType(blockType->blockStorage());
    }

    // Gather field information
    for (const TField *field : blockType->fields())
    {
        const TType &fieldType = *field->type();

        InterfaceBlockField fieldVariable;
        setCommonVariableProperties(fieldType, TName(field->name()), &fieldVariable);
        fieldVariable.isRowMajorLayout =
            (fieldType.getLayoutQualifier().matrixPacking == EmpRowMajor);
        interfaceBlock->fields.push_back(fieldVariable);
    }
}

Uniform CollectVariablesTraverser::recordUniform(const TIntermSymbol &variable) const
{
    Uniform uniform;
    setCommonVariableProperties(variable.getType(), variable.getName(), &uniform);
    uniform.binding  = variable.getType().getLayoutQualifier().binding;
    uniform.location = variable.getType().getLayoutQualifier().location;
    uniform.offset   = variable.getType().getLayoutQualifier().offset;
    return uniform;
}

bool CollectVariablesTraverser::visitDeclaration(Visit, TIntermDeclaration *node)
{
    const TIntermSequence &sequence = *(node->getSequence());
    ASSERT(!sequence.empty());

    const TIntermTyped &typedNode = *(sequence.front()->getAsTyped());
    TQualifier qualifier          = typedNode.getQualifier();

    bool isShaderVariable = qualifier == EvqAttribute || qualifier == EvqVertexIn ||
                            qualifier == EvqFragmentOut || qualifier == EvqUniform ||
                            IsVarying(qualifier);

    if (typedNode.getBasicType() != EbtInterfaceBlock && !isShaderVariable)
    {
        return true;
    }

    for (TIntermNode *variableNode : sequence)
    {
        // The only case in which the sequence will not contain a TIntermSymbol node is
        // initialization. It will contain a TInterBinary node in that case. Since attributes,
        // uniforms, varyings, outputs and interface blocks cannot be initialized in a shader, we
        // must have only TIntermSymbol nodes in the sequence in the cases we are interested in.
        const TIntermSymbol &variable = *variableNode->getAsSymbolNode();
        if (variable.getName().isInternal())
        {
            // Internal variables are not collected.
            continue;
        }

        // TODO(jiawei.shao@intel.com): implement GL_OES_shader_io_blocks.
        if (typedNode.getBasicType() == EbtInterfaceBlock)
        {
            InterfaceBlock interfaceBlock;
            recordInterfaceBlock(variable.getType(), &interfaceBlock);

            switch (qualifier)
            {
                case EvqUniform:
                    mUniformBlocks->push_back(interfaceBlock);
                    break;
                case EvqBuffer:
                    mShaderStorageBlocks->push_back(interfaceBlock);
                    break;
                default:
                    UNREACHABLE();
            }
        }
        else
        {
            switch (qualifier)
            {
                case EvqAttribute:
                case EvqVertexIn:
                    mAttribs->push_back(recordAttribute(variable));
                    break;
                case EvqFragmentOut:
                    mOutputVariables->push_back(recordOutputVariable(variable));
                    break;
                case EvqUniform:
                    mUniforms->push_back(recordUniform(variable));
                    break;
                default:
                    if (IsVaryingIn(qualifier))
                    {
                        mInputVaryings->push_back(recordVarying(variable));
                    }
                    else
                    {
                        ASSERT(IsVaryingOut(qualifier));
                        mOutputVaryings->push_back(recordVarying(variable));
                    }
                    break;
            }
        }
    }

    // None of the recorded variables can have initializers, so we don't need to traverse the
    // declarators.
    return false;
}

// TODO(jiawei.shao@intel.com): add search on mInBlocks and mOutBlocks when implementing
// GL_OES_shader_io_blocks.
InterfaceBlock *CollectVariablesTraverser::findNamedInterfaceBlock(const TString &blockName) const
{
    InterfaceBlock *namedBlock = FindVariable(blockName, mUniformBlocks);
    if (!namedBlock)
    {
        namedBlock = FindVariable(blockName, mShaderStorageBlocks);
    }
    return namedBlock;
}

bool CollectVariablesTraverser::visitBinary(Visit, TIntermBinary *binaryNode)
{
    if (binaryNode->getOp() == EOpIndexDirectInterfaceBlock)
    {
        // NOTE: we do not determine static use for individual blocks of an array
        TIntermTyped *blockNode = binaryNode->getLeft()->getAsTyped();
        ASSERT(blockNode);

        TIntermConstantUnion *constantUnion = binaryNode->getRight()->getAsConstantUnion();
        ASSERT(constantUnion);

        InterfaceBlock *namedBlock = nullptr;

        bool traverseIndexExpression         = false;
        TIntermBinary *interfaceIndexingNode = blockNode->getAsBinaryNode();
        if (interfaceIndexingNode)
        {
            TIntermTyped *interfaceNode = interfaceIndexingNode->getLeft()->getAsTyped();
            ASSERT(interfaceNode);

            const TType &interfaceType = interfaceNode->getType();
            if (interfaceType.getQualifier() == EvqPerVertexIn)
            {
                namedBlock = recordGLInUsed(interfaceType);
                ASSERT(namedBlock);

                // We need to continue traversing to collect useful variables in the index
                // expression of gl_in.
                traverseIndexExpression = true;
            }
        }

        const TInterfaceBlock *interfaceBlock = blockNode->getType().getInterfaceBlock();
        if (!namedBlock)
        {
            namedBlock = findNamedInterfaceBlock(interfaceBlock->name());
        }
        ASSERT(namedBlock);
        namedBlock->staticUse   = true;
        unsigned int fieldIndex = static_cast<unsigned int>(constantUnion->getIConst(0));
        ASSERT(fieldIndex < namedBlock->fields.size());
        namedBlock->fields[fieldIndex].staticUse = true;

        if (traverseIndexExpression)
        {
            ASSERT(interfaceIndexingNode);
            interfaceIndexingNode->getRight()->traverse(this);
        }
        return false;
    }

    return true;
}

}  // anonymous namespace

void CollectVariables(TIntermBlock *root,
                      std::vector<Attribute> *attributes,
                      std::vector<OutputVariable> *outputVariables,
                      std::vector<Uniform> *uniforms,
                      std::vector<Varying> *inputVaryings,
                      std::vector<Varying> *outputVaryings,
                      std::vector<InterfaceBlock> *uniformBlocks,
                      std::vector<InterfaceBlock> *shaderStorageBlocks,
                      std::vector<InterfaceBlock> *inBlocks,
                      ShHashFunction64 hashFunction,
                      TSymbolTable *symbolTable,
                      int shaderVersion,
                      GLenum shaderType,
                      const TExtensionBehavior &extensionBehavior)
{
    CollectVariablesTraverser collect(attributes, outputVariables, uniforms, inputVaryings,
                                      outputVaryings, uniformBlocks, shaderStorageBlocks, inBlocks,
                                      hashFunction, symbolTable, shaderVersion, shaderType,
                                      extensionBehavior);
    root->traverse(&collect);
}

}  // namespace sh
