//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ReplaceClipCullDistanceVariable.cpp: Find any references to gl_ClipDistance or gl_CullDistance
// and replace it with ANGLEClipDistance or ANGLECullDistance.
//

#include "compiler/translator/tree_util/ReplaceClipCullDistanceVariable.h"

#include "common/bitset_utils.h"
#include "common/debug.h"
#include "common/utilities.h"
#include "compiler/translator/Compiler.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/BuiltIn.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"
#include "compiler/translator/tree_util/RunAtTheBeginningOfShader.h"
#include "compiler/translator/tree_util/RunAtTheEndOfShader.h"

namespace sh
{
namespace
{

using ClipCullDistanceIdxSet = angle::BitSet<32>;

typedef TIntermNode *AssignFunc(const unsigned int index,
                                TIntermSymbol *left,
                                TIntermSymbol *right,
                                const TIntermTyped *enableFlags);

template <typename Variable>
const Variable *FindVariable(const std::vector<Variable> &mVars, const ImmutableString &name)
{
    for (const Variable &var : mVars)
    {
        if (name == var.instanceName)
        {
            return &var;
        }
    }

    return nullptr;
}

// Traverse the tree and collect the redeclaration and all constant index references of
// gl_ClipDistance/gl_CullDistance
class GLClipCullDistanceReferenceTraverser : public TIntermTraverser
{
  public:
    GLClipCullDistanceReferenceTraverser(const TIntermSymbol **redeclaredSymOut,
                                         bool *nonConstIdxUsedOut,
                                         unsigned int *maxConstIdxOut,
                                         ClipCullDistanceIdxSet *constIndicesOut,
                                         TQualifier targetQualifier)
        : TIntermTraverser(true, false, false),
          mRedeclaredSym(redeclaredSymOut),
          mUseNonConstClipCullDistanceIndex(nonConstIdxUsedOut),
          mMaxConstClipCullDistanceIndex(maxConstIdxOut),
          mConstClipCullDistanceIndices(constIndicesOut),
          mTargetQualifier(targetQualifier)
    {
        *mRedeclaredSym                    = nullptr;
        *mUseNonConstClipCullDistanceIndex = false;
        *mMaxConstClipCullDistanceIndex    = 0;
        mConstClipCullDistanceIndices->reset();
    }

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override
    {
        // If gl_ClipDistance/gl_CullDistance is redeclared, we need to collect its information
        const TIntermSequence &sequence = *(node->getSequence());

        if (sequence.size() != 1)
        {
            return true;
        }

        TIntermSymbol *variable = sequence.front()->getAsSymbolNode();
        if (variable == nullptr || variable->getType().getQualifier() != mTargetQualifier)
        {
            return true;
        }

        *mRedeclaredSym = variable->getAsSymbolNode();

        return true;
    }

    bool visitBinary(Visit visit, TIntermBinary *node) override
    {
        TOperator op = node->getOp();
        if (op != EOpIndexDirect && op != EOpIndexIndirect)
        {
            return true;
        }

        // gl_ClipDistance / gl_CullDistance
        TIntermTyped *left = node->getLeft()->getAsTyped();
        if (!left)
        {
            return true;
        }

        ASSERT(op == EOpIndexDirect || op == EOpIndexIndirect);

        TIntermSymbol *clipCullDistance = left->getAsSymbolNode();
        if (!clipCullDistance)
        {
            return true;
        }
        if (clipCullDistance->getType().getQualifier() != mTargetQualifier)
        {
            return true;
        }

        const TConstantUnion *constIdx = node->getRight()->getConstantValue();
        if (!constIdx)
        {
            *mUseNonConstClipCullDistanceIndex = true;
        }
        else
        {
            unsigned int idx = 0;
            switch (constIdx->getType())
            {
                case EbtInt:
                    idx = constIdx->getIConst();
                    break;
                case EbtUInt:
                    idx = constIdx->getUConst();
                    break;
                case EbtFloat:
                    idx = static_cast<unsigned int>(constIdx->getFConst());
                    break;
                case EbtBool:
                    idx = constIdx->getBConst() ? 1 : 0;
                    break;
                default:
                    UNREACHABLE();
                    break;
            }
            ASSERT(idx < mConstClipCullDistanceIndices->size());
            mConstClipCullDistanceIndices->set(idx);

            *mMaxConstClipCullDistanceIndex = std::max(*mMaxConstClipCullDistanceIndex, idx);
        }

        return true;
    }

