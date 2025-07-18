/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "Test.h"

#if PLATFORM(COCOA) && ENABLE(WEBGL)
#import "GraphicsTestUtilities.h"
#import "WebCoreTestUtilities.h"
#import <Metal/Metal.h>
#import <WebCore/Color.h>
#import <WebCore/GraphicsContextGLCocoa.h>
#import <WebCore/ProcessIdentity.h>
#import <atomic>
#import <limits>
#import <optional>
#import <wtf/HashSet.h>
#import <wtf/MemoryFootprint.h>
#import <wtf/StdLibExtras.h>

namespace TestWebKitAPI {

using namespace WebCore;

namespace {

class MockGraphicsContextGLClient final : public GraphicsContextGL::Client {
public:
    void forceContextLost() final { ++m_contextLostCalls; }
    void addDebugMessage(GCGLenum, GCGLenum, GCGLenum, const String&) final { }
    int contextLostCalls() { return m_contextLostCalls; }
private:
    int m_contextLostCalls { 0 };
};

class TestedGraphicsContextGLCocoa : public GraphicsContextGLCocoa {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(TestedGraphicsContextGLCocoa);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(TestedGraphicsContextGLCocoa);
public:
    static RefPtr<TestedGraphicsContextGLCocoa> create(GraphicsContextGLAttributes&& attributes)
    {
        auto context = adoptRef(*new TestedGraphicsContextGLCocoa(WTFMove(attributes)));
        if (!context->initialize())
            return nullptr;
        return context;
    }
    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() final { return nullptr; }
private:
    TestedGraphicsContextGLCocoa(GraphicsContextGLAttributes attributes)
        : GraphicsContextGLCocoa(WTFMove(attributes), { })
    {
    }
};

class GraphicsContextGLCocoaTest : public ::testing::Test {
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
    RefPtr<TestedGraphicsContextGLCocoa> createTestContext(IntSize contextSize);

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

RefPtr<TestedGraphicsContextGLCocoa> AnyContextAttributeTest::createTestContext(IntSize contextSize)
{
    auto context = TestedGraphicsContextGLCocoa::create(attributes());
    if (!context)
        return nullptr;
    context->reshape(contextSize.width(), contextSize.height());
    return context;
}

}

static const int expectedDisplayBufferPoolSize = 3;

static ::testing::AssertionResult checkReadPixel(GraphicsContextGL& context, IntPoint point, Color expected)
{
    uint8_t gotValues[4] = { };
    context.readPixels({ point, { 1, 1 } }, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, gotValues, 1, 0, false);
    Color got { SRGBA<uint8_t> { gotValues[0], gotValues[1], gotValues[2], gotValues[3] } };
    if (got != expected)
        return ::testing::AssertionFailure() << "Got: " << got << ", expected: " << expected << ".";
    return ::testing::AssertionSuccess();

}
static ::testing::AssertionResult changeContextContents(GraphicsContextGL& context, int iteration)
{
    Color expected { iteration % 2 ? Color::green : Color::yellow };
    auto [r, g, b, a] = expected.toColorTypeLossy<SRGBA<float>>().resolved();
    context.clearColor(r, g, b, a);
    context.clear(GraphicsContextGL::COLOR_BUFFER_BIT);
    auto sampleAt = context.getInternalFramebufferSize();
    sampleAt.contract(2, 3);
    sampleAt.clampNegativeToZero();
    return checkReadPixel(context, IntPoint { sampleAt }, expected);
}

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
static RetainPtr<NSArray<id<MTLDevice>>> allDevices()
{
    return adoptNS(MTLCopyAllDevices());
}

static RetainPtr<id<MTLDevice>> lowPowerDevice()
{
    auto devices = allDevices();
    for (id<MTLDevice> device in devices.get()) {
        if (device.lowPower)
            return device;
    }
    return nullptr;
}

static RetainPtr<id<MTLDevice>> highPerformanceDevice()
{
    auto devices = allDevices();
    for (id<MTLDevice> device in devices.get()) {
        if (!device.lowPower && !device.removable)
            return device;
    }
    return nullptr;
}

static bool hasMultipleGPUs()
{
    return highPerformanceDevice() && lowPowerDevice();
}

// Sanity check for the MultipleGPUs* tests.
TEST_F(GraphicsContextGLCocoaTest, MultipleGPUsHaveDifferentGPUs)
{
    if (!hasMultipleGPUs())
        return;
    auto a = highPerformanceDevice();
    auto b = lowPowerDevice();
    EXPECT_NE(a.get(), nullptr);
    EXPECT_NE(b.get(), nullptr);
    EXPECT_NE(a.get(), b.get());
}

// Tests for a bug where high-performance context would use low-power GPU if low-power or default
// context was created first. Test is applicable only for Metal, since GPU selection for OpenGL is
// very different.
TEST_F(GraphicsContextGLCocoaTest, MultipleGPUsDifferentPowerPreferenceMetal)
{
    if (!hasMultipleGPUs())
        return;
    GraphicsContextGLAttributes attributes;
    EXPECT_EQ(attributes.powerPreference, GraphicsContextGLPowerPreference::Default);
    auto defaultContext = TestedGraphicsContextGLCocoa::create(GraphicsContextGLAttributes { attributes });
    ASSERT_NE(defaultContext, nullptr);

    attributes.powerPreference = GraphicsContextGLPowerPreference::LowPower;
    auto lowPowerContext = TestedGraphicsContextGLCocoa::create(GraphicsContextGLAttributes { attributes });
    ASSERT_NE(lowPowerContext, nullptr);

    attributes.powerPreference = GraphicsContextGLPowerPreference::HighPerformance;
    auto highPerformanceContext = TestedGraphicsContextGLCocoa::create(GraphicsContextGLAttributes { attributes });
    ASSERT_NE(highPerformanceContext, nullptr);

    EXPECT_NE(lowPowerContext->getString(GraphicsContextGL::RENDERER), highPerformanceContext->getString(GraphicsContextGL::RENDERER));
    EXPECT_EQ(defaultContext->getString(GraphicsContextGL::RENDERER), lowPowerContext->getString(GraphicsContextGL::RENDERER));
}
#endif

#if PLATFORM(MAC)
// Tests that requesting context with windowGPUID from low power device results
// to same thing as requesting default low power context.
// Tests that windowGPUID from low power device still respects high performance request.
TEST_F(GraphicsContextGLCocoaTest, MultipleGPUsExplicitLowPowerDeviceMetal)
{
    if (!hasMultipleGPUs())
        return;
    GraphicsContextGLAttributes attributes1;
    attributes1.powerPreference = GraphicsContextGLPowerPreference::LowPower;
    auto lowPowerContext = TestedGraphicsContextGLCocoa::create(GraphicsContextGLAttributes { attributes1 });
    ASSERT_NE(lowPowerContext, nullptr);

    GraphicsContextGLAttributes attributes2;
    attributes2.windowGPUID = [lowPowerDevice() registryID];
    auto explicitDeviceContext = TestedGraphicsContextGLCocoa::create(GraphicsContextGLAttributes { attributes2 });
    ASSERT_NE(explicitDeviceContext.get(), nullptr);

    // Context with windowGPUID from low power device results to same thing as requesting default low power context.
    EXPECT_EQ(lowPowerContext->getString(GraphicsContextGL::RENDERER), explicitDeviceContext->getString(GraphicsContextGL::RENDERER));

    // High performance request on a low power explicit device as windowGPUID respects the high performance request.
    attributes2.powerPreference = GraphicsContextGLPowerPreference::HighPerformance;
    auto highPerformanceExplicitDeviceContext = TestedGraphicsContextGLCocoa::create(GraphicsContextGLAttributes { attributes2 });
    ASSERT_NE(highPerformanceExplicitDeviceContext.get(), nullptr);
    EXPECT_NE(highPerformanceExplicitDeviceContext->getString(GraphicsContextGL::RENDERER), explicitDeviceContext->getString(GraphicsContextGL::RENDERER));
}

// Tests that requesting context with windowGPUID from high performance device results to same thing
// as requesting default high performance context.
// Tests that windowGPUID from high performance device still uses that device even when low-power context is requested.
// It is likely that context on a specific window + gpu is the lowest power if the window is on that gpu.
TEST_F(GraphicsContextGLCocoaTest, MultipleGPUsExplicitHighPerformanceDeviceMetal)
{
    if (!hasMultipleGPUs())
        return;
    GraphicsContextGLAttributes attributes1;
    attributes1.powerPreference = GraphicsContextGLPowerPreference::HighPerformance;
    auto highPerformanceContext = TestedGraphicsContextGLCocoa::create(GraphicsContextGLAttributes { attributes1 });
    ASSERT_NE(highPerformanceContext, nullptr);

    GraphicsContextGLAttributes attributes2;
    attributes2.windowGPUID = [highPerformanceDevice() registryID];
    auto explicitDeviceContext = TestedGraphicsContextGLCocoa::create(GraphicsContextGLAttributes { attributes2 });
    ASSERT_NE(explicitDeviceContext.get(), nullptr);

    // Context with windowGPUID from high performance device results to same thing as requesting default high performance context.
    EXPECT_EQ(highPerformanceContext->getString(GraphicsContextGL::RENDERER), explicitDeviceContext->getString(GraphicsContextGL::RENDERER));

    // Low power request on a high performance explicit device as windowGPUID ignores the low power request.
    attributes2.powerPreference = GraphicsContextGLPowerPreference::LowPower;
    auto lowPowerExplicitDeviceContext = TestedGraphicsContextGLCocoa::create(GraphicsContextGLAttributes { attributes2 });
    ASSERT_NE(lowPowerExplicitDeviceContext.get(), nullptr);
    EXPECT_EQ(lowPowerExplicitDeviceContext->getString(GraphicsContextGL::RENDERER), explicitDeviceContext->getString(GraphicsContextGL::RENDERER));
}

// Tests that requesting GraphicsContextGL instances with different devices results in different underlying
// EGLDisplays.
TEST_F(GraphicsContextGLCocoaTest, MultipleGPUsDifferentGPUIDsMetal)
{
    if (!hasMultipleGPUs())
        return;
    Vector<Ref<TestedGraphicsContextGLCocoa>> contexts;
    auto devices = allDevices();
    for (id<MTLDevice> device in devices.get()) {
        GraphicsContextGLAttributes attributes;
        attributes.windowGPUID = [device registryID];
        auto context = TestedGraphicsContextGLCocoa::create(GraphicsContextGLAttributes { attributes });
        EXPECT_NE(context.get(), nullptr);
        if (!context)
            continue;
        contexts.append(context.releaseNonNull());
    }
    EXPECT_GT(contexts.size(), 1u);

    // The requested EGLDisplays must differ if the devices differ.
    for (auto itA = contexts.begin(); itA != contexts.end(); ++itA) {
        for (auto itB = itA + 1; itB != contexts.end(); ++itB)
            EXPECT_NE((*itA)->platformDisplay(), (*itB)->platformDisplay());
    }
}
#endif

TEST_F(GraphicsContextGLCocoaTest, ClearBufferIncorrectSizes)
{
    using GL = GraphicsContextGL;
    GraphicsContextGLAttributes attributes;
    attributes.isWebGL2 = true;
    attributes.depth = true;
    attributes.stencil = true;
    auto gl = TestedGraphicsContextGLCocoa::create(WTFMove(attributes));
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
TEST_F(GraphicsContextGLCocoaTest, DestroyWithoutMakingCurrent)
{
    GraphicsContextGLAttributes attributes;
    attributes.isWebGL2 = true;
    attributes.depth = true;
    attributes.stencil = true;
    RefPtr gl1 = TestedGraphicsContextGLCocoa::create(WTFMove(attributes));
    gl1->reshape(1, 1);
    RefPtr gl2 = TestedGraphicsContextGLCocoa::create(WTFMove(attributes));
    gl2->reshape(1, 1);
    RefPtr gl3 = TestedGraphicsContextGLCocoa::create(WTFMove(attributes));
    gl3->reshape(1, 1);
    // Current context is now 3.
    gl1 = nullptr; // Test the case where we destroy with other context being current.
    // Current context is now nullptr.
    gl2 = nullptr; // Test the case where we destroy without context being current.
}

TEST_F(GraphicsContextGLCocoaTest, TwoLinks)
{
    GraphicsContextGLAttributes attributes;
    auto gl = TestedGraphicsContextGLCocoa::create(WTFMove(attributes));
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

TEST_F(GraphicsContextGLCocoaTest, BufferAsImageNoDrawingBufferReturnsNullptr)
{
    using GL = GraphicsContextGL;
    auto gl = TestedGraphicsContextGLCocoa::create({ });
    RefPtr drawingImage = gl->bufferAsNativeImage(GL::SurfaceBuffer::DrawingBuffer);
    RefPtr displayImage = gl->bufferAsNativeImage(GL::SurfaceBuffer::DisplayBuffer);
    EXPECT_EQ(drawingImage, nullptr);
    EXPECT_EQ(displayImage, nullptr);
}


TEST_F(GraphicsContextGLCocoaTest, BufferAsImageAfterReshape)
{
    using GL = GraphicsContextGL;
    auto gl = TestedGraphicsContextGLCocoa::create({ });
    gl->reshape(10, 10);
    RefPtr drawingImage = gl->bufferAsNativeImage(GL::SurfaceBuffer::DrawingBuffer);
    RefPtr displayImage = gl->bufferAsNativeImage(GL::SurfaceBuffer::DisplayBuffer);
    EXPECT_NE(drawingImage, nullptr);
    EXPECT_EQ(displayImage, nullptr);
    EXPECT_EQ(drawingImage->size(), FloatSize(10, 10));
    EXPECT_TRUE(imagePixelIs(Color::transparentBlack, *drawingImage, FloatPoint(5, 5)));
}

// Test copying images and mutating the drawing buffer.
// The mutations should only be visible in the new buffers, and not the old ones.
TEST_F(GraphicsContextGLCocoaTest, CopyImageAndMutateDrawingBuffer)
{
    using GL = GraphicsContextGL;
    auto gl = TestedGraphicsContextGLCocoa::create({ });
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

TEST_P(AnyContextAttributeTest, DisplayBuffersAreRecycled)
{
    auto context = createTestContext({ 20, 20 });
    ASSERT_NE(context, nullptr);
    RetainPtr<IOSurfaceRef> expectedDisplayBuffers[expectedDisplayBufferPoolSize];
    for (int i = 0; i < 50; ++i) {
        EXPECT_TRUE(changeContextContents(*context, i));
        context->prepareForDisplay();
        auto* surface = context->displayBufferSurface();
        ASSERT_NE(surface, nullptr);
        int slot = i % expectedDisplayBufferPoolSize;
        if (!expectedDisplayBuffers[slot])
            expectedDisplayBuffers[slot] = surface->surface();
        EXPECT_EQ(expectedDisplayBuffers[slot].get(), surface->surface()) << "for i:" << i << " slot: " << slot;
    }
    for (int i = 0; i < expectedDisplayBufferPoolSize - 1; ++i) {
        for (int j = i + 1; j < expectedDisplayBufferPoolSize; ++j)
            EXPECT_NE(expectedDisplayBuffers[i].get(), expectedDisplayBuffers[j].get()) << "for i: " << i << " j:" << j;
    }
}

// Test that drawing buffers are not recycled if the use count of the underlying IOSurface
// changes. Use count is modified for example by CoreAnimation when the IOSurface is attached
// to the contents.
TEST_P(AnyContextAttributeTest, DisplayBuffersAreNotRecycledWhedInUse)
{
    auto context = createTestContext({ 20, 20 });
    ASSERT_NE(context, nullptr);
    HashSet<RetainPtr<IOSurfaceRef>> seenSurfaceRefs;
    for (int i = 0; i < 50; ++i) {
        EXPECT_TRUE(changeContextContents(*context, i));
        context->prepareForDisplay();
        WebCore::IOSurface* surface = context->displayBufferSurface();
        ASSERT_NE(surface, nullptr);
        IOSurfaceRef surfaceRef = surface->surface();
        EXPECT_NE(surfaceRef, nullptr);
        EXPECT_FALSE(seenSurfaceRefs.contains(surfaceRef));
        seenSurfaceRefs.add(surfaceRef);

        IOSurfaceIncrementUseCount(surfaceRef);
    }
    ASSERT_EQ(seenSurfaceRefs.size(), 50u);
}

// Test that drawing to GraphicsContextGL and marking the display buffer in use does not leak big
// amounts of memory for each displayed buffer.
TEST_P(AnyContextAttributeTest, UnrecycledDisplayBuffersNoLeaks)
{
    // The test detects the leak by observing memory footprint. However, some of the freed IOSurface
    // memory (130mb) stays resident, presumably by intention of IOKit. The test would originally leak
    // 2.7gb so the intended bug would be detected with 150mb error range.
    size_t footprintError = 150 * 1024 * 1024;
    size_t footprintChange = 0;

    auto context = createTestContext({ 2048, 2048 });
    ASSERT_NE(context, nullptr);

    WTF::releaseFastMallocFreeMemory();
    auto lastFootprint = memoryFootprint();

    for (int i = 0; i < 50; ++i) {
        EXPECT_TRUE(changeContextContents(*context, i));
        context->prepareForDisplay();
        auto* surface = context->displayBufferSurface();
        EXPECT_NE(surface, nullptr);
        IOSurfaceIncrementUseCount(surface->surface());
    }

    EXPECT_TRUE(memoryFootprintChangedBy(lastFootprint, footprintChange, footprintError));
}

// Test that failing to allocate a new buffer in Prepare:
// - Signals context lost
// - Does not crash, does not crash in subsequent draws
// - Reading pixels returns zeros
TEST_P(AnyContextAttributeTest, PrepareFailureWorks)
{
    MockGraphicsContextGLClient client;
    auto context = createTestContext({ 20, 20 });
    ASSERT_NE(context, nullptr);
    context->setClient(&client);
    EXPECT_TRUE(context->getErrors().isEmpty());
    ASSERT_TRUE(changeContextContents(*context, 0));
    EXPECT_TRUE(context->getErrors().isEmpty());
    context->simulateEventForTesting(GraphicsContextGLSimulatedEventForTesting::DisplayBufferAllocationFailure);
    context->prepareForDisplay();
    EXPECT_NE(context->displayBufferSurface(), nullptr);
    EXPECT_EQ(1, client.contextLostCalls());
    // For documentation purposes how the context behaves afterwards.
    // For WebGL this is not relevant, as the context is marked as lost, and each new WebGL call will
    // check for the context loss flag and does not let the call proceed.
    auto attrs = context->contextAttributes();
    if (attrs.preserveDrawingBuffer && !attrs.antialias) {
        ASSERT_TRUE(changeContextContents(*context, 1));
        EXPECT_TRUE(context->getErrors().isEmpty());
    } else if (attrs.preserveDrawingBuffer || attrs.antialias) {
        ASSERT_FALSE(changeContextContents(*context, 1));
        auto errors = context->getErrors();
        EXPECT_TRUE(errors.containsAny({ GCGLErrorCode::InvalidFramebufferOperation, GCGLErrorCode::InvalidOperation }));
        EXPECT_TRUE(errors.containsOnly({ GCGLErrorCode::InvalidFramebufferOperation, GCGLErrorCode::InvalidOperation }));
    } else {
        ASSERT_FALSE(changeContextContents(*context, 1));
        uint32_t gotValue = 0;
        context->readPixels({ 0, 0, 1, 1 }, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, { reinterpret_cast<uint8_t*>(&gotValue), 4 }, 4, 0, false);
        EXPECT_EQ(0u, gotValue);
        EXPECT_EQ(GCGLErrorCode::InvalidFramebufferOperation, context->getErrors());
    }
    context->prepareForDisplay();
    context->prepareForDisplay();
    EXPECT_EQ(1, client.contextLostCalls());
}

// Tests that prepareForDisplayWithFinishedSignal submits the underlying fence.
// E.g. no other work will be done to context after prepare, but the finished signal will
// eventually come.
TEST_P(AnyContextAttributeTest, FinishIsSignaled)
{
    auto context = createTestContext({ 2048, 2048 });
    ASSERT_NE(context, nullptr);
    context->clearColor(0.f, 1.f, 0.f, 1.f);
    context->clear(GraphicsContextGL::COLOR_BUFFER_BIT);
    std::atomic<bool> signalled = false;
    std::atomic<uint32_t> signalThreadUID = 0;
    context->prepareForDisplayWithFinishedSignal([&signalled, &signalThreadUID] {
        signalled = true;
        signalThreadUID = Thread::currentSingleton().uid();
    });
    while (!signalled)
        sleep(.1_s);
    EXPECT_TRUE(signalled);
    EXPECT_NE(Thread::currentSingleton().uid(), signalThreadUID);
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
        gl->renderbufferStorageMultisampleANGLE(GL::RENDERBUFFER, 0, GL::BGRA_EXT, 2, 2);
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
#endif

INSTANTIATE_TEST_SUITE_P(GraphicsContextGLCocoaTest,
    AnyContextAttributeTest,
    testing::Combine(
        testing::Values(true, false),
        testing::Values(true, false),
        testing::Values(true, false)),
    TestParametersToStringFormatter());

class GraphicsContextGLCocoaReadPixelsTest : public ::testing::Test {
protected:
    void SetUp() override // NOLINT
    {
        GraphicsContextGLAttributes attributes;
        m_context = TestedGraphicsContextGLCocoa::create(WTFMove(attributes));
        m_expectedColor = Color::gray;
        auto [r, g, b, a] = m_expectedColor.toColorTypeLossy<SRGBA<float>>().resolved();
        m_context->reshape(20, 20);
        m_context->clearColor(r, g, b, a);
        m_context->clear(GraphicsContextGL::COLOR_BUFFER_BIT);
    }

    RefPtr<TestedGraphicsContextGLCocoa> m_context { nullptr };
    Color m_expectedColor { };
};

TEST_F(GraphicsContextGLCocoaReadPixelsTest, readPixelsSuccess)
{
    EXPECT_TRUE(m_context->getErrors().isEmpty());
    uint8_t gotValues[4] = { 0, 0, 0, 0 };
    IntRect rect(1, 1, 1, 1);
    m_context->readPixels(rect, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, gotValues, 4, 0, false);
    Color actualColor { SRGBA<uint8_t> { gotValues[0], gotValues[1], gotValues[2], gotValues[3] } };
    EXPECT_EQ(m_expectedColor, actualColor);
    EXPECT_TRUE(m_context->getErrors().isEmpty());
}

TEST_F(GraphicsContextGLCocoaReadPixelsTest, readPixelsTooLargeRect)
{
    EXPECT_TRUE(m_context->getErrors().isEmpty());
    uint8_t gotValues[4] = { 0, 0, 0, 0 };
    IntRect rect(1, 1, 0x7fffffff, 0x7fffffff);
    m_context->readPixels(rect, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, gotValues, 4, 0, false);
    Color actualColor { SRGBA<uint8_t> { gotValues[0], gotValues[1], gotValues[2], gotValues[3] } };
    EXPECT_NE(m_expectedColor, actualColor);
    EXPECT_EQ(GCGLErrorCode::InvalidOperation, m_context->getErrors());
}

TEST_F(GraphicsContextGLCocoaReadPixelsTest, readPixelsWithStatusSuccess)
{
    uint8_t gotValues[4] = { 0, 0, 0, 0 };
    IntRect rect(1, 1, 1, 1);
    m_context->readPixelsWithStatus(rect, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, false, gotValues);
    Color actualColor { SRGBA<uint8_t> { gotValues[0], gotValues[1], gotValues[2], gotValues[3] } };
    EXPECT_EQ(m_expectedColor, actualColor);
    EXPECT_TRUE(m_context->getErrors().isEmpty());
}

TEST_F(GraphicsContextGLCocoaReadPixelsTest, readPixelsWithStatusTooLargeRect)
{
    uint8_t gotValues[4] = { 0, 0, 0, 0 };
    IntRect rect(1, 1, 0x7fffffff, 0x7fffffff);
    m_context->readPixelsWithStatus(rect, GraphicsContextGL::RGBA, GraphicsContextGL::UNSIGNED_BYTE, false, gotValues);
    Color actualColor { SRGBA<uint8_t> { gotValues[0], gotValues[1], gotValues[2], gotValues[3] } };
    EXPECT_NE(m_expectedColor, actualColor);
    EXPECT_EQ(GCGLErrorCode::InvalidOperation, m_context->getErrors());
}

class GraphicsContextGLCocoaReshapeTest : public ::testing::Test {
protected:
    static constexpr int INITIAL_WIDTH = 20;
    static constexpr int INITIAL_HEIGHT = 20;

    void SetUp() override // NOLINT
    {
        GraphicsContextGLAttributes attributes;
        m_context = TestedGraphicsContextGLCocoa::create(WTFMove(attributes));
        m_context->reshape(INITIAL_WIDTH, INITIAL_HEIGHT);
    }

    RefPtr<TestedGraphicsContextGLCocoa> m_context { nullptr };
};

TEST_F(GraphicsContextGLCocoaReshapeTest, reshapeSuccess)
{
    const IntSize framebufferSize { 200, 200 };

    EXPECT_EQ(m_context->getInternalFramebufferSize().width(), INITIAL_WIDTH);
    EXPECT_EQ(m_context->getInternalFramebufferSize().height(), INITIAL_HEIGHT);
    m_context->reshape(framebufferSize.width(), framebufferSize.height());
    EXPECT_EQ(m_context->getInternalFramebufferSize().width(), framebufferSize.width());
    EXPECT_EQ(m_context->getInternalFramebufferSize().height(), framebufferSize.height());
}

TEST_F(GraphicsContextGLCocoaReshapeTest, reshapeWidthTooLarge)
{
    const IntSize framebufferSize { std::numeric_limits<int>::max(), 200 };

    EXPECT_EQ(m_context->getInternalFramebufferSize().width(), INITIAL_WIDTH);
    EXPECT_EQ(m_context->getInternalFramebufferSize().height(), INITIAL_HEIGHT);
    m_context->reshape(framebufferSize.width(), framebufferSize.height());
    EXPECT_EQ(m_context->getInternalFramebufferSize().width(), INITIAL_WIDTH);
    EXPECT_EQ(m_context->getInternalFramebufferSize().height(), INITIAL_HEIGHT);
}

TEST_F(GraphicsContextGLCocoaReshapeTest, reshapeHeightTooLarge)
{
    const IntSize framebufferSize { 200, std::numeric_limits<int>::max() };

    EXPECT_EQ(m_context->getInternalFramebufferSize().width(), INITIAL_WIDTH);
    EXPECT_EQ(m_context->getInternalFramebufferSize().height(), INITIAL_HEIGHT);
    m_context->reshape(framebufferSize.width(), framebufferSize.height());
    EXPECT_EQ(m_context->getInternalFramebufferSize().width(), INITIAL_WIDTH);
    EXPECT_EQ(m_context->getInternalFramebufferSize().height(), INITIAL_HEIGHT);
}

}

#endif
