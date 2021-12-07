//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// UseInterfaceBlockFields.h: insert statements to reference all members in InterfaceBlock list at
// the beginning of main. This is to work around a Mac driver that treats unused standard/shared
// uniform blocks as inactive.

#ifndef COMPILER_TRANSLATOR_TREEOPS_GL_USEINTERFACEBLOCKFIELDS_H_
#define COMPILER_TRANSLATOR_TREEOPS_GL_USEINTERFACEBLOCKFIELDS_H_

#include <GLSLANG/ShaderLang.h>
#include "common/angleutils.h"

namespace sh
{

class TCompiler;
class TIntermBlock;
class TSymbolTable;

using InterfaceBlockList = std::vector<sh::InterfaceBlock>;

#ifdef ANGLE_ENABLE_GLSL
ANGLE_NO_DISCARD bool UseInterfaceBlockFields(TCompiler *compiler,
                                              TIntermBlock *root,
                                              const InterfaceBlockList &blocks,
                                              const TSymbolTable &symbolTable);
#else
ANGLE_NO_DISCARD ANGLE_INLINE bool UseInterfaceBlockFields(TCompiler *compiler,
                                                           TIntermBlock *root,
                                                           const InterfaceBlockList &blocks,
                                                           const TSymbolTable &symbolTable)
{
    UNREACHABLE();
    return false;
}
#endif

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_GL_USEINTERFACEBLOCKFIELDS_H_
