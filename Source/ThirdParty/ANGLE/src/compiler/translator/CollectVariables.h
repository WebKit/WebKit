//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CollectVariables.h: Collect lists of shader interface variables based on the AST.

#ifndef COMPILER_TRANSLATOR_COLLECTVARIABLES_H_
#define COMPILER_TRANSLATOR_COLLECTVARIABLES_H_

#include <GLSLANG/ShaderLang.h>

#include "compiler/translator/ExtensionBehavior.h"

namespace sh
{

class TIntermBlock;
class TSymbolTable;

void CollectVariables(TIntermBlock *root,
                      std::vector<Attribute> *attributes,
                      std::vector<OutputVariable> *outputVariables,
                      std::vector<Uniform> *uniforms,
                      std::vector<Varying> *inputVaryings,
                      std::vector<Varying> *outputVaryings,
                      std::vector<InterfaceBlock> *uniformBlocks,
                      std::vector<InterfaceBlock> *shaderStorageBlocks,
                      std::vector<InterfaceBlock> *inBlocks,
                      ShHashFunction64 hashFunction,
                      TSymbolTable *symbolTable,
                      int shaderVersion,
                      GLenum shaderType,
                      const TExtensionBehavior &extensionBehavior);
}

#endif  // COMPILER_TRANSLATOR_COLLECTVARIABLES_H_
