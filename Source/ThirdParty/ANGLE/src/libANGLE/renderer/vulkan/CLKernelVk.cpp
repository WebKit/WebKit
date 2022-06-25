//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLKernelVk.cpp: Implements the class methods for CLKernelVk.

#include "libANGLE/renderer/vulkan/CLKernelVk.h"

namespace rx
{

CLKernelVk::CLKernelVk(const cl::Kernel &kernel) : CLKernelImpl(kernel) {}

CLKernelVk::~CLKernelVk() = default;

}  // namespace rx
