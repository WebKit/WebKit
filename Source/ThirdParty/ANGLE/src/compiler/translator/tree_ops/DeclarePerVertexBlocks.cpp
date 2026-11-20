//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeclarePerVertexBlocks: Declare gl_PerVertex blocks if not already.
//

#include "compiler/translator/tree_ops/DeclarePerVertexBlocks.h"

#include "compiler/translator/Compiler.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"
#include "compiler/translator/util.h"

namespace sh
{
namespace
{
// Per-member flags of gl_PerVertex, indexed by GetPerVertexMemberIndex().
using PerVertexMemberFlags = std::array<bool, kPerVertexMemberCount>;

// Given the block operand of an EOpIndexDirectInterfaceBlock node, returns the qualifier of the
// block variable being accessed, i.e. EvqPerVertexIn for gl_in[i].X and EvqPerVertexOut for
// gl_out[i].X. Note that the type of an indexed element has the EvqTemporary qualifier, so the
// indexing is unwrapped to find the block variable itself.
TQualifier GetIndexedBlockQualifier(TIntermTyped *blockNode)
{
    while (true)
    {
        TIntermBinary *asBinary = blockNode->getAsBinaryNode();
        if (asBinary == nullptr ||
            (asBinary->getOp() != EOpIndexDirect && asBinary->getOp() != EOpIndexIndirect))
        {
            break;
        }
        blockNode = asBinary->getLeft();
    }
    return blockNode->getType().getQualifier();
}

// Traverser that:
//
// 1. Inspects global qualifier declarations and extracts whether any of the gl_PerVertex built-ins
//    are invariant or precise. These declarations are then dropped.
// 2. Drops gl_ClipDistance and gl_CullDistance array redeclarations.
// 3. Records which of gl_ClipDistance and gl_CullDistance are written by the shader (directly or
//    through gl_out), so that ANGLE's declaration of the output gl_PerVertex block includes only
//    those members.
class InspectPerVertexBuiltInsTraverser : public TIntermTraverser
{
  public:
    InspectPerVertexBuiltInsTraverser(TCompiler *compiler,
                                      TSymbolTable *symbolTable,
                                      PerVertexMemberFlags *invariantFlagsOut,
                                      PerVertexMemberFlags *preciseFlagsOut,
                                      PerVertexMemberFlags *outputMembersOut)
        : TIntermTraverser(true, false, false, symbolTable),
          mInvariantFlagsOut(invariantFlagsOut),
          mPreciseFlagsOut(preciseFlagsOut),
          mOutputMembersOut(outputMembersOut)
    {}

    bool visitGlobalQualifierDeclaration(Visit visit,
                                         TIntermGlobalQualifierDeclaration *node) override
    {
        TIntermSymbol *symbol = node->getSymbol();

        const int fieldIndex = GetPerVertexMemberIndex(symbol->getName());
        if (fieldIndex < 0)
        {
            return false;
        }

        if (node->isInvariant())
        {
            (*mInvariantFlagsOut)[fieldIndex] = true;
        }
        else if (node->isPrecise())
        {
            (*mPreciseFlagsOut)[fieldIndex] = true;
        }

        mMultiReplacements.emplace_back(getParentNode()->getAsBlock(), node, TIntermSequence());

        return false;
    }

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override
    {
        const TIntermSequence &sequence = *(node->getSequence());

        ASSERT(sequence.size() == 1);

        const TIntermSymbol *symbol = sequence.front()->getAsSymbolNode();
        if (symbol == nullptr)
        {
            return true;
        }

        const TType &type = symbol->getType();
        switch (type.getQualifier())
        {
            case EvqClipDistance:
            case EvqCullDistance:
                // Drop the gl_ClipDistance and gl_CullDistance array redeclarations; the built-ins
                // are declared as members of ANGLE's gl_PerVertex block instead.
                mMultiReplacements.emplace_back(getParentNode()->getAsBlock(), node,
                                                TIntermSequence());
                break;
            case EvqPerVertexOut:
                // If the shader redeclared the output gl_PerVertex block, record the invariant and
                // precise qualifiers used on its members so they would be applied to the block
                // declared by ANGLE. The redeclaration itself is dropped by the next traverser.
                for (const TField *field : type.getInterfaceBlock()->fields())
                {
                    const TType &fieldType = *field->type();
                    const int fieldIndex   = GetPerVertexMemberIndex(field->name());
                    ASSERT(fieldIndex >= 0);

                    if (fieldType.isInvariant())
                    {
                        (*mInvariantFlagsOut)[fieldIndex] = true;
                    }
                    if (fieldType.isPrecise())
                    {
                        (*mPreciseFlagsOut)[fieldIndex] = true;
                    }
                }
                break;
            default:
                break;
        }

        return true;
    }

