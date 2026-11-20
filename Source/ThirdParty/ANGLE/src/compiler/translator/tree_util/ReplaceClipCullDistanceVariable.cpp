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
#include "compiler/translator/tree_util/FindMain.h"
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
                                const TIntermTyped *enableFlags,
                                const TIntermTyped *isLastPreRasterizationStage);

// Traverse the tree and collect the redeclaration and all constant index references of
// gl_ClipDistance/gl_CullDistance
class GLClipCullDistanceReferenceTraverser : public TIntermTraverser
{
  public:
    GLClipCullDistanceReferenceTraverser(const TIntermSymbol **redeclaredSymOut,
                                         const TVariable **variableOut,
                                         ClipCullDistanceIdxSet *constIndicesOut,
                                         TQualifier targetQualifier)
        : TIntermTraverser(true, false, false),
          mRedeclaredSym(redeclaredSymOut),
          mVariable(variableOut),
          mConstClipCullDistanceIndices(constIndicesOut),
          mTargetQualifier(targetQualifier)
    {
        *mRedeclaredSym = nullptr;
        *mVariable      = nullptr;
        mConstClipCullDistanceIndices->reset();
    }

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override
    {
        // If gl_ClipDistance/gl_CullDistance is redeclared, we need to collect its information.
        // The built-in may be redeclared either as a plain array, or as a member of the output
        // gl_PerVertex block.
        const TIntermSequence &sequence = *(node->getSequence());

        if (sequence.size() != 1)
        {
            return true;
        }

        TIntermSymbol *variable = sequence.front()->getAsSymbolNode();
        if (variable == nullptr)
        {
            return true;
        }

        if (variable->isInterfaceBlock() && variable->getType().getQualifier() == EvqPerVertexOut)
        {
            bool found = false;
            for (const TField *field : variable->getType().getInterfaceBlock()->fields())
            {
                if (field->type()->getQualifier() == mTargetQualifier)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                return true;
            }
        }
        else if (variable->getType().getQualifier() != mTargetQualifier)
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
        if (constIdx)
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
        }

        return true;
    }

    void visitSymbol(TIntermSymbol *node) override
    {
        if (node->getType().getQualifier() == mTargetQualifier)
        {
            *mVariable = &node->variable();
        }
    }

  private:
    const TIntermSymbol **mRedeclaredSym;
    const TVariable **mVariable;
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
                              const TIntermTyped * /*enableFlags*/,
                              const TIntermTyped * /*isLastPreRasterizationStage*/)
{
    if (!rightSymbol)
        return nullptr;

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
                                       const TIntermTyped *enableFlags,
                                       const TIntermTyped *isLastPreRasterizationStage)
{
    //  if (!isLastPreRasterizationStage ||
    //      (ANGLEUniforms.clipDistancesEnabled & (0x1 << index)) != 0)
    //      gl_ClipDistance[index] = ANGLEClipDistance[index];
    //  else
    //      gl_ClipDistance[index] = 0;
    //
    // The isLastPreRasterizationStage condition is only present if provided; the disabled clip
    // distances are zeroed out only by the last pre-rasterization stage.
    TIntermConstantUnion *bitMask = CreateUIntNode(0x1 << index);
    TIntermBinary *bitwiseAnd = new TIntermBinary(EOpBitwiseAnd, enableFlags->deepCopy(), bitMask);
    TIntermTyped *nonZero         = new TIntermBinary(EOpNotEqual, bitwiseAnd, CreateUIntNode(0));
    if (isLastPreRasterizationStage != nullptr)
    {
        TIntermTyped *notLastStage =
            new TIntermUnary(EOpLogicalNot, isLastPreRasterizationStage->deepCopy(), nullptr);
        nonZero = new TIntermBinary(EOpLogicalOr, notLastStage, nonZero);
    }

    TIntermBinary *left =
        new TIntermBinary(EOpIndexDirect, leftSymbol->deepCopy(), CreateIndexNode(index));
    TIntermBlock *trueBlock = new TIntermBlock();
    if (rightSymbol)
    {
        TIntermBinary *right =
            new TIntermBinary(EOpIndexDirect, rightSymbol->deepCopy(), CreateIndexNode(index));
        TIntermBinary *assignment = new TIntermBinary(EOpAssign, left, right);
        trueBlock->appendStatement(assignment);
    }

    TIntermBinary *zeroAssignment =
        new TIntermBinary(EOpAssign, left->deepCopy(), CreateFloatNode(0, EbpMedium));
    TIntermBlock *falseBlock = new TIntermBlock();
    falseBlock->appendStatement(zeroAssignment);

    return new TIntermIfElse(nonZero, trueBlock, falseBlock);
}

