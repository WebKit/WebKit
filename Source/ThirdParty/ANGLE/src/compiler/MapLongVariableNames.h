//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_MAP_LONG_VARIABLE_NAMES_H_
#define COMPILER_MAP_LONG_VARIABLE_NAMES_H_

#include "GLSLANG/ShaderLang.h"

#include "compiler/intermediate.h"
#include "compiler/VariableInfo.h"

// This size does not include '\0' in the end.
#define MAX_IDENTIFIER_NAME_SIZE 32

// Traverses intermediate tree to map attributes and uniforms names that are
// longer than MAX_IDENTIFIER_NAME_SIZE to MAX_IDENTIFIER_NAME_SIZE.
class MapLongVariableNames : public TIntermTraverser {
public:
    MapLongVariableNames(TMap<TString, TString>& varyingLongNameMap);

    virtual void visitSymbol(TIntermSymbol*);
    virtual void visitConstantUnion(TIntermConstantUnion*);
    virtual bool visitBinary(Visit, TIntermBinary*);
    virtual bool visitUnary(Visit, TIntermUnary*);
    virtual bool visitSelection(Visit, TIntermSelection*);
    virtual bool visitAggregate(Visit, TIntermAggregate*);
    virtual bool visitLoop(Visit, TIntermLoop*);
    virtual bool visitBranch(Visit, TIntermBranch*);

private:
    TString mapVaryingLongName(const TString& name);

    TMap<TString, TString>& mVaryingLongNameMap;
};

#endif  // COMPILER_MAP_LONG_VARIABLE_NAMES_H_
