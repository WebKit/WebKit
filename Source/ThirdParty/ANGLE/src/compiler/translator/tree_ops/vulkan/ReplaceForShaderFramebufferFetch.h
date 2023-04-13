//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ReplaceForShaderFramebufferFetch.h: Find any references to gl_LastFragData, and replace it with
// ANGLELastFragData.
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_VULKAN_REPLACEFORSHADERFRAMEBUFFERFETCH_H_
#define COMPILER_TRANSLATOR_TREEOPS_VULKAN_REPLACEFORSHADERFRAMEBUFFERFETCH_H_

#include "common/angleutils.h"

namespace sh
{

class TCompiler;
class TIntermBlock;
class TSymbolTable;
struct ShaderVariable;

enum class FramebufferFetchReplaceTarget
{
    LastFragData,
    LastFragColor,
};

// Declare the global variable, "ANGLELastFragData", and at the begining of the main function,
// assign a subpassLoad value to it. Then replace every gl_LastFragData/Color to
// "ANGLELastFragData". This is to solve the problem GLSL for Vulkan can't process
// gl_LastFragData/Color variable.
[[nodiscard]] bool ReplaceLastFrag(TCompiler *compiler,
                                   TIntermBlock *root,
                                   TSymbolTable *symbolTable,
                                   std::vector<ShaderVariable> *uniforms,
                                   FramebufferFetchReplaceTarget target);

// Similar to "ANGLELastFragData", but the difference is the variable for loading a framebuffer
// data. The variable decorated with a inout qualifier will be replaced to the variable decorated
// with a out qualifier. And this variable will be used to load the framebuffer data.
[[nodiscard]] bool ReplaceInOutVariables(TCompiler *compiler,
                                         TIntermBlock *root,
                                         TSymbolTable *symbolTable,
                                         std::vector<ShaderVariable> *uniforms);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_VULKAN_REPLACEFORSHADERFRAMEBUFFERFETCH_H_
