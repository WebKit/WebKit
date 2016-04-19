//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DeferGlobalInitializers is an AST traverser that moves global initializers into a function, and
// adds a function call to that function in the beginning of main().
// This enables initialization of globals with uniforms or non-constant globals, as allowed by
// the WebGL spec. Some initializers referencing non-constants may need to be unfolded into if
// statements in HLSL - this kind of steps should be done after DeferGlobalInitializers is run.
//

#ifndef COMPILER_TRANSLATOR_DEFERGLOBALINITIALIZERS_H_
#define COMPILER_TRANSLATOR_DEFERGLOBALINITIALIZERS_H_

class TIntermNode;

void DeferGlobalInitializers(TIntermNode *root);

#endif  // COMPILER_TRANSLATOR_DEFERGLOBALINITIALIZERS_H_
