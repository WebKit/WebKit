//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <algorithm>
#include <unordered_map>

#include "compiler/translator/TranslatorMetalDirect/AstHelpers.h"
#include "compiler/translator/TranslatorMetalDirect/ReduceInterfaceBlocks.h"
#include "compiler/translator/tree_ops/SeparateDeclarations.h"
#include "compiler/translator/tree_util/IntermRebuild.h"

using namespace sh;

////////////////////////////////////////////////////////////////////////////////

namespace
{

class Reducer : public TIntermRebuild
{
    std::unordered_map<const TInterfaceBlock *, std::map<ImmutableString, const TVariable *>>
        mLiftedMap;
    std::unordered_map<const TVariable *, const TVariable *> mInstanceMap;

  public:
    Reducer(TCompiler &compiler) : TIntermRebuild(compiler, true, false) {}

    PreResult visitDeclarationPre(TIntermDeclaration &declNode) override
    {
        ASSERT(declNode.getChildCount() == 1);
        TIntermNode &node = *declNode.getChildNode(0);

        if (TIntermSymbol *symbolNode = node.getAsSymbolNode())
        {
            const TVariable &var        = symbolNode->variable();
            const TType &type           = var.getType();
            const SymbolType symbolType = var.symbolType();
            if (const TInterfaceBlock *interfaceBlock = type.getInterfaceBlock())
            {
                if (symbolType == SymbolType::Empty)
                {
                    auto &nameToVar = mLiftedMap[interfaceBlock];
                    std::vector<TIntermNode *> replacements;
                    for (TField *field : interfaceBlock->fields())
                    {
                        auto &liftedType = CloneType(*field->type());
                        ASSERT(liftedType.getQualifier() == TQualifier::EvqUniform ||
                               liftedType.getQualifier() == TQualifier::EvqGlobal);
                        liftedType.setQualifier(TQualifier::EvqUniform);
                        auto *liftedVar = new TVariable(&mSymbolTable, field->name(), &liftedType,
                                                        field->symbolType());

                        nameToVar[field->name()] = liftedVar;

                        replacements.push_back(
                            new TIntermDeclaration{new TIntermSymbol(liftedVar)});
                    }
                    return PreResult::Multi(std::move(replacements));
                }
                else
                {
                    ASSERT(type.getQualifier() == TQualifier::EvqUniform);

                    auto &structure =
                        *new TStructure(&mSymbolTable, interfaceBlock->name(),
                                        &interfaceBlock->fields(), interfaceBlock->symbolType());
                    auto &structVar = CreateStructTypeVariable(mSymbolTable, structure);
                    auto &instanceVar =
                        CreateInstanceVariable(mSymbolTable, structure, Name(var),
                                               TQualifier::EvqUniform, &type.getArraySizes());

                    mInstanceMap[&var] = &instanceVar;

                    TIntermNode *replacements[] = {
                        new TIntermDeclaration{new TIntermSymbol(&structVar)},
                        new TIntermDeclaration{new TIntermSymbol(&instanceVar)}};
                    return PreResult::Multi(std::begin(replacements), std::end(replacements));
                }
            }
        }

        return {declNode, VisitBits::Neither};
    }

    PreResult visitSymbolPre(TIntermSymbol &symbolNode) override
    {
        const TVariable &var = symbolNode.variable();
        {
            auto it = mInstanceMap.find(&var);
            if (it != mInstanceMap.end())
            {
                return *new TIntermSymbol(it->second);
            }
        }
        if (const TInterfaceBlock *ib = var.getType().getInterfaceBlock())
        {
            auto it = mLiftedMap.find(ib);
            if (it != mLiftedMap.end())
            {
                auto *liftedVar = it->second[var.name()];
                ASSERT(liftedVar);
                return *new TIntermSymbol(liftedVar);
            }
        }
        return symbolNode;
    }
};

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////

bool sh::ReduceInterfaceBlocks(TCompiler &compiler, TIntermBlock &root)
{
    Reducer reducer(compiler);
    if (!reducer.rebuildRoot(root))
    {
        return false;
    }

    if (!SeparateDeclarations(&compiler, &root))
    {
        return false;
    }

    return true;
}
