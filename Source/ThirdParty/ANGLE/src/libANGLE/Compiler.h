//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Compiler.h: Defines the gl::Compiler class, abstracting the ESSL compiler
// that a GL context holds.

#ifndef LIBANGLE_COMPILER_H_
#define LIBANGLE_COMPILER_H_

#include "libANGLE/Error.h"

namespace rx
{
class CompilerImpl;
}

namespace gl
{

class Compiler final
{
  public:
    explicit Compiler(rx::CompilerImpl *impl);
    ~Compiler();

    Error release();

    rx::CompilerImpl *getImplementation();

  private:
    rx::CompilerImpl *mCompiler;
};

}

#endif // LIBANGLE_COMPILER_H_
