//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_REGENERATESTRUCTNAMES_H_
#define COMPILER_TRANSLATOR_REGENERATESTRUCTNAMES_H_

#include "compiler/translator/IntermTraverse.h"
#include "compiler/translator/SymbolTable.h"

#include <set>

namespace sh
{

class RegenerateStructNames : public TIntermTraverser
{
  public:
    RegenerateStructNames(TSymbolTable *symbolTable, int shaderVersion)
        : TIntermTraverser(true, false, false, symbolTable),
          mShaderVersion(shaderVersion),
          mScopeDepth(0)
    {
    }

  protected:
    void visitSymbol(TIntermSymbol *) override;
    bool visitBlock(Visit, TIntermBlock *block) override;

  private:
    int mShaderVersion;

    // Indicating the depth of the current scope.
    // The global scope is 1.
    int mScopeDepth;

    // If a struct's declared globally, push its ID in this set.
    std::set<int> mDeclaredGlobalStructs;
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_REGENERATESTRUCTNAMES_H_
