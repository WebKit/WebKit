//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "common/unsafe_buffers.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class ViewportTest : public ANGLETest<>
{
  protected:
    ViewportTest()
    {
        setWindowWidth(512);
        setWindowHeight(512);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);

        mProgram = 0;
    }

    void runNonScissoredTest()
    {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        runTest();
    }

    void runScissoredTest()
    {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glEnable(GL_SCISSOR_TEST);
        glScissor(0, getWindowHeight() / 2, getWindowWidth(), getWindowHeight() / 2);

        runTest();
    }

    void runTest()
    {
        // Firstly ensure that no errors have been hit.
        EXPECT_GL_NO_ERROR();

        std::array<GLint, 4> viewportSize;
        glGetIntegerv(GL_VIEWPORT, viewportSize.data());

        // Clear to green. Might be a scissored clear, if scissorSize != window size
        glClearColor(0, 1, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw a red quad centered in the middle of the viewport, with dimensions 25% of the size
        // of the viewport.
        drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f, 0.25f);

        GLint centerViewportX = viewportSize[0] + (viewportSize[2] / 2);
        GLint centerViewportY = viewportSize[1] + (viewportSize[3] / 2);

        GLint redQuadLeftSideX   = viewportSize[0] + viewportSize[2] * 3 / 8;
        GLint redQuadRightSideX  = viewportSize[0] + viewportSize[2] * 5 / 8;
        GLint redQuadTopSideY    = viewportSize[1] + viewportSize[3] * 3 / 8;
        GLint redQuadBottomSideY = viewportSize[1] + viewportSize[3] * 5 / 8;

        // The midpoint of the viewport should be red.
        checkPixel(centerViewportX, centerViewportY, true);

        // Pixels just inside the red quad should be red.
        checkPixel(redQuadLeftSideX, redQuadTopSideY, true);
        checkPixel(redQuadLeftSideX, redQuadBottomSideY - 1, true);
        checkPixel(redQuadRightSideX - 1, redQuadTopSideY, true);
        checkPixel(redQuadRightSideX - 1, redQuadBottomSideY - 1, true);

        // Pixels just outside the red quad shouldn't be red.
        checkPixel(redQuadLeftSideX - 1, redQuadTopSideY - 1, false);
        checkPixel(redQuadLeftSideX - 1, redQuadBottomSideY, false);
        checkPixel(redQuadRightSideX, redQuadTopSideY - 1, false);
        checkPixel(redQuadRightSideX, redQuadBottomSideY, false);

        // Pixels just within the viewport shouldn't be red.
        checkPixel(viewportSize[0], viewportSize[1], false);
        checkPixel(viewportSize[0], viewportSize[1] + viewportSize[3] - 1, false);
        checkPixel(viewportSize[0] + viewportSize[2] - 1, viewportSize[1], false);
        checkPixel(viewportSize[0] + viewportSize[2] - 1, viewportSize[1] + viewportSize[3] - 1,
                   false);
    }

    void checkPixel(GLint x, GLint y, GLboolean renderedRed)
    {
        // By default, expect the pixel to be black.
        GLint expectedRedChannel   = 0;
        GLint expectedGreenChannel = 0;

        GLint scissorSize[4];
        glGetIntegerv(GL_SCISSOR_BOX, scissorSize);

        EXPECT_GL_NO_ERROR();

        if (scissorSize[0] <= x && x < scissorSize[0] + scissorSize[2] && scissorSize[1] <= y &&
            y < scissorSize[1] + scissorSize[3])
        {
            // If the pixel lies within the scissor rect, then it should have been cleared to green.
            // If we rendered a red square on top of it, then the pixel should be red (the green
            // channel will have been reset to 0).
            expectedRedChannel   = renderedRed ? 255 : 0;
            expectedGreenChannel = renderedRed ? 0 : 255;
        }

        // If the pixel is within the bounds of the window, then we check it. Otherwise we skip it.
        if (0 <= x && x < getWindowWidth() && 0 <= y && y < getWindowHeight())
        {
            EXPECT_PIXEL_EQ(x, y, expectedRedChannel, expectedGreenChannel, 0, 255);
        }
    }

    void testSetUp() override
    {
        mProgram = CompileProgram(essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
        if (mProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        glUseProgram(mProgram);

        glClearColor(0, 0, 0, 1);
        glClearDepthf(0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Call glViewport and glScissor with default parameters.
        glScissor(0, 0, getWindowWidth(), getWindowHeight());
        glViewport(0, 0, getWindowWidth(), getWindowHeight());

        glDisable(GL_DEPTH_TEST);
    }

    void testTearDown() override { glDeleteProgram(mProgram); }

    GLuint mProgram;
};

TEST_P(ViewportTest, QuarterWindow)
{
    glViewport(0, 0, getWindowWidth() / 4, getWindowHeight() / 4);

    runNonScissoredTest();

    runScissoredTest();
}

TEST_P(ViewportTest, QuarterWindowCentered)
{
    glViewport(getWindowWidth() * 3 / 8, getWindowHeight() * 3 / 8, getWindowWidth() / 4,
               getWindowHeight() / 4);

    runNonScissoredTest();

    runScissoredTest();
}

TEST_P(ViewportTest, FullWindow)
{
    glViewport(0, 0, getWindowWidth(), getWindowHeight());

    runNonScissoredTest();

    runScissoredTest();
}

TEST_P(ViewportTest, FullWindowOffCenter)
{
    glViewport(-getWindowWidth() / 2, getWindowHeight() / 2, getWindowWidth(), getWindowHeight());

    runNonScissoredTest();

    runScissoredTest();
}

TEST_P(ViewportTest, DoubleWindow)
{
    glViewport(0, 0, getWindowWidth() * 2, getWindowHeight() * 2);

    runNonScissoredTest();

    runScissoredTest();
}

TEST_P(ViewportTest, DoubleWindowCentered)
{
    glViewport(-getWindowWidth() / 2, -getWindowHeight() / 2, getWindowWidth() * 2,
               getWindowHeight() * 2);

    runNonScissoredTest();

    runScissoredTest();
}

TEST_P(ViewportTest, DoubleWindowOffCenter)
{
    glViewport(-getWindowWidth() * 3 / 4, getWindowHeight() * 3 / 4, getWindowWidth(),
               getWindowHeight());

    runNonScissoredTest();

    runScissoredTest();
}

TEST_P(ViewportTest, TripleWindow)
{
    glViewport(0, 0, getWindowWidth() * 3, getWindowHeight() * 3);

    runNonScissoredTest();

    runScissoredTest();
}

TEST_P(ViewportTest, TripleWindowCentered)
{
    glViewport(-getWindowWidth(), -getWindowHeight(), getWindowWidth() * 3, getWindowHeight() * 3);

    runNonScissoredTest();

    runScissoredTest();
}

TEST_P(ViewportTest, TripleWindowOffCenter)
{
    glViewport(-getWindowWidth() * 3 / 2, -getWindowHeight() * 3 / 2, getWindowWidth() * 3,
               getWindowHeight() * 3);

    runNonScissoredTest();

    runScissoredTest();
}

// Test line rendering with a non-standard viewport.
TEST_P(ViewportTest, DrawLineWithViewport)
{
    // We assume in the test the width and height are equal and we are tracing
    // the line from bottom left to top right. Verify that all pixels along that line
    // have been traced with green.
    ASSERT_EQ(getWindowWidth(), getWindowHeight());

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
    glUseProgram(program);

    std::vector<Vector3> vertices = {{-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};

    const GLint positionLocation = glGetAttribLocation(program, essl1_shaders::PositionAttrib());
    ASSERT_NE(-1, positionLocation);

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(positionLocation);

    // Set the viewport.
    GLint quarterWidth  = getWindowWidth() / 4;
    GLint quarterHeight = getWindowHeight() / 4;
    glViewport(quarterWidth, quarterHeight, quarterWidth, quarterHeight);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size()));

    glDisableVertexAttribArray(positionLocation);

    ASSERT_GL_NO_ERROR();

    for (GLint x = quarterWidth; x < getWindowWidth() / 2; x++)
    {
        EXPECT_PIXEL_COLOR_EQ(x, x, GLColor::green);
    }
}

// Test line rendering with an overly large viewport.
TEST_P(ViewportTest, DrawLineWithLargeViewport)
{
    // We assume in the test the width and height are equal and we are tracing
    // the line from bottom left to top right. Verify that all pixels along that line
    // have been traced with green.
    ASSERT_EQ(getWindowWidth(), getWindowHeight());

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
    glUseProgram(program);

    std::vector<Vector3> vertices = {{-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};

    const GLint positionLocation = glGetAttribLocation(program, essl1_shaders::PositionAttrib());
    ASSERT_NE(-1, positionLocation);

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(positionLocation);

    // Set the viewport.
    glViewport(0, 0, getWindowWidth() * 2, getWindowHeight() * 2);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size()));

    glDisableVertexAttribArray(positionLocation);

    ASSERT_GL_NO_ERROR();

    for (GLint x = 0; x < getWindowWidth(); x++)
    {
        EXPECT_PIXEL_COLOR_EQ(x, x, GLColor::green);
    }
}

