//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CompilerNULL.cpp:
//    Implements the class methods for CompilerNULL.
//

#include "libANGLE/renderer/null/CompilerNULL.h"

#include "common/debug.h"

namespace rx
{

CompilerNULL::CompilerNULL() : CompilerImpl()
{
}

CompilerNULL::~CompilerNULL()
{
}

gl::Error CompilerNULL::release()
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

ShShaderOutput CompilerNULL::getTranslatorOutputType() const
{
    UNIMPLEMENTED();
    return ShShaderOutput();
}

}  // namespace rx
