//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VulkanSharedFenceTest:
//   Various tests related for rx::vk::SharedFence class.
//

#include "test_utils/ANGLETest.h"

#include "libANGLE/Display.h"
#include "libANGLE/renderer/vulkan/CommandProcessor.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"

using namespace angle;

namespace
{

class VulkanSharedFenceTest : public ANGLETest<>
{
  protected:
    gl::Context *hackContext() const
    {
        egl::Display *display   = static_cast<egl::Display *>(getEGLWindow()->getDisplay());
        gl::ContextID contextID = {
            static_cast<GLuint>(reinterpret_cast<uintptr_t>(getEGLWindow()->getContext()))};
        return display->getContext(contextID);
    }

    rx::ContextVk *hackANGLE() const
    {
        // Hack the angle!
        return rx::GetImplAs<rx::ContextVk>(hackContext());
    }
};

// Test init/release/init sequence.
TEST_P(VulkanSharedFenceTest, InitReleaseInit)
{
    ASSERT_TRUE(IsVulkan());

    rx::ContextVk *contextVk = hackANGLE();
    VkDevice device          = contextVk->getDevice();

    rx::vk::FenceRecycler recycler;
    {
        rx::vk::SharedFence fence;

        VkResult result = fence.init(device, &recycler);
        ASSERT_EQ(result, VK_SUCCESS);

        fence.release();

        result = fence.init(device, &recycler);
        ASSERT_EQ(result, VK_SUCCESS);
    }
    recycler.destroy(contextVk);
}

// Test init/destroy/init sequence.
TEST_P(VulkanSharedFenceTest, InitDestroyInit)
{
    ASSERT_TRUE(IsVulkan());

    rx::ContextVk *contextVk = hackANGLE();
    VkDevice device          = contextVk->getDevice();

    rx::vk::FenceRecycler recycler;
    {
        rx::vk::SharedFence fence;

        VkResult result = fence.init(device, &recycler);
        ASSERT_EQ(result, VK_SUCCESS);

        fence.destroy(device);

        result = fence.init(device, &recycler);
        ASSERT_EQ(result, VK_SUCCESS);
    }
    recycler.destroy(contextVk);
}

ANGLE_INSTANTIATE_TEST(VulkanSharedFenceTest, ES2_VULKAN());

}  // namespace
