//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <algorithm>
#include <unordered_map>

#include "compiler/translator/TranslatorMetalDirect/AstHelpers.h"
#include "compiler/translator/TranslatorMetalDirect/TransposeRowMajorMatrices.h"
#include "compiler/translator/tree_util/IntermRebuild.h"

using namespace sh;

////////////////////////////////////////////////////////////////////////////////

namespace
{

class Rewriter : public TIntermRebuild
{
  public:
    Rewriter(TCompiler &compiler) : TIntermRebuild(compiler, true, false) {}
    // PreResult visitConstantUnionPre(TIntermSymbol &symbolNode) override
    // {
    //     const TVariable &var = symbolNode.variable();
    //     const TType &type = var.getType();
    //     //const TLayoutQualifier &layoutQualifier = type.getLayoutQualifier();
    //     if(type.isMatrix())
    //     {
    //         return CreateBuiltInUnaryFunctionCallNode("transpose", symbolNode.deepCopy(),
    //                                                   mSymbolTable, 300));
    //     }
    //     return symbolNode;
    // }
};

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////

bool sh::TransposeRowMajorMatricies(TCompiler &compiler, TIntermBlock &root)
{
    Rewriter rewriter(compiler);
    if (!rewriter.rebuildRoot(root))
    {
        return false;
    }
    return true;
}
