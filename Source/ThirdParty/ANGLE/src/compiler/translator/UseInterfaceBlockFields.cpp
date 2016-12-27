//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// UseInterfaceBlockFields.cpp: insert statements to reference all members in InterfaceBlock list at
// the beginning of main. This is to work around a Mac driver that treats unused standard/shared
// uniform blocks as inactive.

#include "compiler/translator/UseInterfaceBlockFields.h"

#include "compiler/translator/IntermNode.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{

class UseUniformBlockMembers : public TIntermTraverser
{
  public:
    UseUniformBlockMembers(const InterfaceBlockList &blocks)
        : TIntermTraverser(true, false, false), mBlocks(blocks), mCodeInserted(false)
    {
    }

  protected:
    bool visitAggregate(Visit visit, TIntermAggregate *node) override { return !mCodeInserted; }
    bool visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node) override;

  private:
    void insertUseCode(TIntermSequence *sequence);
    void AddFieldUseStatements(const ShaderVariable &var, TIntermSequence *sequence);

    const InterfaceBlockList &mBlocks;
    bool mCodeInserted;
};

bool UseUniformBlockMembers::visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node)
{
    ASSERT(visit == PreVisit);
    if (node->getFunctionSymbolInfo()->isMain())
    {
        TIntermBlock *body = node->getBody();
        ASSERT(body);
        insertUseCode(body->getSequence());
        mCodeInserted = true;
        return false;
    }
    return !mCodeInserted;
}

void UseUniformBlockMembers::AddFieldUseStatements(const ShaderVariable &var,
                                                   TIntermSequence *sequence)
{
    TString name = TString(var.name.c_str());
    TType type   = GetShaderVariableType(var);

    if (var.isArray())
    {
        size_t pos = name.find_last_of('[');
        if (pos != TString::npos)
        {
            name = name.substr(0, pos);
        }
        TType elementType = type;
        elementType.clearArrayness();

        TIntermSymbol *arraySymbol = new TIntermSymbol(0, name, type);
        for (unsigned int i = 0; i < var.arraySize; ++i)
        {
            TIntermBinary *element =
                new TIntermBinary(EOpIndexDirect, arraySymbol, TIntermTyped::CreateIndexNode(i));

            sequence->insert(sequence->begin(), element);
        }
    }
    else if (var.isStruct())
    {
        TIntermSymbol *structSymbol = new TIntermSymbol(0, name, type);
        for (unsigned int i = 0; i < var.fields.size(); ++i)
        {
            TIntermBinary *element = new TIntermBinary(EOpIndexDirectStruct, structSymbol,
                                                       TIntermTyped::CreateIndexNode(i));

            sequence->insert(sequence->begin(), element);
        }
    }
    else
    {
        TIntermSymbol *symbol = new TIntermSymbol(0, name, type);

        sequence->insert(sequence->begin(), symbol);
    }
}

void UseUniformBlockMembers::insertUseCode(TIntermSequence *sequence)
{
    for (const auto &block : mBlocks)
    {
        if (block.instanceName.empty())
        {
            for (const auto &var : block.fields)
            {
                AddFieldUseStatements(var, sequence);
            }
        }
        else if (block.arraySize > 0)
        {
            TType type                 = GetInterfaceBlockType(block);
            TString name               = TString(block.instanceName.c_str());
            TIntermSymbol *arraySymbol = new TIntermSymbol(0, name, type);
            for (unsigned int i = 0; i < block.arraySize; ++i)
            {
                TIntermBinary *instanceSymbol = new TIntermBinary(EOpIndexDirect, arraySymbol,
                                                                  TIntermTyped::CreateIndexNode(i));
                for (unsigned int j = 0; j < block.fields.size(); ++j)
                {
                    TIntermBinary *element =
                        new TIntermBinary(EOpIndexDirectInterfaceBlock, instanceSymbol,
                                          TIntermTyped::CreateIndexNode(j));
                    sequence->insert(sequence->begin(), element);
                }
            }
        }
        else
        {
            TType type                 = GetInterfaceBlockType(block);
            TString name               = TString(block.instanceName.c_str());
            TIntermSymbol *blockSymbol = new TIntermSymbol(0, name, type);
            for (unsigned int i = 0; i < block.fields.size(); ++i)
            {
                TIntermBinary *element = new TIntermBinary(
                    EOpIndexDirectInterfaceBlock, blockSymbol, TIntermTyped::CreateIndexNode(i));

                sequence->insert(sequence->begin(), element);
            }
        }
    }
}

}  // namespace anonymous

void UseInterfaceBlockFields(TIntermNode *root, const InterfaceBlockList &blocks)
{
    UseUniformBlockMembers useUniformBlock(blocks);
    root->traverse(&useUniformBlock);
}

}  // namespace sh
