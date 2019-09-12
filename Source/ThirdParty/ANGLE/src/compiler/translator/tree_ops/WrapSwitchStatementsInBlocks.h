//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WrapSwitchStatementsInBlocks.h: Wrap switch statements in blocks and declare all switch-scoped
// variables there to make the AST compatible with HLSL output.

#ifndef COMPILER_TRANSLATOR_TREEOPS_WRAPSWITCHSTATEMENTSINBLOCKS_H_
#define COMPILER_TRANSLATOR_TREEOPS_WRAPSWITCHSTATEMENTSINBLOCKS_H_

namespace sh
{

class TIntermBlock;

// Wrap switch statements in the AST into blocks when needed. Returns true if the AST was changed.
void WrapSwitchStatementsInBlocks(TIntermBlock *root);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_WRAPSWITCHSTATEMENTSINBLOCKS_H_
