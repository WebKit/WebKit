//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeferGlobalInitializers is an AST traverser that moves global initializers into a function, and
// adds a function call to that function in the beginning of main().
// This enables initialization of globals with uniforms or non-constant globals, as allowed by
// the WebGL spec. Some initializers referencing non-constants may need to be unfolded into if
// statements in HLSL - this kind of steps should be done after DeferGlobalInitializers is run.
//

#include "compiler/translator/DeferGlobalInitializers.h"

#include "compiler/translator/IntermNode.h"
#include "compiler/translator/SymbolTable.h"

namespace sh
{

namespace
{

class DeferGlobalInitializersTraverser : public TIntermTraverser
{
  public:
    DeferGlobalInitializersTraverser();

    bool visitBinary(Visit visit, TIntermBinary *node) override;

    void insertInitFunction(TIntermBlock *root);

  private:
    TIntermSequence mDeferredInitializers;
};

DeferGlobalInitializersTraverser::DeferGlobalInitializersTraverser()
    : TIntermTraverser(true, false, false)
{
}

bool DeferGlobalInitializersTraverser::visitBinary(Visit visit, TIntermBinary *node)
{
    if (node->getOp() == EOpInitialize)
    {
        TIntermSymbol *symbolNode = node->getLeft()->getAsSymbolNode();
        ASSERT(symbolNode);
        TIntermTyped *expression = node->getRight();

        if (mInGlobalScope && (expression->getQualifier() != EvqConst ||
                               (expression->getAsConstantUnion() == nullptr &&
                                !expression->isConstructorWithOnlyConstantUnionParameters())))
        {
            // For variables which are not constant, defer their real initialization until
            // after we initialize uniforms.
            // Deferral is done also in any cases where the variable has not been constant folded,
            // since otherwise there's a chance that HLSL output will generate extra statements
            // from the initializer expression.
            TIntermBinary *deferredInit =
                new TIntermBinary(EOpAssign, symbolNode->deepCopy(), node->getRight());
            mDeferredInitializers.push_back(deferredInit);

            // Change const global to a regular global if its initialization is deferred.
            // This can happen if ANGLE has not been able to fold the constant expression used
            // as an initializer.
            ASSERT(symbolNode->getQualifier() == EvqConst ||
                   symbolNode->getQualifier() == EvqGlobal);
            if (symbolNode->getQualifier() == EvqConst)
            {
                // All of the siblings in the same declaration need to have consistent qualifiers.
                auto *siblings = getParentNode()->getAsDeclarationNode()->getSequence();
                for (TIntermNode *siblingNode : *siblings)
                {
                    TIntermBinary *siblingBinary = siblingNode->getAsBinaryNode();
                    if (siblingBinary)
                    {
                        ASSERT(siblingBinary->getOp() == EOpInitialize);
                        siblingBinary->getLeft()->getTypePointer()->setQualifier(EvqGlobal);
                    }
                    siblingNode->getAsTyped()->getTypePointer()->setQualifier(EvqGlobal);
                }
                // This node is one of the siblings.
                ASSERT(symbolNode->getQualifier() == EvqGlobal);
            }
            // Remove the initializer from the global scope and just declare the global instead.
            queueReplacement(node, symbolNode, OriginalNode::IS_DROPPED);
        }
    }
    return false;
}

void DeferGlobalInitializersTraverser::insertInitFunction(TIntermBlock *root)
{
    if (mDeferredInitializers.empty())
    {
        return;
    }
    TSymbolUniqueId initFunctionId;

    const char *functionName = "initializeDeferredGlobals";

    // Add function prototype to the beginning of the shader
    TIntermFunctionPrototype *functionPrototypeNode =
        CreateInternalFunctionPrototypeNode(TType(EbtVoid), functionName, initFunctionId);
    root->getSequence()->insert(root->getSequence()->begin(), functionPrototypeNode);

    // Add function definition to the end of the shader
    TIntermBlock *functionBodyNode = new TIntermBlock();
    TIntermSequence *functionBody  = functionBodyNode->getSequence();
    for (const auto &deferredInit : mDeferredInitializers)
    {
        functionBody->push_back(deferredInit);
    }
    TIntermFunctionDefinition *functionDefinition = CreateInternalFunctionDefinitionNode(
        TType(EbtVoid), functionName, functionBodyNode, initFunctionId);
    root->getSequence()->push_back(functionDefinition);

    // Insert call into main function
    for (TIntermNode *node : *root->getSequence())
    {
        TIntermFunctionDefinition *nodeFunction = node->getAsFunctionDefinition();
        if (nodeFunction != nullptr && nodeFunction->getFunctionSymbolInfo()->isMain())
        {
            TIntermAggregate *functionCallNode = CreateInternalFunctionCallNode(
                TType(EbtVoid), functionName, initFunctionId, nullptr);

            TIntermBlock *mainBody = nodeFunction->getBody();
            ASSERT(mainBody != nullptr);
            mainBody->getSequence()->insert(mainBody->getSequence()->begin(), functionCallNode);
        }
    }
}

}  // namespace

void DeferGlobalInitializers(TIntermBlock *root)
{
    DeferGlobalInitializersTraverser traverser;
    root->traverse(&traverser);

    // Replace the initializers of the global variables.
    traverser.updateTree();

    // Add the function with initialization and the call to that.
    traverser.insertInitFunction(root);
}

}  // namespace sh
