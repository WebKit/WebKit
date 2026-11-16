//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EmulateMutableFunctionParams: if any function params are written to, replace
// them with a temp variable initialized at the start of the function with the value of the params,
// because params are immutable in WGSL, for now.
//

#include "compiler/translator/tree_ops/wgsl/EmulateMutableFunctionParams.h"

#include "common/log_utils.h"
#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/Compiler.h"
#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/Operator_autogen.h"
#include "compiler/translator/SymbolUniqueId.h"
#include "compiler/translator/tree_util/DriverUniform.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/Visit.h"
#include "compiler/translator/util.h"

namespace sh
{
namespace
{

bool IsParamImmutableInWgsl(const TVariable *var)
{
    TQualifier q = var->getType().getQualifier();
    // Outparams (EvqOut, EvqInOut) are translated as pointers and don't need any extra mutability.
    // EvqParamConst is obviously immutable.
    return q == EvqParamIn;
}

class EmulateMutableFunctionParamsTraverser : public TLValueTrackingTraverser
{
  public:
    EmulateMutableFunctionParamsTraverser(TSymbolTable *symbolTable)
        : TLValueTrackingTraverser(true, false, false, symbolTable)
    {}

    bool visitFunctionDefinition(Visit visit, TIntermFunctionDefinition *func) override
    {
        mCurrentFunc = func;
        return true;
    }

    void visitSymbol(TIntermSymbol *node) override
    {
        const TVariable *paramVar = &node->variable();
        // Only looking at params used within functions.
        if (mInGlobalScope || !IsParamImmutableInWgsl(paramVar))
        {
            return;
        }

        // Keeps track of each param (TFunctionDefinition*, TVariable*) that is possibly written to.
        if (isLValueRequiredHere())
        {
            mParamsWrittenTo[paramVar->uniqueId()] = {mCurrentFunc, paramVar};
        }

        // Keep track of all param usage in the function, so later in traversal the param is written
        // to and therefore will be replaced with a temporary, the traverser can replace this usage
        // with a usage of the temporary.
        mParamUsages[paramVar->uniqueId()].push_back({node, getParentNode()});
    }

    bool update(TCompiler *compiler, TIntermBlock *root)
    {
        for (auto &[paramId, paramInfo] : mParamsWrittenTo)
        {
            // Declare the temporary and initialize it with the parameter.
            TIntermDeclaration *tempVarDecl = nullptr;
            TVariable *tempVar              = DeclareTempVariable(
                mSymbolTable, new TIntermSymbol(paramInfo.paramVar), EvqTemporary, &tempVarDecl);

            // Put the declaration at the top of the function body.
            insertStatementsInBlockAtPosition(paramInfo.funcDef->getBody(), 0, {tempVarDecl}, {});

            // Replace all the references to the parameter with references to the temp var.
            for (const ParamUsageInfo &paramUse : mParamUsages[paramId])
            {
                queueReplacementWithParent(paramUse.paramUsageParent, paramUse.paramUsage,
                                           new TIntermSymbol(tempVar), OriginalNode::IS_DROPPED);
            }
        }

        // Apply updates and validate
        return updateTree(compiler, root);
    }

  private:
    struct ParamInfo
    {
        TIntermFunctionDefinition *funcDef;
        const TVariable *paramVar;
    };
    struct ParamUsageInfo
    {
        TIntermSymbol *paramUsage;
        TIntermNode *paramUsageParent;
    };

    TMap<TSymbolUniqueId, ParamInfo> mParamsWrittenTo;
    TMap<TSymbolUniqueId, TVector<ParamUsageInfo>> mParamUsages;

    TIntermFunctionDefinition *mCurrentFunc;
};

}  // anonymous namespace

bool EmulateMutableFunctionParams(TCompiler *compiler, TIntermBlock *root)
{
    EmulateMutableFunctionParamsTraverser traverser(&compiler->getSymbolTable());
    root->traverse(&traverser);
    return traverser.update(compiler, root);
}
}  // namespace sh
