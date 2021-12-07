//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLEventVk.cpp: Implements the class methods for CLEventVk.

#include "libANGLE/renderer/vulkan/CLEventVk.h"

namespace rx
{

CLEventVk::CLEventVk(const cl::Event &event) : CLEventImpl(event) {}

CLEventVk::~CLEventVk() = default;

}  // namespace rx