    void visitSymbol(TIntermSymbol *symbol) override
    {
        // gl_ClipDistance and gl_CullDistance referenced directly (not through gl_in/gl_out) are
        // outputs in all the shader stages this pass is run on. Note that this includes the
        // symbol in the declaration of a redeclared array, in which case the built-in is declared
        // in the output block even if not otherwise referenced.
        if (symbol->variable().symbolType() == SymbolType::BuiltIn)
        {
            markPerVertexMemberUsed(symbol->getName());
        }
    }

    bool visitBinary(Visit visit, TIntermBinary *node) override
    {
        if (node->getOp() != EOpIndexDirectInterfaceBlock)
        {
            return true;
        }

        // Track the members written through gl_out (tessellation control shaders).
        if (GetIndexedBlockQualifier(node->getLeft()) != EvqPerVertexOut)
        {
            return true;
        }

        const TInterfaceBlock *block = node->getLeft()->getType().getInterfaceBlock();
        ASSERT(block != nullptr);
        const int fieldIndex = node->getRight()->getAsConstantUnion()->getIConst(0);
        markPerVertexMemberUsed(block->fields()[fieldIndex]->name());

        return true;
    }

  private:
    void markPerVertexMemberUsed(const ImmutableString &name)
    {
        const int memberIndex = GetPerVertexMemberIndex(name);
        if (memberIndex >= 0)
        {
            (*mOutputMembersOut)[memberIndex] = true;
        }
    }

    PerVertexMemberFlags *mInvariantFlagsOut;
    PerVertexMemberFlags *mPreciseFlagsOut;
    PerVertexMemberFlags *mOutputMembersOut;
};

// Traverser that:
//
// 1. Declares the input and output gl_PerVertex types and variables if not already (based on shader
//    type).
// 2. Turns built-in references into indexes into these variables.
//
// The gl_PerVertex blocks declared by ANGLE always include gl_Position and gl_PointSize, but only
// include gl_ClipDistance and gl_CullDistance if used by the shader (sized as used by the shader):
//
// - The output block includes them only if written by the shader, so that clipping is never done
//   against undefined values.
// - The input block includes them only if read through gl_in, sized by the gl_in accesses or
//   redeclaration, independently of the output sizes.
//
// This is different from both the built-in declaration of gl_PerVertex used during parsing (which
// includes every member, with the maximum array sizes) and the shader's own redeclaration of
// gl_PerVertex (which may include any subset of the members, in any order). References to the
// members of gl_in and gl_out are therefore remapped to the corresponding member of the block
// declared by ANGLE.
class DeclarePerVertexBlocksTraverser : public TIntermTraverser
{
  public:
    DeclarePerVertexBlocksTraverser(TCompiler *compiler,
                                    TSymbolTable *symbolTable,
                                    const PerVertexMemberFlags &invariantFlags,
                                    const PerVertexMemberFlags &preciseFlags,
                                    const PerVertexMemberFlags &inputMembers,
                                    const PerVertexMemberFlags &outputMembers,
                                    uint8_t inputClipDistanceArraySize,
                                    uint8_t inputCullDistanceArraySize,
                                    uint8_t clipDistanceArraySize,
                                    uint8_t cullDistanceArraySize)
        : TIntermTraverser(true, false, false, symbolTable),
          mShaderType(compiler->getShaderType()),
          mShaderVersion(compiler->getShaderVersion()),
          mResources(compiler->getResources()),
          mInputMembers(inputMembers),
          mOutputMembers(outputMembers),
          mInputClipDistanceArraySize(inputClipDistanceArraySize),
          mInputCullDistanceArraySize(inputCullDistanceArraySize),
          mClipDistanceArraySize(clipDistanceArraySize),
          mCullDistanceArraySize(cullDistanceArraySize),
          mPerVertexInVar(nullptr),
          mPerVertexOutVar(nullptr),
          mPerVertexOutInvariantFlags(invariantFlags),
          mPerVertexOutPreciseFlags(preciseFlags)
    {}

