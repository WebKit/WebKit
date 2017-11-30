//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RunAtTheEndOfShader.cpp: Add code to be run at the end of the shader. In case main() contains a
// return statement, this is done by replacing the main() function with another function that calls
// the old main, like this:
//
// void main() { body }
// =>
// void main0() { body }
// void main()
// {
//     main0();
//     codeToRun
// }
//
// This way the code will get run even if the return statement inside main is executed.
//

#include "compiler/translator/RunAtTheEndOfShader.h"

#include "compiler/translator/FindMain.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/IntermNode_util.h"
#include "compiler/translator/IntermTraverse.h"
#include "compiler/translator/SymbolTable.h"

namespace sh
{

namespace
{

class ContainsReturnTraverser : public TIntermTraverser
{
  public:
    ContainsReturnTraverser() : TIntermTraverser(true, false, false), mContainsReturn(false) {}

    bool visitBranch(Visit visit, TIntermBranch *node) override
    {
        if (node->getFlowOp() == EOpReturn)
        {
            mContainsReturn = true;
        }
        return false;
    }

    bool containsReturn() { return mContainsReturn; }

  private:
    bool mContainsReturn;
};

bool ContainsReturn(TIntermNode *node)
{
    ContainsReturnTraverser traverser;
    node->traverse(&traverser);
    return traverser.containsReturn();
}

void WrapMainAndAppend(TIntermBlock *root,
                       TIntermFunctionDefinition *main,
                       TIntermNode *codeToRun,
                       TSymbolTable *symbolTable)
{
    // Replace main() with main0() with the same body.
    TSymbolUniqueId oldMainId(symbolTable);
    std::stringstream oldMainName;
    oldMainName << "main" << oldMainId.get();
    TIntermFunctionDefinition *oldMain = CreateInternalFunctionDefinitionNode(
        TType(EbtVoid), oldMainName.str().c_str(), main->getBody(), oldMainId);

    bool replaced = root->replaceChildNode(main, oldMain);
    ASSERT(replaced);

    // void main()
    TIntermFunctionPrototype *newMainProto = new TIntermFunctionPrototype(
        TType(EbtVoid), main->getFunctionPrototype()->getFunctionSymbolInfo()->getId());
    newMainProto->getFunctionSymbolInfo()->setName("main");

    // {
    //     main0();
    //     codeToRun
    // }
    TIntermBlock *newMainBody     = new TIntermBlock();
    TIntermAggregate *oldMainCall = CreateInternalFunctionCallNode(
        TType(EbtVoid), oldMainName.str().c_str(), oldMainId, new TIntermSequence());
    newMainBody->appendStatement(oldMainCall);
    newMainBody->appendStatement(codeToRun);

    // Add the new main() to the root node.
    TIntermFunctionDefinition *newMain = new TIntermFunctionDefinition(newMainProto, newMainBody);
    root->appendStatement(newMain);
}

}  // anonymous namespace

void RunAtTheEndOfShader(TIntermBlock *root, TIntermNode *codeToRun, TSymbolTable *symbolTable)
{
    TIntermFunctionDefinition *main = FindMain(root);
    if (!ContainsReturn(main))
    {
        main->getBody()->appendStatement(codeToRun);
        return;
    }

    WrapMainAndAppend(root, main, codeToRun, symbolTable);
}

}  // namespace sh
