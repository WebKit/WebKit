//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Rewrite condition in for and while loops to work around driver bug on Intel Mac.

#ifndef COMPILER_TRANSLATOR_TREEOPS_APPLE_ADDANDTRUETOLOOPCONDITION_H_
#define COMPILER_TRANSLATOR_TREEOPS_APPLE_ADDANDTRUETOLOOPCONDITION_H_

#include "common/angleutils.h"

namespace sh
{
class TCompiler;
class TIntermNode;

#if defined(ANGLE_ENABLE_GLSL) && defined(ANGLE_ENABLE_APPLE_WORKAROUNDS)
[[nodiscard]] bool AddAndTrueToLoopCondition(TCompiler *compiler, TIntermNode *root);
#else
[[nodiscard]] ANGLE_INLINE bool AddAndTrueToLoopCondition(TCompiler *compiler, TIntermNode *root)
{
    UNREACHABLE();
    return false;
}
#endif

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_APPLE_ADDANDTRUETOLOOPCONDITION_H_