  private:
    const TIntermSymbol **mRedeclaredSym;
    // Flag indicating whether there is at least one reference of gl_ClipDistance with non-constant
    // index
    bool *mUseNonConstClipCullDistanceIndex;
    // Max constant index that is used to reference gl_ClipDistance
    unsigned int *mMaxConstClipCullDistanceIndex;
    // List of constant index reference of gl_ClipDistance
    ClipCullDistanceIdxSet *mConstClipCullDistanceIndices;
    // Qualifier for gl_ClipDistance/gl_CullDistance
    const TQualifier mTargetQualifier;
};

// Replace all symbolic occurrences of given variables except one symbol.
class ReplaceVariableExceptOneTraverser : public TIntermTraverser
{
  public:
    ReplaceVariableExceptOneTraverser(const TVariable *toBeReplaced,
                                      const TIntermTyped *replacement,
                                      const TIntermSymbol *exception)
        : TIntermTraverser(true, false, false),
          mToBeReplaced(toBeReplaced),
          mException(exception),
          mReplacement(replacement)
    {}

    void visitSymbol(TIntermSymbol *node) override
    {
        if (&node->variable() == mToBeReplaced && node != mException)
        {
            queueReplacement(mReplacement->deepCopy(), OriginalNode::IS_DROPPED);
        }
    }

  private:
    const TVariable *const mToBeReplaced;
    const TIntermSymbol *const mException;
    const TIntermTyped *const mReplacement;
};

TIntermNode *simpleAssignFunc(const unsigned int index,
                              TIntermSymbol *leftSymbol,
                              TIntermSymbol *rightSymbol,
                              const TIntermTyped * /*enableFlags*/)
{
    // leftSymbol[index] = rightSymbol[index]
    // E.g., ANGLEClipDistance[index] = gl_ClipDistance[index]
    TIntermBinary *left =
        new TIntermBinary(EOpIndexDirect, leftSymbol->deepCopy(), CreateIndexNode(index));
    TIntermBinary *right =
        new TIntermBinary(EOpIndexDirect, rightSymbol->deepCopy(), CreateIndexNode(index));

    return new TIntermBinary(EOpAssign, left, right);
}

// This is only used for gl_ClipDistance
TIntermNode *assignFuncWithEnableFlags(const unsigned int index,
                                       TIntermSymbol *leftSymbol,
                                       TIntermSymbol *rightSymbol,
                                       const TIntermTyped *enableFlags)
{
    //  if (ANGLEUniforms.clipDistancesEnabled & (0x1 << index))
    //      gl_ClipDistance[index] = ANGLEClipDistance[index];
    //  else
    //      gl_ClipDistance[index] = 0;
    TIntermConstantUnion *bitMask = CreateUIntNode(0x1 << index);
    TIntermBinary *bitwiseAnd = new TIntermBinary(EOpBitwiseAnd, enableFlags->deepCopy(), bitMask);
    TIntermBinary *nonZero    = new TIntermBinary(EOpNotEqual, bitwiseAnd, CreateUIntNode(0));

    TIntermBinary *left =
        new TIntermBinary(EOpIndexDirect, leftSymbol->deepCopy(), CreateIndexNode(index));
    TIntermBinary *right =
        new TIntermBinary(EOpIndexDirect, rightSymbol->deepCopy(), CreateIndexNode(index));
    TIntermBinary *assignment = new TIntermBinary(EOpAssign, left, right);
    TIntermBlock *trueBlock   = new TIntermBlock();
    trueBlock->appendStatement(assignment);

    TIntermBinary *zeroAssignment =
        new TIntermBinary(EOpAssign, left->deepCopy(), CreateFloatNode(0, EbpMedium));
    TIntermBlock *falseBlock = new TIntermBlock();
    falseBlock->appendStatement(zeroAssignment);

    return new TIntermIfElse(nonZero, trueBlock, falseBlock);
}

class ReplaceClipCullDistanceAssignments : angle::NonCopyable
{
  public:
    ReplaceClipCullDistanceAssignments(TCompiler *compiler,
                                       TIntermBlock *root,
                                       TSymbolTable *symbolTable,
                                       const TVariable *glClipCullDistanceVar,
                                       const TIntermSymbol *redeclaredGlClipDistance,
                                       const ImmutableString &angleVarName)
        : mCompiler(compiler),
          mRoot(root),
          mSymbolTable(symbolTable),
          mGlVar(glClipCullDistanceVar),
          mRedeclaredGLVar(redeclaredGlClipDistance),
          mANGLEVarName(angleVarName)
    {
        mEnabledDistances = 0;
    }