// This is only used for gl_ClipDistance, in shader stages that write gl_ClipDistance directly
// (i.e. without going through ANGLEClipDistance).
TIntermNode *assignFuncZeroDisabledBits(const unsigned int index,
                                        TIntermSymbol *leftSymbol,
                                        TIntermSymbol * /*rightSymbol*/,
                                        const TIntermTyped *enableFlags,
                                        const TIntermTyped *isLastPreRasterizationStage)
{
    //  if (isLastPreRasterizationStage &&
    //      (ANGLEUniforms.clipDistancesEnabled & (0x1 << index)) == 0)
    //  {
    //      gl_ClipDistance[index] = 0;
    //  }
    //
    // The isLastPreRasterizationStage condition is only present if provided; the disabled clip
    // distances are zeroed out only by the last pre-rasterization stage.
    TIntermConstantUnion *bitMask = CreateUIntNode(0x1 << index);
    TIntermBinary *bitwiseAnd = new TIntermBinary(EOpBitwiseAnd, enableFlags->deepCopy(), bitMask);
    TIntermTyped *zero        = new TIntermBinary(EOpEqual, bitwiseAnd, CreateUIntNode(0));
    if (isLastPreRasterizationStage != nullptr)
    {
        zero = new TIntermBinary(EOpLogicalAnd, isLastPreRasterizationStage->deepCopy(), zero);
    }

    TIntermBinary *left =
        new TIntermBinary(EOpIndexDirect, leftSymbol->deepCopy(), CreateIndexNode(index));

    TIntermBinary *zeroAssignment =
        new TIntermBinary(EOpAssign, left, CreateFloatNode(0, EbpMedium));
    TIntermBlock *trueBlock = new TIntermBlock();
    trueBlock->appendStatement(zeroAssignment);

    return new TIntermIfElse(zero, trueBlock, nullptr);
}

class ReplaceClipCullDistanceAssignments : angle::NonCopyable
{
  public:
    ReplaceClipCullDistanceAssignments(TCompiler *compiler,
                                       TIntermBlock *root,
                                       TSymbolTable *symbolTable,
                                       const TVariable *glClipCullDistanceVar,
                                       const ImmutableString &angleVarName,
                                       unsigned int enabledDistances)
        : mCompiler(compiler),
          mRoot(root),
          mSymbolTable(symbolTable),
          mGlVar(glClipCullDistanceVar),
          mANGLEVarName(angleVarName),
          mEnabledDistances(enabledDistances)
    {}

    const TVariable *declareANGLEVariable(const TVariable *originalVariable);
    bool assignOriginalValueToANGLEVariable(const GLenum shaderType);
    bool assignValueToOriginalVariable(const GLenum shaderType,
                                       const bool isRedeclared,
                                       const TIntermTyped *enableFlags,
                                       const TIntermTyped *isLastPreRasterizationStage,
                                       const ClipCullDistanceIdxSet *constIndices);

  private:
    bool assignOriginalValueToANGLEVariableImpl();
    bool assignValueToOriginalVariableImpl(const bool assignAllDistances,
                                           const TIntermTyped *enableFlags,
                                           const TIntermTyped *isLastPreRasterizationStage,
                                           const ClipCullDistanceIdxSet *constIndices,
                                           AssignFunc assignFunc,
                                           const GLenum shaderType);

    // Common variables for replacing gl_Clip/CullDistances with ANGLEClip/CullDistances
    TCompiler *mCompiler;
    TIntermBlock *mRoot;
    TSymbolTable *mSymbolTable;

    const TVariable *mGlVar;
    const ImmutableString mANGLEVarName;

    const unsigned int mEnabledDistances;
    const TVariable *mANGLEVar = nullptr;
};

const TVariable *ReplaceClipCullDistanceAssignments::declareANGLEVariable(
    const TVariable *originalVariable)
{
    ASSERT(mEnabledDistances > 0);

    // Note that the type is created from scratch rather than copied from the original variable,
    // because the original variable may be a member of a redeclared gl_PerVertex block.
    const TType &originalType = originalVariable->getType();
    TType *clipCullDistanceType =
        new TType(originalType.getBasicType(), originalType.getPrecision(), EvqGlobal, 1, 1);
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
        readBlock->appendStatement(simpleAssignFunc(i, clipCullDistanceSymbol,
                                                    glClipCullDistanceSymbol, nullptr, nullptr));
    }

    return RunAtTheBeginningOfShader(mCompiler, mRoot, readBlock);
}

