//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of dFdy viewport transformation.
// See header for more info.

#include "compiler/translator/tree_ops/RewriteDfdy.h"

#include "common/angleutils.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{

namespace
{

class Traverser : public TIntermTraverser
{
  public:
    ANGLE_NO_DISCARD static bool Apply(TCompiler *compiler,
                                       TIntermNode *root,
                                       const TSymbolTable &symbolTable,
                                       TIntermBinary *flipXY,
                                       TIntermTyped *fragRotation);

  private:
    Traverser(TIntermBinary *flipXY, TIntermTyped *fragRotation, TSymbolTable *symbolTable);
    bool visitUnary(Visit visit, TIntermUnary *node) override;

    TIntermBinary *mFlipXY      = nullptr;
    TIntermTyped *mFragRotation = nullptr;
};

Traverser::Traverser(TIntermBinary *flipXY, TIntermTyped *fragRotation, TSymbolTable *symbolTable)
    : TIntermTraverser(true, false, false, symbolTable),
      mFlipXY(flipXY),
      mFragRotation(fragRotation)
{}

// static
bool Traverser::Apply(TCompiler *compiler,
                      TIntermNode *root,
                      const TSymbolTable &symbolTable,
                      TIntermBinary *flipXY,
                      TIntermTyped *fragRotation)
{
    TSymbolTable *pSymbolTable = const_cast<TSymbolTable *>(&symbolTable);
    Traverser traverser(flipXY, fragRotation, pSymbolTable);
    root->traverse(&traverser);
    return traverser.updateTree(compiler, root);
}

bool Traverser::visitUnary(Visit visit, TIntermUnary *node)
{
    // Decide if the node represents a call to dFdx() or dFdy()
    if ((node->getOp() != EOpDFdx) && (node->getOp() != EOpDFdy))
    {
        return true;
    }

    // Prior to supporting Android pre-rotation, dFdy() needed to be multiplied by mFlipXY.y:
    //
    //   correctedDfdy(operand) = dFdy(operand) * mFlipXY.y
    //
    // For Android pre-rotation, both dFdx() and dFdy() need to be "rotated" and multiplied by
    // mFlipXY.  "Rotation" means to swap them for 90 and 270 degrees, or to not swap them for 0
    // and 180 degrees.  This rotation is accomplished with mFragRotation, which is a 2x2 matrix
    // used for fragment shader rotation.  The 1st half (a vec2 that is either (1,0) or (0,1)) is
    // used for rewriting dFdx() and the 2nd half (either (0,1) or (1,0)) is used for rewriting
    // dFdy().  Otherwise, the formula for the rewrite is the same:
    //
    //     result = ((dFdx(operand) * (mFragRotation[half] * mFlipXY).x) +
    //               (dFdy(operand) * (mFragRotation[half] * mFlipXY).y))
    //
    // For dFdx(), half is 0 (the 1st half).  For dFdy(), half is 1 (the 2nd half).  Depending on
    // the rotation, mFragRotation[half] will cause either dFdx(operand) or dFdy(operand) to be
    // zeroed-out.  That effectively means that the above code results in the following for 0 and
    // 180 degrees:
    //
    //   correctedDfdx(operand) = dFdx(operand) * mFlipXY.x
    //   correctedDfdy(operand) = dFdy(operand) * mFlipXY.y
    //
    // and the following for 90 and 270 degrees:
    //
    //   correctedDfdx(operand) = dFdy(operand) * mFlipXY.y
    //   correctedDfdy(operand) = dFdx(operand) * mFlipXY.x
    //
    // TODO(ianelliott): Look at the performance of this approach and potentially optimize it
    // http://anglebug.com/4678

    // Get a vec2 with the correct half of ANGLEUniforms.fragRotation
    TIntermBinary *halfRotationMat = nullptr;
    if (node->getOp() == EOpDFdx)
    {
        halfRotationMat =
            new TIntermBinary(EOpIndexDirect, mFragRotation->deepCopy(), CreateIndexNode(0));
    }
    else
    {
        halfRotationMat =
            new TIntermBinary(EOpIndexDirect, mFragRotation->deepCopy(), CreateIndexNode(1));
    }

    // Multiply halfRotationMat by ANGLEUniforms.flipXY and store in a temporary variable
    TIntermBinary *rotatedFlipXY = new TIntermBinary(EOpMul, mFlipXY->deepCopy(), halfRotationMat);
    const TType *vec2Type        = StaticType::GetBasic<EbtFloat, 2>();
    TIntermSymbol *tmpRotFlipXY  = new TIntermSymbol(CreateTempVariable(mSymbolTable, vec2Type));
    TIntermSequence *tmpDecl     = new TIntermSequence;
    tmpDecl->push_back(CreateTempInitDeclarationNode(&tmpRotFlipXY->variable(), rotatedFlipXY));
    insertStatementsInParentBlock(*tmpDecl);

    // Get the .x and .y swizzles to use as multipliers
    TVector<int> swizzleOffsetX = {0};
    TVector<int> swizzleOffsetY = {1};
    TIntermSwizzle *multiplierX = new TIntermSwizzle(tmpRotFlipXY, swizzleOffsetX);
    TIntermSwizzle *multiplierY = new TIntermSwizzle(tmpRotFlipXY->deepCopy(), swizzleOffsetY);

    // Get the results of dFdx(operand) and dFdy(operand), and multiply them by the swizzles
    TIntermTyped *operand = node->getOperand();
    TIntermUnary *dFdx    = new TIntermUnary(EOpDFdx, operand->deepCopy(), node->getFunction());
    TIntermUnary *dFdy    = new TIntermUnary(EOpDFdy, operand->deepCopy(), node->getFunction());
    size_t objectSize     = node->getType().getObjectSize();
    TOperator multiplyOp  = (objectSize == 1) ? EOpMul : EOpVectorTimesScalar;
    TIntermBinary *rotatedFlippedDfdx = new TIntermBinary(multiplyOp, dFdx, multiplierX);
    TIntermBinary *rotatedFlippedDfdy = new TIntermBinary(multiplyOp, dFdy, multiplierY);

    // Sum them together into the result:
    TIntermBinary *correctedResult =
        new TIntermBinary(EOpAdd, rotatedFlippedDfdx, rotatedFlippedDfdy);

    // Replace the old dFdx() or dFdy() node with the new node that contains the corrected value
    queueReplacement(correctedResult, OriginalNode::IS_DROPPED);

    return true;
}

}  // anonymous namespace

bool RewriteDfdy(TCompiler *compiler,
                 TIntermNode *root,
                 const TSymbolTable &symbolTable,
                 int shaderVersion,
                 TIntermBinary *flipXY,
                 TIntermTyped *fragRotation)
{
    // dFdy is only valid in GLSL 3.0 and later.
    if (shaderVersion < 300)
        return true;

    return Traverser::Apply(compiler, root, symbolTable, flipXY, fragRotation);
}

}  // namespace sh
