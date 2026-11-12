/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import <IOSurface/IOSurfaceRef.h>
#import <WebCore/ColorSpace.h>
#import <WebCore/WebGPU.h>
#import <WebCore/WebGPUCompositorIntegration.h>
#import <WebCore/WebGPUCreateImpl.h>
#import <WebCore/WebGPUPresentationContextDescriptor.h>
#import <wtf/MachSendRight.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

namespace TestWebKitAPI {

using namespace WebCore;

static constexpr uint32_t canvasSize = 4;
static constexpr ::WebGPU::Color greenColor { 0, 1, 0, 1 };
static constexpr ::WebGPU::Color blueColor { 0, 0, 1, 1 };
static constexpr ::WebGPU::Color redColor { 1, 0, 0, 1 };
static constexpr uint32_t greenPixel = 0xFF00FF00;
static constexpr uint32_t redPixel = 0xFFFF0000;

struct PresentedFrame {
    RetainPtr<IOSurfaceRef> displayBuffer;
    std::optional<uint32_t> replacedRenderBufferIndex;
};

static uint32_t firstPixel(IOSurfaceRef surface)
{
    IOSurfaceLock(surface, kIOSurfaceLockReadOnly, nullptr);
    auto pixel = *static_cast<const uint32_t*>(IOSurfaceGetBaseAddress(surface));
    IOSurfaceUnlock(surface, kIOSurfaceLockReadOnly, nullptr);
    return pixel;
}

class WebGPUCompositorIntegrationTest : public testing::Test {
public:
    void SetUp() final
    {
        m_gpu = createWebGPUIntegration([](Function<void()>&& workItem) {
            callOnMainRunLoop(WTF::move(workItem));
        }, nullptr);
        ASSERT_TRUE(m_gpu);

        RefPtr<::WebGPU::Adapter> adapter;
        bool done = false;
        m_gpu->requestAdapter({ }, [&](RefPtr<::WebGPU::Adapter>&& result) {
            adapter = WTF::move(result);
            done = true;
        });
        Util::run(&done);
        ASSERT_TRUE(adapter);

        done = false;
        adapter->requestDevice({ }, [&](RefPtr<::WebGPU::Device>&& result) {
            m_device = WTF::move(result);
            done = true;
        });
        Util::run(&done);
        ASSERT_TRUE(m_device);

        m_compositorIntegration = m_gpu->createCompositorIntegration();
        ASSERT_TRUE(m_compositorIntegration);
        m_presentationContext = m_gpu->createPresentationContext({ Ref { *m_compositorIntegration } });
        ASSERT_TRUE(m_presentationContext);

        EXPECT_EQ(m_compositorIntegration->recreateRenderBuffers(canvasSize, canvasSize, ColorSpace { ColorSpace::SRGB() }, AlphaPremultiplication::Premultiplied, ::WebGPU::TextureFormat::Bgra8unorm, *m_device), 3u);
        m_presentationContext->configure({
            .device = *m_device,
            .format = ::WebGPU::TextureFormat::Bgra8unorm,
            .usage = ::WebGPU::TextureUsage::RenderAttachment,
            .compositingAlphaMode = ::WebGPU::CanvasAlphaMode::Premultiplied,
            .width = canvasSize,
            .height = canvasSize,
        });
    }

    PresentedFrame present(uint32_t frameIndex, const ::WebGPU::Color& clearValue)
    {
        RefPtr texture = m_presentationContext->getCurrentTexture(frameIndex);
        RefPtr encoder = m_device->createCommandEncoder({ });
        std::array<std::optional<::WebGPU::RenderPassColorAttachment>, 1> colorAttachments { ::WebGPU::RenderPassColorAttachment {
            .view = Ref { *texture },
            .clearValue = clearValue,
            .loadOp = ::WebGPU::LoadOp::Clear,
        } };
        RefPtr pass = encoder->beginRenderPass({ .colorAttachments = colorAttachments });
        pass->end();
        m_device->queue()->submit({ encoder->finish({ }).releaseNonNull() });

        PresentedFrame frame;
        bool done = false;
        m_compositorIntegration->prepareForDisplay(frameIndex, [&](MachSendRight&& displayBuffer, std::optional<uint32_t> replacedRenderBufferIndex) {
            frame = { adoptCF(IOSurfaceLookupFromMachPort(displayBuffer.sendRight())), replacedRenderBufferIndex };
            done = true;
        });
        Util::run(&done);
        return frame;
    }

    void waitForSubmittedWork()
    {
        bool done = false;
        m_device->queue()->onSubmittedWorkDone([&] {
            done = true;
        });
        Util::run(&done);
    }

private:
    RefPtr<WebGPUIntegration> m_gpu;
    RefPtr<::WebGPU::Device> m_device;
    RefPtr<WebGPUCompositorIntegration> m_compositorIntegration;
    RefPtr<::WebGPU::PresentationContext> m_presentationContext;
};

TEST_F(WebGPUCompositorIntegrationTest, RenderBufferInUseIsNotDrawnInto)
{
    auto heldFrame = present(0, greenColor);
    ASSERT_TRUE(heldFrame.displayBuffer);
    IOSurfaceIncrementUseCount(heldFrame.displayBuffer.get());

    EXPECT_FALSE(present(1, blueColor).replacedRenderBufferIndex);
    EXPECT_EQ(present(2, blueColor).replacedRenderBufferIndex, std::optional<uint32_t> { 0 });
    auto nextFrame = present(0, redColor);
    waitForSubmittedWork();

    ASSERT_TRUE(nextFrame.displayBuffer);
    EXPECT_NE(IOSurfaceGetID(nextFrame.displayBuffer.get()), IOSurfaceGetID(heldFrame.displayBuffer.get()));
    EXPECT_EQ(firstPixel(heldFrame.displayBuffer.get()), greenPixel);
    EXPECT_EQ(firstPixel(nextFrame.displayBuffer.get()), redPixel);

    IOSurfaceDecrementUseCount(heldFrame.displayBuffer.get());
}

TEST_F(WebGPUCompositorIntegrationTest, RenderBuffersAreNotReplacedWhileNotInUse)
{
    Vector<IOSurfaceID> displayBuffers;
    for (uint32_t frame = 0; frame < 9; ++frame) {
        auto presentedFrame = present(frame % 3, greenColor);
        EXPECT_FALSE(presentedFrame.replacedRenderBufferIndex);
        ASSERT_TRUE(presentedFrame.displayBuffer);
        displayBuffers.appendIfNotContains(IOSurfaceGetID(presentedFrame.displayBuffer.get()));
    }
    EXPECT_EQ(displayBuffers.size(), 3u);
}

} // namespace TestWebKitAPI

#endif // HAVE(WEBGPU_IMPLEMENTATION)
