//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BufferImpl.cpp: Implementation methods rx::BufferImpl class.

#include "libANGLE/renderer/BufferImpl.h"

namespace rx
{

angle::Result BufferImpl::getSubData(const gl::Context *context,
                                     GLintptr offset,
                                     GLsizeiptr size,
                                     void *outData)
{
    UNREACHABLE();
    return angle::Result::Stop;
}

}  // namespace rx
