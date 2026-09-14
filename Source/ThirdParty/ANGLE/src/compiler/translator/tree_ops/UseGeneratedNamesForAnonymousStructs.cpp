//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/tree_ops/UseGeneratedNamesForAnonymousStructs.h"

#include "compiler/translator/Compiler.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/Symbol.h"

namespace sh
{
bool UseGeneratedNamesForAnonymousStructs(TCompiler *compiler, TIntermBlock *root)
{
    // Only structs at global scope may have SymbolType::Empty; the rest are already given a
    // generated name by the parser.
    for (TIntermNode *node : *root->getSequence())
    {
        TIntermDeclaration *decl = node->getAsDeclarationNode();
        if (decl != nullptr)
        {
            // SeparateDeclarations must already be run
            const TIntermSequence &sequence = *decl->getSequence();
            ASSERT(sequence.size() == 1);

            TIntermSymbol *symbol       = sequence.front()->getAsSymbolNode();
            const TStructure *structure = nullptr;
            if (symbol != nullptr)
            {
                structure = symbol->getType().getStruct();
            }
            else
            {
                TIntermBinary *initNode = sequence.front()->getAsBinaryNode();
                ASSERT(initNode && initNode->getOp() == EOpInitialize);
                ASSERT(initNode->getLeft()->getAsSymbolNode());
                structure = initNode->getLeft()->getType().getStruct();
            }

            if (structure != nullptr && structure->symbolType() == SymbolType::Empty)
            {
                structure->forceGeneratedName();
            }
        }
    }

    return compiler->validateAST(root);
}

}  // namespace sh
