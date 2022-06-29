//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_SEPARATECOMPOUNDSTRUCTDECLARATIONS_H_
#define COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_SEPARATECOMPOUNDSTRUCTDECLARATIONS_H_

#include "common/angleutils.h"
#include "compiler/translator/Compiler.h"
#include "compiler/translator/TranslatorMetalDirect/IdGen.h"
namespace sh
{
class TSymbolTable;

// Example:
//  struct Foo { int x; } foo;
// Becomes:
//  struct Foo {int x; }; Foo foo;
[[nodiscard]] bool SeparateCompoundStructDeclarations(TCompiler &compiler,
                                                      IdGen &idGen,
                                                      TIntermBlock &root,
                                                      TSymbolTable *symbolTable);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_SEPARATECOMPOUNDSTRUCTDECLARATIONS_H_