// Test very large viewport sizes so sanitizers can verify there is no undefined behaviour
TEST_P(ViewportTest, Overflow)
{
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
    glUseProgram(program);

    std::vector<Vector3> vertices = {{-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}};

    const GLint positionLocation = glGetAttribLocation(program, essl1_shaders::PositionAttrib());
    ASSERT_NE(-1, positionLocation);

    GLBuffer vertexBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), vertices.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(positionLocation);

    constexpr int kMaxSize = std::numeric_limits<int>::max();
    constexpr std::array<std::array<int, 4>, 4> kTestViewportSizes = {{
        {
            kMaxSize,
            kMaxSize,
            1,
            1,
        },
        {
            0,
            0,
            kMaxSize,
            kMaxSize,
        },
        {
            1,
            1,
            kMaxSize,
            kMaxSize,
        },
        {
            kMaxSize,
            kMaxSize,
            kMaxSize,
            kMaxSize,
        },
    }};

    for (const auto &viewportSize : kTestViewportSizes)
    {
        // Set the viewport.
        glViewport(viewportSize[0], viewportSize[1], viewportSize[2], viewportSize[3]);

        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size()));

        glDisableVertexAttribArray(positionLocation);

        ASSERT_GL_NO_ERROR();
    }
}

// Test that viewport dimensions are clamped to implementation-defined maximums
TEST_P(ViewportTest, ClampOnStore)
{
    GLint maxDims[2];
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, maxDims);
    ASSERT_GT(maxDims[0], 0);
    ASSERT_GT(maxDims[1], 0);

    GLint viewport[4];
    glViewport(0, 0, 1, 1);
    ASSERT_GL_NO_ERROR();
    glGetIntegerv(GL_VIEWPORT, viewport);
    ASSERT_EQ(viewport[0], 0);
    ASSERT_EQ(viewport[1], 0);
    ASSERT_EQ(viewport[2], 1);
    ASSERT_EQ(viewport[3], 1);

    glViewport(0, 0, -2, 2);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);
    glGetIntegerv(GL_VIEWPORT, viewport);
    ASSERT_EQ(viewport[0], 0);
    ASSERT_EQ(viewport[1], 0);
    ASSERT_EQ(viewport[2], 1);
    ASSERT_EQ(viewport[3], 1);

    glViewport(0, 0, 3, -3);
    ASSERT_GL_ERROR(GL_INVALID_VALUE);
    glGetIntegerv(GL_VIEWPORT, viewport);
    ASSERT_EQ(viewport[0], 0);
    ASSERT_EQ(viewport[1], 0);
    ASSERT_EQ(viewport[2], 1);
    ASSERT_EQ(viewport[3], 1);

    glViewport(0, 0, maxDims[0], maxDims[1]);
    ASSERT_GL_NO_ERROR();
    glGetIntegerv(GL_VIEWPORT, viewport);
    ASSERT_EQ(viewport[0], 0);
    ASSERT_EQ(viewport[1], 0);
    ASSERT_EQ(viewport[2], maxDims[0]);
    ASSERT_EQ(viewport[3], maxDims[1]);

    glViewport(0, 0, maxDims[0] + 1, maxDims[1] + 1);
    ASSERT_GL_NO_ERROR();
    glGetIntegerv(GL_VIEWPORT, viewport);
    ASSERT_EQ(viewport[0], 0);
    ASSERT_EQ(viewport[1], 0);
    ASSERT_EQ(viewport[2], maxDims[0]);
    ASSERT_EQ(viewport[3], maxDims[1]);
}

