//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FindSymbolNode.h:
//     Utility for finding a symbol node inside an AST tree.

#ifndef COMPILER_TRANSLATOR_FIND_SYMBOL_H_
#define COMPILER_TRANSLATOR_FIND_SYMBOL_H_

#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/Common.h"

namespace sh
{

class TIntermNode;
class TIntermSymbol;

const TIntermSymbol *FindSymbolNode(TIntermNode *root,
                                    const TString &symbolName,
                                    TBasicType basicType);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_FIND_SYMBOL_H_