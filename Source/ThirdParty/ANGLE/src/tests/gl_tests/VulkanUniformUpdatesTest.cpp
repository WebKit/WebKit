//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VulkanUniformUpdatesTest:
//   Tests to validate our Vulkan dynamic uniform updates are working as expected.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/angle_test_instantiate.h"
// 'None' is defined as 'struct None {};' in
// third_party/googletest/src/googletest/include/gtest/internal/gtest-type-util.h.
// But 'None' is also defined as a numeric constant 0L in <X11/X.h>.
// So we need to include ANGLETest.h first to avoid this conflict.

#include "libANGLE/Context.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/ProgramVk.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

using namespace angle;

namespace
{

class VulkanUniformUpdatesTest : public ANGLETest
{
  protected:
    rx::ContextVk *hackANGLE() const
    {
        // Hack the angle!
        const gl::Context *context = static_cast<gl::Context *>(getEGLWindow()->getContext());
        return rx::GetImplAs<rx::ContextVk>(context);
    }

    rx::ProgramVk *hackProgram(GLuint handle) const
    {
        // Hack the angle!
        const gl::Context *context = static_cast<gl::Context *>(getEGLWindow()->getContext());
        const gl::Program *program = context->getProgramResolveLink(handle);
        return rx::vk::GetImpl(program);
    }

    static constexpr uint32_t kMaxSetsForTesting = 32;

    void limitMaxSets()
    {
        rx::ContextVk *contextVk = hackANGLE();

        // Force a small limit on the max sets per pool to more easily trigger a new allocation.
        rx::vk::DynamicDescriptorPool *uniformPool =
            contextVk->getDynamicDescriptorPool(rx::kUniformsDescriptorSetIndex);
        uniformPool->setMaxSetsPerPoolForTesting(kMaxSetsForTesting);
        VkDescriptorPoolSize uniformSetSize = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                               rx::GetUniformBufferDescriptorCount()};
        (void)uniformPool->init(contextVk, &uniformSetSize, 1);

        rx::vk::DynamicDescriptorPool *texturePool =
            contextVk->getDynamicDescriptorPool(rx::kTextureDescriptorSetIndex);
        texturePool->setMaxSetsPerPoolForTesting(kMaxSetsForTesting);
        VkDescriptorPoolSize textureSetSize = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               contextVk->getRenderer()->getMaxActiveTextures()};
        (void)texturePool->init(contextVk, &textureSetSize, 1);
    }
};

// This test updates a uniform until a new buffer is allocated and then make sure the uniform
// updates still work.
TEST_P(VulkanUniformUpdatesTest, UpdateUntilNewBufferIsAllocated)
{
    ASSERT_TRUE(IsVulkan());

    limitMaxSets();

    constexpr char kPositionUniformVertexShader[] = R"(attribute vec2 position;
uniform vec2 uniPosModifier;
void main()
{
    gl_Position = vec4(position + uniPosModifier, 0, 1);
})";

    constexpr char kColorUniformFragmentShader[] = R"(precision mediump float;
uniform vec4 uniColor;
void main()
{
    gl_FragColor = uniColor;
})";

    ANGLE_GL_PROGRAM(program, kPositionUniformVertexShader, kColorUniformFragmentShader);
    glUseProgram(program);

    rx::ProgramVk *programVk = hackProgram(program);

    // Set a really small min size so that uniform updates often allocates a new buffer.
    programVk->setDefaultUniformBlocksMinSizeForTesting(128);

    GLint posUniformLocation = glGetUniformLocation(program, "uniPosModifier");
    ASSERT_NE(posUniformLocation, -1);
    GLint colorUniformLocation = glGetUniformLocation(program, "uniColor");
    ASSERT_NE(colorUniformLocation, -1);

    // Sets both uniforms 10 times, it should certainly trigger new buffers creations by the
    // underlying StreamingBuffer.
    for (int i = 0; i < 100; i++)
    {
        glUniform2f(posUniformLocation, -0.5, 0.0);
        glUniform4f(colorUniformLocation, 1.0, 0.0, 0.0, 1.0);
        drawQuad(program, "position", 0.5f, 1.0f);
        swapBuffers();
        ASSERT_GL_NO_ERROR();
    }
}

