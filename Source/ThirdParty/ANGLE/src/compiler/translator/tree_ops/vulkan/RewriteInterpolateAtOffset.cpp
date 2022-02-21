//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of InterpolateAtOffset viewport transformation.
// See header for more info.

#include "compiler/translator/tree_ops/vulkan/RewriteInterpolateAtOffset.h"

#include "common/angleutils.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/TranslatorVulkan.h"
#include "compiler/translator/tree_util/DriverUniform.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/SpecializationConstant.h"

namespace sh
{

namespace
{

class Traverser : public TIntermTraverser
{
  public:
    ANGLE_NO_DISCARD static bool Apply(TCompiler *compiler,
                                       ShCompileOptions compileOptions,
                                       TIntermNode *root,
                                       const TSymbolTable &symbolTable,
                                       int ShaderVersion,
                                       SpecConst *specConst,
                                       const DriverUniform *driverUniforms);

  private:
    Traverser(TSymbolTable *symbolTable,
              ShCompileOptions compileOptions,
              int shaderVersion,
              SpecConst *specConst,
              const DriverUniform *driverUniforms);
    bool visitAggregate(Visit visit, TIntermAggregate *node) override;

    const TSymbolTable *symbolTable = nullptr;
    const int shaderVersion;
    SpecConst *mRotationSpecConst        = nullptr;
    const DriverUniform *mDriverUniforms = nullptr;
    bool mUsePreRotation                 = false;
};

Traverser::Traverser(TSymbolTable *symbolTable,
                     ShCompileOptions compileOptions,
                     int shaderVersion,
                     SpecConst *specConst,
                     const DriverUniform *driverUniforms)
    : TIntermTraverser(true, false, false, symbolTable),
      symbolTable(symbolTable),
      shaderVersion(shaderVersion),
      mRotationSpecConst(specConst),
      mDriverUniforms(driverUniforms),
      mUsePreRotation((compileOptions & SH_ADD_PRE_ROTATION) != 0)
{}

// static
bool Traverser::Apply(TCompiler *compiler,
                      ShCompileOptions compileOptions,
                      TIntermNode *root,
                      const TSymbolTable &symbolTable,
                      int shaderVersion,
                      SpecConst *specConst,
                      const DriverUniform *driverUniforms)
{
    TSymbolTable *pSymbolTable = const_cast<TSymbolTable *>(&symbolTable);
    Traverser traverser(pSymbolTable, compileOptions, shaderVersion, specConst, driverUniforms);
    root->traverse(&traverser);
    return traverser.updateTree(compiler, root);
}

bool Traverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    // Decide if the node represents the call of texelFetchOffset.
    if (!BuiltInGroup::IsBuiltIn(node->getOp()))
    {
        return true;
    }

    ASSERT(node->getFunction()->symbolType() == SymbolType::BuiltIn);
    if (node->getFunction()->name() != "interpolateAtOffset")
    {
        return true;
    }

    const TIntermSequence *sequence = node->getSequence();
    ASSERT(sequence->size() == 2u);

    TIntermSequence interpolateAtOffsetArguments;
    // interpolant node
    interpolateAtOffsetArguments.push_back(sequence->at(0));
    // offset
    TIntermTyped *offsetNode = sequence->at(1)->getAsTyped();
    ASSERT(offsetNode->getType().getBasicType() == EbtFloat &&
           offsetNode->getType().getNominalSize() == 2);

    // If pre-rotation is enabled apply the transformation else just flip the Y-coordinate
    TIntermTyped *rotatedXY;
    TOperator mulOp = EOpMul;
    if (mUsePreRotation)
    {
        rotatedXY = mRotationSpecConst->getFragRotationMultiplyFlipXY();
        mulOp     = EOpVectorTimesMatrix;
        if (!rotatedXY)
        {
            TIntermTyped *fragRotation = mDriverUniforms->getFragRotationMatrixRef();
            offsetNode = new TIntermBinary(EOpVectorTimesMatrix, offsetNode, fragRotation);

            rotatedXY = mDriverUniforms->getFlipXYRef();
            mulOp     = EOpMul;
        }
    }
    else
    {
        rotatedXY = mRotationSpecConst->getFlipXY();
        if (!rotatedXY)
        {
            rotatedXY = mDriverUniforms->getFlipXYRef();
        }
    }

    TIntermBinary *correctedOffset = new TIntermBinary(mulOp, offsetNode, rotatedXY);
    correctedOffset->setLine(offsetNode->getLine());
    interpolateAtOffsetArguments.push_back(correctedOffset);

    TIntermTyped *interpolateAtOffsetNode = CreateBuiltInFunctionCallNode(
        "interpolateAtOffset", &interpolateAtOffsetArguments, *symbolTable, shaderVersion);
    interpolateAtOffsetNode->setLine(node->getLine());

    // Replace the old node by this new node.
    queueReplacement(interpolateAtOffsetNode, OriginalNode::IS_DROPPED);

    return true;
}

}  // anonymous namespace

bool RewriteInterpolateAtOffset(TCompiler *compiler,
                                ShCompileOptions compileOptions,
                                TIntermNode *root,
                                const TSymbolTable &symbolTable,
                                int shaderVersion,
                                SpecConst *specConst,
                                const DriverUniform *driverUniforms)
{
    // interpolateAtOffset is only valid in GLSL 3.0 and later.
    if (shaderVersion < 300)
    {
        return true;
    }

    return Traverser::Apply(compiler, compileOptions, root, symbolTable, shaderVersion, specConst,
                            driverUniforms);
}

}  // namespace sh
