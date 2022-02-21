//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CheckEarlyFragmentTestsOptimization is an AST traverser to check if early fragment
// tests as an optimization is feasible.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_VULKAN_EARLYFRAGMENTTESTSOPTIMIZATION_H_
#define COMPILER_TRANSLATOR_TREEOPS_VULKAN_EARLYFRAGMENTTESTSOPTIMIZATION_H_

#include "common/angleutils.h"

namespace sh
{
class TCompiler;
class TIntermNode;

#ifdef ANGLE_ENABLE_VULKAN
ANGLE_NO_DISCARD bool CheckEarlyFragmentTestsFeasible(TCompiler *compiler, TIntermNode *root);
#else
ANGLE_NO_DISCARD ANGLE_INLINE bool CheckEarlyFragmentTestsFeasible(TCompiler *compiler,
                                                                   TIntermNode *root)
{
    UNREACHABLE();
    return false;
}
#endif
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_VULKAN_EARLYFRAGMENTTESTSOPTIMIZATION_H_
