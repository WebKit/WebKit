//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_REWRITE_PIXELLOCALSTORAGE_H_
#define COMPILER_TRANSLATOR_TREEOPS_REWRITE_PIXELLOCALSTORAGE_H_

#include <GLSLANG/ShaderVars.h>

namespace sh
{

class TCompiler;
class TIntermBlock;
class TSymbolTable;

// This mutating tree traversal rewrites high level ANGLE_shader_pixel_local_storage operations to
// shader image operations.
[[nodiscard]] bool RewritePixelLocalStorageToImages(TCompiler *compiler,
                                                    TIntermBlock *root,
                                                    TSymbolTable &symbolTable,
                                                    ShCompileOptions compileOptions,
                                                    int shaderVersion);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_REWRITE_PIXELLOCALSTORAGE_H_
