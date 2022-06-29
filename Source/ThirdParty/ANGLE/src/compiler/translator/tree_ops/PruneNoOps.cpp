//
// Copyright 2002 The ANGLE Project Authors. All rights reserved.
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
//   3. Statements after discard, return, break and continue.

#include "compiler/translator/tree_ops/PruneNoOps.h"

#include "compiler/translator/Symbol.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

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
    [[nodiscard]] static bool apply(TCompiler *compiler,
                                    TIntermBlock *root,
                                    TSymbolTable *symbolTable);

  private:
    PruneNoOpsTraverser(TSymbolTable *symbolTable);
    bool visitDeclaration(Visit, TIntermDeclaration *node) override;
    bool visitBlock(Visit visit, TIntermBlock *node) override;
    bool visitLoop(Visit visit, TIntermLoop *loop) override;
    bool visitBranch(Visit visit, TIntermBranch *node) override;

    bool mIsBranchVisited = false;
};

bool PruneNoOpsTraverser::apply(TCompiler *compiler, TIntermBlock *root, TSymbolTable *symbolTable)
{
    PruneNoOpsTraverser prune(symbolTable);
    root->traverse(&prune);
    return prune.updateTree(compiler, root);
}

PruneNoOpsTraverser::PruneNoOpsTraverser(TSymbolTable *symbolTable)
    : TIntermTraverser(true, true, true, symbolTable)
{}

bool PruneNoOpsTraverser::visitDeclaration(Visit visit, TIntermDeclaration *node)
{
    if (visit != PreVisit)
    {
        return true;
    }

    TIntermSequence *sequence = node->getSequence();
    if (sequence->size() >= 1)
    {
        TIntermSymbol *declaratorSymbol = sequence->front()->getAsSymbolNode();
        // Prune declarations without a variable name, unless it's an interface block declaration.
        if (declaratorSymbol != nullptr &&
            declaratorSymbol->variable().symbolType() == SymbolType::Empty &&
            !declaratorSymbol->isInterfaceBlock())
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
                mMultiReplacements.emplace_back(node, declaratorSymbol,
                                                std::move(emptyReplacement));
            }
            else if (declaratorSymbol->getBasicType() != EbtStruct)
            {
                // If there are entirely empty non-struct declarations, they result in
                // TIntermDeclaration nodes without any children in the parsing stage. These are
                // handled in visitBlock and visitLoop.
                UNREACHABLE();
            }
            else if (declaratorSymbol->getQualifier() != EvqGlobal &&
                     declaratorSymbol->getQualifier() != EvqTemporary)
            {
                // Single struct declarations may just declare the struct type and no variables, so
                // they should not be pruned. Here we handle an empty struct declaration with a
                // qualifier, for example like this:
                //   const struct a { int i; };
                // NVIDIA GL driver version 367.27 doesn't accept this kind of declarations, so we
                // convert the declaration to a regular struct declaration. This is okay, since ESSL
                // 1.00 spec section 4.1.8 says about structs that "The optional qualifiers only
                // apply to any declarators, and are not part of the type being defined for name."

                // Create a new variable to use in the declarator so that the variable and node
                // types are kept consistent.
                TType *type = new TType(declaratorSymbol->getType());
                if (mInGlobalScope)
                {
                    type->setQualifier(EvqGlobal);
                }
                else
                {
                    type->setQualifier(EvqTemporary);
                }
                TVariable *variable =
                    new TVariable(mSymbolTable, kEmptyImmutableString, type, SymbolType::Empty);
                queueReplacementWithParent(node, declaratorSymbol, new TIntermSymbol(variable),
                                           OriginalNode::IS_DROPPED);
            }
        }
    }
    return false;
}

bool PruneNoOpsTraverser::visitBlock(Visit visit, TIntermBlock *node)
{
    ASSERT(visit == PreVisit);

    TIntermSequence &statements = *node->getSequence();

    // Visit each statement in the block one by one.  Once a branch is visited (break, continue,
    // return or discard), drop the rest of the statements.
    for (size_t statementIndex = 0; statementIndex < statements.size(); ++statementIndex)
    {
        TIntermNode *statement = statements[statementIndex];

        // If the statement is a switch case label, stop pruning and continue visiting the children.
        if (statement->getAsCaseNode() != nullptr)
        {
            mIsBranchVisited = false;
        }

        // If a branch is visited, prune the statement.  If the statement is a no-op, also prune it.
        if (mIsBranchVisited || IsNoOp(statement))
        {
            TIntermSequence emptyReplacement;
            mMultiReplacements.emplace_back(node, statement, std::move(emptyReplacement));
            continue;
        }

        // Visit the statement if not pruned.
        statement->traverse(this);
    }

    // If the parent is a block and mIsBranchVisited is set, this is a nested block without any
    // condition (like if, loop or switch), so the rest of the parent block should also be pruned.
    // Otherwise the parent block should be unaffected.
    if (mIsBranchVisited && getParentNode()->getAsBlock() == nullptr)
    {
        mIsBranchVisited = false;
    }

    return false;
}

bool PruneNoOpsTraverser::visitLoop(Visit visit, TIntermLoop *loop)
{
    if (visit != PreVisit)
    {
        return true;
    }

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

bool PruneNoOpsTraverser::visitBranch(Visit visit, TIntermBranch *node)
{
    ASSERT(visit == PreVisit);

    mIsBranchVisited = true;

    // Only possible child is the value of a return statement, which has nothing to prune.
    return false;
}
}  // namespace

bool PruneNoOps(TCompiler *compiler, TIntermBlock *root, TSymbolTable *symbolTable)
{
    return PruneNoOpsTraverser::apply(compiler, root, symbolTable);
}

}  // namespace sh
