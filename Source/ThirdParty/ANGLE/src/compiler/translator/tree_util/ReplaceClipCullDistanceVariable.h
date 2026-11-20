//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ReplaceClipCullDistanceVariable.h: Find any references to gl_ClipDistance or gl_CullDistance and
// replace it with ANGLEClipDistance or ANGLECullDistance.
//

#ifndef COMPILER_TRANSLATOR_TREEUTIL_REPLACECLIPCULLDISTANCEVARIABLE_H_
#define COMPILER_TRANSLATOR_TREEUTIL_REPLACECLIPCULLDISTANCEVARIABLE_H_

#include "GLSLANG/ShaderLang.h"
#include "common/angleutils.h"

namespace sh
{

struct InterfaceBlock;
class TCompiler;
class TIntermBlock;
class TSymbolTable;
class TIntermTyped;

// Replace every gl_ClipDistance assignment with assignment to "ANGLEClipDistance",
// then at the end of shader re-assign the values of this global variable to gl_ClipDistance.
// This to solve some complex usages such as user passing gl_ClipDistance as output reference
// to a function.
// Furthermore, at the end shader, some disabled gl_ClipDistance[i] can be skipped from the
// assignment.
//
// isLastPreRasterizationStage is an optional boolean expression that is true if the shader stage
// is the last pre-rasterization stage of the pipeline. If provided, the disabled gl_ClipDistance
// values are zeroed only in that case. Otherwise, the values are passed through to the next stage
// as written by the shader. Applies only for vertex and tessellation evaluation shaders.
[[nodiscard]] bool ReplaceClipDistanceAssignments(TCompiler *compiler,
                                                  TIntermBlock *root,
                                                  TSymbolTable *symbolTable,
                                                  const GLenum shaderType,
                                                  const TIntermTyped *clipDistanceEnableFlags,
                                                  const TIntermTyped *isLastPreRasterizationStage);

[[nodiscard]] bool ReplaceCullDistanceAssignments(TCompiler *compiler,
                                                  TIntermBlock *root,
                                                  TSymbolTable *symbolTable,
                                                  const GLenum shaderType);

[[nodiscard]] bool ZeroDisabledClipDistanceAssignments(TCompiler *compiler,
                                                       TIntermBlock *root,
                                                       TSymbolTable *symbolTable,
                                                       const GLenum shaderType,
                                                       const TIntermTyped *clipDistanceEnableFlags);
}  // namespace sh

#endif
