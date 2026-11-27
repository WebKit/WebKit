/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/AlphaPremultiplication.h>
#include <WebCore/IntSize.h>
#include <WebCore/WebGPUCppAPI.h>
#include <WebCore/WebGPUDevice.h>

#include <algorithm>
#include <optional>
#include <wtf/CompletionHandler.h>
#include <wtf/Function.h>
#include <wtf/Platform.h>
#include <wtf/Ref.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/Seconds.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include <wtf/MachSendRight.h>
#include <wtf/Vector.h>
#endif

namespace WebCore {

class ColorSpace;
class NativeImage;
class VideoFrame;

// The render buffers that the compositor shows a WebGPU canvas from.
class WebGPUCompositorIntegration : public RefCountedAndCanMakeWeakPtr<WebGPUCompositorIntegration> {
public:
    virtual ~WebGPUCompositorIntegration() = default;

    // The size of the render buffers that recreateRenderBuffers() makes for the given size.
    static IntSize renderBufferSize(int width, int height)
    {
        constexpr int max2DTextureSize = 16384;
        return { std::clamp(width, 1, max2DTextureSize), std::clamp(height, 1, max2DTextureSize) };
    }

#if PLATFORM(COCOA)
    virtual uint32_t recreateRenderBuffers(int width, int height, ColorSpace&&, AlphaPremultiplication, ::WebGPU::TextureFormat, ::WebGPU::Device&) = 0;
    virtual void prepareForDisplay(uint32_t frameIndex, CompletionHandler<void(MachSendRight&& displayBuffer, std::optional<uint32_t> replacedRenderBufferIndex)>&&) = 0;
#endif

    virtual Seconds lastFrameGPUCost() const { return 0_s; }
    virtual Seconds lastFramePresentStall() const { return 0_s; }
    // Returns a copy of the contents of the render buffer, which stays the same when the render
    // buffer is drawn to again.
    virtual RefPtr<NativeImage> copyNativeImage(uint32_t bufferIndex) = 0;
#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
    // Returns a video frame of a copy of the contents of the render buffer, or null when the
    // contents cannot be held in a video frame.
    virtual RefPtr<VideoFrame> copyVideoFrame(uint32_t bufferIndex) = 0;
#endif
    virtual void updateContentsHeadroom(float) = 0;
    virtual bool isRemoteCompositorIntegrationProxy() const { return false; }
    virtual bool isWebGPUCompositorIntegrationImpl() const { return false; }

protected:
    WebGPUCompositorIntegration() = default;

private:
    WebGPUCompositorIntegration(const WebGPUCompositorIntegration&) = delete;
    WebGPUCompositorIntegration(WebGPUCompositorIntegration&&) = delete;
    WebGPUCompositorIntegration& operator=(const WebGPUCompositorIntegration&) = delete;
    WebGPUCompositorIntegration& operator=(WebGPUCompositorIntegration&&) = delete;
};

} // namespace WebCore
