//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UnfoldShortCircuitAST is an AST traverser to replace short-circuiting
// operations with ternary operations.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_UNFOLDSHORTCIRCUITAST_H_
#define COMPILER_TRANSLATOR_TREEOPS_UNFOLDSHORTCIRCUITAST_H_

namespace sh
{

class TIntermBlock;

void UnfoldShortCircuitAST(TIntermBlock *root);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_UNFOLDSHORTCIRCUITAST_H_
