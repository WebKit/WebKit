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
#include "libANGLE/renderer/vulkan/TextureVk.h"
#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"
#include "util/shader_utils.h"

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
        const gl::Program *program = context->getProgramResolveLink({handle});
        return rx::vk::GetImpl(program);
    }

    rx::TextureVk *hackTexture(GLuint handle) const
    {
        // Hack the angle!
        const gl::Context *context = static_cast<gl::Context *>(getEGLWindow()->getContext());
        const gl::Texture *texture = context->getTexture({handle});
        return rx::vk::GetImpl(texture);
    }

    static constexpr uint32_t kMaxSetsForTesting = 32;

    void limitMaxSets(GLuint program)
    {
        rx::ContextVk *contextVk = hackANGLE();
        rx::ProgramVk *programVk = hackProgram(program);

        // Force a small limit on the max sets per pool to more easily trigger a new allocation.
        rx::vk::DynamicDescriptorPool *uniformPool =
            programVk->getDynamicDescriptorPool(rx::kUniformsAndXfbDescriptorSetIndex);
        uniformPool->setMaxSetsPerPoolForTesting(kMaxSetsForTesting);
        VkDescriptorPoolSize uniformSetSize = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                               rx::kReservedDefaultUniformBindingCount};
        (void)uniformPool->init(contextVk, &uniformSetSize, 1);

        uint32_t textureCount =
            static_cast<uint32_t>(programVk->getState().getSamplerBindings().size());

        // To support the bindEmptyForUnusedDescriptorSets workaround.
        textureCount = std::max(textureCount, 1u);

        rx::vk::DynamicDescriptorPool *texturePool =
            programVk->getDynamicDescriptorPool(rx::kTextureDescriptorSetIndex);
        texturePool->setMaxSetsPerPoolForTesting(kMaxSetsForTesting);
        VkDescriptorPoolSize textureSetSize = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               textureCount};
        (void)texturePool->init(contextVk, &textureSetSize, 1);
    }

    static constexpr size_t kTextureStagingBufferSizeForTesting = 128;

    void limitTextureStagingBufferSize(GLuint texture)
    {
        rx::TextureVk *textureVk = hackTexture(texture);
        textureVk->overrideStagingBufferSizeForTesting(kTextureStagingBufferSizeForTesting);
    }
};

// This test updates a uniform until a new buffer is allocated and then make sure the uniform
// updates still work.
TEST_P(VulkanUniformUpdatesTest, UpdateUntilNewBufferIsAllocated)
{
    ASSERT_TRUE(IsVulkan());

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

    limitMaxSets(program);

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

    // Initialize texture program.
    GLuint program = get2DTexturedQuadProgram();
    ASSERT_NE(0u, program);
    glUseProgram(program);

    // Force a small limit on the max sets per pool to more easily trigger a new allocation.
    limitMaxSets(program);

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

    // Initialize texture program.
    constexpr char kFS[] = R"(varying mediump vec2 v_texCoord;
uniform sampler2D tex;
uniform mediump vec4 colorMask;
void main()
{
    gl_FragColor = texture2D(tex, v_texCoord) * colorMask;
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Texture2D(), kFS);
    glUseProgram(program);

    limitMaxSets(program);

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
        drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);

        // Draw with white masking out red.
        glUniform4f(colorMaskLoc, 0.0f, 1.0f, 1.0f, 1.0f);
        drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);

        // Draw with magenta.
        glUniform1i(texLoc, 1);
        glUniform4f(colorMaskLoc, 1.0f, 1.0f, 1.0f, 1.0f);
        drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);

        // Draw with magenta masking out red.
        glUniform4f(colorMaskLoc, 0.0f, 1.0f, 1.0f, 1.0f);
        drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);

        swapBuffers();
        ASSERT_GL_NO_ERROR();
    }
}