// Tests that a reversed depth range stays in effect after the viewport or the framebuffer changes.
TEST_P(ViewportTest, ReversedDepthRangeAfterViewportAndFramebufferChanges)
{
    constexpr char kFS[] = R"(precision highp float;
void main()
{
    gl_FragColor = vec4(gl_FragCoord.z, 0.0, 0.0, 1.0);
})";
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);

    // The window depth is n + (f - n) * (z + 1) / 2, also when n > f: 0.375 for z = 0.5.
    glDepthRangef(0.75f, 0.25f);
    constexpr float kExpectedRed = 0.375f * 255.0f;
    // Draw before changing the viewport so that the depth range change is not pending anymore.
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(getWindowWidth() / 2, getWindowHeight() / 2, kExpectedRed, 0, 0, 255, 1);

    glViewport(0, 0, getWindowWidth() / 2, getWindowHeight());
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(getWindowWidth() / 4, getWindowHeight() / 2, kExpectedRed, 0, 0, 255, 1);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(0, 0, kExpectedRed, 0, 0, 255, 1);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND(ViewportTest, ES2_D3D11_PRESENT_PATH_FAST());

// This test suite is not instantiated on some OSes.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ViewportTest);

struct Rect
{
    bool contains(int px, int py) const
    {
        return px >= x && px < x + width && py >= y && py < y + height;
    }

