//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_INITIALIZEVARIABLES_H_
#define COMPILER_TRANSLATOR_INITIALIZEVARIABLES_H_

#include <GLSLANG/ShaderLang.h>

class TIntermNode;

typedef std::vector<sh::ShaderVariable> InitVariableList;

// This function cannot currently initialize structures containing arrays for an ESSL 1.00 backend.
void InitializeVariables(TIntermNode *root, const InitVariableList &vars);

#endif  // COMPILER_TRANSLATOR_INITIALIZEVARIABLES_H_
