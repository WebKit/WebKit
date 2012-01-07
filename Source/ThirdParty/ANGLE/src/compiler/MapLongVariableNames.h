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
#define MAX_SHORTENED_IDENTIFIER_SIZE 32

// Traverses intermediate tree to map attributes and uniforms names that are
// longer than MAX_SHORTENED_IDENTIFIER_SIZE to MAX_SHORTENED_IDENTIFIER_SIZE.
class MapLongVariableNames : public TIntermTraverser {
public:
    MapLongVariableNames(std::map<std::string, std::string>& varyingLongNameMap);

    virtual void visitSymbol(TIntermSymbol*);
    virtual bool visitLoop(Visit, TIntermLoop*);

private:
    TString mapVaryingLongName(const TString& name);

    std::map<std::string, std::string>& mVaryingLongNameMap;
};

#endif  // COMPILER_MAP_LONG_VARIABLE_NAMES_H_
