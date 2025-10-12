/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Igalia, S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Test.h"

#if ENABLE(WEBGL)
#include "GraphicsTestUtilities.h"
#include "WebCoreTestUtilities.h"
#include <WebCore/Color.h>
#include <WebCore/GraphicsContextGLTextureMapperANGLE.h>
#include <WebCore/PlatformDisplaySurfaceless.h>
#include <WebCore/ProcessIdentity.h>
#include <atomic>
#include <limits>
#include <optional>
#include <wtf/HashSet.h>
#include <wtf/MemoryFootprint.h>
#include <wtf/StdLibExtras.h>

namespace TestWebKitAPI {

using namespace WebCore;

namespace {

static void initializePlatformDisplayIfNeeded()
{
    if (PlatformDisplay::sharedDisplayIfExists())
        return;
    auto display = PlatformDisplaySurfaceless::create();
    RELEASE_ASSERT(display);
    PlatformDisplay::setSharedDisplay(WTFMove(display));
}

using TestedGraphicsContextGLTextureMapper = GraphicsContextGLTextureMapperANGLE;

static RefPtr<TestedGraphicsContextGLTextureMapper> createTestedGraphicsContextGL(GraphicsContextGLAttributes attribute)
{
    initializePlatformDisplayIfNeeded();
    return TestedGraphicsContextGLTextureMapper::create(WTFMove(attribute));
}

class MockGraphicsContextGLClient final : public GraphicsContextGL::Client {
public:
    void forceContextLost() final { ++m_contextLostCalls; }
    void addDebugMessage(GCGLenum, GCGLenum, GCGLenum, const CString&) final { }
    int contextLostCalls() { return m_contextLostCalls; }
private:
    int m_contextLostCalls { 0 };
};

class GraphicsContextGLTextureMapperTest : public ::testing::Test {
protected:
    void SetUp() override // NOLINT
    {
        m_scopedProcessType = ScopedSetAuxiliaryProcessTypeForTesting { WTF::AuxiliaryProcessType::GPU };
    }
    void TearDown() override // NOLINT
    {
        m_scopedProcessType = std::nullopt;
    }
private:
    std::optional<ScopedSetAuxiliaryProcessTypeForTesting> m_scopedProcessType;
};

class AnyContextAttributeTest : public testing::TestWithParam<std::tuple<bool, bool, bool>> {
protected:
    bool antialias() const { return std::get<0>(GetParam()); }
    bool preserveDrawingBuffer() const { return std::get<1>(GetParam()); }
    bool isWebGL2() const { return std::get<2>(GetParam()); }
    GraphicsContextGLAttributes attributes();
    RefPtr<TestedGraphicsContextGLTextureMapper> createTestContext(IntSize contextSize);

