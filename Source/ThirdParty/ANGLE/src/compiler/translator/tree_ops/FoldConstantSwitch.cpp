//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/tree_ops/FoldConstantSwitch.h"

#include "compiler/translator/IntermNode.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{

namespace
{

struct ControlFlowInfo
{
    // Whether the current target of |break| is a switch (possibly nested under another switch) with
    // a constant expression.  In particular, this switch would have to specially handle |continue|
    // (see comments below).
    bool isConstantSwitch = false;
    // Whether the current constant-switch has had a |break| (other than the one terminating the
    // case) or |continue| branch inside.
    bool hasBranch = false;
    // If the current constant-switch has had a |continue| branch, a variable is declared before the
    // switch and set to true at the point of |continue|, and the branch is changed to |break|.  An
    // |if (flag) continue;| statement is added after the (replaced) switch.
    const TVariable *continueVariable        = nullptr;
    TIntermDeclaration *continueVariableDecl = nullptr;
    // As the switch body is visited, some variables may be pruned, but they may be used in what's
    // not dead-code eliminated, like:
    //    switch(1){
    //        case 0:
    //            break;
    //            vec4 d = vec4(0);
    //        default:
    //            d = vec4(1);
    //    }
    //
    // Declaration of those variables is moved to before the switch.
    TVector<const TVariable *> prunedDeclarations;
};

class FoldConstantSwitchTraverser : public TIntermTraverser
{
  public:
    FoldConstantSwitchTraverser(TSymbolTable *symbolTable)
        : TIntermTraverser(true, false, true, symbolTable)
    {}

  protected:
    bool visitBranch(Visit visit, TIntermBranch *node) override;
    bool visitSwitch(Visit visit, TIntermSwitch *node) override;
    bool visitLoop(Visit visit, TIntermLoop *node) override;

    // No-op some common nodes that don't need to be recursed as they cannot affect this
    // transformation
    bool visitUnary(Visit visit, TIntermUnary *node) override { return false; }
    bool visitBinary(Visit visit, TIntermBinary *node) override { return false; }
    bool visitTernary(Visit visit, TIntermTernary *node) override { return false; }
    bool visitAggregate(Visit visit, TIntermAggregate *node) override { return false; }
    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override { return false; }

  private:
    void visitSwitchBody(TIntermSwitch *node);
    void onSwitchPostVisit(TIntermSwitch *node);
    void keepDeclaredVariables(TIntermNode *statement);
    TIntermSymbol *getContinueVariable();

