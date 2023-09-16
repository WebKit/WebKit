//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EmulateFramebufferFetch.h: Replace input, gl_LastFragData and gl_LastFragColorARM with usages of
// input attachments.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_SPIRV_EMULATEFRAMEBUFFERFETCH_H_
#define COMPILER_TRANSLATOR_TREEOPS_SPIRV_EMULATEFRAMEBUFFERFETCH_H_

#include "common/angleutils.h"

namespace sh
{

class TCompiler;
class TIntermBlock;
struct ShaderVariable;

// Emulate framebuffer fetch through the use of input attachments.
[[nodiscard]] bool EmulateFramebufferFetch(TCompiler *compiler,
                                           TIntermBlock *root,
                                           std::vector<ShaderVariable> *uniforms);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_SPIRV_REPLACEFORSHADERFRAMEBUFFERFETCH_H_