void InitTexture(GLColor color, GLTexture *texture)
{
    const std::vector<GLColor> colors(4, color);
    glBindTexture(GL_TEXTURE_2D, *texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

// Force uniform updates until the dynamic descriptor pool wraps into a new pool allocation.
TEST_P(VulkanUniformUpdatesTest, DescriptorPoolUpdates)
{
    ASSERT_TRUE(IsVulkan());

    // Force a small limit on the max sets per pool to more easily trigger a new allocation.
    limitMaxSets();

    // Initialize texture program.
    GLuint program = get2DTexturedQuadProgram();
    ASSERT_NE(0u, program);
    glUseProgram(program);

    GLint texLoc = glGetUniformLocation(program, "tex");
    ASSERT_NE(-1, texLoc);

    // Initialize basic red texture.
    const std::vector<GLColor> redColors(4, GLColor::red);
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, redColors.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Draw multiple times, each iteration will create a new descriptor set.
    for (uint32_t iteration = 0; iteration < kMaxSetsForTesting * 8; ++iteration)
    {
        glUniform1i(texLoc, 0);
        drawQuad(program, "position", 0.5f, 1.0f, true);
        swapBuffers();
        ASSERT_GL_NO_ERROR();
    }
}

// Uniform updates along with Texture updates.
TEST_P(VulkanUniformUpdatesTest, DescriptorPoolUniformAndTextureUpdates)
{
    ASSERT_TRUE(IsVulkan());

    limitMaxSets();

    // Initialize texture program.
    constexpr char kVS[] = R"(attribute vec2 position;
varying mediump vec2 texCoord;
void main()
{
    gl_Position = vec4(position, 0, 1);
    texCoord = position * 0.5 + vec2(0.5);
})";

    constexpr char kFS[] = R"(varying mediump vec2 texCoord;
uniform sampler2D tex;
uniform mediump vec4 colorMask;
void main()
{
    gl_FragColor = texture2D(tex, texCoord) * colorMask;
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    // Get uniform locations.
    GLint texLoc = glGetUniformLocation(program, "tex");
    ASSERT_NE(-1, texLoc);

    GLint colorMaskLoc = glGetUniformLocation(program, "colorMask");
    ASSERT_NE(-1, colorMaskLoc);

    // Initialize white texture.
    GLTexture whiteTexture;
    InitTexture(GLColor::white, &whiteTexture);
    ASSERT_GL_NO_ERROR();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, whiteTexture);

    // Initialize magenta texture.
    GLTexture magentaTexture;
    InitTexture(GLColor::magenta, &magentaTexture);
    ASSERT_GL_NO_ERROR();

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, magentaTexture);

    // Draw multiple times, each iteration will create a new descriptor set.
    for (uint32_t iteration = 0; iteration < kMaxSetsForTesting * 2; ++iteration)
    {
        // Draw with white.
        glUniform1i(texLoc, 0);
        glUniform4f(colorMaskLoc, 1.0f, 1.0f, 1.0f, 1.0f);
        drawQuad(program, "position", 0.5f, 1.0f, true);

        // Draw with white masking out red.
        glUniform4f(colorMaskLoc, 0.0f, 1.0f, 1.0f, 1.0f);
        drawQuad(program, "position", 0.5f, 1.0f, true);

        // Draw with magenta.
        glUniform1i(texLoc, 1);
        glUniform4f(colorMaskLoc, 1.0f, 1.0f, 1.0f, 1.0f);
        drawQuad(program, "position", 0.5f, 1.0f, true);

        // Draw with magenta masking out red.
        glUniform4f(colorMaskLoc, 0.0f, 1.0f, 1.0f, 1.0f);
        drawQuad(program, "position", 0.5f, 1.0f, true);

        swapBuffers();
        ASSERT_GL_NO_ERROR();
    }
}

// Uniform updates along with Texture updates.
TEST_P(VulkanUniformUpdatesTest, DescriptorPoolUniformAndTextureUpdatesTwoShaders)
{
    ASSERT_TRUE(IsVulkan());

    // Force a small limit on the max sets per pool to more easily trigger a new allocation.
    limitMaxSets();

    // Initialize program.
    constexpr char kVS[] = R"(attribute vec2 position;
varying mediump vec2 texCoord;
void main()
{
    gl_Position = vec4(position, 0, 1);
    texCoord = position * 0.5 + vec2(0.5);
})";

    constexpr char kFS[] = R"(varying mediump vec2 texCoord;
uniform mediump vec4 colorMask;
void main()
{
    gl_FragColor = colorMask;
})";

    ANGLE_GL_PROGRAM(program1, kVS, kFS);
    ANGLE_GL_PROGRAM(program2, kVS, kFS);
    glUseProgram(program1);

    rx::ProgramVk *program1Vk = hackProgram(program1);
    rx::ProgramVk *program2Vk = hackProgram(program2);

    // Set a really small min size so that uniform updates often allocates a new buffer.
    program1Vk->setDefaultUniformBlocksMinSizeForTesting(128);
    program2Vk->setDefaultUniformBlocksMinSizeForTesting(128);

    // Get uniform locations.
    GLint colorMaskLoc1 = glGetUniformLocation(program1, "colorMask");
    ASSERT_NE(-1, colorMaskLoc1);
    GLint colorMaskLoc2 = glGetUniformLocation(program2, "colorMask");
    ASSERT_NE(-1, colorMaskLoc2);

    // Draw with white using program1.
    glUniform4f(colorMaskLoc1, 1.0f, 1.0f, 1.0f, 1.0f);
    drawQuad(program1, "position", 0.5f, 1.0f, true);
    swapBuffers();
    ASSERT_GL_NO_ERROR();

    // Now switch to use program2
    glUseProgram(program2);
    // Draw multiple times w/ program2, each iteration will create a new descriptor set.
    // This will cause the first descriptor pool to be cleaned up
    for (uint32_t iteration = 0; iteration < kMaxSetsForTesting * 2; ++iteration)
    {
        // Draw with white.
        glUniform4f(colorMaskLoc2, 1.0f, 1.0f, 1.0f, 1.0f);
        drawQuad(program2, "position", 0.5f, 1.0f, true);

        // Draw with white masking out red.
        glUniform4f(colorMaskLoc2, 0.0f, 1.0f, 1.0f, 1.0f);
        drawQuad(program2, "position", 0.5f, 1.0f, true);

        // Draw with magenta.
        glUniform4f(colorMaskLoc2, 1.0f, 1.0f, 1.0f, 1.0f);
        drawQuad(program2, "position", 0.5f, 1.0f, true);

        // Draw with magenta masking out red.
        glUniform4f(colorMaskLoc2, 0.0f, 1.0f, 1.0f, 1.0f);
        drawQuad(program2, "position", 0.5f, 1.0f, true);

        swapBuffers();
        ASSERT_GL_NO_ERROR();
    }
    // Finally, attempt to draw again with program1, with original uniform values.
    glUseProgram(program1);
    drawQuad(program1, "position", 0.5f, 1.0f, true);
    swapBuffers();
    ASSERT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST(VulkanUniformUpdatesTest, ES2_VULKAN());

}  // anonymous namespace