    void visitSymbol(TIntermSymbol *symbol) override
    {
        const TVariable *variable = &symbol->variable();
        const TType *type         = &variable->getType();

        // Replace gl_out if necessary.
        if (mShaderType == GL_TESS_CONTROL_SHADER && type->getQualifier() == EvqPerVertexOut)
        {
            ASSERT(variable->name() == "gl_out");

            // Declare gl_out if not already.
            if (mPerVertexOutVar == nullptr)
            {
                declareDefaultGlOut();
            }

            replacePerVertexAccessChain(mPerVertexOutVar, false);
            return;
        }

        // Replace gl_in if necessary.
        if ((mShaderType == GL_TESS_CONTROL_SHADER || mShaderType == GL_TESS_EVALUATION_SHADER ||
             mShaderType == GL_GEOMETRY_SHADER) &&
            type->getQualifier() == EvqPerVertexIn)
        {
            ASSERT(variable->name() == "gl_in");

            // Declare gl_in if not already.
            if (mPerVertexInVar == nullptr)
            {
                declareDefaultGlIn();
            }

            replacePerVertexAccessChain(mPerVertexInVar, true);
            return;
        }

        // Turn gl_Position, gl_PointSize, gl_ClipDistance and gl_CullDistance into references to
        // the output gl_PerVertex.  Note that the default gl_PerVertex is declared as follows:
        //
        //     out gl_PerVertex
        //     {
        //         vec4 gl_Position;
        //         float gl_PointSize;
        //         float gl_ClipDistance[];  // Only if used by the shader
        //         float gl_CullDistance[];  // Only if used by the shader
        //     };
        //

        if (variable->symbolType() != SymbolType::BuiltIn)
        {
            ASSERT(variable->name() != "gl_Position" && variable->name() != "gl_PointSize" &&
                   variable->name() != "gl_ClipDistance" && variable->name() != "gl_CullDistance" &&
                   variable->name() != "gl_in" && variable->name() != "gl_out");

            return;
        }

        // If this built-in was already visited, reuse the variable defined for it.
        auto replacement = mVariableMap.find(variable->uniqueId());
        if (replacement != mVariableMap.end())
        {
            queueReplacement(replacement->second->deepCopy(), OriginalNode::IS_DROPPED);
            return;
        }

        const int fieldIndex = GetPerVertexMemberIndex(variable->name());

        // Not the built-in we are looking for.
        if (fieldIndex < 0)
        {
            return;
        }

        // Declare the output gl_PerVertex if not already.
        if (mPerVertexOutVar == nullptr)
        {
            declareDefaultGlOut();
        }

        const TInterfaceBlock *perVertexBlock = mPerVertexOutVar->getType().getInterfaceBlock();
        const uint32_t declaredFieldIndex     = getDeclaredFieldIndex(false, fieldIndex);

        TType *newType = new TType(*perVertexBlock->fields()[declaredFieldIndex]->type());
        newType->setInterfaceBlockField(perVertexBlock, declaredFieldIndex);

        TVariable *newVariable = new TVariable(mSymbolTable, variable->name(), newType,
                                               variable->symbolType(), variable->extensions());

        TIntermSymbol *newSymbol = new TIntermSymbol(newVariable);
        mVariableMap[variable->uniqueId()] = newSymbol;

        queueReplacement(newSymbol, OriginalNode::IS_DROPPED);
    }

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override
    {
        if (!mInGlobalScope)
        {
            return true;
        }

        TIntermSequence *sequence = node->getSequence();
        TIntermSymbol *symbol     = sequence->front()->getAsSymbolNode();
        if (symbol == nullptr)
        {
            return true;
        }

        const TQualifier qualifier = symbol->getType().getQualifier();

        // When EXT_separate_shader_objects is enabled, gl_Position and gl_PointSize are required to
        // be redeclared by the vertex shader. Make sure that is taken into account.
        //
        // The shader may also redeclare gl_PerVertex (gl_in, gl_out or the unnamed output block)
        // with a subset of the members. ANGLE always declares its own gl_PerVertex blocks,
        // so the shader's redeclaration is dropped.
        if (qualifier == EvqPosition || qualifier == EvqPointSize || qualifier == EvqPerVertexIn ||
            qualifier == EvqPerVertexOut)
        {
            mMultiReplacements.emplace_back(getParentNode()->getAsBlock(), node, TIntermSequence());
            return false;
        }

        return true;
    }

    // The gl_PerVertex variables declared by ANGLE, or nullptr if not used by the shader.
    const TVariable *getPerVertexOutVar() const { return mPerVertexOutVar; }
    const TVariable *getPerVertexInVar() const { return mPerVertexInVar; }