    unsigned int getEnabledClipCullDistance(const bool useNonConstIndex,
                                            const unsigned int maxConstIndex);
    const TVariable *declareANGLEVariable(const TVariable *originalVariable);
    bool assignOriginalValueToANGLEVariable(const GLenum shaderType);
    bool assignANGLEValueToOriginalVariable(const GLenum shaderType,
                                            const bool isRedeclared,
                                            const TIntermTyped *enableFlags,
                                            const ClipCullDistanceIdxSet *constIndices);

  private:
    bool assignOriginalValueToANGLEVariableImpl();
    bool assignANGLEValueToOriginalVariableImpl(const bool isRedeclared,
                                                const TIntermTyped *enableFlags,
                                                const ClipCullDistanceIdxSet *constIndices,
                                                AssignFunc assignFunc);

    // Common variables for replacing gl_Clip/CullDistances with ANGLEClip/CullDistances
    TCompiler *mCompiler;
    TIntermBlock *mRoot;
    TSymbolTable *mSymbolTable;

    const TVariable *mGlVar;
    const TIntermSymbol *mRedeclaredGLVar;
    const ImmutableString mANGLEVarName;

    unsigned int mEnabledDistances;
    const TVariable *mANGLEVar;
};

unsigned int ReplaceClipCullDistanceAssignments::getEnabledClipCullDistance(
    const bool useNonConstIndex,
    const unsigned int maxConstIndex)
{
    if (mRedeclaredGLVar)
    {
        // If array is redeclared by user, use that redeclared size.
        mEnabledDistances = mRedeclaredGLVar->getType().getOutermostArraySize();
    }
    else if (!useNonConstIndex)
    {
        ASSERT(maxConstIndex < mGlVar->getType().getOutermostArraySize());
        // Only use constant index, then use max array index used.
        mEnabledDistances = maxConstIndex + 1;
    }

    return mEnabledDistances;
}

const TVariable *ReplaceClipCullDistanceAssignments::declareANGLEVariable(
    const TVariable *originalVariable)
{
    ASSERT(mEnabledDistances > 0);

    TType *clipCullDistanceType = new TType(originalVariable->getType());
    clipCullDistanceType->setQualifier(EvqGlobal);
    clipCullDistanceType->toArrayBaseType();
    clipCullDistanceType->makeArray(mEnabledDistances);

    mANGLEVar =
        new TVariable(mSymbolTable, mANGLEVarName, clipCullDistanceType, SymbolType::AngleInternal);

    TIntermSymbol *clipCullDistanceDeclarator = new TIntermSymbol(mANGLEVar);
    TIntermDeclaration *clipCullDistanceDecl  = new TIntermDeclaration;
    clipCullDistanceDecl->appendDeclarator(clipCullDistanceDeclarator);

    // Must declare ANGLEClipdistance/ANGLECullDistance before any function, since
    // gl_ClipDistance/gl_CullDistance might be accessed within a function declared before main.
    mRoot->insertStatement(0, clipCullDistanceDecl);

    return mANGLEVar;
}

bool ReplaceClipCullDistanceAssignments::assignOriginalValueToANGLEVariableImpl()
{
    ASSERT(mEnabledDistances > 0);

    TIntermBlock *readBlock                 = new TIntermBlock;
    TIntermSymbol *glClipCullDistanceSymbol = new TIntermSymbol(mGlVar);
    TIntermSymbol *clipCullDistanceSymbol   = new TIntermSymbol(mANGLEVar);

    for (unsigned int i = 0; i < mEnabledDistances; i++)
    {
        readBlock->appendStatement(
            simpleAssignFunc(i, clipCullDistanceSymbol, glClipCullDistanceSymbol, nullptr));
    }

    return RunAtTheBeginningOfShader(mCompiler, mRoot, readBlock);
}

bool ReplaceClipCullDistanceAssignments::assignANGLEValueToOriginalVariableImpl(
    const bool isRedeclared,
    const TIntermTyped *enableFlags,
    const ClipCullDistanceIdxSet *constIndices,
    AssignFunc assignFunc)
{
    ASSERT(mEnabledDistances > 0);

    TIntermBlock *assignBlock               = new TIntermBlock;
    TIntermSymbol *glClipCullDistanceSymbol = new TIntermSymbol(mGlVar);
    TIntermSymbol *clipCullDistanceSymbol   = new TIntermSymbol(mANGLEVar);

    // The array size is decided by either redeclaring the variable or accessing the variable with a
    // integral constant index. And this size is the count of the enabled value. So, if the index
    // which is greater than the array size, is used to access the variable, this access will be
    // ignored.
    if (isRedeclared || !constIndices)
    {
        for (unsigned int i = 0; i < mEnabledDistances; ++i)
        {
            assignBlock->appendStatement(
                assignFunc(i, glClipCullDistanceSymbol, clipCullDistanceSymbol, enableFlags));
        }
    }
    else
    {
        // Assign ANGLEClip/CullDistance[i]'s value to gl_Clip/CullDistance[i] if i is in the
        // constant indices list. Those elements whose index is not in the constant index list will
        // be zeroise for initialization.
        for (unsigned int i = 0; i < mEnabledDistances; ++i)
        {
            if (constIndices->test(i))
            {
                assignBlock->appendStatement(
                    assignFunc(i, glClipCullDistanceSymbol, clipCullDistanceSymbol, enableFlags));
            }
            else
            {
                // gl_Clip/CullDistance[i] = 0;
                TIntermBinary *left = new TIntermBinary(
                    EOpIndexDirect, glClipCullDistanceSymbol->deepCopy(), CreateIndexNode(i));
                TIntermBinary *zeroAssignment =
                    new TIntermBinary(EOpAssign, left, CreateFloatNode(0, EbpMedium));
                assignBlock->appendStatement(zeroAssignment);
            }
        }
    }

    return RunAtTheEndOfShader(mCompiler, mRoot, assignBlock, mSymbolTable);
}

ANGLE_NO_DISCARD bool ReplaceClipCullDistanceAssignments::assignOriginalValueToANGLEVariable(
    const GLenum shaderType)
{
    switch (shaderType)
    {
        case GL_VERTEX_SHADER:
            // Vertex shader can use gl_Clip/CullDistance as a output only
            break;
        case GL_FRAGMENT_SHADER:
        {
            // These shader types can use gl_Clip/CullDistance as input
            if (!assignOriginalValueToANGLEVariableImpl())
            {
                return false;
            }
            break;
        }
        default:
        {
            UNREACHABLE();
            return false;
        }
    }

    return true;
}

ANGLE_NO_DISCARD bool ReplaceClipCullDistanceAssignments::assignANGLEValueToOriginalVariable(
    const GLenum shaderType,
    const bool isRedeclared,
    const TIntermTyped *enableFlags,
    const ClipCullDistanceIdxSet *constIndices)
{
    switch (shaderType)
    {
        case GL_VERTEX_SHADER:
        {
            // Vertex shader can use gl_Clip/CullDistance as output.
            // If the enabled gl_Clip/CullDistances are not initialized, results are undefined.
            // EXT_clip_cull_distance spec :
            // The shader must also set all values in gl_ClipDistance that have been enabled via the
            // OpenGL ES API, or results are undefined. Values written into gl_ClipDistance for
            // planes that are not enabled have no effect.
            // ...
            // Shaders writing gl_CullDistance must write all enabled distances, or culling results
            // are undefined.
            if (!assignANGLEValueToOriginalVariableImpl(
                    isRedeclared, enableFlags, constIndices,
                    enableFlags ? assignFuncWithEnableFlags : simpleAssignFunc))
            {
                return false;
            }
            break;
        }
        case GL_FRAGMENT_SHADER:
            // Fragment shader can use gl_Clip/CullDistance as input only
            break;
        default:
        {
            UNREACHABLE();
            return false;
        }
    }

    return true;
}

// Common code to transform gl_ClipDistance and gl_CullDistance.  Comments reference
// gl_ClipDistance, but are also applicable to gl_CullDistance.
ANGLE_NO_DISCARD bool ReplaceClipCullDistanceAssignmentsImpl(
    TCompiler *compiler,
    TIntermBlock *root,
    TSymbolTable *symbolTable,
    const GLenum shaderType,
    const TIntermTyped *clipDistanceEnableFlags,
    const char *builtInName,
    const char *replacementName,
    TQualifier builtInQualifier)
{
    // Collect all constant index references of gl_ClipDistance
    ImmutableString name(builtInName);
    ClipCullDistanceIdxSet constIndices;
    bool useNonConstIndex                  = false;
    const TIntermSymbol *redeclaredBuiltIn = nullptr;
    unsigned int maxConstIndex             = 0;
    GLClipCullDistanceReferenceTraverser indexTraverser(
        &redeclaredBuiltIn, &useNonConstIndex, &maxConstIndex, &constIndices, builtInQualifier);
    root->traverse(&indexTraverser);
    if (!useNonConstIndex && constIndices.none())
    {
        // No references of gl_ClipDistance
        return true;
    }

    // Retrieve gl_ClipDistance variable reference
    // Search user redeclared gl_ClipDistance first
    const TVariable *builtInVar = nullptr;
    if (redeclaredBuiltIn)
    {
        builtInVar = &redeclaredBuiltIn->variable();
    }
    else
    {
        // User defined not found, find in built-in table
        builtInVar = static_cast<const TVariable *>(
            symbolTable->findBuiltIn(name, compiler->getShaderVersion()));
    }
    if (!builtInVar)
    {
        return false;
    }

    ReplaceClipCullDistanceAssignments replacementUtils(compiler, root, symbolTable, builtInVar,
                                                        redeclaredBuiltIn,
                                                        ImmutableString(replacementName));

    // Declare a global variable substituting gl_ClipDistance
    unsigned int enabledClipDistances =
        replacementUtils.getEnabledClipCullDistance(useNonConstIndex, maxConstIndex);
    if (!enabledClipDistances)
    {
        // Spec :
        // The gl_ClipDistance array is predeclared as unsized and must be explicitly sized by the
        // shader either redeclaring it with a size or implicitly sized by indexing it only with
        // integral constant expressions.
        return false;
    }

    const TVariable *replacementVar = replacementUtils.declareANGLEVariable(builtInVar);

    // Replace gl_ClipDistance reference with ANGLEClipDistance, except the declaration
    ReplaceVariableExceptOneTraverser replaceTraverser(builtInVar,
                                                       new TIntermSymbol(replacementVar),
                                                       /** exception */ redeclaredBuiltIn);
    root->traverse(&replaceTraverser);
    if (!replaceTraverser.updateTree(compiler, root))
    {
        return false;
    }

    // Read gl_ClipDistance to ANGLEClipDistance for getting a original data
    if (!replacementUtils.assignOriginalValueToANGLEVariable(shaderType))
    {
        return false;
    }

    // Reassign ANGLEClipDistance to gl_ClipDistance but ignore those that are disabled
    const bool isRedeclared = redeclaredBuiltIn != nullptr;
    if (!replacementUtils.assignANGLEValueToOriginalVariable(
            shaderType, isRedeclared, clipDistanceEnableFlags, &constIndices))
    {
        return false;
    }

    // If not redeclared, replace the built-in with one that is appropriately sized
    if (!isRedeclared)
    {
        TType *resizedType = new TType(builtInVar->getType());
        resizedType->setArraySize(0, enabledClipDistances);

        TVariable *resizedVar = new TVariable(symbolTable, name, resizedType, SymbolType::BuiltIn);

        return ReplaceVariable(compiler, root, builtInVar, resizedVar);
    }

    return true;
}

}  // anonymous namespace

ANGLE_NO_DISCARD bool ReplaceClipDistanceAssignments(TCompiler *compiler,
                                                     TIntermBlock *root,
                                                     TSymbolTable *symbolTable,
                                                     const GLenum shaderType,
                                                     const TIntermTyped *clipDistanceEnableFlags)
{
    return ReplaceClipCullDistanceAssignmentsImpl(compiler, root, symbolTable, shaderType,
                                                  clipDistanceEnableFlags, "gl_ClipDistance",
                                                  "ANGLEClipDistance", EvqClipDistance);
}

ANGLE_NO_DISCARD bool ReplaceCullDistanceAssignments(TCompiler *compiler,
                                                     TIntermBlock *root,
                                                     TSymbolTable *symbolTable,
                                                     const GLenum shaderType)
{
    return ReplaceClipCullDistanceAssignmentsImpl(compiler, root, symbolTable, shaderType, nullptr,
                                                  "gl_CullDistance", "ANGLECullDistance",
                                                  EvqCullDistance);
}

}  // namespace sh
