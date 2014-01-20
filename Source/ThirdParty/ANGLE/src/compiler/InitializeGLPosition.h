//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_INITIALIZE_GL_POSITION_H_
#define COMPILER_INITIALIZE_GL_POSITION_H_

#include "compiler/intermediate.h"

class InitializeGLPosition : public TIntermTraverser
{
public:
    InitializeGLPosition() : mCodeInserted(false) { }

protected:
    virtual bool visitBinary(Visit visit, TIntermBinary* node) { return false; }
    virtual bool visitUnary(Visit visit, TIntermUnary* node) { return false; }
    virtual bool visitSelection(Visit visit, TIntermSelection* node) { return false; }
    virtual bool visitLoop(Visit visit, TIntermLoop* node) { return false; }
    virtual bool visitBranch(Visit visit, TIntermBranch* node) { return false; }

    virtual bool visitAggregate(Visit visit, TIntermAggregate* node);

private:
    // Insert AST node in the beginning of main() for "gl_Position = vec4(0.0, 0.0, 0.0, 1.0);".
    void insertCode(TIntermSequence& sequence);

    bool mCodeInserted;
};

#endif  // COMPILER_INITIALIZE_GL_POSITION_H_
