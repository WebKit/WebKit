//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "SizeClipCullDistance.h"

#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/IntermTraverse.h"
#include "compiler/translator/tree_util/ReplaceVariable.h"
#include "compiler/translator/util.h"

namespace sh
{
bool SizeClipCullDistance(TCompiler *compiler,
                          TIntermBlock *root,
                          const ImmutableString &name,
                          uint8_t size)
{
    const TVariable *var = static_cast<const TVariable *>(
        compiler->getSymbolTable().findBuiltIn(name, compiler->getShaderVersion()));
    ASSERT(var != nullptr);

    if (size != var->getType().getOutermostArraySize())
    {
        TType *resizedType = new TType(var->getType());
        resizedType->setArraySize(0, size);
        TVariable *resizedVar =
            new TVariable(&compiler->getSymbolTable(), name, resizedType, SymbolType::BuiltIn);
        if (!ReplaceVariable(compiler, root, var, resizedVar))
        {
            return false;
        }
        var = resizedVar;
    }

    TIntermDeclaration *globalDecl = new TIntermDeclaration();
    globalDecl->appendDeclarator(new TIntermSymbol(var));
    root->insertStatement(0, globalDecl);

    return compiler->validateAST(root);
}
}  // namespace sh
