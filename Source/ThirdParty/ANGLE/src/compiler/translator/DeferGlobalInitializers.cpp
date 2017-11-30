//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeferGlobalInitializers is an AST traverser that moves global initializers into a separate
// function that is called in the beginning of main(). This enables initialization of globals with
// uniforms or non-constant globals, as allowed by the WebGL spec. Some initializers referencing
// non-constants may need to be unfolded into if statements in HLSL - this kind of steps should be
// done after DeferGlobalInitializers is run. Note that it's important that the function definition
// is at the end of the shader, as some globals may be declared after main().
//
// It can also initialize all uninitialized globals.
//

#include "compiler/translator/DeferGlobalInitializers.h"

#include "compiler/translator/FindMain.h"
#include "compiler/translator/InitializeVariables.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/IntermNode_util.h"
#include "compiler/translator/SymbolTable.h"

namespace sh
{

namespace
{

void GetDeferredInitializers(TIntermDeclaration *declaration,
                             bool initializeUninitializedGlobals,
                             bool canUseLoopsToInitialize,
                             TIntermSequence *deferredInitializersOut,
                             TSymbolTable *symbolTable)
{
    // SeparateDeclarations should have already been run.
    ASSERT(declaration->getSequence()->size() == 1);

    TIntermNode *declarator = declaration->getSequence()->back();
    TIntermBinary *init     = declarator->getAsBinaryNode();
    if (init)
    {
        TIntermSymbol *symbolNode = init->getLeft()->getAsSymbolNode();
        ASSERT(symbolNode);
        TIntermTyped *expression = init->getRight();

        if ((expression->getQualifier() != EvqConst ||
             (expression->getAsConstantUnion() == nullptr &&
              !expression->isConstructorWithOnlyConstantUnionParameters())))
        {
            // For variables which are not constant, defer their real initialization until
            // after we initialize uniforms.
            // Deferral is done also in any cases where the variable has not been constant
            // folded, since otherwise there's a chance that HLSL output will generate extra
            // statements from the initializer expression.

            // Change const global to a regular global if its initialization is deferred.
            // This can happen if ANGLE has not been able to fold the constant expression used
            // as an initializer.
            ASSERT(symbolNode->getQualifier() == EvqConst ||
                   symbolNode->getQualifier() == EvqGlobal);
            if (symbolNode->getQualifier() == EvqConst)
            {
                symbolNode->getTypePointer()->setQualifier(EvqGlobal);
            }

            TIntermBinary *deferredInit =
                new TIntermBinary(EOpAssign, symbolNode->deepCopy(), init->getRight());
            deferredInitializersOut->push_back(deferredInit);

            // Remove the initializer from the global scope and just declare the global instead.
            declaration->replaceChildNode(init, symbolNode);
        }
    }
    else if (initializeUninitializedGlobals)
    {
        TIntermSymbol *symbolNode = declarator->getAsSymbolNode();
        ASSERT(symbolNode);

        // Ignore ANGLE internal variables.
        if (symbolNode->getName().isInternal())
            return;

        if (symbolNode->getQualifier() == EvqGlobal && symbolNode->getSymbol() != "")
        {
            TIntermSequence *initCode =
                CreateInitCode(symbolNode, canUseLoopsToInitialize, symbolTable);
            deferredInitializersOut->insert(deferredInitializersOut->end(), initCode->begin(),
                                            initCode->end());
        }
    }
}

void InsertInitCallToMain(TIntermBlock *root,
                          TIntermSequence *deferredInitializers,
                          TSymbolTable *symbolTable)
{
    TIntermBlock *initGlobalsBlock = new TIntermBlock();
    initGlobalsBlock->getSequence()->swap(*deferredInitializers);

    TSymbolUniqueId initGlobalsFunctionId(symbolTable);

    const char *kInitGlobalsFunctionName = "initGlobals";

    TIntermFunctionPrototype *initGlobalsFunctionPrototype =
        CreateInternalFunctionPrototypeNode(TType(), kInitGlobalsFunctionName, initGlobalsFunctionId);
    root->getSequence()->insert(root->getSequence()->begin(), initGlobalsFunctionPrototype);
    TIntermFunctionDefinition *initGlobalsFunctionDefinition = CreateInternalFunctionDefinitionNode(
        TType(), kInitGlobalsFunctionName, initGlobalsBlock, initGlobalsFunctionId);
    root->appendStatement(initGlobalsFunctionDefinition);

    TIntermAggregate *initGlobalsCall = CreateInternalFunctionCallNode(
        TType(), kInitGlobalsFunctionName, initGlobalsFunctionId, new TIntermSequence());

    TIntermBlock *mainBody = FindMainBody(root);
    mainBody->getSequence()->insert(mainBody->getSequence()->begin(), initGlobalsCall);
}

}  // namespace

void DeferGlobalInitializers(TIntermBlock *root,
                             bool initializeUninitializedGlobals,
                             bool canUseLoopsToInitialize,
                             TSymbolTable *symbolTable)
{
    TIntermSequence *deferredInitializers = new TIntermSequence();

    // Loop over all global statements and process the declarations. This is simpler than using a
    // traverser.
    for (TIntermNode *statement : *root->getSequence())
    {
        TIntermDeclaration *declaration = statement->getAsDeclarationNode();
        if (declaration)
        {
            GetDeferredInitializers(declaration, initializeUninitializedGlobals,
                                    canUseLoopsToInitialize, deferredInitializers, symbolTable);
        }
    }

    // Add the function with initialization and the call to that.
    if (!deferredInitializers->empty())
    {
        InsertInitCallToMain(root, deferredInitializers, symbolTable);
    }
}

}  // namespace sh
