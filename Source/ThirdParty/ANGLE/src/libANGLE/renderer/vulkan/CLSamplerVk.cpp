//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLSamplerVk.cpp: Implements the class methods for CLSamplerVk.

#include "libANGLE/renderer/vulkan/CLSamplerVk.h"

namespace rx
{

CLSamplerVk::CLSamplerVk(const cl::Sampler &sampler) : CLSamplerImpl(sampler) {}

CLSamplerVk::~CLSamplerVk() = default;

}  // namespace rx
