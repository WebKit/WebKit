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
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{

class UseUniformBlockMembers : public TIntermTraverser
{
  public:
    UseUniformBlockMembers(const InterfaceBlockList &blocks, const TSymbolTable &symbolTable)
        : TIntermTraverser(true, false, false),
          mBlocks(blocks),
          mSymbolTable(symbolTable),
          mCodeInserted(false)
    {
        ASSERT(mSymbolTable.atGlobalLevel());
    }

  protected:
    bool visitAggregate(Visit visit, TIntermAggregate *node) override { return !mCodeInserted; }
    bool visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *node) override;

  private:
    void insertUseCode(TIntermSequence *sequence);
    void AddFieldUseStatements(const ShaderVariable &var, TIntermSequence *sequence);

    const InterfaceBlockList &mBlocks;
    const TSymbolTable &mSymbolTable;
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
    if (var.isArray())
    {
        size_t pos = name.find_last_of('[');
        if (pos != TString::npos)
        {
            name = name.substr(0, pos);
        }
    }
    const TType *type;
    TType basicType;
    if (var.isStruct())
    {
        TVariable *structInfo = reinterpret_cast<TVariable *>(mSymbolTable.findGlobal(name));
        ASSERT(structInfo);
        const TType &structType = structInfo->getType();
        type                    = &structType;
    }
    else
    {
        basicType = sh::GetShaderVariableBasicType(var);
        type      = &basicType;
    }
    ASSERT(type);

    TIntermSymbol *symbol = new TIntermSymbol(0, name, *type);
    if (var.isArray())
    {
        for (unsigned int i = 0; i < var.arraySize; ++i)
        {
            TIntermBinary *element =
                new TIntermBinary(EOpIndexDirect, symbol, TIntermTyped::CreateIndexNode(i));
            sequence->insert(sequence->begin(), element);
        }
    }
    else
    {
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
            TString name      = TString(block.instanceName.c_str());
            TVariable *ubInfo = reinterpret_cast<TVariable *>(mSymbolTable.findGlobal(name));
            ASSERT(ubInfo);
            TIntermSymbol *arraySymbol = new TIntermSymbol(0, name, ubInfo->getType());
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
            TString name      = TString(block.instanceName.c_str());
            TVariable *ubInfo = reinterpret_cast<TVariable *>(mSymbolTable.findGlobal(name));
            ASSERT(ubInfo);
            TIntermSymbol *blockSymbol = new TIntermSymbol(0, name, ubInfo->getType());
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

void UseInterfaceBlockFields(TIntermNode *root,
                             const InterfaceBlockList &blocks,
                             const TSymbolTable &symbolTable)
{
    UseUniformBlockMembers useUniformBlock(blocks, symbolTable);
    root->traverse(&useUniformBlock);
}

}  // namespace sh