    void SetUp() override // NOLINT
    {
        m_scopedProcessType = ScopedSetAuxiliaryProcessTypeForTesting { WTF::AuxiliaryProcessType::GPU };
    }
    void TearDown() override // NOLINT
    {
        m_scopedProcessType = std::nullopt;
    }

private:
    std::optional<ScopedSetAuxiliaryProcessTypeForTesting> m_scopedProcessType;
};

GraphicsContextGLAttributes AnyContextAttributeTest::attributes()
{
    GraphicsContextGLAttributes attributes;
    attributes.isWebGL2 = isWebGL2();
    attributes.antialias = antialias();
    attributes.depth = false;
    attributes.stencil = false;
    attributes.alpha = true;
    attributes.preserveDrawingBuffer = preserveDrawingBuffer();
    return attributes;
}

RefPtr<TestedGraphicsContextGLTextureMapper> AnyContextAttributeTest::createTestContext(IntSize contextSize)
{
    auto context = createTestedGraphicsContextGL(attributes());
    if (!context)
        return nullptr;
    context->reshape(contextSize.width(), contextSize.height());
    return context;
}

} // namespace

#if ENABLE(WEBXR)
static ::testing::AssertionResult checkReadPixel(GraphicsContextGL& context, IntPoint point, Color expected)
{
    uint8_t gotValues[4] = { };
    context.readPixels({ point, { 1, 1 } }, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, gotValues, 1, 0, false);
    Color got { SRGBA<uint8_t> { gotValues[0], gotValues[1], gotValues[2], gotValues[3] } };
    if (got != expected)
        return ::testing::AssertionFailure() << "Got: " << got << ", expected: " << expected << ".";
    return ::testing::AssertionSuccess();

}
#endif // ENABLE(WEBXR)

TEST_F(GraphicsContextGLTextureMapperTest, ClearBufferIncorrectSizes)
{
    using GL = GraphicsContextGL;
    GraphicsContextGLAttributes attributes;
    attributes.isWebGL2 = true;
    attributes.depth = true;
    attributes.stencil = true;
    auto gl = createTestedGraphicsContextGL(attributes);
    gl->reshape(1, 1);

    float floats5[5] { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f };
    float floats4[4] { 0.1f, 0.2f, 0.3f, 0.4f };
    float floats3[3] { 0.1f, 0.2f, 0.3f };
    float floats2[2] { 0.1f, 0.2f };
    float floats1[1] { 0.1f };
    std::span<const float> floats0;

    gl->clearBufferfv(GL::COLOR, 0, floats4);
    EXPECT_TRUE(gl->getErrors().isEmpty());

    gl->clearBufferfv(GL::COLOR, 0, floats5);
    EXPECT_TRUE(gl->getErrors().contains(GCGLErrorCode::InvalidOperation));
    EXPECT_TRUE(gl->getErrors().isEmpty());

    gl->clearBufferfv(GL::COLOR, 0, floats3);
    EXPECT_TRUE(gl->getErrors().contains(GCGLErrorCode::InvalidOperation));
    EXPECT_TRUE(gl->getErrors().isEmpty());

    gl->clearBufferfv(GL::COLOR, 0, floats2);
    EXPECT_TRUE(gl->getErrors().contains(GCGLErrorCode::InvalidOperation));
    EXPECT_TRUE(gl->getErrors().isEmpty());

    gl->clearBufferfv(GL::COLOR, 0, floats1);
    EXPECT_TRUE(gl->getErrors().contains(GCGLErrorCode::InvalidOperation));
    EXPECT_TRUE(gl->getErrors().isEmpty());

    gl->clearBufferfv(GL::COLOR, 0, floats0);
    EXPECT_TRUE(gl->getErrors().contains(GCGLErrorCode::InvalidOperation));
    EXPECT_TRUE(gl->getErrors().isEmpty());

    gl->clearBufferfv(GL::DEPTH, 0, floats1);
    EXPECT_TRUE(gl->getErrors().isEmpty());

    gl->clearBufferfv(GL::DEPTH, 0, floats4);
    EXPECT_TRUE(gl->getErrors().contains(GCGLErrorCode::InvalidOperation));
    EXPECT_TRUE(gl->getErrors().isEmpty());

    int ints2[2] { 1, 2 };
    int ints1[1] { 1 };

    gl->clearBufferiv(GL::STENCIL, 0, ints1);
    EXPECT_TRUE(gl->getErrors().isEmpty());

    gl->clearBufferiv(GL::STENCIL, 0, ints2);
    EXPECT_TRUE(gl->getErrors().contains(GCGLErrorCode::InvalidOperation));
    EXPECT_TRUE(gl->getErrors().isEmpty());

    auto texture = gl->createTexture();
    gl->bindTexture(GL::TEXTURE_2D, texture);
    gl->texParameteri(GL::TEXTURE_2D, GL::TEXTURE_MIN_FILTER, GL::NEAREST);
    gl->texImage2D(GL::TEXTURE_2D, 0, GL::R8UI, 1, 1, 0, GL::RED_INTEGER, GL::UNSIGNED_BYTE, 0);
    ASSERT_TRUE(gl->getErrors().isEmpty());

    auto fbo = gl->createFramebuffer();
    gl->bindFramebuffer(GL::FRAMEBUFFER, fbo);
    gl->framebufferTexture2D(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::TEXTURE_2D, texture, 0);
    ASSERT_EQ(gl->checkFramebufferStatus(GL::FRAMEBUFFER), GL::FRAMEBUFFER_COMPLETE);

    unsigned uints4[4] { 1, 2, 3, 4 };
    unsigned uints2[2] { 1, 2 };
    unsigned uints1[1] { 1 };

    gl->clearBufferuiv(GL::COLOR, 0, uints4);
    EXPECT_TRUE(gl->getErrors().isEmpty());

    gl->clearBufferuiv(GL::COLOR, 0, uints2);
    EXPECT_TRUE(gl->getErrors().contains(GCGLErrorCode::InvalidOperation));
    EXPECT_TRUE(gl->getErrors().isEmpty());

    gl->clearBufferuiv(GL::COLOR, 0, uints1);
    EXPECT_TRUE(gl->getErrors().contains(GCGLErrorCode::InvalidOperation));
    EXPECT_TRUE(gl->getErrors().isEmpty());

    gl = nullptr;
}

// Test destroying graphics contexts so that the underlying current OpenGL context is different
// than the underlying OpenGL context of destroyed context.
TEST_F(GraphicsContextGLTextureMapperTest, DestroyWithoutMakingCurrent)
{
    GraphicsContextGLAttributes attributes;
    attributes.isWebGL2 = true;
    attributes.depth = true;
    attributes.stencil = true;
    RefPtr gl1 = createTestedGraphicsContextGL(attributes);
    gl1->reshape(1, 1);
    RefPtr gl2 = createTestedGraphicsContextGL(attributes);
    gl2->reshape(1, 1);
    RefPtr gl3 = createTestedGraphicsContextGL(attributes);
    gl3->reshape(1, 1);
    // Current context is now 3.
    gl1 = nullptr; // Test the case where we destroy with other context being current.
    // Current context is now nullptr.
    gl2 = nullptr; // Test the case where we destroy without context being current.
}

TEST_F(GraphicsContextGLTextureMapperTest, TwoLinks)
{
    GraphicsContextGLAttributes attributes;
    auto gl = createTestedGraphicsContextGL(attributes);
    auto vs = gl->createShader(GraphicsContextGL::VERTEX_SHADER);
    gl->shaderSource(vs, "void main() { }"_s);
    gl->compileShader(vs);
    auto fs = gl->createShader(GraphicsContextGL::FRAGMENT_SHADER);
    gl->shaderSource(fs, "void main() { }"_s);
    gl->compileShader(fs);
    auto program = gl->createProgram();
    gl->attachShader(program, vs);
    gl->attachShader(program, fs);
    gl->linkProgram(program);
    gl->useProgram(program);
    gl->linkProgram(program);
    EXPECT_TRUE(gl->getErrors().isEmpty());
    gl = nullptr;
}

TEST_F(GraphicsContextGLTextureMapperTest, BufferAsImageNoDrawingBufferReturnsNullptr)
{
    using GL = GraphicsContextGL;
    auto gl = createTestedGraphicsContextGL({ });
    RefPtr drawingImage = gl->bufferAsNativeImage(GL::SurfaceBuffer::DrawingBuffer);
    RefPtr displayImage = gl->bufferAsNativeImage(GL::SurfaceBuffer::DisplayBuffer);
    EXPECT_EQ(drawingImage, nullptr);
    EXPECT_EQ(displayImage, nullptr);
}

// Test copying images and mutating the drawing buffer.
// The mutations should only be visible in the new buffers, and not the old ones.
TEST_F(GraphicsContextGLTextureMapperTest, CopyImageAndMutateDrawingBuffer)
{
    using GL = GraphicsContextGL;
    auto gl = createTestedGraphicsContextGL({ });
    gl->reshape(10, 10);
    RefPtr drawingImage0 = gl->bufferAsNativeImage(GL::SurfaceBuffer::DrawingBuffer);
    ASSERT_NE(drawingImage0, nullptr);
    EXPECT_TRUE(imagePixelIs(Color::transparentBlack, *drawingImage0, FloatPoint(5, 5)));
    gl->clearColor(0.f, 1.f, 0.f, 1.f);
    gl->clear(GL::COLOR_BUFFER_BIT);
    RefPtr drawingImage1 = gl->bufferAsNativeImage(GL::SurfaceBuffer::DrawingBuffer);
    ASSERT_NE(drawingImage1, nullptr);
    EXPECT_TRUE(imagePixelIs(Color::transparentBlack, *drawingImage0, FloatPoint(5, 5)));
    EXPECT_TRUE(imagePixelIs(Color::green, *drawingImage1, FloatPoint(5, 5)));

    gl->clearColor(0.f, 0.f, 1.f, 1.f);
    gl->clear(GL::COLOR_BUFFER_BIT);
    EXPECT_TRUE(imagePixelIs(Color::transparentBlack, *drawingImage0, FloatPoint(5, 5)));
    EXPECT_TRUE(imagePixelIs(Color::green, *drawingImage1, FloatPoint(5, 5)));
    RefPtr drawingImage2 = gl->bufferAsNativeImage(GL::SurfaceBuffer::DrawingBuffer);
    ASSERT_NE(drawingImage2, nullptr);
    EXPECT_TRUE(imagePixelIs(Color::transparentBlack, *drawingImage0, FloatPoint(5, 5)));
    EXPECT_TRUE(imagePixelIs(Color::green, *drawingImage1, FloatPoint(5, 5)));
    EXPECT_TRUE(imagePixelIs(Color::blue, *drawingImage2, FloatPoint(5, 5)));
    gl->prepareForDisplay();
    RefPtr displayImage = gl->bufferAsNativeImage(GL::SurfaceBuffer::DisplayBuffer);
    ASSERT_NE(displayImage, nullptr);
    EXPECT_TRUE(imagePixelIs(Color::transparentBlack, *drawingImage0, FloatPoint(5, 5)));
    EXPECT_TRUE(imagePixelIs(Color::green, *drawingImage1, FloatPoint(5, 5)));
    EXPECT_TRUE(imagePixelIs(Color::blue, *drawingImage2, FloatPoint(5, 5)));
    EXPECT_TRUE(imagePixelIs(Color::blue, *displayImage, FloatPoint(5, 5)));
}

#if ENABLE(WEBXR)

// Render to RGBA+depth MSAA renderbuffers.
// Resolve to RGBA+depth renderbuffers.
// Copy two halves to individual BGRA_EXT+depth renderbuffers.
// Tests that we can call BlitFramebuffer with (0,0 WxH) -> (0, 0, WxH) as well as (x1,y1 WxH) -> (0,0 WxH) rects.
// Some BlitFramebuffer variants had limitations for this.
TEST_P(AnyContextAttributeTest, WebXRBlitTest)
{
    using GL = GraphicsContextGL;
    MockGraphicsContextGLClient client;
    auto gl = createTestContext({ 2, 2 });
    ASSERT_NE(gl, nullptr);
    gl->setClient(&client);

    gl->enableRequiredWebXRExtensions();
    int maxSamples = 0;
    gl->getIntegerv(GL::MAX_SAMPLES, singleElementSpan(maxSamples));
    ASSERT_GT(maxSamples, 0);
    PlatformGLObject fbo = gl->createFramebuffer();
    gl->bindFramebuffer(GL::FRAMEBUFFER, fbo);
    {
        PlatformGLObject color = gl->createRenderbuffer();
        ASSERT_NE(color, 0u);
        gl->bindRenderbuffer(GL::RENDERBUFFER, color);
        gl->renderbufferStorageMultisampleANGLE(GL::RENDERBUFFER, maxSamples, GL::RGBA8, 4, 4);
        gl->framebufferRenderbuffer(GL::FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::RENDERBUFFER, color);
    }
    {
        PlatformGLObject depth = gl->createRenderbuffer();
        ASSERT_NE(depth, 0u);
        gl->bindRenderbuffer(GL::RENDERBUFFER, depth);
        gl->renderbufferStorageMultisampleANGLE(GL::RENDERBUFFER, maxSamples, GL::DEPTH24_STENCIL8, 4, 4);
        gl->framebufferRenderbuffer(GL::FRAMEBUFFER, GL::DEPTH_STENCIL_ATTACHMENT, GL::RENDERBUFFER, depth);
    }
    // Simulated draw: left blue, right green.
    {
        gl->enable(GL::SCISSOR_TEST);
        gl->scissor(0, 0, 2, 2);
        gl->clearDepth(.1f);
        gl->clearColor(.0f, .0f, 1.f, 1.f);
        gl->clear(GL::COLOR_BUFFER_BIT | GL::DEPTH_BUFFER_BIT);
        gl->scissor(2, 2, 4, 4);
        gl->clearDepth(.2f);
        gl->clearColor(.0f, 1.f, .0f, 1.f);
        gl->clear(GL::COLOR_BUFFER_BIT | GL::DEPTH_BUFFER_BIT);
        gl->disable(GL::SCISSOR_TEST);
    }

    // Resolve MSAA to single sample.
    PlatformGLObject resolveFBO = gl->createFramebuffer();
    gl->bindFramebuffer(GL::DRAW_FRAMEBUFFER, resolveFBO);
    {
        PlatformGLObject color = gl->createRenderbuffer();
        ASSERT_NE(color, 0u);
        gl->bindRenderbuffer(GL::RENDERBUFFER, color);
        gl->renderbufferStorageMultisampleANGLE(GL::RENDERBUFFER, 0, GL::RGBA8, 4, 4);
        gl->framebufferRenderbuffer(GL::DRAW_FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::RENDERBUFFER, color);
    }
    {
        PlatformGLObject depth = gl->createRenderbuffer();
        ASSERT_NE(depth, 0u);
        gl->bindRenderbuffer(GL::RENDERBUFFER, depth);
        gl->renderbufferStorageMultisampleANGLE(GL::RENDERBUFFER, 0, GL::DEPTH24_STENCIL8, 4, 4);
        gl->framebufferRenderbuffer(GL::DRAW_FRAMEBUFFER, GL::DEPTH_STENCIL_ATTACHMENT, GL::RENDERBUFFER, depth);
    }

    gl->blitFramebuffer(0, 0, 4, 4, 0, 0, 4, 4, GL::COLOR_BUFFER_BIT | GL::DEPTH_BUFFER_BIT, GL::NEAREST);

    // Copy single sample to layer, ensure the contents.
    gl->bindFramebuffer(GL::READ_FRAMEBUFFER, resolveFBO);

    PlatformGLObject layerFBO = gl->createFramebuffer();
    gl->bindFramebuffer(GL::DRAW_FRAMEBUFFER, layerFBO);
    {
        PlatformGLObject color = gl->createRenderbuffer();
        ASSERT_NE(color, 0u);
        gl->bindRenderbuffer(GL::RENDERBUFFER, color);
        gl->renderbufferStorageMultisampleANGLE(GL::RENDERBUFFER, 0, GL::RGBA8, 2, 2);
        gl->framebufferRenderbuffer(GL::DRAW_FRAMEBUFFER, GL::COLOR_ATTACHMENT0, GL::RENDERBUFFER, color);
    }
    {
        PlatformGLObject depth = gl->createRenderbuffer();
        ASSERT_NE(depth, 0u);
        gl->bindRenderbuffer(GL::RENDERBUFFER, depth);
        gl->renderbufferStorageMultisampleANGLE(GL::RENDERBUFFER, 0, GL::DEPTH24_STENCIL8, 2, 2);
        gl->framebufferRenderbuffer(GL::DRAW_FRAMEBUFFER, GL::DEPTH_STENCIL_ATTACHMENT, GL::RENDERBUFFER, depth);
    }
    gl->blitFramebuffer(0, 0, 2, 2, 0, 0, 2, 2, GL::COLOR_BUFFER_BIT | GL::DEPTH_BUFFER_BIT, GL::NEAREST);
    gl->bindFramebuffer(GL::READ_FRAMEBUFFER, layerFBO);
    EXPECT_TRUE(checkReadPixel(*gl, { 0, 0 }, Color::blue));
    EXPECT_TRUE(checkReadPixel(*gl, { 1, 1 }, Color::blue));

    gl->bindFramebuffer(GL::READ_FRAMEBUFFER, resolveFBO);
    gl->bindFramebuffer(GL::DRAW_FRAMEBUFFER, layerFBO);
    gl->blitFramebuffer(2, 2, 4, 4, 0, 0, 2, 2, GL::COLOR_BUFFER_BIT | GL::DEPTH_BUFFER_BIT,  GL::NEAREST);
    gl->bindFramebuffer(GL::READ_FRAMEBUFFER, layerFBO);
    EXPECT_TRUE(checkReadPixel(*gl, { 0, 0 }, Color::green));
    EXPECT_TRUE(checkReadPixel(*gl, { 1, 1 }, Color::green));

    EXPECT_TRUE(gl->getErrors().isEmpty());
}
#endif // ENABLE(WEBXR)

INSTANTIATE_TEST_SUITE_P(GraphicsContextGLTextureMapperTest,
    AnyContextAttributeTest,
    testing::Combine(
        testing::Values(true, false),
        testing::Values(true, false),
        testing::Values(true, false)),
    TestParametersToStringFormatter());

class GraphicsContextGLTextureMapperReadPixelsTest : public ::testing::Test {
protected:
    void SetUp() override // NOLINT
    {
        GraphicsContextGLAttributes attributes;
        m_context = createTestedGraphicsContextGL(attributes);
        m_expectedColor = Color::gray;
        auto [r, g, b, a] = m_expectedColor.toColorTypeLossy<SRGBA<float>>().resolved();
        m_context->reshape(20, 20);
        m_context->clearColor(r, g, b, a);
        m_context->clear(GraphicsContextGL::COLOR_BUFFER_BIT);
    }

