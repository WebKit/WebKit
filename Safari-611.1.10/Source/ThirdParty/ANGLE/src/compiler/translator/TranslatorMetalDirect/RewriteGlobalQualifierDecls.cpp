//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/TranslatorMetalDirect/RewriteGlobalQualifierDecls.h"
#include "compiler/translator/TranslatorMetalDirect/Debug.h"
#include "compiler/translator/tree_util/IntermRebuild.h"

using namespace sh;

namespace
{

class FindDeclaredGlobals : public TIntermRebuild
{
  public:
    std::unordered_set<const TVariable *> mDeclaredGlobals;

    FindDeclaredGlobals(TCompiler &compiler) : TIntermRebuild(compiler, true, false) {}

    PreResult visitDeclarationPre(TIntermDeclaration &declNode) override
    {
        TIntermNode *declaratorNode = declNode.getChildNode(0);
        TIntermSymbol *symbolNode   = nullptr;

        if (TIntermBinary *initNode = declaratorNode->getAsBinaryNode())
        {
            symbolNode = initNode->getLeft()->getAsSymbolNode();
        }
        else
        {
            symbolNode = declaratorNode->getAsSymbolNode();
        }

        ASSERT(symbolNode);
        const TVariable &var = symbolNode->variable();

        mDeclaredGlobals.insert(&var);

        return {declNode, VisitBits::Neither};
    }

    PreResult visitFunctionDefinitionPre(TIntermFunctionDefinition &node) override
    {
        return {node, VisitBits::Neither};
    }
};

class Rewriter : public TIntermRebuild
{
    const std::unordered_set<const TVariable *> &mDeclaredGlobals;
    Invariants &mOutInvariants;

  public:
    Rewriter(TCompiler &compiler,
             const std::unordered_set<const TVariable *> &declaredGlobals,
             Invariants &outInvariants)
        : TIntermRebuild(compiler, true, false),
          mDeclaredGlobals(declaredGlobals),
          mOutInvariants(outInvariants)
    {}

    PreResult visitGlobalQualifierDeclarationPre(
        TIntermGlobalQualifierDeclaration &gqDeclNode) override
    {
        TIntermSymbol &symbolNode = *gqDeclNode.getSymbol();
        const TVariable &var      = symbolNode.variable();

        if (gqDeclNode.isInvariant())
        {
            mOutInvariants.insert(var);
        }

        if (mDeclaredGlobals.find(&var) == mDeclaredGlobals.end())
        {
            return *new TIntermDeclaration{&symbolNode};
        }
        return nullptr;
    }

    PreResult visitDeclarationPre(TIntermDeclaration &node) override
    {
        return {node, VisitBits::Neither};
    }

    PreResult visitFunctionDefinitionPre(TIntermFunctionDefinition &node) override
    {
        return {node, VisitBits::Neither};
    }
};

}  // anonymous namespace

bool sh::RewriteGlobalQualifierDecls(TCompiler &compiler,
                                     TIntermBlock &root,
                                     Invariants &outInvariants)
{
    FindDeclaredGlobals fdg(compiler);
    if (!fdg.rebuildRoot(root))
    {
        LOGIC_ERROR();
        return false;
    }

    Rewriter rewriter(compiler, fdg.mDeclaredGlobals, outInvariants);
    if (!rewriter.rebuildRoot(root))
    {
        return false;
    }

    return true;
}