bool ReplaceClipCullDistanceAssignments::assignValueToOriginalVariableImpl(
    const bool assignAllDistances,
    const TIntermTyped *enableFlags,
    const TIntermTyped *isLastPreRasterizationStage,
    const ClipCullDistanceIdxSet *constIndices,
    AssignFunc assignFunc,
    const GLenum shaderType)
{
    ASSERT(mEnabledDistances > 0);

    TIntermBlock *assignBlock               = new TIntermBlock;
    TIntermSymbol *glClipCullDistanceSymbol = new TIntermSymbol(mGlVar);
    TIntermSymbol *clipCullDistanceSymbol   = mANGLEVar ? new TIntermSymbol(mANGLEVar) : nullptr;

    // The array size is decided by either redeclaring the variable or accessing the variable with a
    // integral constant index. And this size is the count of the enabled value. So, if the index
    // which is greater than the array size, is used to access the variable, this access will be
    // ignored.
    if (assignAllDistances || !constIndices)
    {
        for (unsigned int i = 0; i < mEnabledDistances; ++i)
        {
            assignBlock->appendStatement(assignFunc(i, glClipCullDistanceSymbol,
                                                    clipCullDistanceSymbol, enableFlags,
                                                    isLastPreRasterizationStage));
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
                assignBlock->appendStatement(assignFunc(i, glClipCullDistanceSymbol,
                                                        clipCullDistanceSymbol, enableFlags,
                                                        isLastPreRasterizationStage));
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

    if (shaderType == GL_GEOMETRY_SHADER)
    {
        // In geometry shaders, the built-ins take effect when EmitVertex() is called.
        return RunBeforeEmitVertex(mCompiler, mRoot, assignBlock, mSymbolTable);
    }

    return RunAtTheEndOfShader(mCompiler, mRoot, assignBlock, mSymbolTable);
}

[[nodiscard]] bool ReplaceClipCullDistanceAssignments::assignOriginalValueToANGLEVariable(
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

[[nodiscard]] bool ReplaceClipCullDistanceAssignments::assignValueToOriginalVariable(
    const GLenum shaderType,
    const bool isRedeclared,
    const TIntermTyped *enableFlags,
    const TIntermTyped *isLastPreRasterizationStage,
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
            if (!assignValueToOriginalVariableImpl(
                    isRedeclared, enableFlags, isLastPreRasterizationStage, constIndices,
                    enableFlags ? assignFuncWithEnableFlags : simpleAssignFunc, shaderType))
            {
                return false;
            }
            break;
        }
        case GL_TESS_EVALUATION_SHADER:
        case GL_GEOMETRY_SHADER:
        {
            // These shader stages write gl_Clip/CullDistance directly; there is no
            // ANGLEClip/CullDistance replacement. The only thing to do is to zero the disabled
            // gl_ClipDistance elements so that they don't clip. gl_CullDistance has no enable
            // state, so there is nothing to do for it.
            if (enableFlags == nullptr)
            {
                break;
            }
            if (!assignValueToOriginalVariableImpl(true, enableFlags, isLastPreRasterizationStage,
                                                   constIndices, assignFuncZeroDisabledBits,
                                                   shaderType))
            {
                return false;
            }
            break;
        }
        case GL_TESS_CONTROL_SHADER:
            // Tessellation control shader only does a passthrough of gl_Clip/CullDistances
            break;
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
[[nodiscard]] bool ReplaceClipCullDistanceAssignmentsImpl(
    TCompiler *compiler,
    TIntermBlock *root,
    TSymbolTable *symbolTable,
    const GLenum shaderType,
    const TIntermTyped *clipDistanceEnableFlags,
    const TIntermTyped *isLastPreRasterizationStage,
    const char *builtInName,
    const char *replacementName,
    TQualifier builtInQualifier)
{
    // gl_ClipDistance is not available in compute shaders.
    if (shaderType == GL_COMPUTE_SHADER)
    {
        return true;
    }

    // Collect all constant index references of gl_ClipDistance
    ImmutableString name(builtInName);
    ClipCullDistanceIdxSet constIndices;
    const TIntermSymbol *redeclaredBuiltIn = nullptr;
    const TVariable *builtInVariable       = nullptr;
    GLClipCullDistanceReferenceTraverser indexTraverser(&redeclaredBuiltIn, &builtInVariable,
                                                        &constIndices, builtInQualifier);
    root->traverse(&indexTraverser);

    // gl_ClipDistance may be redeclared as a plain array (`out float gl_ClipDistance[N];`) or as a
    // member of the output gl_PerVertex block.  In the latter case, the redeclaration itself does
    // not reference gl_ClipDistance; the references are found by the traverser when the array is
    // indexed.
    const bool isRedeclared = redeclaredBuiltIn != nullptr;
    const bool isRedeclaredAsPlainArray =
        isRedeclared && !redeclaredBuiltIn->getType().isInterfaceBlock();

    if (builtInVariable == nullptr && !isRedeclaredAsPlainArray)
    {
        // gl_ClipDistance is not referenced directly in this shader. It may be declared as a
        // member of gl_PerVertex without being written, or only accessed through gl_in/gl_out
        // (which are passed through as is). There is nothing to do.
        return true;
    }

    // If redeclared as a plain array, use the redeclaration. Otherwise use the variable found
    // when indexing gl_ClipDistance.
    const TVariable *builtInVar =
        isRedeclaredAsPlainArray ? &redeclaredBuiltIn->variable() : builtInVariable;

    // The array size is determined by the compiler, either from the redeclaration or from the
    // indices used to access the array (including through gl_in in geometry and tessellation
    // shaders).
    const unsigned int enabledClipDistances = builtInQualifier == EvqClipDistance
                                                  ? compiler->getClipDistanceArraySize()
                                                  : compiler->getCullDistanceArraySize();
    if (!enabledClipDistances)
    {
        // Spec :
        // The gl_ClipDistance array is predeclared as unsized and must be explicitly sized by the
        // shader either redeclaring it with a size or implicitly sized by indexing it only with
        // integral constant expressions.
        return false;
    }

    ReplaceClipCullDistanceAssignments replacementUtils(compiler, root, symbolTable, builtInVar,
                                                        ImmutableString(replacementName),
                                                        enabledClipDistances);

    // In vertex and fragment shaders, gl_ClipDistance is replaced with ANGLEClipDistance. Other
    // stages write gl_ClipDistance directly.
    if (replacementName && (shaderType == GL_VERTEX_SHADER || shaderType == GL_FRAGMENT_SHADER))
    {
        // Declare a global variable substituting gl_ClipDistance
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

        // Read gl_ClipDistance to ANGLEClipDistance for getting original data
        if (!replacementUtils.assignOriginalValueToANGLEVariable(shaderType))
        {
            return false;
        }
    }

    // Reassign ANGLEClipDistance to gl_ClipDistance but ignore those that are disabled
    if (!replacementUtils.assignValueToOriginalVariable(shaderType, isRedeclared,
                                                        clipDistanceEnableFlags,
                                                        isLastPreRasterizationStage, &constIndices))
    {
        return false;
    }

    // If not redeclared, replace the built-in with one that is appropriately sized
    if (!isRedeclared)
    {
        TType *resizedType = new TType(builtInVar->getType());
        resizedType->setArraySize(0, enabledClipDistances);

        TVariable *resizedVar = new TVariable(symbolTable, name, resizedType, SymbolType::BuiltIn);

        if (!ReplaceVariable(compiler, root, builtInVar, resizedVar))
        {
            return false;
        }

        // Built-in was not redeclared in the original shader, add it to the vertex out struct
        return root->insertChildNodes(FindMainIndex(root), {new TIntermDeclaration{resizedVar}});
    }

    return true;
}

}  // anonymous namespace

[[nodiscard]] bool ReplaceClipDistanceAssignments(TCompiler *compiler,
                                                  TIntermBlock *root,
                                                  TSymbolTable *symbolTable,
                                                  const GLenum shaderType,
                                                  const TIntermTyped *clipDistanceEnableFlags,
                                                  const TIntermTyped *isLastPreRasterizationStage)
{
    return ReplaceClipCullDistanceAssignmentsImpl(
        compiler, root, symbolTable, shaderType, clipDistanceEnableFlags,
        isLastPreRasterizationStage, "gl_ClipDistance", "ANGLEClipDistance", EvqClipDistance);
}

[[nodiscard]] bool ReplaceCullDistanceAssignments(TCompiler *compiler,
                                                  TIntermBlock *root,
                                                  TSymbolTable *symbolTable,
                                                  const GLenum shaderType)
{
    return ReplaceClipCullDistanceAssignmentsImpl(compiler, root, symbolTable, shaderType, nullptr,
                                                  nullptr, "gl_CullDistance", "ANGLECullDistance",
                                                  EvqCullDistance);
}

[[nodiscard]] bool ZeroDisabledClipDistanceAssignments(TCompiler *compiler,
                                                       TIntermBlock *root,
                                                       TSymbolTable *symbolTable,
                                                       const GLenum shaderType,
                                                       const TIntermTyped *clipDistanceEnableFlags)
{
    return ReplaceClipCullDistanceAssignmentsImpl(compiler, root, symbolTable, shaderType,
                                                  clipDistanceEnableFlags, nullptr,
                                                  "gl_ClipDistance", nullptr, EvqClipDistance);
}

}  // namespace sh
