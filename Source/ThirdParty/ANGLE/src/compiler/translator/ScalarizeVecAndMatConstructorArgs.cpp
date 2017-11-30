//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Scalarize vector and matrix constructor args, so that vectors built from components don't have
// matrix arguments, and matrices built from components don't have vector arguments. This avoids
// driver bugs around vector and matrix constructors.
//

#include "common/debug.h"
#include "compiler/translator/ScalarizeVecAndMatConstructorArgs.h"

#include <algorithm>

#include "angle_gl.h"
#include "common/angleutils.h"
#include "compiler/translator/IntermNode_util.h"
#include "compiler/translator/IntermTraverse.h"

namespace sh
{

namespace
{

bool ContainsMatrixNode(const TIntermSequence &sequence)
{
    for (size_t ii = 0; ii < sequence.size(); ++ii)
    {
        TIntermTyped *node = sequence[ii]->getAsTyped();
        if (node && node->isMatrix())
            return true;
    }
    return false;
}

bool ContainsVectorNode(const TIntermSequence &sequence)
{
    for (size_t ii = 0; ii < sequence.size(); ++ii)
    {
        TIntermTyped *node = sequence[ii]->getAsTyped();
        if (node && node->isVector())
            return true;
    }
    return false;
}

TIntermBinary *ConstructVectorIndexBinaryNode(TIntermSymbol *symbolNode, int index)
{
    return new TIntermBinary(EOpIndexDirect, symbolNode, CreateIndexNode(index));
}

TIntermBinary *ConstructMatrixIndexBinaryNode(TIntermSymbol *symbolNode, int colIndex, int rowIndex)
{
    TIntermBinary *colVectorNode = ConstructVectorIndexBinaryNode(symbolNode, colIndex);

    return new TIntermBinary(EOpIndexDirect, colVectorNode, CreateIndexNode(rowIndex));
}

class ScalarizeArgsTraverser : public TIntermTraverser
{
  public:
    ScalarizeArgsTraverser(sh::GLenum shaderType,
                           bool fragmentPrecisionHigh,
                           TSymbolTable *symbolTable)
        : TIntermTraverser(true, false, false, symbolTable),
          mShaderType(shaderType),
          mFragmentPrecisionHigh(fragmentPrecisionHigh)
    {
    }

  protected:
    bool visitAggregate(Visit visit, TIntermAggregate *node) override;
    bool visitBlock(Visit visit, TIntermBlock *node) override;

  private:
    void scalarizeArgs(TIntermAggregate *aggregate, bool scalarizeVector, bool scalarizeMatrix);

    // If we have the following code:
    //   mat4 m(0);
    //   vec4 v(1, m);
    // We will rewrite to:
    //   mat4 m(0);
    //   mat4 s0 = m;
    //   vec4 v(1, s0[0][0], s0[0][1], s0[0][2]);
    // This function is to create nodes for "mat4 s0 = m;" and insert it to the code sequence. This
    // way the possible side effects of the constructor argument will only be evaluated once.
    void createTempVariable(TIntermTyped *original);

    std::vector<TIntermSequence> mBlockStack;

