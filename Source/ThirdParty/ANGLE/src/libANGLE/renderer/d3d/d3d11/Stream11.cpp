//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Stream11.cpp: Defines the rx::Stream11 class which implements rx::StreamImpl.

#include "libANGLE/renderer/d3d/d3d11/Stream11.h"

#include "common/utilities.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

namespace rx
{

Stream11::Stream11(Renderer11 *renderer) : mRenderer(renderer)
{
    UNUSED_VARIABLE(mRenderer);
}

Stream11::~Stream11()
{
}
}  // namespace rx
