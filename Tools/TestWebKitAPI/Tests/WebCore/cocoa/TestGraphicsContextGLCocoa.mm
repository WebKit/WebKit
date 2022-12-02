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
#import "WebCoreTestUtilities.h"
#import <Metal/Metal.h>
#import <WebCore/Color.h>
#import <WebCore/GraphicsContextGLCocoa.h>
#import <WebCore/ProcessIdentity.h>
#import <optional>
#import <wtf/HashSet.h>
#import <wtf/MemoryFootprint.h>

namespace TestWebKitAPI {

namespace {

class TestedGraphicsContextGLCocoa : public WebCore::GraphicsContextGLCocoa {
public:
    static RefPtr<TestedGraphicsContextGLCocoa> create(WebCore::GraphicsContextGLAttributes&& attributes)
    {
        auto context = adoptRef(*new TestedGraphicsContextGLCocoa(WTFMove(attributes)));
        if (!context->initialize())
            return nullptr;
        return context;
    }
    RefPtr<WebCore::GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() final { return nullptr; }
private:
    TestedGraphicsContextGLCocoa(WebCore::GraphicsContextGLAttributes attributes)
        : WebCore::GraphicsContextGLCocoa(WTFMove(attributes), { })
    {
    }
};

class GraphicsContextGLCocoaTest : public ::testing::Test {
public:
    void SetUp() override // NOLINT
    {
        m_scopedProcessType = ScopedSetAuxiliaryProcessTypeForTesting { WebCore::AuxiliaryProcessType::GPU };
    }
    void TearDown() override // NOLINT
    {
        m_scopedProcessType = std::nullopt;
    }
private:
    std::optional<ScopedSetAuxiliaryProcessTypeForTesting> m_scopedProcessType;
};

}

static const int expectedDisplayBufferPoolSize = 3;

static RefPtr<TestedGraphicsContextGLCocoa> createDefaultTestContext(WebCore::IntSize contextSize)
{
    WebCore::GraphicsContextGLAttributes attributes;
    attributes.useMetal = true;
    attributes.antialias = false;
    attributes.depth = false;
    attributes.stencil = false;
    attributes.alpha = true;
    attributes.preserveDrawingBuffer = false;
    auto context = TestedGraphicsContextGLCocoa::create(WTFMove(attributes));
    if (!context)
        return nullptr;
    context->reshape(contextSize.width(), contextSize.height());
    return context;
}

static ::testing::AssertionResult changeContextContents(TestedGraphicsContextGLCocoa& context, int iteration)
{
    context.markContextChanged();
    WebCore::Color expected { iteration % 2 ? WebCore::Color::green : WebCore::Color::yellow };
    auto [r, g, b, a] = expected.toColorTypeLossy<WebCore::SRGBA<float>>().resolved();
    context.clearColor(r, g, b, a);
    context.clear(WebCore::GraphicsContextGL::COLOR_BUFFER_BIT);
    uint8_t gotValues[4] = { };
    auto sampleAt = context.getInternalFramebufferSize();
    sampleAt.contract(2, 3);
    sampleAt.clampNegativeToZero();
    context.readnPixels(sampleAt.width(), sampleAt.height(), 1, 1, WebCore::GraphicsContextGL::RGBA, WebCore::GraphicsContextGL::UNSIGNED_BYTE, gotValues);
    WebCore::Color got { WebCore::SRGBA<uint8_t> { gotValues[0], gotValues[1], gotValues[2], gotValues[3] } };
    if (got != expected)
        return ::testing::AssertionFailure() << "Failed to verify draw to context. Got: " << got << ", expected: " << expected << ".";
    return ::testing::AssertionSuccess();
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
    WebCore::GraphicsContextGLAttributes attributes;
    attributes.useMetal = true;
    EXPECT_EQ(attributes.powerPreference, WebCore::GraphicsContextGLPowerPreference::Default);
    auto defaultContext = TestedGraphicsContextGLCocoa::create(WebCore::GraphicsContextGLAttributes { attributes });
    ASSERT_NE(defaultContext, nullptr);

    attributes.powerPreference = WebCore::GraphicsContextGLPowerPreference::LowPower;
    auto lowPowerContext = TestedGraphicsContextGLCocoa::create(WebCore::GraphicsContextGLAttributes { attributes });
    ASSERT_NE(lowPowerContext, nullptr);

    attributes.powerPreference = WebCore::GraphicsContextGLPowerPreference::HighPerformance;
    auto highPerformanceContext = TestedGraphicsContextGLCocoa::create(WebCore::GraphicsContextGLAttributes { attributes });
    ASSERT_NE(highPerformanceContext, nullptr);

    EXPECT_NE(lowPowerContext->getString(WebCore::GraphicsContextGL::RENDERER), highPerformanceContext->getString(WebCore::GraphicsContextGL::RENDERER));
    EXPECT_EQ(defaultContext->getString(WebCore::GraphicsContextGL::RENDERER), lowPowerContext->getString(WebCore::GraphicsContextGL::RENDERER));
}

// Tests that requesting context with windowGPUID from low power device results
// to same thing as requesting default low power context.
// Tests that windowGPUID from low power device still respects high performance request.
TEST_F(GraphicsContextGLCocoaTest, MultipleGPUsExplicitLowPowerDeviceMetal)
{
    if (!hasMultipleGPUs())
        return;
    WebCore::GraphicsContextGLAttributes attributes1;
    attributes1.useMetal = true;
    attributes1.powerPreference = WebCore::GraphicsContextGLPowerPreference::LowPower;
    auto lowPowerContext = TestedGraphicsContextGLCocoa::create(WebCore::GraphicsContextGLAttributes { attributes1 });
    ASSERT_NE(lowPowerContext, nullptr);

    WebCore::GraphicsContextGLAttributes attributes2;
    attributes2.useMetal = true;
    attributes2.windowGPUID = [lowPowerDevice() registryID];
    auto explicitDeviceContext = TestedGraphicsContextGLCocoa::create(WebCore::GraphicsContextGLAttributes { attributes2 });
    ASSERT_NE(explicitDeviceContext.get(), nullptr);

    // Context with windowGPUID from low power device results to same thing as requesting default low power context.
    EXPECT_EQ(lowPowerContext->getString(WebCore::GraphicsContextGL::RENDERER), explicitDeviceContext->getString(WebCore::GraphicsContextGL::RENDERER));

    // High performance request on a low power explicit device as windowGPUID respects the high performance request.
    attributes2.powerPreference = WebCore::GraphicsContextGLPowerPreference::HighPerformance;
    auto highPerformanceExplicitDeviceContext = TestedGraphicsContextGLCocoa::create(WebCore::GraphicsContextGLAttributes { attributes2 });
    ASSERT_NE(highPerformanceExplicitDeviceContext.get(), nullptr);
    EXPECT_NE(highPerformanceExplicitDeviceContext->getString(WebCore::GraphicsContextGL::RENDERER), explicitDeviceContext->getString(WebCore::GraphicsContextGL::RENDERER));
}

// Tests that requesting context with windowGPUID from high performance device results to same thing
// as requesting default high performance context.
// Tests that windowGPUID from high performance device still uses that device even when low-power context is requested.
// It is likely that context on a specific window + gpu is the lowest power if the window is on that gpu.
TEST_F(GraphicsContextGLCocoaTest, MultipleGPUsExplicitHighPerformanceDeviceMetal)
{
    if (!hasMultipleGPUs())
        return;
    WebCore::GraphicsContextGLAttributes attributes1;
    attributes1.useMetal = true;
    attributes1.powerPreference = WebCore::GraphicsContextGLPowerPreference::HighPerformance;
    auto highPerformanceContext = TestedGraphicsContextGLCocoa::create(WebCore::GraphicsContextGLAttributes { attributes1 });
    ASSERT_NE(highPerformanceContext, nullptr);

    WebCore::GraphicsContextGLAttributes attributes2;
    attributes2.useMetal = true;
    attributes2.windowGPUID = [highPerformanceDevice() registryID];
    auto explicitDeviceContext = TestedGraphicsContextGLCocoa::create(WebCore::GraphicsContextGLAttributes { attributes2 });
    ASSERT_NE(explicitDeviceContext.get(), nullptr);

    // Context with windowGPUID from high performance device results to same thing as requesting default high performance context.
    EXPECT_EQ(highPerformanceContext->getString(WebCore::GraphicsContextGL::RENDERER), explicitDeviceContext->getString(WebCore::GraphicsContextGL::RENDERER));

    // Low power request on a high performance explicit device as windowGPUID ignores the low power request.
    attributes2.powerPreference = WebCore::GraphicsContextGLPowerPreference::LowPower;
    auto lowPowerExplicitDeviceContext = TestedGraphicsContextGLCocoa::create(WebCore::GraphicsContextGLAttributes { attributes2 });
    ASSERT_NE(lowPowerExplicitDeviceContext.get(), nullptr);
    EXPECT_EQ(lowPowerExplicitDeviceContext->getString(WebCore::GraphicsContextGL::RENDERER), explicitDeviceContext->getString(WebCore::GraphicsContextGL::RENDERER));
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
        WebCore::GraphicsContextGLAttributes attributes;
        attributes.useMetal = true;
        attributes.windowGPUID = [device registryID];
        auto context = TestedGraphicsContextGLCocoa::create(WebCore::GraphicsContextGLAttributes { attributes });
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

TEST_F(GraphicsContextGLCocoaTest, DisplayBuffersAreRecycled)
{
    auto context = createDefaultTestContext({ 20, 20 });
    ASSERT_NE(context, nullptr);
    RetainPtr<IOSurfaceRef> expectedDisplayBuffers[expectedDisplayBufferPoolSize];
    for (int i = 0; i < 50; ++i) {
        EXPECT_TRUE(changeContextContents(*context, i));
        context->prepareForDisplay();
        auto* surface = context->displayBuffer();
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

// Test that drawing buffers are not recycled if `GraphicsContextGLOpenGL::markDisplayBufferInUse()`
// is called.
TEST_F(GraphicsContextGLCocoaTest, DisplayBuffersAreNotRecycledWhenMarkedInUse)
{
    auto context = createDefaultTestContext({ 20, 20 });
    ASSERT_NE(context, nullptr);
    HashSet<RetainPtr<IOSurfaceRef>> seenSurfaceRefs;
    for (int i = 0; i < 50; ++i) {
        EXPECT_TRUE(changeContextContents(*context, i));
        context->prepareForDisplay();
        WebCore::IOSurface* surface = context->displayBuffer();
        ASSERT_NE(surface, nullptr);
        IOSurfaceRef surfaceRef = surface->surface();
        EXPECT_NE(surfaceRef, nullptr);
        EXPECT_FALSE(seenSurfaceRefs.contains(surfaceRef));
        seenSurfaceRefs.add(surfaceRef);

        context->markDisplayBufferInUse();
    }
    ASSERT_EQ(seenSurfaceRefs.size(), 50u);
}

// Test that drawing buffers are not recycled if the use count of the underlying IOSurface
// changes. Use count is modified for example by CoreAnimation when the IOSurface is attached
// to the contents.
TEST_F(GraphicsContextGLCocoaTest, DisplayBuffersAreNotRecycledWhedInUse)
{
    auto context = createDefaultTestContext({ 20, 20 });
    ASSERT_NE(context, nullptr);
    HashSet<RetainPtr<IOSurfaceRef>> seenSurfaceRefs;
    for (int i = 0; i < 50; ++i) {
        EXPECT_TRUE(changeContextContents(*context, i));
        context->prepareForDisplay();
        WebCore::IOSurface* surface = context->displayBuffer();
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
TEST_F(GraphicsContextGLCocoaTest, UnrecycledDisplayBuffersNoLeaks)
{
    // The test detects the leak by observing memory footprint. However, some of the freed IOSurface
    // memory (130mb) stays resident, presumably by intention of IOKit. The test would originally leak
    // 2.7gb so the intended bug would be detected with 150mb error range.
    size_t footprintError = 150 * 1024 * 1024;
    size_t footprintChange = 0;

    auto context = createDefaultTestContext({ 2048, 2048 });
    ASSERT_NE(context, nullptr);

    WTF::releaseFastMallocFreeMemory();
    auto lastFootprint = memoryFootprint();

    for (int i = 0; i < 50; ++i) {
        EXPECT_TRUE(changeContextContents(*context, i));
        context->prepareForDisplay();
        EXPECT_NE(context->displayBuffer(), nullptr);
        context->markDisplayBufferInUse();
    }

    EXPECT_TRUE(memoryFootprintChangedBy(lastFootprint, footprintChange, footprintError));
}

}

#endif
