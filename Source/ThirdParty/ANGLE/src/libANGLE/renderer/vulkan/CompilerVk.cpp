//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CompilerVk.cpp:
//    Implements the class methods for CompilerVk.
//

#include "libANGLE/renderer/vulkan/CompilerVk.h"

#include "common/debug.h"

namespace rx
{

CompilerVk::CompilerVk() : CompilerImpl()
{
}

CompilerVk::~CompilerVk()
{
}

gl::Error CompilerVk::release()
{
    UNIMPLEMENTED();
    return gl::Error(GL_INVALID_OPERATION);
}

ShShaderOutput CompilerVk::getTranslatorOutputType() const
{
    UNIMPLEMENTED();
    return ShShaderOutput();
}

}  // namespace rx
