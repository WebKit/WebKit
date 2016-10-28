//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ShaderVk.cpp:
//    Implements the class methods for ShaderVk.
//

#include "libANGLE/renderer/vulkan/ShaderVk.h"

#include "common/debug.h"

namespace rx
{

ShaderVk::ShaderVk(const gl::ShaderState &data) : ShaderImpl(data)
{
}

ShaderVk::~ShaderVk()
{
}

ShCompileOptions ShaderVk::prepareSourceAndReturnOptions(std::stringstream *sourceStream,
                                                         std::string *sourcePath)
{
    UNIMPLEMENTED();
    return int();
}

bool ShaderVk::postTranslateCompile(gl::Compiler *compiler, std::string *infoLog)
{
    UNIMPLEMENTED();
    return bool();
}

std::string ShaderVk::getDebugInfo() const
{
    UNIMPLEMENTED();
    return std::string();
}

}  // namespace rx
