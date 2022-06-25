//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CompilerMtl.mm:
//    Implements the class methods for CompilerMtl.
//

#include "libANGLE/renderer/metal/CompilerMtl.h"

#include <stdio.h>

#include "common/debug.h"

namespace rx
{

CompilerMtl::CompilerMtl(ShShaderOutput translatorOutputType)
    : CompilerImpl(), mTranslatorOutputType(translatorOutputType)
{}

CompilerMtl::~CompilerMtl() {}

ShShaderOutput CompilerMtl::getTranslatorOutputType() const
{
    return mTranslatorOutputType;
}

}  // namespace rx
