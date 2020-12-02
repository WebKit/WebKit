//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VulkanDescriptorSetTest:
//   Various tests related for Vulkan descriptor sets.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include "libANGLE/Context.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

using namespace angle;

namespace
{

class VulkanDescriptorSetTest : public ANGLETest
{
  protected:
    VulkanDescriptorSetTest() {}

    void testSetUp() override
    {
        mMaxSetsPerPool = rx::vk::DynamicDescriptorPool::GetMaxSetsPerPoolForTesting();
        mMaxSetsPerPoolMultiplier =
            rx::vk::DynamicDescriptorPool::GetMaxSetsPerPoolMultiplierForTesting();
    }

    void testTearDown() override
    {
        rx::vk::DynamicDescriptorPool::SetMaxSetsPerPoolForTesting(mMaxSetsPerPool);
        rx::vk::DynamicDescriptorPool::SetMaxSetsPerPoolMultiplierForTesting(
            mMaxSetsPerPoolMultiplier);
    }

    static constexpr uint32_t kMaxSetsForTesting           = 1;
    static constexpr uint32_t kMaxSetsMultiplierForTesting = 1;

    void limitMaxSets()
    {
        rx::vk::DynamicDescriptorPool::SetMaxSetsPerPoolForTesting(kMaxSetsForTesting);
        rx::vk::DynamicDescriptorPool::SetMaxSetsPerPoolMultiplierForTesting(
            kMaxSetsMultiplierForTesting);
    }

  private:
    uint32_t mMaxSetsPerPool;
    uint32_t mMaxSetsPerPoolMultiplier;
};

// Test atomic counter read.
TEST_P(VulkanDescriptorSetTest, AtomicCounterReadLimitedDescriptorPool)
{
    // Skipping due to a bug on the Qualcomm Vulkan Android driver.
    // http://anglebug.com/3726
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsVulkan());

    // Skipping test while we work on enabling atomic counter buffer support in th D3D renderer.
    // http://anglebug.com/1729
    ANGLE_SKIP_TEST_IF(IsD3D11());

    // Must be before program creation to limit the descriptor pool sizes when creating the pipeline
    // layout.
    limitMaxSets();

    constexpr char kFS[] =
        "#version 310 es\n"
        "precision highp float;\n"
        "layout(binding = 0, offset = 4) uniform atomic_uint ac;\n"
        "out highp vec4 my_color;\n"
        "void main()\n"
        "{\n"
        "    my_color = vec4(0.0);\n"
        "    uint a1 = atomicCounter(ac);\n"
        "    if (a1 == 3u) my_color = vec4(1.0);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, essl31_shaders::vs::Simple(), kFS);

    glUseProgram(program.get());

    // The initial value of counter 'ac' is 3u.
    unsigned int bufferData[3] = {11u, 3u, 1u};
    GLBuffer atomicCounterBuffer;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomicCounterBuffer);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomicCounterBuffer);

    for (int i = 0; i < 5; ++i)
    {
        glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(bufferData), bufferData, GL_STATIC_DRAW);
        drawQuad(program.get(), essl31_shaders::PositionAttrib(), 0.0f);
        ASSERT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);
    }
}

ANGLE_INSTANTIATE_TEST(VulkanDescriptorSetTest, ES31_VULKAN(), ES31_VULKAN_SWIFTSHADER());

}  // namespace