//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FindMain.cpp: Find the main() function definition in a given AST.

#include "compiler/translator/FindMain.h"

#include "compiler/translator/IntermNode.h"

namespace sh
{

TIntermFunctionDefinition *FindMain(TIntermBlock *root)
{
    for (TIntermNode *node : *root->getSequence())
    {
        TIntermFunctionDefinition *nodeFunction = node->getAsFunctionDefinition();
        if (nodeFunction != nullptr && nodeFunction->getFunctionSymbolInfo()->isMain())
        {
            return nodeFunction;
        }
    }
    return nullptr;
}

TIntermBlock *FindMainBody(TIntermBlock *root)
{
    TIntermFunctionDefinition *main = FindMain(root);
    ASSERT(main != nullptr);
    TIntermBlock *mainBody = main->getBody();
    ASSERT(mainBody != nullptr);
    return mainBody;
}

}  // namespace sh