    sh::GLenum mShaderType;
    bool mFragmentPrecisionHigh;
};

bool ScalarizeArgsTraverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    if (visit == PreVisit && node->getOp() == EOpConstruct)
    {
        if (node->getType().isVector() && ContainsMatrixNode(*(node->getSequence())))
            scalarizeArgs(node, false, true);
        else if (node->getType().isMatrix() && ContainsVectorNode(*(node->getSequence())))
            scalarizeArgs(node, true, false);
    }
    return true;
}

bool ScalarizeArgsTraverser::visitBlock(Visit visit, TIntermBlock *node)
{
    mBlockStack.push_back(TIntermSequence());
    {
        for (TIntermNode *child : *node->getSequence())
        {
            ASSERT(child != nullptr);
            child->traverse(this);
            mBlockStack.back().push_back(child);
        }
    }
    if (mBlockStack.back().size() > node->getSequence()->size())
    {
        node->getSequence()->clear();
        *(node->getSequence()) = mBlockStack.back();
    }
    mBlockStack.pop_back();
    return false;
}

void ScalarizeArgsTraverser::scalarizeArgs(TIntermAggregate *aggregate,
                                           bool scalarizeVector,
                                           bool scalarizeMatrix)
{
    ASSERT(aggregate);
    ASSERT(!aggregate->isArray());
    int size                  = static_cast<int>(aggregate->getType().getObjectSize());
    TIntermSequence *sequence = aggregate->getSequence();
    TIntermSequence original(*sequence);
    sequence->clear();
    for (size_t ii = 0; ii < original.size(); ++ii)
    {
        ASSERT(size > 0);
        TIntermTyped *node = original[ii]->getAsTyped();
        ASSERT(node);
        createTempVariable(node);
        if (node->isScalar())
        {
            sequence->push_back(createTempSymbol(node->getType()));
            size--;
        }
        else if (node->isVector())
        {
            if (scalarizeVector)
            {
                int repeat = std::min(size, node->getNominalSize());
                size -= repeat;
                for (int index = 0; index < repeat; ++index)
                {
                    TIntermSymbol *symbolNode = createTempSymbol(node->getType());
                    TIntermBinary *newNode    = ConstructVectorIndexBinaryNode(symbolNode, index);
                    sequence->push_back(newNode);
                }
            }
            else
            {
                TIntermSymbol *symbolNode = createTempSymbol(node->getType());
                sequence->push_back(symbolNode);
                size -= node->getNominalSize();
            }
        }
        else
        {
            ASSERT(node->isMatrix());
            if (scalarizeMatrix)
            {
                int colIndex = 0, rowIndex = 0;
                int repeat = std::min(size, node->getCols() * node->getRows());
                size -= repeat;
                while (repeat > 0)
                {
                    TIntermSymbol *symbolNode = createTempSymbol(node->getType());
                    TIntermBinary *newNode =
                        ConstructMatrixIndexBinaryNode(symbolNode, colIndex, rowIndex);
                    sequence->push_back(newNode);
                    rowIndex++;
                    if (rowIndex >= node->getRows())
                    {
                        rowIndex = 0;
                        colIndex++;
                    }
                    repeat--;
                }
            }
            else
            {
                TIntermSymbol *symbolNode = createTempSymbol(node->getType());
                sequence->push_back(symbolNode);
                size -= node->getCols() * node->getRows();
            }
        }
    }
}

void ScalarizeArgsTraverser::createTempVariable(TIntermTyped *original)
{
    ASSERT(original);
    nextTemporaryId();
    TIntermDeclaration *decl = createTempInitDeclaration(original);

    TType type = original->getType();
    if (mShaderType == GL_FRAGMENT_SHADER && type.getBasicType() == EbtFloat &&
        type.getPrecision() == EbpUndefined)
    {
        // We use the highest available precision for the temporary variable
        // to avoid computing the actual precision using the rules defined
        // in GLSL ES 1.0 Section 4.5.2.
        TIntermBinary *init = decl->getSequence()->at(0)->getAsBinaryNode();
        init->getTypePointer()->setPrecision(mFragmentPrecisionHigh ? EbpHigh : EbpMedium);
        init->getLeft()->getTypePointer()->setPrecision(mFragmentPrecisionHigh ? EbpHigh
                                                                               : EbpMedium);
    }

    ASSERT(mBlockStack.size() > 0);
    TIntermSequence &sequence = mBlockStack.back();
    sequence.push_back(decl);
}

}  // namespace anonymous

void ScalarizeVecAndMatConstructorArgs(TIntermBlock *root,
                                       sh::GLenum shaderType,
                                       bool fragmentPrecisionHigh,
                                       TSymbolTable *symbolTable)
{
    ScalarizeArgsTraverser scalarizer(shaderType, fragmentPrecisionHigh, symbolTable);
    root->traverse(&scalarizer);
}

}  // namespace sh
