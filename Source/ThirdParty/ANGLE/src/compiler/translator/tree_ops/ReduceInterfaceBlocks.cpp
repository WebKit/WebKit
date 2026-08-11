//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <algorithm>
#include <unordered_map>

#include <iostream>

#include "compiler/translator/IntermRebuild.h"
#include "compiler/translator/Name.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_ops/ReduceInterfaceBlocks.h"
#include "compiler/translator/tree_ops/SeparateDeclarations.h"
#include "compiler/translator/tree_util/IntermNode_util.h"

using namespace sh;

////////////////////////////////////////////////////////////////////////////////

namespace
{

class Reducer : public TIntermRebuild
{
    std::unordered_map<const TInterfaceBlock *, const TVariable *> mLiftedMap;
    std::unordered_map<const TVariable *, const TVariable *> mInstanceMap;
    InterfaceBlockInstanceVarNameGen &mInstanceVarNameGen;

  public:
    Reducer(TCompiler &compiler, InterfaceBlockInstanceVarNameGen &instanceVarNameGen)
        : TIntermRebuild(compiler, true, false), mInstanceVarNameGen(instanceVarNameGen)
    {}

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
                const bool isEmptySymbol = symbolType == SymbolType::Empty;
                Name newInstanceVarName =
                    isEmptySymbol ? Name(mInstanceVarNameGen(), SymbolType::AngleInternal)
                                  : Name(var);

                auto &structure =
                    *new TStructure(&mSymbolTable, interfaceBlock->name(),
                                    &interfaceBlock->fields(), interfaceBlock->symbolType());
                auto &structVar = CreateStructTypeVariable(mSymbolTable, structure);
                auto &instanceVar =
                    CreateInstanceVariable(mSymbolTable, structure, newInstanceVarName,
                                           TQualifier::EvqBuffer, &type.getArraySizes());

                if (isEmptySymbol)
                {
                    mLiftedMap[interfaceBlock] = &instanceVar;
                }
                else
                {
                    ASSERT(type.getQualifier() == TQualifier::EvqUniform);
                    mInstanceMap[&var] = &instanceVar;
                }

                TIntermNode *replacements[] = {
                    new TIntermDeclaration{new TIntermSymbol(&structVar)},
                    new TIntermDeclaration{new TIntermSymbol(&instanceVar)}};
                return PreResult::Multi(std::begin(replacements), std::end(replacements));
            }
        }

        return {declNode, VisitBits::Both};
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
                return AccessField(*(it->second), Name(var));
            }
        }
        return symbolNode;
    }
};

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////

bool sh::ReduceInterfaceBlocks(TCompiler &compiler,
                               TIntermBlock &root,
                               InterfaceBlockInstanceVarNameGen nameGen)
{
    Reducer reducer(compiler, nameGen);
    if (!reducer.rebuildRoot(root))
    {
        return false;
    }

    if (!SeparateDeclarations(compiler, root, false))
    {
        return false;
    }

    return true;
}
