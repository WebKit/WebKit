//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLMemoryVk.cpp: Implements the class methods for CLMemoryVk.

#include "libANGLE/renderer/vulkan/CLMemoryVk.h"

namespace rx
{

CLMemoryVk::CLMemoryVk(const cl::Memory &memory) : CLMemoryImpl(memory) {}

CLMemoryVk::~CLMemoryVk() = default;

}  // namespace rx