    int x;
    int y;
    int width;
    int height;
};

// Tests viewports that do not cover the whole framebuffer, on the default framebuffer and on a
// framebuffer object.
class ViewportOffsetTest : public ANGLETest<>
{
  protected:
    static constexpr int kSize = 32;

    ViewportOffsetTest()
    {
        setWindowWidth(kSize);
        setWindowHeight(kSize);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testSetUp() override
    {
        mProgram = CompileProgram(essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
        ASSERT_NE(0u, mProgram);
        mColorLocation = glGetUniformLocation(mProgram, essl1_shaders::ColorUniform());
        ASSERT_NE(-1, mColorLocation);
    }

    void testTearDown() override { glDeleteProgram(mProgram); }

    // Binds the default framebuffer, or a framebuffer object with an RGBA8 color texture.
    void bindFramebuffer(bool useFramebufferObject)
    {
        if (!useFramebufferObject)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return;
        }
        glBindTexture(GL_TEXTURE_2D, mColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColorTexture,
                               0);
        ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
    }

    void clear()
    {
        glViewport(0, 0, kSize, kSize);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    // Draws a quad that covers the clip space coordinates [-scale, scale].
    void drawColorQuad(const GLColor &color, float scale)
    {
        glUseProgram(mProgram);
        const angle::Vector4 colorF = color.toNormalizedVector();
        glUniform4f(mColorLocation, colorF[0], colorF[1], colorF[2], colorF[3]);
        drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.0f, scale, true);
    }

    GLuint mProgram      = 0;
    GLint mColorLocation = -1;
    GLTexture mColorTexture;
    GLFramebuffer mFramebuffer;
};

// Tests drawing into a grid of viewports that tile the framebuffer.
TEST_P(ViewportOffsetTest, Tiles)
{
    constexpr int kTileSize = kSize / 4;
    auto tileColor          = [](int tileX, int tileY) {
        return GLColor(static_cast<GLubyte>(32 + tileX * 64), static_cast<GLubyte>(32 + tileY * 64),
                       128, 255);
    };
    for (bool useFramebufferObject : {false, true})
    {
        bindFramebuffer(useFramebufferObject);
        clear();
        for (int tileY = 0; tileY < 4; ++tileY)
        {
            for (int tileX = 0; tileX < 4; ++tileX)
            {
                glViewport(tileX * kTileSize, tileY * kTileSize, kTileSize, kTileSize);
                drawColorQuad(tileColor(tileX, tileY), 1.0f);
            }
        }
        ASSERT_GL_NO_ERROR();
        for (int tileY = 0; tileY < 4; ++tileY)
        {
            for (int tileX = 0; tileX < 4; ++tileX)
            {
                SCOPED_TRACE(testing::Message() << "framebuffer object: " << useFramebufferObject
                                                << ", tile: (" << tileX << ", " << tileY << ")");
                EXPECT_PIXEL_RECT_EQ(tileX * kTileSize, tileY * kTileSize, kTileSize, kTileSize,
                                     tileColor(tileX, tileY));
            }
        }
    }
}

// Tests viewports that extend past the edges of the framebuffer. The viewport maps clip space to
// window coordinates; only the rasterization is limited to the framebuffer.
TEST_P(ViewportOffsetTest, PartiallyOutsideFramebuffer)
{
    struct Case
    {
        Rect viewport;
        // The window coordinates of the quad that covers the clip space coordinates [-0.5, 0.5],
        // clipped to the framebuffer.
        Rect expectedQuad;
    };
    constexpr int kHalf    = kSize / 2;
    constexpr int kQuarter = kSize / 4;
    const Case kCases[]    = {
        {{kHalf, kHalf, kSize, kSize}, {kHalf + kQuarter, kHalf + kQuarter, kQuarter, kQuarter}},
        {{-kHalf, -kHalf, kSize, kSize}, {0, 0, kQuarter, kQuarter}},
        {{0, kHalf, kSize, kSize}, {kQuarter, kHalf + kQuarter, kHalf, kQuarter}},
        {{kHalf, 0, kSize, kSize}, {kHalf + kQuarter, kQuarter, kQuarter, kHalf}},
        {{0, -kHalf, kSize, kSize}, {kQuarter, 0, kHalf, kQuarter}},
        {{-kHalf, 0, kSize, kSize}, {0, kQuarter, kQuarter, kHalf}},
        {{-kSize, -kSize, kSize * 4, kSize * 4}, {0, 0, kSize, kSize}},
    };
    for (bool useFramebufferObject : {false, true})
    {
        bindFramebuffer(useFramebufferObject);
        for (const Case &testCase : kCases)
        {
            clear();
            glViewport(testCase.viewport.x, testCase.viewport.y, testCase.viewport.width,
                       testCase.viewport.height);
            drawColorQuad(GLColor::green, 0.5f);
            ASSERT_GL_NO_ERROR();

            SCOPED_TRACE(testing::Message()
                         << "framebuffer object: " << useFramebufferObject << ", viewport: ("
                         << testCase.viewport.x << ", " << testCase.viewport.y << ", "
                         << testCase.viewport.width << ", " << testCase.viewport.height << ")");
            std::vector<GLColor> pixels(kSize * kSize);
            glReadPixels(0, 0, kSize, kSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            int wrongPixels = 0;
            for (int y = 0; y < kSize; ++y)
            {
                for (int x = 0; x < kSize; ++x)
                {
                    const GLColor &expected =
                        testCase.expectedQuad.contains(x, y) ? GLColor::green : GLColor::black;
                    if (pixels[y * kSize + x] != expected && wrongPixels++ == 0)
                    {
                        ADD_FAILURE() << "first wrong pixel: (" << x << ", " << y
                                      << "), expected " << expected << ", actual "
                                      << pixels[y * kSize + x];
                    }
                }
            }
            EXPECT_EQ(0, wrongPixels);
        }
    }
}

// Tests viewports that do not intersect the framebuffer: nothing is drawn.
TEST_P(ViewportOffsetTest, OutsideFramebuffer)
{
    const Rect kViewports[] = {
        {kSize, 0, kSize, kSize},
        {0, kSize, kSize, kSize},
        {-kSize, 0, kSize, kSize},
        {0, -kSize, kSize, kSize},
    };
    for (bool useFramebufferObject : {false, true})
    {
        bindFramebuffer(useFramebufferObject);
        for (const Rect &viewport : kViewports)
        {
            clear();
            glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
            drawColorQuad(GLColor::green, 1.0f);
            ASSERT_GL_NO_ERROR();
            SCOPED_TRACE(testing::Message()
                         << "framebuffer object: " << useFramebufferObject << ", viewport: ("
                         << viewport.x << ", " << viewport.y << ", " << viewport.width << ", "
                         << viewport.height << ")");
            EXPECT_PIXEL_RECT_EQ(0, 0, kSize, kSize, GLColor::black);
        }
    }
}

// Tests that gl_FragCoord.xy are the window coordinates of the fragment, with the origin at the
// lower left corner, whatever the viewport.
TEST_P(ViewportOffsetTest, FragCoord)
{
    constexpr char kFS[] = R"(precision highp float;
void main()
{
    gl_FragColor = vec4(gl_FragCoord.xy / 32.0, 0.0, 1.0);
})";
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), kFS);
    static_assert(kSize == 32);

    const Rect kViewports[] = {
        {0, 0, kSize, kSize},
        {kSize / 4, kSize / 8, kSize / 2, kSize / 2},
        {kSize / 2, kSize / 2, kSize, kSize},
    };
    for (bool useFramebufferObject : {false, true})
    {
        bindFramebuffer(useFramebufferObject);
        for (const Rect &viewport : kViewports)
        {
            clear();
            glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
            drawQuad(program, essl1_shaders::PositionAttrib(), 0.0f, 1.0f, true);
            ASSERT_GL_NO_ERROR();

            SCOPED_TRACE(testing::Message()
                         << "framebuffer object: " << useFramebufferObject << ", viewport: ("
                         << viewport.x << ", " << viewport.y << ", " << viewport.width << ", "
                         << viewport.height << ")");
            std::vector<GLColor> pixels(kSize * kSize);
            glReadPixels(0, 0, kSize, kSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            int wrongPixels = 0;
            for (int y = 0; y < kSize; ++y)
            {
                for (int x = 0; x < kSize; ++x)
                {
                    GLColor expected = GLColor::black;
                    if (viewport.contains(x, y))
                    {
                        expected.R = static_cast<GLubyte>(std::round((x + 0.5f) / 32.0f * 255.0f));
                        expected.G = static_cast<GLubyte>(std::round((y + 0.5f) / 32.0f * 255.0f));
                    }
                    const GLColor &actual = pixels[y * kSize + x];
                    const bool near       = std::abs(actual.R - expected.R) <= 1 &&
                                      std::abs(actual.G - expected.G) <= 1 &&
                                      actual.B == expected.B && actual.A == expected.A;
                    if (!near && wrongPixels++ == 0)
                    {
                        ADD_FAILURE() << "first wrong pixel: (" << x << ", " << y
                                      << "), expected " << expected << ", actual " << actual;
                    }
                }
            }
            EXPECT_EQ(0, wrongPixels);
        }
    }
}

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(ViewportOffsetTest);

}  // namespace
