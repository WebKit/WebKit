//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_INITIALIZEVARIABLES_H_
#define COMPILER_TRANSLATOR_INITIALIZEVARIABLES_H_

#include <GLSLANG/ShaderLang.h>

namespace sh
{
class TIntermNode;
class TSymbolTable;

typedef std::vector<sh::ShaderVariable> InitVariableList;

// Currently this function is only capable of initializing variables of basic types,
// array of basic types, or struct of basic types.
// For now it is used for the following two scenarios:
//   1. initializing gl_Position;
//   2. initializing ESSL 3.00 shaders' output variables (which might be structs).
// Specifically, it's not feasible to make it work for local variables because if their
// types are structs, we can't look into TSymbolTable to find the TType data.
void InitializeVariables(TIntermNode *root,
                         const InitVariableList &vars,
                         const TSymbolTable &symbolTable);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_INITIALIZEVARIABLES_H_
