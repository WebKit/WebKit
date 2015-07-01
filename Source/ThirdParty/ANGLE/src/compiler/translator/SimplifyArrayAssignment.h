//
// Copyright (c) 2002-2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SimplifyArrayAssignment is an AST traverser to replace statements where
// the return value of array assignment is used with statements where
// the return value of array assignment is not used.
//

#ifndef COMPILER_TRANSLATOR_SIMPLIFYARRAYASSIGNMENT_H_
#define COMPILER_TRANSLATOR_SIMPLIFYARRAYASSIGNMENT_H_

#include "common/angleutils.h"
#include "compiler/translator/IntermNode.h"

class SimplifyArrayAssignment : public TIntermTraverser
{
  public:
    SimplifyArrayAssignment() { }

    virtual bool visitBinary(Visit visit, TIntermBinary *node);
};

#endif  // COMPILER_TRANSLATOR_SIMPLIFYARRAYASSIGNMENT_H_