    RefPtr<TestedGraphicsContextGLTextureMapper> m_context { nullptr };
    Color m_expectedColor { };
};

TEST_F(GraphicsContextGLTextureMapperReadPixelsTest, readPixelsSuccess)
{
    EXPECT_TRUE(m_context->getErrors().isEmpty());
    uint8_t gotValues[4] = { 0, 0, 0, 0 };
    IntRect rect(1, 1, 1, 1);
    m_context->readPixels(rect, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, gotValues, 4, 0, false);
    Color actualColor { SRGBA<uint8_t> { gotValues[0], gotValues[1], gotValues[2], gotValues[3] } };
    EXPECT_EQ(m_expectedColor, actualColor);
    EXPECT_TRUE(m_context->getErrors().isEmpty());
}

TEST_F(GraphicsContextGLTextureMapperReadPixelsTest, readPixelsTooLargeRect)
{
    EXPECT_TRUE(m_context->getErrors().isEmpty());
    uint8_t gotValues[4] = { 0, 0, 0, 0 };
    IntRect rect(1, 1, 0x7fffffff, 0x7fffffff);
    m_context->readPixels(rect, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, gotValues, 4, 0, false);
    Color actualColor { SRGBA<uint8_t> { gotValues[0], gotValues[1], gotValues[2], gotValues[3] } };
    EXPECT_NE(m_expectedColor, actualColor);
    EXPECT_EQ(GCGLErrorCode::InvalidOperation, m_context->getErrors());
}

TEST_F(GraphicsContextGLTextureMapperReadPixelsTest, readPixelsWithStatusSuccess)
{
    uint8_t gotValues[4] = { 0, 0, 0, 0 };
    IntRect rect(1, 1, 1, 1);
    m_context->readPixelsWithStatus(rect, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, false, gotValues);
    Color actualColor { SRGBA<uint8_t> { gotValues[0], gotValues[1], gotValues[2], gotValues[3] } };
    EXPECT_EQ(m_expectedColor, actualColor);
    EXPECT_TRUE(m_context->getErrors().isEmpty());
}

TEST_F(GraphicsContextGLTextureMapperReadPixelsTest, readPixelsWithStatusTooLargeRect)
{
    uint8_t gotValues[4] = { 0, 0, 0, 0 };
    IntRect rect(1, 1, 0x7fffffff, 0x7fffffff);
    m_context->readPixelsWithStatus(rect, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, false, gotValues);
    Color actualColor { SRGBA<uint8_t> { gotValues[0], gotValues[1], gotValues[2], gotValues[3] } };
    EXPECT_NE(m_expectedColor, actualColor);
    EXPECT_EQ(GCGLErrorCode::InvalidOperation, m_context->getErrors());
}

class GraphicsContextGLTextureMapperReshapeTest : public ::testing::Test {
protected:
    static constexpr int INITIAL_WIDTH = 20;
    static constexpr int INITIAL_HEIGHT = 20;

