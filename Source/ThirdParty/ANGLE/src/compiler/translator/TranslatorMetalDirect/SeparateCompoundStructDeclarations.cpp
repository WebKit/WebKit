//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <algorithm>

#include "compiler/translator/TranslatorMetalDirect/SeparateCompoundStructDeclarations.h"
#include "compiler/translator/tree_ops/SeparateDeclarations.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

using namespace sh;

////////////////////////////////////////////////////////////////////////////////

namespace
{

class Separator : public TIntermTraverser
{
  public:
    std::unordered_map<int, TIntermSymbol *> replacementMap;
    Separator(TSymbolTable &symbolTable) : TIntermTraverser(false, false, true, &symbolTable) {}

    bool visitDeclaration(Visit, TIntermDeclaration *declNode) override
    {
        ASSERT(declNode->getChildCount() == 1);
        TIntermNode &node = *declNode->getChildNode(0);

        if (TIntermSymbol *symbolNode = node.getAsSymbolNode())
        {
            const TVariable &var        = symbolNode->variable();
            const TType &type           = var.getType();
            const SymbolType symbolType = var.symbolType();
            if (type.isStructSpecifier() && symbolType != SymbolType::Empty)
            {
                const TStructure *structure = type.getStruct();
                auto *structVar             = new TVariable(mSymbolTable, ImmutableString(""),
                                                new TType(structure, true), SymbolType::Empty);

                auto *instanceType = new TType(structure, false);
                instanceType->setQualifier(type.getQualifier());
                auto *instanceVar = new TVariable(mSymbolTable, var.name(), instanceType,
                                                  symbolType, var.extension());

                TIntermSequence replacements;
                TIntermSymbol * instanceSymbol = new TIntermSymbol(instanceVar);
                replacements.push_back(new TIntermSymbol(structVar));
                replacements.push_back(instanceSymbol);
                replacementMap[symbolNode->uniqueId().get()] = instanceSymbol;
                mMultiReplacements.push_back(
                    NodeReplaceWithMultipleEntry(declNode, symbolNode, std::move(replacements)));
            }
        }

        return false;
    }
    
    void visitSymbol(TIntermSymbol *decl) override
    {
        auto symbol = replacementMap.find(decl->uniqueId().get());
        if(symbol != replacementMap.end())
        {
            queueReplacement(symbol->second->deepCopy(), OriginalNode::IS_DROPPED);
        }
    }
};

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////

bool sh::SeparateCompoundStructDeclarations(TCompiler &compiler, TIntermBlock &root)
{
    Separator separator(compiler.getSymbolTable());
    root.traverse(&separator);
    if (!separator.updateTree(&compiler, &root))
    {
        return false;
    }

    if (!SeparateDeclarations(&compiler, &root))
    {
        return false;
    }

    return true;
}
