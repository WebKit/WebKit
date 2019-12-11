//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FenceNVVk.cpp:
//    Implements the class methods for FenceNVVk.
//

#include "libANGLE/renderer/vulkan/FenceNVVk.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace rx
{

FenceNVVk::FenceNVVk() : FenceNVImpl() {}

FenceNVVk::~FenceNVVk() {}

angle::Result FenceNVVk::set(const gl::Context *context, GLenum condition)
{
    ANGLE_VK_UNREACHABLE(vk::GetImpl(context));
    return angle::Result::Stop;
}

angle::Result FenceNVVk::test(const gl::Context *context, GLboolean *outFinished)
{
    ANGLE_VK_UNREACHABLE(vk::GetImpl(context));
    return angle::Result::Stop;
}

angle::Result FenceNVVk::finish(const gl::Context *context)
{
    ANGLE_VK_UNREACHABLE(vk::GetImpl(context));
    return angle::Result::Stop;
}

}  // namespace rx
