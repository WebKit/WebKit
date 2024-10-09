//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/tree_ops/SeparateCompoundStructDeclarations.h"

#include "compiler/translator/tree_ops/SeparateDeclarations.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"
#include "compiler/translator/util.h"

#include <algorithm>

namespace sh
{
namespace
{
class Separator : public TIntermTraverser
{
  public:
    Separator(TSymbolTable &symbolTable, StructNameGenerator nameGenerator)
        : TIntermTraverser(false, false, true, &symbolTable), mNameGenerator(nameGenerator)
    {}

    bool visitDeclaration(Visit, TIntermDeclaration *declNode) override
    {
        ASSERT(declNode->getChildCount() == 1);
        Declaration declaration = ViewDeclaration(*declNode);

        const TVariable &var        = declaration.symbol.variable();
        const TType &type           = var.getType();
        const SymbolType symbolType = var.symbolType();
        if (type.isStructSpecifier() && symbolType != SymbolType::Empty)
        {
            const TStructure *structure = type.getStruct();
            if (structure->symbolType() == SymbolType::Empty)
            {
                // Name unnamed inline structs.
                structure = new TStructure(mSymbolTable, mNameGenerator(), &structure->fields(),
                                           SymbolType::AngleInternal);
            }
            TVariable *structVar = new TVariable(mSymbolTable, kEmptyImmutableString,
                                                 new TType(structure, true), SymbolType::Empty);
            TType *instanceType  = new TType(structure, false);
            if (type.isArray())
            {
                instanceType->makeArrays(type.getArraySizes());
            }
            instanceType->setQualifier(type.getQualifier());
            auto *instanceVar =
                new TVariable(mSymbolTable, var.name(), instanceType, symbolType, var.extensions());

            TIntermSequence replacements;
            replacements.push_back(new TIntermDeclaration({structVar}));

            TIntermSymbol *instanceSymbol    = new TIntermSymbol(instanceVar);
            TIntermDeclaration *instanceDecl = new TIntermDeclaration;
            if (declaration.initExpr)
            {
                instanceDecl->appendDeclarator(new TIntermBinary(
                    TOperator::EOpInitialize, instanceSymbol, declaration.initExpr));
            }
            else
            {
                instanceDecl->appendDeclarator(instanceSymbol);
            }
            replacements.push_back(instanceDecl);

            mVariableMap[&declaration.symbol.variable()] = instanceSymbol;
            ASSERT(getParentNode() != nullptr);
            ASSERT(getParentNode()->getAsBlock() != nullptr);
            mMultiReplacements.push_back(NodeReplaceWithMultipleEntry(
                getParentNode()->getAsBlock(), declNode, std::move(replacements)));
        }

        return false;
    }

    void visitSymbol(TIntermSymbol *symbol) override
    {
        const TVariable *variable = &symbol->variable();
        if (mVariableMap.count(variable) > 0)
        {
            queueAccessChainReplacement(mVariableMap[variable]->deepCopy());
        }
    }

    StructNameGenerator mNameGenerator;
    VariableReplacementMap mVariableMap;
};
}  // anonymous namespace

bool SeparateCompoundStructDeclarations(TCompiler &compiler,
                                        StructNameGenerator nameGenerator,
                                        TIntermBlock &root)
{
    Separator separator(compiler.getSymbolTable(), nameGenerator);
    root.traverse(&separator);
    if (!separator.updateTree(&compiler, &root))
    {
        return false;
    }

    if (!SeparateDeclarations(compiler, root))
    {
        return false;
    }

    return true;
}
}  // namespace sh
