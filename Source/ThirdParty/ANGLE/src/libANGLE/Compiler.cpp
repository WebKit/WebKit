//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Compiler.cpp: implements the gl::Compiler class.

#include "libANGLE/Compiler.h"
#include "libANGLE/renderer/CompilerImpl.h"

#include "common/debug.h"

namespace gl
{

Compiler::Compiler(rx::CompilerImpl *impl)
    : mCompiler(impl)
{
    ASSERT(mCompiler);
}

Compiler::~Compiler()
{
    SafeDelete(mCompiler);
}

Error Compiler::release()
{
    return mCompiler->release();
}

rx::CompilerImpl *Compiler::getImplementation()
{
    return mCompiler;
}

}
