//
// Copyright (c) 2002-2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The ArrayReturnValueToOutParameter function changes return values of an array type to out
// parameters in
// function definitions, prototypes, and call sites.

#include "compiler/translator/ArrayReturnValueToOutParameter.h"

#include "compiler/translator/IntermNode.h"

namespace sh
{

namespace
{

void CopyAggregateChildren(TIntermAggregateBase *from, TIntermAggregateBase *to)
{
    const TIntermSequence *fromSequence = from->getSequence();
    for (size_t ii = 0; ii < fromSequence->size(); ++ii)
    {
        to->getSequence()->push_back(fromSequence->at(ii));
    }
}

TIntermSymbol *CreateReturnValueSymbol(const TType &type)
{
    TIntermSymbol *node = new TIntermSymbol(0, "angle_return", type);
    node->setInternal(true);
    return node;
}

TIntermSymbol *CreateReturnValueOutSymbol(const TType &type)
{
    TType outType(type);
    outType.setQualifier(EvqOut);
    return CreateReturnValueSymbol(outType);
}

TIntermAggregate *CreateReplacementCall(TIntermAggregate *originalCall,
                                        TIntermTyped *returnValueTarget)
{
    TIntermSequence *replacementArguments = new TIntermSequence();
    TIntermSequence *originalArguments    = originalCall->getSequence();
    for (auto &arg : *originalArguments)
    {
        replacementArguments->push_back(arg);
    }
    replacementArguments->push_back(returnValueTarget);
    TIntermAggregate *replacementCall = TIntermAggregate::CreateFunctionCall(
        TType(EbtVoid), originalCall->getFunctionSymbolInfo()->getId(),
        originalCall->getFunctionSymbolInfo()->getNameObj(), replacementArguments);
    replacementCall->setLine(originalCall->getLine());
    return replacementCall;
}

class ArrayReturnValueToOutParameterTraverser : private TIntermTraverser
{
  public:
    static void apply(TIntermNode *root, unsigned int *temporaryIndex);

  private:
    ArrayReturnValueToOutParameterTraverser();

    bool visitFunctionPrototype(Visit visit, TIntermFunctionPrototype *node) override;
    bool visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node) override;
    bool visitAggregate(Visit visit, TIntermAggregate *node) override;
    bool visitBranch(Visit visit, TIntermBranch *node) override;
    bool visitBinary(Visit visit, TIntermBinary *node) override;

    bool mInFunctionWithArrayReturnValue;
};

void ArrayReturnValueToOutParameterTraverser::apply(TIntermNode *root, unsigned int *temporaryIndex)
{
    ArrayReturnValueToOutParameterTraverser arrayReturnValueToOutParam;
    arrayReturnValueToOutParam.useTemporaryIndex(temporaryIndex);
    root->traverse(&arrayReturnValueToOutParam);
    arrayReturnValueToOutParam.updateTree();
}

ArrayReturnValueToOutParameterTraverser::ArrayReturnValueToOutParameterTraverser()
    : TIntermTraverser(true, false, true), mInFunctionWithArrayReturnValue(false)
{
}

bool ArrayReturnValueToOutParameterTraverser::visitFunctionDefinition(
    Visit visit,
    TIntermFunctionDefinition *node)
{
    if (node->getFunctionPrototype()->isArray() && visit == PreVisit)
    {
        // Replacing the function header is done on visitFunctionPrototype().
        mInFunctionWithArrayReturnValue = true;
    }
    if (visit == PostVisit)
    {
        mInFunctionWithArrayReturnValue = false;
    }
    return true;
}

bool ArrayReturnValueToOutParameterTraverser::visitFunctionPrototype(Visit visit,
                                                                     TIntermFunctionPrototype *node)
{
    if (visit == PreVisit && node->isArray())
    {
        // Replace the whole prototype node with another node that has the out parameter
        // added. Also set the function to return void.
        TIntermFunctionPrototype *replacement =
            new TIntermFunctionPrototype(TType(EbtVoid), node->getFunctionSymbolInfo()->getId());
        CopyAggregateChildren(node, replacement);
        replacement->getSequence()->push_back(CreateReturnValueOutSymbol(node->getType()));
        *replacement->getFunctionSymbolInfo() = *node->getFunctionSymbolInfo();
        replacement->setLine(node->getLine());

        queueReplacement(node, replacement, OriginalNode::IS_DROPPED);
    }
    return false;
}

bool ArrayReturnValueToOutParameterTraverser::visitAggregate(Visit visit, TIntermAggregate *node)
{
    ASSERT(!node->isArray() || node->getOp() != EOpCallInternalRawFunction);
    if (visit == PreVisit && node->isArray() && node->getOp() == EOpCallFunctionInAST)
    {
        // Handle call sites where the returned array is not assigned.
        // Examples where f() is a function returning an array:
        // 1. f();
        // 2. another_array == f();
        // 3. another_function(f());
        // 4. return f();
        // Cases 2 to 4 are already converted to simpler cases by
        // SeparateExpressionsReturningArrays, so we only need to worry about the case where a
        // function call returning an array forms an expression by itself.
        TIntermBlock *parentBlock = getParentNode()->getAsBlock();
        if (parentBlock)
        {
            nextTemporaryIndex();
            TIntermSequence replacements;
            replacements.push_back(createTempDeclaration(node->getType()));
            TIntermSymbol *returnSymbol = createTempSymbol(node->getType());
            replacements.push_back(CreateReplacementCall(node, returnSymbol));
            mMultiReplacements.push_back(
                NodeReplaceWithMultipleEntry(parentBlock, node, replacements));
        }
        return false;
    }
    return true;
}

bool ArrayReturnValueToOutParameterTraverser::visitBranch(Visit visit, TIntermBranch *node)
{
    if (mInFunctionWithArrayReturnValue && node->getFlowOp() == EOpReturn)
    {
        // Instead of returning a value, assign to the out parameter and then return.
        TIntermSequence replacements;

        TIntermTyped *expression = node->getExpression();
        ASSERT(expression != nullptr);
        TIntermSymbol *returnValueSymbol = CreateReturnValueSymbol(expression->getType());
        TIntermBinary *replacementAssignment =
            new TIntermBinary(EOpAssign, returnValueSymbol, expression);
        replacementAssignment->setLine(expression->getLine());
        replacements.push_back(replacementAssignment);

        TIntermBranch *replacementBranch = new TIntermBranch(EOpReturn, nullptr);
        replacementBranch->setLine(node->getLine());
        replacements.push_back(replacementBranch);

        mMultiReplacements.push_back(
            NodeReplaceWithMultipleEntry(getParentNode()->getAsBlock(), node, replacements));
    }
    return false;
}

bool ArrayReturnValueToOutParameterTraverser::visitBinary(Visit visit, TIntermBinary *node)
{
    if (node->getOp() == EOpAssign && node->getLeft()->isArray())
    {
        TIntermAggregate *rightAgg = node->getRight()->getAsAggregate();
        ASSERT(rightAgg == nullptr || rightAgg->getOp() != EOpCallInternalRawFunction);
        if (rightAgg != nullptr && rightAgg->getOp() == EOpCallFunctionInAST)
        {
            TIntermAggregate *replacementCall = CreateReplacementCall(rightAgg, node->getLeft());
            queueReplacement(node, replacementCall, OriginalNode::IS_DROPPED);
        }
    }
    return false;
}

}  // namespace

void ArrayReturnValueToOutParameter(TIntermNode *root, unsigned int *temporaryIndex)
{
    ArrayReturnValueToOutParameterTraverser::apply(root, temporaryIndex);
}

}  // namespace sh
