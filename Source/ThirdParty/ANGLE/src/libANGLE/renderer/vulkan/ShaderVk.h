//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ShaderVk.h:
//    Defines the class interface for ShaderVk, implementing ShaderImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_SHADERVK_H_
#define LIBANGLE_RENDERER_VULKAN_SHADERVK_H_

#include "libANGLE/renderer/ShaderImpl.h"

namespace rx
{

class ShaderVk : public ShaderImpl
{
  public:
    ShaderVk(const gl::ShaderState &data);
    ~ShaderVk() override;

    // Returns additional ShCompile options.
    ShCompileOptions prepareSourceAndReturnOptions(std::stringstream *sourceStream,
                                                   std::string *sourcePath) override;
    // Returns success for compiling on the driver. Returns success.
    bool postTranslateCompile(gl::Compiler *compiler, std::string *infoLog) override;

    std::string getDebugInfo() const override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_SHADERVK_H_
