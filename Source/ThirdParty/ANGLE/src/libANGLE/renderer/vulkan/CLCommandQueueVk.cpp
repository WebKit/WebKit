//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLCommandQueueVk.cpp: Implements the class methods for CLCommandQueueVk.

#include "libANGLE/renderer/vulkan/CLCommandQueueVk.h"

namespace rx
{

CLCommandQueueVk::CLCommandQueueVk(const cl::CommandQueue &commandQueue)
    : CLCommandQueueImpl(commandQueue)
{}

CLCommandQueueVk::~CLCommandQueueVk() = default;

}  // namespace rx
