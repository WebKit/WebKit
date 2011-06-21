//
// Copyright (c) 2002-2011 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/OutputGLSL.h"

TOutputGLSL::TOutputGLSL(TInfoSinkBase& objSink)
    : TOutputGLSLBase(objSink)
{
}

bool TOutputGLSL::writeVariablePrecision(TPrecision)
{
    return false;
}