    void SetUp() override // NOLINT
    {
        GraphicsContextGLAttributes attributes;
        m_context = createTestedGraphicsContextGL(attributes);
        m_context->reshape(INITIAL_WIDTH, INITIAL_HEIGHT);
    }

    RefPtr<TestedGraphicsContextGLTextureMapper> m_context { nullptr };
};

TEST_F(GraphicsContextGLTextureMapperReshapeTest, reshapeSuccess)
{
    const IntSize framebufferSize { 200, 200 };

    EXPECT_EQ(m_context->getInternalFramebufferSize().width(), INITIAL_WIDTH);
    EXPECT_EQ(m_context->getInternalFramebufferSize().height(), INITIAL_HEIGHT);
    m_context->reshape(framebufferSize.width(), framebufferSize.height());
    EXPECT_EQ(m_context->getInternalFramebufferSize().width(), framebufferSize.width());
    EXPECT_EQ(m_context->getInternalFramebufferSize().height(), framebufferSize.height());
}

TEST_F(GraphicsContextGLTextureMapperReshapeTest, reshapeWidthTooLarge)
{
    const IntSize framebufferSize { std::numeric_limits<int>::max(), 200 };

    EXPECT_EQ(m_context->getInternalFramebufferSize().width(), INITIAL_WIDTH);
    EXPECT_EQ(m_context->getInternalFramebufferSize().height(), INITIAL_HEIGHT);
    m_context->reshape(framebufferSize.width(), framebufferSize.height());
    EXPECT_EQ(m_context->getInternalFramebufferSize().width(), INITIAL_WIDTH);
    EXPECT_EQ(m_context->getInternalFramebufferSize().height(), INITIAL_HEIGHT);
}

TEST_F(GraphicsContextGLTextureMapperReshapeTest, reshapeHeightTooLarge)
{
    const IntSize framebufferSize { 200, std::numeric_limits<int>::max() };

    EXPECT_EQ(m_context->getInternalFramebufferSize().width(), INITIAL_WIDTH);
    EXPECT_EQ(m_context->getInternalFramebufferSize().height(), INITIAL_HEIGHT);
    m_context->reshape(framebufferSize.width(), framebufferSize.height());
    EXPECT_EQ(m_context->getInternalFramebufferSize().width(), INITIAL_WIDTH);
    EXPECT_EQ(m_context->getInternalFramebufferSize().height(), INITIAL_HEIGHT);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEBGL)
