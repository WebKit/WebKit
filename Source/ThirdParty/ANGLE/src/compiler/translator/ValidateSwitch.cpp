//
// Copyright (c) 2002-2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/ValidateSwitch.h"

#include "compiler/translator/IntermNode.h"
#include "compiler/translator/Diagnostics.h"

namespace sh
{

namespace
{

class ValidateSwitch : public TIntermTraverser
{
  public:
    static bool validate(TBasicType switchType,
                         TDiagnostics *diagnostics,
                         TIntermBlock *statementList,
                         const TSourceLoc &loc);

    void visitSymbol(TIntermSymbol *) override;
    void visitConstantUnion(TIntermConstantUnion *) override;
    bool visitBinary(Visit, TIntermBinary *) override;
    bool visitUnary(Visit, TIntermUnary *) override;
    bool visitTernary(Visit, TIntermTernary *) override;
    bool visitIfElse(Visit visit, TIntermIfElse *) override;
    bool visitSwitch(Visit, TIntermSwitch *) override;
    bool visitCase(Visit, TIntermCase *node) override;
    bool visitAggregate(Visit, TIntermAggregate *) override;
    bool visitLoop(Visit visit, TIntermLoop *) override;
    bool visitBranch(Visit, TIntermBranch *) override;

  private:
    ValidateSwitch(TBasicType switchType, TDiagnostics *context);

    bool validateInternal(const TSourceLoc &loc);

