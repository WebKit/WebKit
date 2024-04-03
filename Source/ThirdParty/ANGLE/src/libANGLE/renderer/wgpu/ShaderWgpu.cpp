//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ShaderWgpu.cpp:
//    Implements the class methods for ShaderWgpu.
//

#include "libANGLE/renderer/wgpu/ShaderWgpu.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/ContextImpl.h"

namespace rx
{

ShaderWgpu::ShaderWgpu(const gl::ShaderState &data) : ShaderImpl(data) {}

ShaderWgpu::~ShaderWgpu() {}

std::shared_ptr<ShaderTranslateTask> ShaderWgpu::compile(const gl::Context *context,
                                                         ShCompileOptions *options)
{
    const gl::Extensions &extensions = context->getImplementation()->getExtensions();
    if (extensions.shaderPixelLocalStorageANGLE)
    {
        options->pls = context->getImplementation()->getNativePixelLocalStorageOptions();
    }
    return std::shared_ptr<ShaderTranslateTask>(new ShaderTranslateTask);
}

std::string ShaderWgpu::getDebugInfo() const
{
    return "";
}

}  // namespace rx
