//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GatherDefaultUniforms.h: gathers all default uniforms and puts them in an interface block,
// rewriting accesses of the default uniforms appropriately.

#ifndef COMPILER_TRANSLATOR_TREEOPS_GATHERDEFAULTUNIFORMS_H_
#define COMPILER_TRANSLATOR_TREEOPS_GATHERDEFAULTUNIFORMS_H_

#include "common/PackedGLEnums_autogen.h"
#include "compiler/translator/ImmutableString.h"

namespace sh
{
class TCompiler;
class TIntermBlock;
class TSymbolTable;
class TVariable;
class TType;

[[nodiscard]] bool IsDefaultUniform(const TType &type);

// Gathers all default uniforms into an interface block named `uniformBlockType`, with optional
// `uniformBlockVarName`. Rewrites accesses to the default uniform variables appropriately. Inserts
// the resulting interface block into the `root` tree and stores a pointer to the interface block in
// `outUniformBlock`.
[[nodiscard]] bool GatherDefaultUniforms(TCompiler *compiler,
                                         TIntermBlock *root,
                                         TSymbolTable *symbolTable,
                                         gl::ShaderType shaderType,
                                         const ImmutableString &uniformBlockType,
                                         const ImmutableString &uniformBlockVarName,
                                         const TVariable **outUniformBlock);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_GATHERDEFAULTUNIFORMS_H_
