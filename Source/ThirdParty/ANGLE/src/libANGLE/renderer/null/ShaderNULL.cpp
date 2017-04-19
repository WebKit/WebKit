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

namespace rx
{

ShaderNULL::ShaderNULL(const gl::ShaderState &data) : ShaderImpl(data)
{
}

ShaderNULL::~ShaderNULL()
{
}

ShCompileOptions ShaderNULL::prepareSourceAndReturnOptions(std::stringstream *sourceStream,
                                                           std::string *sourcePath)
{
    *sourceStream << mData.getSource();
    return 0;
}

bool ShaderNULL::postTranslateCompile(gl::Compiler *compiler, std::string *infoLog)
{
    return true;
}

std::string ShaderNULL::getDebugInfo() const
{
    return "";
}

}  // namespace rx