    TBasicType mSwitchType;
    TDiagnostics *mDiagnostics;
    bool mCaseTypeMismatch;
    bool mFirstCaseFound;
    bool mStatementBeforeCase;
    bool mLastStatementWasCase;
    int mControlFlowDepth;
    bool mCaseInsideControlFlow;
    int mDefaultCount;
    std::set<int> mCasesSigned;
    std::set<unsigned int> mCasesUnsigned;
    bool mDuplicateCases;
};

bool ValidateSwitch::validate(TBasicType switchType,
                              TDiagnostics *diagnostics,
                              TIntermBlock *statementList,
                              const TSourceLoc &loc)
{
    ValidateSwitch validate(switchType, diagnostics);
    ASSERT(statementList);
    statementList->traverse(&validate);
    return validate.validateInternal(loc);
}

ValidateSwitch::ValidateSwitch(TBasicType switchType, TDiagnostics *diagnostics)
    : TIntermTraverser(true, false, true),
      mSwitchType(switchType),
      mDiagnostics(diagnostics),
      mCaseTypeMismatch(false),
      mFirstCaseFound(false),
      mStatementBeforeCase(false),
      mLastStatementWasCase(false),
      mControlFlowDepth(0),
      mCaseInsideControlFlow(false),
      mDefaultCount(0),
      mDuplicateCases(false)
{
}

void ValidateSwitch::visitSymbol(TIntermSymbol *)
{
    if (!mFirstCaseFound)
        mStatementBeforeCase = true;
    mLastStatementWasCase    = false;
}

void ValidateSwitch::visitConstantUnion(TIntermConstantUnion *)
{
    // Conditions of case labels are not traversed, so this is some other constant
    // Could be just a statement like "0;"
    if (!mFirstCaseFound)
        mStatementBeforeCase = true;
    mLastStatementWasCase    = false;
}

bool ValidateSwitch::visitBinary(Visit, TIntermBinary *)
{
    if (!mFirstCaseFound)
        mStatementBeforeCase = true;
    mLastStatementWasCase    = false;
    return true;
}

bool ValidateSwitch::visitUnary(Visit, TIntermUnary *)
{
    if (!mFirstCaseFound)
        mStatementBeforeCase = true;
    mLastStatementWasCase    = false;
    return true;
}

bool ValidateSwitch::visitTernary(Visit, TIntermTernary *)
{
    if (!mFirstCaseFound)
        mStatementBeforeCase = true;
    mLastStatementWasCase    = false;
    return true;
}

bool ValidateSwitch::visitIfElse(Visit visit, TIntermIfElse *)
{
    if (visit == PreVisit)
        ++mControlFlowDepth;
    if (visit == PostVisit)
        --mControlFlowDepth;
    if (!mFirstCaseFound)
        mStatementBeforeCase = true;
    mLastStatementWasCase    = false;
    return true;
}

bool ValidateSwitch::visitSwitch(Visit, TIntermSwitch *)
{
    if (!mFirstCaseFound)
        mStatementBeforeCase = true;
    mLastStatementWasCase    = false;
    // Don't go into nested switch statements
    return false;
}

bool ValidateSwitch::visitCase(Visit, TIntermCase *node)
{
    const char *nodeStr = node->hasCondition() ? "case" : "default";
    if (mControlFlowDepth > 0)
    {
        mDiagnostics->error(node->getLine(), "label statement nested inside control flow", nodeStr);
        mCaseInsideControlFlow = true;
    }
    mFirstCaseFound       = true;
    mLastStatementWasCase = true;
    if (!node->hasCondition())
    {
        ++mDefaultCount;
        if (mDefaultCount > 1)
        {
            mDiagnostics->error(node->getLine(), "duplicate default label", nodeStr);
        }
    }
    else
    {
        TIntermConstantUnion *condition = node->getCondition()->getAsConstantUnion();
        if (condition == nullptr)
        {
            // This can happen in error cases.
            return false;
        }
        TBasicType conditionType = condition->getBasicType();
        if (conditionType != mSwitchType)
        {
            mDiagnostics->error(condition->getLine(),
                                "case label type does not match switch init-expression type",
                                nodeStr);
            mCaseTypeMismatch = true;
        }

        if (conditionType == EbtInt)
        {
            int iConst = condition->getIConst(0);
            if (mCasesSigned.find(iConst) != mCasesSigned.end())
            {
                mDiagnostics->error(condition->getLine(), "duplicate case label", nodeStr);
                mDuplicateCases = true;
            }
            else
            {
                mCasesSigned.insert(iConst);
            }
        }
        else if (conditionType == EbtUInt)
        {
            unsigned int uConst = condition->getUConst(0);
            if (mCasesUnsigned.find(uConst) != mCasesUnsigned.end())
            {
                mDiagnostics->error(condition->getLine(), "duplicate case label", nodeStr);
                mDuplicateCases = true;
            }
            else
            {
                mCasesUnsigned.insert(uConst);
            }
        }
        // Other types are possible only in error cases, where the error has already been generated
        // when parsing the case statement.
    }
    // Don't traverse the condition of the case statement
    return false;
}

bool ValidateSwitch::visitAggregate(Visit visit, TIntermAggregate *)
{
    if (getParentNode() != nullptr)
    {
        // This is not the statementList node, but some other node.
        if (!mFirstCaseFound)
            mStatementBeforeCase = true;
        mLastStatementWasCase    = false;
    }
    return true;
}

bool ValidateSwitch::visitLoop(Visit visit, TIntermLoop *)
{
    if (visit == PreVisit)
        ++mControlFlowDepth;
    if (visit == PostVisit)
        --mControlFlowDepth;
    if (!mFirstCaseFound)
        mStatementBeforeCase = true;
    mLastStatementWasCase    = false;
    return true;
}

bool ValidateSwitch::visitBranch(Visit, TIntermBranch *)
{
    if (!mFirstCaseFound)
        mStatementBeforeCase = true;
    mLastStatementWasCase    = false;
    return true;
}

bool ValidateSwitch::validateInternal(const TSourceLoc &loc)
{
    if (mStatementBeforeCase)
    {
        mDiagnostics->error(loc, "statement before the first label", "switch");
    }
    if (mLastStatementWasCase)
    {
        mDiagnostics->error(
            loc, "no statement between the last label and the end of the switch statement",
            "switch");
    }
    return !mStatementBeforeCase && !mLastStatementWasCase && !mCaseInsideControlFlow &&
           !mCaseTypeMismatch && mDefaultCount <= 1 && !mDuplicateCases;
}

}  // anonymous namespace

bool ValidateSwitchStatementList(TBasicType switchType,
                                 TDiagnostics *diagnostics,
                                 TIntermBlock *statementList,
                                 const TSourceLoc &loc)
{
    return ValidateSwitch::validate(switchType, diagnostics, statementList, loc);
}

}  // namespace sh