  private:
    // Map the logical index of a gl_PerVertex member to its index in the block declared by ANGLE.
    // gl_ClipDistance and gl_CullDistance are only declared if used through the block, so the index
    // of gl_CullDistance depends on whether gl_ClipDistance is declared.
    uint32_t getDeclaredFieldIndex(bool isInput, int fieldIndex) const
    {
        const PerVertexMemberFlags &members = isInput ? mInputMembers : mOutputMembers;
        ASSERT(fieldIndex >= 0 && members[fieldIndex]);

        // The index in the declared block is the number of declared members preceding this one.
        return static_cast<uint32_t>(
            std::count(members.begin(), members.begin() + fieldIndex, true));
    }

    // Replace the gl_in or gl_out symbol being visited with a reference to the block declared by
    // ANGLE. Similar to queueAccessChainReplacement, the EOpIndex[In]Direct chain rooted at the
    // symbol is rebuilt so that the types along the chain are updated. If the chain ends in an
    // EOpIndexDirectInterfaceBlock node (i.e. a member of gl_in/gl_out is accessed), that node is
    // rebuilt as well with the member index remapped to the member of ANGLE's declaration. The
    // member is identified by its qualifier, so this works whether the shader used the built-in
    // declaration of gl_in/gl_out or redeclared them with a subset of the members.
    void replacePerVertexAccessChain(const TVariable *perVertexVar, bool isInput)
    {
        TIntermTyped *replacement = new TIntermSymbol(perVertexVar);
        TIntermNode *toReplace    = nullptr;
        uint32_t ancestorIndex    = 0;

        while (true)
        {
            TIntermNode *ancestor = getAncestorNode(ancestorIndex);
            ASSERT(ancestor != nullptr);

            TIntermBinary *asBinary = ancestor->getAsBinaryNode();
            if (asBinary == nullptr)
            {
                break;
            }

            const TOperator op = asBinary->getOp();
            if (op == EOpIndexDirect || op == EOpIndexIndirect)
            {
                replacement = new TIntermBinary(op, replacement, asBinary->getRight());
                toReplace   = asBinary;
                ++ancestorIndex;
                continue;
            }

            if (op == EOpIndexDirectInterfaceBlock)
            {
                const TInterfaceBlock *originalBlock =
                    asBinary->getLeft()->getType().getInterfaceBlock();
                ASSERT(originalBlock != nullptr);

                const int originalFieldIndex =
                    asBinary->getRight()->getAsConstantUnion()->getIConst(0);
                const TField *field = originalBlock->fields()[originalFieldIndex];

                const int fieldIndex = GetPerVertexMemberIndex(field->name());
                ASSERT(fieldIndex >= 0);

                replacement = new TIntermBinary(
                    EOpIndexDirectInterfaceBlock, replacement,
                    CreateIndexNode(static_cast<int>(getDeclaredFieldIndex(isInput, fieldIndex))));
                toReplace = asBinary;
                ++ancestorIndex;
            }

            break;
        }

        if (toReplace == nullptr)
        {
            queueReplacement(replacement, OriginalNode::IS_DROPPED);
        }
        else
        {
            queueReplacementWithParent(getAncestorNode(ancestorIndex), toReplace, replacement,
                                       OriginalNode::IS_DROPPED);
        }
    }

