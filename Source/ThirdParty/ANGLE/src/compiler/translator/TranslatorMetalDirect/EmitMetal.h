//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_EMITMETAL_H_
#define COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_EMITMETAL_H_

#include "common/angleutils.h"
#include "compiler/translator/Compiler.h"
#include "compiler/translator/TranslatorMetalDirect/IdGen.h"
#include "compiler/translator/TranslatorMetalDirect/ProgramPrelude.h"
#include "compiler/translator/TranslatorMetalDirect/RewritePipelines.h"
#include "compiler/translator/TranslatorMetalDirect/SymbolEnv.h"

namespace sh
{

// Walks the AST and emits Metal code.
ANGLE_NO_DISCARD bool EmitMetal(TCompiler &compiler,
                                TIntermBlock &root,
                                IdGen &idGen,
                                const PipelineStructs &pipelineStructs,
                                SymbolEnv &symbolEnv,
                                const ProgramPreludeConfig &ppc);

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TRANSLATORMETALDIRECT_EMITMETAL_H_
