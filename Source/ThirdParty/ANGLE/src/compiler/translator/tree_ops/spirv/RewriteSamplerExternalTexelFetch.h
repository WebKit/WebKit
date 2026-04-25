//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RewriteSamplerExternalTexelFetch: Rewrite texelFetch() for external samplers to texture() so that
// yuv decoding happens according to the sampler.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_SPIRV_REWRITESAMPLEREXTERNALTEXELFETCH_H_
#define COMPILER_TRANSLATOR_TREEOPS_SPIRV_REWRITESAMPLEREXTERNALTEXELFETCH_H_

#include "common/angleutils.h"
#include "compiler/translator/Compiler.h"

namespace sh
{
class TCompiler;
class TIntermBlock;
class TSymbolTable;

[[nodiscard]] bool RewriteSamplerExternalTexelFetch(TCompiler *compiler,
                                                    TIntermBlock *root,
                                                    TSymbolTable *symbolTable);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_SPIRV_REWRITESAMPLEREXTERNALTEXELFETCH_H_
