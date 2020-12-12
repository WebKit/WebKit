//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_REWRITEGLOBALQUALIFIERDECLS_H_
#define COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_REWRITEGLOBALQUALIFIERDECLS_H_

#include <unordered_set>

#include "compiler/translator/Compiler.h"

namespace sh
{

// Tracks TVariables and TFields that are marked as "invariant".
class Invariants
{
  public:
    void insert(const TVariable &var) { mInvariants.insert(&var); }

    void insert(const TField &field) { mInvariants.insert(&field); }

    bool contains(const TVariable &var) const
    {
        return mInvariants.find(&var) != mInvariants.end();
    }

    bool contains(const TField &field) const
    {
        return mInvariants.find(&field) != mInvariants.end();
    }

  private:
    std::unordered_set<const void *> mInvariants;
};

// This rewrites TIntermGlobalQualifierDeclarations as TIntermDeclarations.
// `outInvariants` is populated with the information that would otherwise be lost by such a
// transform.
ANGLE_NO_DISCARD bool RewriteGlobalQualifierDecls(TCompiler &compiler,
                                                  TIntermBlock &root,
                                                  Invariants &outInvariants);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_REWRITEGLOBALQUALIFIERDECLS_H_