    const TVariable *declarePerVertex(TQualifier qualifier,
                                      uint32_t arraySize,
                                      ImmutableString &variableName)
    {
        const bool isInput                  = qualifier == EvqPerVertexIn;
        const PerVertexMemberFlags &members = isInput ? mInputMembers : mOutputMembers;

        TFieldList *fields = new TFieldList;

        const TType *vec4Type  = StaticType::GetBasic<EbtFloat, EbpHigh, 4>();
        const TType *floatType = StaticType::GetBasic<EbtFloat, EbpHigh, 1>();

        // gl_ClipDistance and gl_CullDistance are only declared if used. The input and output
        // blocks are sized independently. The compiler ensures each array has the same size
        // throughout the shader, and link-time validation ensures the gl_in sizes match the
        // output sizes of the previous stage.
        const uint8_t clipDistanceArraySize =
            isInput ? mInputClipDistanceArraySize : mClipDistanceArraySize;
        const uint8_t cullDistanceArraySize =
            isInput ? mInputCullDistanceArraySize : mCullDistanceArraySize;
        TType *positionType     = new TType(*vec4Type);
        TType *pointSizeType    = new TType(*floatType);
        TType *clipDistanceType = members[2] ? new TType(*floatType) : nullptr;
        TType *cullDistanceType = members[3] ? new TType(*floatType) : nullptr;

        positionType->setQualifier(EvqPosition);
        pointSizeType->setQualifier(EvqPointSize);
        if (clipDistanceType)
        {
            clipDistanceType->setQualifier(EvqClipDistance);
        }
        if (cullDistanceType)
        {
            cullDistanceType->setQualifier(EvqCullDistance);
        }

        TPrecision pointSizePrecision = EbpHigh;
        if (mShaderType == GL_VERTEX_SHADER)
        {
            // gl_PointSize is mediump in ES100 and highp in ES300+.
            const TVariable *glPointSize = static_cast<const TVariable *>(
                mSymbolTable->findBuiltIn(ImmutableString("gl_PointSize"), mShaderVersion));
            ASSERT(glPointSize);

            pointSizePrecision = glPointSize->getType().getPrecision();
        }
        pointSizeType->setPrecision(pointSizePrecision);

        if (clipDistanceType)
        {
            ASSERT(clipDistanceArraySize > 0);
            clipDistanceType->makeArray(clipDistanceArraySize);
        }
        if (cullDistanceType)
        {
            ASSERT(cullDistanceArraySize > 0);
            cullDistanceType->makeArray(cullDistanceArraySize);
        }

        if (qualifier == EvqPerVertexOut)
        {
            positionType->setInvariant(mPerVertexOutInvariantFlags[0]);
            pointSizeType->setInvariant(mPerVertexOutInvariantFlags[1]);
            if (clipDistanceType)
            {
                clipDistanceType->setInvariant(mPerVertexOutInvariantFlags[2]);
            }
            if (cullDistanceType)
            {
                cullDistanceType->setInvariant(mPerVertexOutInvariantFlags[3]);
            }

            positionType->setPrecise(mPerVertexOutPreciseFlags[0]);
            pointSizeType->setPrecise(mPerVertexOutPreciseFlags[1]);
            if (clipDistanceType)
            {
                clipDistanceType->setPrecise(mPerVertexOutPreciseFlags[2]);
            }
            if (cullDistanceType)
            {
                cullDistanceType->setPrecise(mPerVertexOutPreciseFlags[3]);
            }
        }

        fields->push_back(new TField(positionType, ImmutableString("gl_Position"), TSourceLoc(),
                                     SymbolType::AngleInternal));
        fields->push_back(new TField(pointSizeType, ImmutableString("gl_PointSize"), TSourceLoc(),
                                     SymbolType::AngleInternal));
        if (clipDistanceType)
        {
            fields->push_back(new TField(clipDistanceType, ImmutableString("gl_ClipDistance"),
                                         TSourceLoc(), SymbolType::AngleInternal));
        }
        if (cullDistanceType)
        {
            fields->push_back(new TField(cullDistanceType, ImmutableString("gl_CullDistance"),
                                         TSourceLoc(), SymbolType::AngleInternal));
        }

        TInterfaceBlock *interfaceBlock =
            new TInterfaceBlock(mSymbolTable, ImmutableString("gl_PerVertex"), fields,
                                TLayoutQualifier::Create(), SymbolType::AngleInternal);

        TType *interfaceBlockType =
            new TType(interfaceBlock, qualifier, TLayoutQualifier::Create());
        if (arraySize > 0)
        {
            interfaceBlockType->makeArray(arraySize);
        }

        TVariable *interfaceBlockVar =
            new TVariable(mSymbolTable, variableName, interfaceBlockType,
                          variableName.empty() ? SymbolType::Empty : SymbolType::AngleInternal);

        return interfaceBlockVar;
    }

    void declareDefaultGlOut()
    {
        ASSERT(mPerVertexOutVar == nullptr);

        // For tessellation control shaders, gl_out is an array of MaxPatchVertices
        // For other shaders, there's no explicit name or array size

        ImmutableString varName("");
        uint32_t arraySize = 0;
        if (mShaderType == GL_TESS_CONTROL_SHADER)
        {
            varName   = ImmutableString("gl_out");
            arraySize = mResources.MaxPatchVertices;
        }

        mPerVertexOutVar = declarePerVertex(EvqPerVertexOut, arraySize, varName);
    }

