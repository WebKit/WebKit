//
// Copyright (c) 2002-2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PruneNoOps.cpp: The PruneNoOps function prunes:
//   1. Empty declarations "int;". Empty declarators will be pruned as well, so for example:
//        int , a;
//      is turned into
//        int a;
//   2. Literal statements: "1.0;". The ESSL output doesn't define a default precision for float,
//      so float literal statements would end up with no precision which is invalid ESSL.

#include "compiler/translator/PruneNoOps.h"

#include "compiler/translator/IntermTraverse.h"

namespace sh
{

namespace
{

bool IsNoOp(TIntermNode *node)
{
    if (node->getAsConstantUnion() != nullptr)
    {
        return true;
    }
    bool isEmptyDeclaration = node->getAsDeclarationNode() != nullptr &&
                              node->getAsDeclarationNode()->getSequence()->empty();
    if (isEmptyDeclaration)
    {
        return true;
    }
    return false;
}

class PruneNoOpsTraverser : private TIntermTraverser
{
  public:
    static void apply(TIntermBlock *root);

  private:
    PruneNoOpsTraverser();
    bool visitDeclaration(Visit, TIntermDeclaration *node) override;
    bool visitBlock(Visit visit, TIntermBlock *node) override;
    bool visitLoop(Visit visit, TIntermLoop *loop) override;
};

void PruneNoOpsTraverser::apply(TIntermBlock *root)
{
    PruneNoOpsTraverser prune;
    root->traverse(&prune);
    prune.updateTree();
}

PruneNoOpsTraverser::PruneNoOpsTraverser() : TIntermTraverser(true, false, false)
{
}

bool PruneNoOpsTraverser::visitDeclaration(Visit, TIntermDeclaration *node)
{
    TIntermSequence *sequence = node->getSequence();
    if (sequence->size() >= 1)
    {
        TIntermSymbol *sym = sequence->front()->getAsSymbolNode();
        // Prune declarations without a variable name, unless it's an interface block declaration.
        if (sym != nullptr && sym->getSymbol() == "" && !sym->isInterfaceBlock())
        {
            if (sequence->size() > 1)
            {
                // Generate a replacement that will remove the empty declarator in the beginning of
                // a declarator list. Example of a declaration that will be changed:
                // float, a;
                // will be changed to
                // float a;
                // This applies also to struct declarations.
                TIntermSequence emptyReplacement;
                mMultiReplacements.push_back(
                    NodeReplaceWithMultipleEntry(node, sym, emptyReplacement));
            }
            else if (sym->getBasicType() != EbtStruct)
            {
                // If there are entirely empty non-struct declarations, they result in
                // TIntermDeclaration nodes without any children in the parsing stage. These are
                // handled in visitBlock and visitLoop.
                UNREACHABLE();
            }
            else if (sym->getType().getQualifier() != EvqGlobal &&
                     sym->getType().getQualifier() != EvqTemporary)
            {
                // Single struct declarations may just declare the struct type and no variables, so
                // they should not be pruned. Here we handle an empty struct declaration with a
                // qualifier, for example like this:
                //   const struct a { int i; };
                // NVIDIA GL driver version 367.27 doesn't accept this kind of declarations, so we
                // convert the declaration to a regular struct declaration. This is okay, since ESSL
                // 1.00 spec section 4.1.8 says about structs that "The optional qualifiers only
                // apply to any declarators, and are not part of the type being defined for name."

                if (mInGlobalScope)
                {
                    sym->getTypePointer()->setQualifier(EvqGlobal);
                }
                else
                {
                    sym->getTypePointer()->setQualifier(EvqTemporary);
                }
            }
        }
    }
    return false;
}

bool PruneNoOpsTraverser::visitBlock(Visit visit, TIntermBlock *node)
{
    TIntermSequence *statements = node->getSequence();

    for (TIntermNode *statement : *statements)
    {
        if (IsNoOp(statement))
        {
            TIntermSequence emptyReplacement;
            mMultiReplacements.push_back(
                NodeReplaceWithMultipleEntry(node, statement, emptyReplacement));
        }
    }

    return true;
}

bool PruneNoOpsTraverser::visitLoop(Visit visit, TIntermLoop *loop)
{
    TIntermTyped *expr = loop->getExpression();
    if (expr != nullptr && IsNoOp(expr))
    {
        loop->setExpression(nullptr);
    }
    TIntermNode *init = loop->getInit();
    if (init != nullptr && IsNoOp(init))
    {
        loop->setInit(nullptr);
    }

    return true;
}

}  // namespace

void PruneNoOps(TIntermBlock *root)
{
    PruneNoOpsTraverser::apply(root);
}

}  // namespace sh
