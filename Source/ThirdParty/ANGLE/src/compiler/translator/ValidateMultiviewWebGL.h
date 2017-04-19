//
// Copyright (c) 2002-2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ValidateMultiviewWebGL.h:
//   Validate the AST according to rules in the WEBGL_multiview extension spec. Only covers those
//   rules not already covered in ParseContext.
//

#ifndef COMPILER_TRANSLATOR_VALIDATEMULTIVIEWWEBGL_H_
#define COMPILER_TRANSLATOR_VALIDATEMULTIVIEWWEBGL_H_

#include "GLSLANG/ShaderVars.h"

namespace sh
{
class TDiagnostics;
class TIntermBlock;
class TSymbolTable;

// Check for errors and output error messages with diagnostics.
// Returns true if there are no errors.
bool ValidateMultiviewWebGL(TIntermBlock *root,
                            sh::GLenum shaderType,
                            const TSymbolTable &symbolTable,
                            int shaderVersion,
                            bool multiview2,
                            TDiagnostics *diagnostics);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_VALIDATEMULTIVIEWWEBGL_H_
