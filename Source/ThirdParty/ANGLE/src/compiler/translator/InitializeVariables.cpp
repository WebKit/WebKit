//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/InitializeVariables.h"

#include "angle_gl.h"
#include "common/debug.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/util.h"

namespace
{

class VariableInitializer : public TIntermTraverser
{
  public:
    VariableInitializer(const InitVariableList &vars)
        : TIntermTraverser(true, false, false), mVariables(vars), mCodeInserted(false)
    {
    }

  protected:
    bool visitBinary(Visit, TIntermBinary *node) override { return false; }
    bool visitUnary(Visit, TIntermUnary *node) override { return false; }
    bool visitIfElse(Visit, TIntermIfElse *node) override { return false; }
    bool visitLoop(Visit, TIntermLoop *node) override { return false; }
    bool visitBranch(Visit, TIntermBranch *node) override { return false; }
    bool visitAggregate(Visit, TIntermAggregate *node) override { return false; }

    bool visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node) override;

  private:
    void insertInitCode(TIntermSequence *sequence);

    const InitVariableList &mVariables;
    bool mCodeInserted;
};

// VariableInitializer implementation.

bool VariableInitializer::visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node)
{
    // Function definition.
    ASSERT(visit == PreVisit);
    if (node->getFunctionSymbolInfo()->isMain())
    {
        TIntermBlock *body = node->getBody();
        insertInitCode(body->getSequence());
        mCodeInserted = true;
    }
    return false;
}

void VariableInitializer::insertInitCode(TIntermSequence *sequence)
{
    for (const auto &var : mVariables)
    {
        TString name = TString(var.name.c_str());
        TType type   = sh::GetShaderVariableType(var);

        // Assign the array elements one by one to keep the AST compatible with ESSL 1.00 which
        // doesn't have array assignment.
        if (var.isArray())
        {
            size_t pos = name.find_last_of('[');
            if (pos != TString::npos)
            {
                name = name.substr(0, pos);
            }
            TType elementType = type;
            elementType.clearArrayness();

            for (unsigned int i = 0; i < var.arraySize; ++i)
            {
                TIntermSymbol *arraySymbol = new TIntermSymbol(0, name, type);
                TIntermBinary *element     = new TIntermBinary(EOpIndexDirect, arraySymbol,
                                                           TIntermTyped::CreateIndexNode(i));

                TIntermTyped *zero        = TIntermTyped::CreateZero(elementType);
                TIntermBinary *assignment = new TIntermBinary(EOpAssign, element, zero);

                sequence->insert(sequence->begin(), assignment);
            }
        }
        else
        {
            TIntermSymbol *symbol = new TIntermSymbol(0, name, type);
            TIntermTyped *zero    = TIntermTyped::CreateZero(type);

            TIntermBinary *assign = new TIntermBinary(EOpAssign, symbol, zero);
            sequence->insert(sequence->begin(), assign);
        }
    }
}

}  // namespace anonymous

void InitializeVariables(TIntermNode *root, const InitVariableList &vars)
{
    VariableInitializer initializer(vars);
    root->traverse(&initializer);
}
