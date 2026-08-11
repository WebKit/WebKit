//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The SizeClipCullDistance function redeclares gl_ClipDistance and gl_CullDistance when implicitly
// sized.
//

#ifndef COMPILER_TRANSLATOR_SIZECLIPCULLDISTANCE_H_
#define COMPILER_TRANSLATOR_SIZECLIPCULLDISTANCE_H_

#include "GLSLANG/ShaderVars.h"
#include "compiler/translator/Compiler.h"

namespace sh
{

class TIntermBlock;

bool SizeClipCullDistance(TCompiler *compiler,
                          TIntermBlock *root,
                          const ImmutableString &name,
                          uint8_t size);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_SIZECLIPCULLDISTANCE_H_
