//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CLProgramVk.cpp: Implements the class methods for CLProgramVk.

#include "libANGLE/renderer/vulkan/CLProgramVk.h"

namespace rx
{

CLProgramVk::CLProgramVk(const cl::Program &program) : CLProgramImpl(program) {}

CLProgramVk::~CLProgramVk() = default;

}  // namespace rx
