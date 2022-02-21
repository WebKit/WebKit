// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Rewrite "-float" to "0.0 - float" to work around unary minus operator on float issue on Intel Mac
// OSX 10.11.

#ifndef COMPILER_TRANSLATOR_TREEOPS_APPLE_REWRITEUNARYMINUSOPERATORFLOAT_H_
#define COMPILER_TRANSLATOR_TREEOPS_APPLE_REWRITEUNARYMINUSOPERATORFLOAT_H_

#include "common/angleutils.h"

namespace sh
{
class TCompiler;
class TIntermNode;

#if defined(ANGLE_ENABLE_GLSL) && defined(ANGLE_ENABLE_APPLE_WORKAROUNDS)
ANGLE_NO_DISCARD bool RewriteUnaryMinusOperatorFloat(TCompiler *compiler, TIntermNode *root);
#else
ANGLE_NO_DISCARD ANGLE_INLINE bool RewriteUnaryMinusOperatorFloat(TCompiler *compiler,
                                                                  TIntermNode *root)
{
    UNREACHABLE();
    return false;
}
#endif

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_APPLE_REWRITEUNARYMINUSOPERATORFLOAT_H_
