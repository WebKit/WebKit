//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ShaderImpl.h: Defines the abstract rx::ShaderImpl class.

#ifndef LIBANGLE_RENDERER_SHADERIMPL_H_
#define LIBANGLE_RENDERER_SHADERIMPL_H_

#include "common/angleutils.h"
#include "libANGLE/Shader.h"

namespace rx
{

class ShaderImpl : angle::NonCopyable
{
  public:
    ShaderImpl(const gl::ShaderState &data) : mData(data) {}
    virtual ~ShaderImpl() { }

    // Returns additional ShCompile options.
    virtual ShCompileOptions prepareSourceAndReturnOptions(std::stringstream *sourceStream,
                                                           std::string *sourcePath) = 0;
    // Returns success for compiling on the driver. Returns success.
    virtual bool postTranslateCompile(gl::Compiler *compiler, std::string *infoLog) = 0;

    virtual std::string getDebugInfo() const = 0;

    const gl::ShaderState &getData() const { return mData; }

  protected:
    const gl::ShaderState &mData;
};

}

#endif // LIBANGLE_RENDERER_SHADERIMPL_H_