    std::vector<ControlFlowInfo> mControlFlowStack;
};

bool FoldConstantSwitchTraverser::visitLoop(Visit visit, TIntermLoop *node)
{
    // Push/pop into the control flow stack so that nested break/continue are not attributed to an
    // enclosing switch.
    if (visit == PreVisit)
    {
        mControlFlowStack.push_back({});
    }
    else
    {
        ASSERT(visit == PostVisit);
        mControlFlowStack.pop_back();
    }
    return true;
}

bool FoldConstantSwitchTraverser::visitSwitch(Visit visit, TIntermSwitch *node)
{
    if (visit == PostVisit)
    {
        onSwitchPostVisit(node);
        return true;
    }

    const bool nestedUnderConstantSwitch =
        !mControlFlowStack.empty() && mControlFlowStack.back().isConstantSwitch;
    mControlFlowStack.push_back({});
    mControlFlowStack.back().isConstantSwitch = nestedUnderConstantSwitch;

    // If the selector is not a constant, traverse the children as usual
    const bool isConstantSelector = node->getInit()->getAsConstantUnion() != nullptr;
    if (!isConstantSelector)
    {
        return true;
    }

    mControlFlowStack.back().isConstantSwitch = true;

    node->getInit()->traverse(this);
    visitSwitchBody(node);

    onSwitchPostVisit(node);
    return false;
}

void FoldConstantSwitchTraverser::onSwitchPostVisit(TIntermSwitch *node)
{
    TIntermBlock *body          = node->getStatementList();
    TIntermSequence &statements = *body->getSequence();

    TIntermSequence replacement;

    // If the switch has no instructions left, just remove the whole switch.
    if (statements.empty())
    {
        // Keep the selector if it has a side effect.
        if (node->getInit()->hasSideEffects())
        {
            replacement.push_back(node->getInit());
        }
        mMultiReplacements.emplace_back(getParentNode()->getAsBlock(), node,
                                        std::move(replacement));
        return;
    }

    // If there are pruned declarations, declare them before the switch.
    const bool hasPrunedDeclarations = !mControlFlowStack.back().prunedDeclarations.empty();
    for (const TVariable *toDeclare : mControlFlowStack.back().prunedDeclarations)
    {
        TIntermDeclaration *decl = new TIntermDeclaration();
        decl->appendDeclarator(new TIntermSymbol(toDeclare));
        replacement.push_back(decl);
    }

    // If a continue branch was visited, declare the continue variable before the switch, and add an
    // |if (flag) continue;| after it.
    const TVariable *continueVariable        = mControlFlowStack.back().continueVariable;
    TIntermDeclaration *continueVariableDecl = mControlFlowStack.back().continueVariableDecl;
    if (continueVariableDecl)
    {
        replacement.push_back(continueVariableDecl);
    }

    // Different cases to handle for the switch itself:
    //
    // * Selector is not constant: Keep switch as-is
    // * Select is constant, no branches: Use the body of the switch without the selector
    // * Select is constant, there are branches: Create a while(true), with the switch's body as its
    //   body.  Append |break| to the body.
    if (node->getInit()->getAsConstantUnion() == nullptr)
    {
        replacement.push_back(node);
    }
    else if (!mControlFlowStack.back().hasBranch)
    {
        replacement.push_back(body);
    }
    else
    {
        statements.push_back(new TIntermBranch(EOpBreak, nullptr));
        replacement.push_back(new TIntermLoop(ELoopFor, nullptr, nullptr, nullptr, body));
    }

    // Variables declared in the switch must be scoped to it.  If there are any that have been
    // hoisted out, create a scope for them.
    if (hasPrunedDeclarations)
    {
        TIntermBlock *scope = new TIntermBlock;
        scope->replaceAllChildren(std::move(replacement));
        ASSERT(replacement.empty());
        replacement.push_back(scope);
    }

    mControlFlowStack.pop_back();

    // |if (flag) continue;|.  Note that if nested under a switch with constant selector, this
    // continue should be replaced with break too.
    if (continueVariableDecl)
    {
        TIntermBlock *ifBody = new TIntermBlock;
        if (!mControlFlowStack.empty() && mControlFlowStack.back().isConstantSwitch)
        {
            TIntermSymbol *parentContinueVar = getContinueVariable();
            ifBody->appendStatement(
                new TIntermBinary(EOpAssign, parentContinueVar, CreateBoolNode(true)));
            ifBody->appendStatement(new TIntermBranch(EOpBreak, nullptr));
        }
        else
        {
            ifBody->appendStatement(new TIntermBranch(EOpContinue, nullptr));
        }
        if (!mControlFlowStack.empty())
        {
            mControlFlowStack.back().hasBranch = true;
        }

        replacement.push_back(
            new TIntermIfElse(new TIntermSymbol(continueVariable), ifBody, nullptr));
    }

    mMultiReplacements.emplace_back(getParentNode()->getAsBlock(), node, std::move(replacement));
}

TIntermSymbol *FoldConstantSwitchTraverser::getContinueVariable()
{
    if (mControlFlowStack.back().continueVariable == nullptr)
    {
        mControlFlowStack.back().continueVariable =
            DeclareTempVariable(mSymbolTable, CreateBoolNode(false), EvqTemporary,
                                &mControlFlowStack.back().continueVariableDecl);
    }

    return new TIntermSymbol(mControlFlowStack.back().continueVariable);
}

bool FoldConstantSwitchTraverser::visitBranch(Visit visit, TIntermBranch *node)
{
    if (mControlFlowStack.empty() || !mControlFlowStack.back().isConstantSwitch)
    {
        return false;
    }

    if (node->getFlowOp() == EOpContinue || node->getFlowOp() == EOpBreak)
    {
        mControlFlowStack.back().hasBranch = true;
    }

    if (node->getFlowOp() == EOpContinue)
    {
        // If |continue| is encountered in a to-be-pruned switch, set a variable and issue a break
        // instead.
        TIntermSymbol *continueVar = getContinueVariable();
        TIntermSequence replacement;
        replacement.push_back(new TIntermBinary(EOpAssign, continueVar, CreateBoolNode(true)));
        replacement.push_back(new TIntermBranch(EOpBreak, nullptr));
        mMultiReplacements.emplace_back(getParentNode()->getAsBlock(), node,
                                        std::move(replacement));
    }
    return false;
}

uint32_t GetConstantAsUInt(const TConstantUnion *value)
{
    TConstantUnion asUInt;
    if (value->getType() == EbtYuvCscStandardEXT)
    {
        asUInt.setUConst(value->getYuvCscStandardEXTConst());
    }
    else
    {
        bool valid = asUInt.cast(EbtUInt, *value);
        ASSERT(valid);
    }
    return asUInt.getUConst();
}

void FoldConstantSwitchTraverser::visitSwitchBody(TIntermSwitch *node)
{
    TIntermBlock *body          = node->getStatementList();
    TIntermSequence &statements = *body->getSequence();

    ScopedNodeInTraversalPath addToPath(this, body);

    TIntermConstantUnion *selectorConstant = node->getInit()->getAsConstantUnion();
    ASSERT(selectorConstant != nullptr);
    const uint32_t selectorValue = GetConstantAsUInt(selectorConstant->getConstantValue());

    // First, find if there's an exact match for the selector value, or otherwise if there is a
    // default to match.
    size_t matchedCaseIndex = statements.size();
    if (selectorConstant)
    {
        for (size_t statementIndex = 0; statementIndex < statements.size(); ++statementIndex)
        {
            TIntermNode *statement = statements[statementIndex];
            TIntermCase *caseLabel = statement->getAsCaseNode();
            if (caseLabel == nullptr)
            {
                continue;
            }
            // Default matches everything.  Remember its index if no exact match is yet visited.
            // Later, if an exact match is visited, it will overwrite this index.
            if (!caseLabel->hasCondition())
            {
                if (matchedCaseIndex == statements.size())
                {
                    matchedCaseIndex = statementIndex;
                }
                continue;
            }

            TIntermConstantUnion *condition = caseLabel->getCondition()->getAsConstantUnion();
            ASSERT(condition != nullptr);

            // If any case matches the value, it's not a no-op.
            const uint32_t caseValue = GetConstantAsUInt(condition->getConstantValue());
            if (caseValue == selectorValue)
            {
                matchedCaseIndex = statementIndex;
                break;
            }
        }
    }

    // Prune every statement up to the matching case
    for (size_t statementIndex = 0; statementIndex < matchedCaseIndex; ++statementIndex)
    {
        // Keep the declared variables to be declared above the if.  The statements that are not
        // pruned may reference these variables.
        keepDeclaredVariables(statements[statementIndex]);
    }

    // Visit every surviving statement, starting from the matching case.
    size_t writeIndex = 0;
    for (size_t statementIndex = matchedCaseIndex; statementIndex < statements.size();
         ++statementIndex)
    {
        TIntermNode *statement = statements[statementIndex];

        // Drop case statements since the switch is going to be folded.  This naturally makes
        // fallthrough work.
        if (statement->getAsCaseNode() != nullptr)
        {
            continue;
        }

        // If a branch is visited, stop; everything after this is going to be dead code.
        TIntermBranch *asBranch = statement->getAsBranchNode();
        if (asBranch == nullptr || asBranch->getFlowOp() != EOpBreak)
        {
            // The statement itself needs to stay, except for a terminating break which can be
            // pruned.
            statement->traverse(this);
            statements[writeIndex++] = statement;
        }
        if (asBranch != nullptr)
        {
            // Drop everything after this.  Note that |{ break; }| is not matched, since it's inside
            // a block.  That's ok, PruneNoOps will do a thorough job later.
            break;
        }
    }
    statements.resize(writeIndex);
}

void FoldConstantSwitchTraverser::keepDeclaredVariables(TIntermNode *statement)
{
    TIntermDeclaration *decl = statement->getAsDeclarationNode();
    if (decl == nullptr)
    {
        return;
    }

    for (TIntermNode *declarator : *decl->getSequence())
    {
        TIntermSymbol *symbol        = declarator->getAsSymbolNode();
        const TVariable *declaredVar = nullptr;
        if (symbol != nullptr)
        {
            declaredVar = &symbol->variable();
        }
        else
        {
            TIntermBinary *initNode = declarator->getAsBinaryNode();
            ASSERT(initNode && initNode->getOp() == EOpInitialize);
            ASSERT(initNode->getLeft()->getAsSymbolNode());
            declaredVar = &initNode->getLeft()->getAsSymbolNode()->variable();
        }

        mControlFlowStack.back().prunedDeclarations.push_back(declaredVar);
    }
}
}  // anonymous namespace

bool FoldConstantSwitch(TCompiler *compiler, TIntermBlock *root, TSymbolTable *symbolTable)
{
    FoldConstantSwitchTraverser traverser(symbolTable);
    root->traverse(&traverser);
    return traverser.updateTree(compiler, root);
}

}  // namespace sh
