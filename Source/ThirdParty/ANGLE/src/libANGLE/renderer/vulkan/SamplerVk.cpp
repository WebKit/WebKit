//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SamplerVk.cpp:
//    Implements the class methods for SamplerVk.
//

#include "libANGLE/renderer/vulkan/SamplerVk.h"

#include "common/debug.h"

namespace rx
{

SamplerVk::SamplerVk(const gl::SamplerState &state) : SamplerImpl(state) {}

SamplerVk::~SamplerVk() {}

}  // namespace rx
