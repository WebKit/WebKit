//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ShaderMtl.mm:
//    Implements the class methods for ShaderMtl.
//

#include "libANGLE/renderer/metal/ShaderMtl.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/metal/ContextMtl.h"

namespace rx
{

ShaderMtl::ShaderMtl(const gl::ShaderState &data) : ShaderImpl(data) {}

ShaderMtl::~ShaderMtl() {}

std::shared_ptr<WaitableCompileEvent> ShaderMtl::compile(const gl::Context *context,
                                                         gl::ShCompilerInstance *compilerInstance,
                                                         ShCompileOptions options)
{
    ShCompileOptions compileOptions = SH_INITIALIZE_UNINITIALIZED_LOCALS;

    bool isWebGL = context->getExtensions().webglCompatibility;
    if (isWebGL && mData.getShaderType() != gl::ShaderType::Compute)
    {
        compileOptions |= SH_INIT_OUTPUT_VARIABLES;
    }

    compileOptions |= SH_CLAMP_POINT_SIZE;

    return compileImpl(context, compilerInstance, mData.getSource(), compileOptions | options);
}

std::string ShaderMtl::getDebugInfo() const
{
    return mData.getTranslatedSource();
}

}  // namespace rx
