//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ShaderNULL.h:
//    Defines the class interface for ShaderNULL, implementing ShaderImpl.
//

#ifndef LIBANGLE_RENDERER_NULL_SHADERNULL_H_
#define LIBANGLE_RENDERER_NULL_SHADERNULL_H_

#include "libANGLE/renderer/ShaderImpl.h"

namespace rx
{

class ShaderNULL : public ShaderImpl
{
  public:
    ShaderNULL(const gl::ShaderState &data);
    ~ShaderNULL() override;

    // Returns additional sh::Compile options.
    ShCompileOptions prepareSourceAndReturnOptions(std::stringstream *sourceStream,
                                                   std::string *sourcePath) override;
    // Returns success for compiling on the driver. Returns success.
    bool postTranslateCompile(gl::Compiler *compiler, std::string *infoLog) override;

    std::string getDebugInfo() const override;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_NULL_SHADERNULL_H_