// Uniform updates along with Texture regeneration.
TEST_P(VulkanUniformUpdatesTest, DescriptorPoolUniformAndTextureRegeneration)
{
    ASSERT_TRUE(IsVulkan());

    // Initialize texture program.
    constexpr char kFS[] = R"(varying mediump vec2 v_texCoord;
uniform sampler2D tex;
uniform mediump vec4 colorMask;
void main()
{
    gl_FragColor = texture2D(tex, v_texCoord) * colorMask;
})";

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Texture2D(), kFS);
    glUseProgram(program);

    limitMaxSets(program);

    // Initialize large arrays of textures.
    std::vector<GLTexture> whiteTextures;
    std::vector<GLTexture> magentaTextures;

    for (uint32_t iteration = 0; iteration < kMaxSetsForTesting * 2; ++iteration)
    {
        // Initialize white texture.
        GLTexture whiteTexture;
        InitTexture(GLColor::white, &whiteTexture);
        ASSERT_GL_NO_ERROR();
        whiteTextures.emplace_back(std::move(whiteTexture));

        // Initialize magenta texture.
        GLTexture magentaTexture;
        InitTexture(GLColor::magenta, &magentaTexture);
        ASSERT_GL_NO_ERROR();
        magentaTextures.emplace_back(std::move(magentaTexture));
    }

    // Get uniform locations.
    GLint texLoc = glGetUniformLocation(program, "tex");
    ASSERT_NE(-1, texLoc);

    GLint colorMaskLoc = glGetUniformLocation(program, "colorMask");
    ASSERT_NE(-1, colorMaskLoc);

    // Draw multiple times, each iteration will create a new descriptor set.
    for (int outerIteration = 0; outerIteration < 2; ++outerIteration)
    {
        for (uint32_t iteration = 0; iteration < kMaxSetsForTesting * 2; ++iteration)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, whiteTextures[iteration]);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, magentaTextures[iteration]);

            // Draw with white.
            glUniform1i(texLoc, 0);
            glUniform4f(colorMaskLoc, 1.0f, 1.0f, 1.0f, 1.0f);
            drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);

            // Draw with white masking out red.
            glUniform4f(colorMaskLoc, 0.0f, 1.0f, 1.0f, 1.0f);
            drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);

            // Draw with magenta.
            glUniform1i(texLoc, 1);
            glUniform4f(colorMaskLoc, 1.0f, 1.0f, 1.0f, 1.0f);
            drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);

            // Draw with magenta masking out red.
            glUniform4f(colorMaskLoc, 0.0f, 1.0f, 1.0f, 1.0f);
            drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);

            swapBuffers();
            ASSERT_GL_NO_ERROR();
        }
    }
}

// Uniform updates along with Texture updates.
TEST_P(VulkanUniformUpdatesTest, DescriptorPoolUniformAndTextureUpdatesTwoShaders)
{
    ASSERT_TRUE(IsVulkan());

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

    // Force a small limit on the max sets per pool to more easily trigger a new allocation.
    limitMaxSets(program1);
    limitMaxSets(program2);

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

// Verify that overflowing a Texture's staging buffer doesn't overwrite current data.
TEST_P(VulkanUniformUpdatesTest, TextureStagingBufferRecycling)
{
    ASSERT_TRUE(IsVulkan());

    // Fails on older MESA drivers.  http://crbug.com/979349
    ANGLE_SKIP_TEST_IF(IsAMD() && IsLinux());

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    limitTextureStagingBufferSize(tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    const GLColor kColors[4] = {GLColor::red, GLColor::green, GLColor::blue, GLColor::yellow};

    // Repeatedly update the staging buffer to trigger multiple recyclings.
    const GLsizei kHalfX      = getWindowWidth() / 2;
    const GLsizei kHalfY      = getWindowHeight() / 2;
    constexpr int kIterations = 4;
    for (int x = 0; x < 2; ++x)
    {
        for (int y = 0; y < 2; ++y)
        {
            const int kColorIndex = x + y * 2;
            const GLColor kColor  = kColors[kColorIndex];

            for (int iteration = 0; iteration < kIterations; ++iteration)
            {
                for (int subX = 0; subX < kHalfX; ++subX)
                {
                    for (int subY = 0; subY < kHalfY; ++subY)
                    {
                        const GLsizei xoffset = x * kHalfX + subX;
                        const GLsizei yoffset = y * kHalfY + subY;

                        // Update a single pixel.
                        glTexSubImage2D(GL_TEXTURE_2D, 0, xoffset, yoffset, 1, 1, GL_RGBA,
                                        GL_UNSIGNED_BYTE, kColor.data());
                    }
                }
            }
        }
    }

    draw2DTexturedQuad(0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    // Verify pixels.
    for (int x = 0; x < 2; ++x)
    {
        for (int y = 0; y < 2; ++y)
        {
            const GLsizei xoffset = x * kHalfX;
            const GLsizei yoffset = y * kHalfY;
            const int kColorIndex = x + y * 2;
            const GLColor kColor  = kColors[kColorIndex];
            EXPECT_PIXEL_RECT_EQ(xoffset, yoffset, kHalfX, kHalfY, kColor);
        }
    }
}

ANGLE_INSTANTIATE_TEST(VulkanUniformUpdatesTest, ES2_VULKAN(), ES3_VULKAN());

}  // anonymous namespace
