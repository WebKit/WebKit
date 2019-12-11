//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ShaderNULL.cpp:
//    Implements the class methods for ShaderNULL.
//

#include "libANGLE/renderer/null/ShaderNULL.h"

#include "common/debug.h"
#include "libANGLE/Context.h"

namespace rx
{

ShaderNULL::ShaderNULL(const gl::ShaderState &data) : ShaderImpl(data) {}

ShaderNULL::~ShaderNULL() {}

std::shared_ptr<WaitableCompileEvent> ShaderNULL::compile(const gl::Context *context,
                                                          gl::ShCompilerInstance *compilerInstance,
                                                          ShCompileOptions options)
{
    return compileImpl(context, compilerInstance, mData.getSource(), options);
}

std::string ShaderNULL::getDebugInfo() const
{
    return "";
}

}  // namespace rx