    void declareDefaultGlIn()
    {
        ASSERT(mPerVertexInVar == nullptr);

        // For tessellation shaders, gl_in is an array of MaxPatchVertices.
        // For geometry shaders, gl_in is sized based on the primitive type.

        ImmutableString varName("gl_in");
        uint32_t arraySize = mResources.MaxPatchVertices;
        if (mShaderType == GL_GEOMETRY_SHADER)
        {
            arraySize =
                mSymbolTable->getGlInVariableWithArraySize()->getType().getOutermostArraySize();
        }

        mPerVertexInVar = declarePerVertex(EvqPerVertexIn, arraySize, varName);
    }

    GLenum mShaderType;
    int mShaderVersion;
    const ShBuiltInResources &mResources;
    // Which members are declared in the input and output gl_PerVertex blocks.
    PerVertexMemberFlags mInputMembers;
    PerVertexMemberFlags mOutputMembers;
    uint8_t mInputClipDistanceArraySize;
    uint8_t mInputCullDistanceArraySize;
    uint8_t mClipDistanceArraySize;
    uint8_t mCullDistanceArraySize;

    const TVariable *mPerVertexInVar;
    const TVariable *mPerVertexOutVar;

    // A map of already replaced built-in variables.
    VariableReplacementMap mVariableMap;

    // Whether each field is invariant or precise.
    PerVertexMemberFlags mPerVertexOutInvariantFlags;
    PerVertexMemberFlags mPerVertexOutPreciseFlags;
};

void AddPerVertexDecl(TIntermBlock *root, const TVariable *variable)
{
    if (variable == nullptr)
    {
        return;
    }

    TIntermDeclaration *decl = new TIntermDeclaration;
    TIntermSymbol *symbol    = new TIntermSymbol(variable);
    decl->appendDeclarator(symbol);

    // Insert the declaration before the first function.
    size_t firstFunctionIndex = FindFirstFunctionDefinitionIndex(root);
    root->insertChildNodes(firstFunctionIndex, {decl});
}
}  // anonymous namespace

bool DeclarePerVertexBlocks(TCompiler *compiler,
                            TIntermBlock *root,
                            TSymbolTable *symbolTable,
                            const TVariable **inputPerVertexOut,
                            const TVariable **outputPerVertexOut)
{
    if (compiler->getShaderType() == GL_COMPUTE_SHADER ||
        compiler->getShaderType() == GL_FRAGMENT_SHADER)
    {
        return true;
    }

    // First, visit all global qualifier declarations and find which built-ins are invariant or
    // precise. At the same time, remove gl_ClipDistance and gl_CullDistance array redeclarations.
    PerVertexMemberFlags invariantFlags = {};
    PerVertexMemberFlags preciseFlags   = {};
    // gl_Position and gl_PointSize are always declared in the output block. gl_ClipDistance and
    // gl_CullDistance declared only if written by the shader.
    PerVertexMemberFlags outputMembers = {true, true, false, false};

    InspectPerVertexBuiltInsTraverser infoTraverser(compiler, symbolTable, &invariantFlags,
                                                    &preciseFlags, &outputMembers);
    root->traverse(&infoTraverser);
    if (!infoTraverser.updateTree(compiler, root))
    {
        return false;
    }

    const PerVertexMemberFlags inputMembers = {true, true,
                                               compiler->getInputClipDistanceArraySize() > 0,
                                               compiler->getInputCullDistanceArraySize() > 0};

    // If #pragma STDGL invariant(all) is specified, make all outputs invariant.
    if (compiler->getPragma().stdgl.invariantAll)
    {
        std::fill(invariantFlags.begin(), invariantFlags.end(), true);
    }

    // Then declare the in and out gl_PerVertex I/O blocks.
    DeclarePerVertexBlocksTraverser traverser(
        compiler, symbolTable, invariantFlags, preciseFlags, inputMembers, outputMembers,
        compiler->getInputClipDistanceArraySize(), compiler->getInputCullDistanceArraySize(),
        compiler->getClipDistanceArraySize(), compiler->getCullDistanceArraySize());
    root->traverse(&traverser);
    if (!traverser.updateTree(compiler, root))
    {
        return false;
    }

    AddPerVertexDecl(root, traverser.getPerVertexOutVar());
    AddPerVertexDecl(root, traverser.getPerVertexInVar());

    if (inputPerVertexOut)
    {
        *inputPerVertexOut = traverser.getPerVertexInVar();
    }
    if (outputPerVertexOut)
    {
        *outputPerVertexOut = traverser.getPerVertexOutVar();
    }

    return compiler->validateAST(root);
}
}  // namespace sh
