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

#if ENABLE(WEBGL) && ENABLE(WEBGL_WEBGPU) && PLATFORM(COCOA)

#include <WebCore/ColorSpace.h>
#include <WebCore/GraphicsContextGLANGLE.h>
#include <WebCore/WebGPUCppAPI.h>
#include <optional>
#include <wtf/MachSendRight.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class GraphicsContextGLANGLEWebGPUDisplayDelegate;
class WebGPUCompositorIntegration;
class WebGPUIntegration;

// A WebGL context that runs ANGLE on its WebGPU backend, on the WebGPU device that create() gets.
// The drawing buffers are the render buffers of a canvas presentation context of the device, and the
// compositor integration shows them, as for a WebGPU canvas.
class GraphicsContextGLANGLEWebGPU final : public GraphicsContextGLANGLE, public CanMakeWeakPtr<GraphicsContextGLANGLEWebGPU> {
    WTF_MAKE_TZONE_ALLOCATED(GraphicsContextGLANGLEWebGPU);
public:
    // ANGLE gets the device and its instance as the C API handles of WebGPU.framework, and calls the C
    // API of the framework on them, which calls the device. In the Web Process, the device is a proxy
    // of a device of the GPU Process.
    WEBCORE_EXPORT static RefPtr<GraphicsContextGLANGLEWebGPU> create(const GraphicsContextGLAttributes&, Ref<WebGPUIntegration>&&, Ref<::WebGPU::Device>&&);
    ~GraphicsContextGLANGLEWebGPU();

    // GraphicsContextGL overrides.
    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() final;
    void prepareForDisplay() final;
    void setDrawingBufferColorSpace(const ColorSpace&) final;
#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
    RefPtr<VideoFrame> surfaceBufferToVideoFrame(SurfaceBuffer) final;
#endif
#if ENABLE(VIDEO)
    bool copyTextureFromVideoFrame(VideoFrame&, PlatformGLObject texture, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLenum type, bool unpackFlipY, bool unpackPremultiplyAlpha) final;
#endif
#if ENABLE(WEBXR)
    bool addFoveation(IntSize, IntSize, IntSize, std::span<const GCGLfloat>, std::span<const GCGLfloat>, std::span<const GCGLfloat>) final;
    void enableFoveation(GCGLuint) final;
    void disableFoveation() final;
#endif

private:
    GraphicsContextGLANGLEWebGPU(const GraphicsContextGLAttributes&, Ref<WebGPUIntegration>&&, Ref<::WebGPU::Device>&&, Ref<WebGPUCompositorIntegration>&&, Ref<::WebGPU::PresentationContext>&&);

    // GraphicsContextGLANGLE overrides.
    bool platformInitializeContext() final;
    bool platformInitializeExtensions() final;
    bool platformInitialize() final;
    bool reshapeDrawingBuffer() final;
    RefPtr<PixelBuffer> readCompositedResults() final;

    GCGLDisplay initializeEGLDisplay(intptr_t displayKey);
    // Starts a frame in the render buffer, and binds it to m_texture.
    bool bindDrawingBuffer(uint32_t index);
    void freeDrawingBuffers();
    void didPrepareForDisplay(uint32_t frameIndex, MachSendRight&& displayBuffer, std::optional<uint32_t> replacedRenderBufferIndex);

    const Ref<WebGPUIntegration> m_gpu;
    const Ref<::WebGPU::Device> m_device;
    const Ref<WebGPUCompositorIntegration> m_compositorIntegration;
    const Ref<::WebGPU::PresentationContext> m_presentationContext;
    const Ref<GraphicsContextGLANGLEWebGPUDisplayDelegate> m_layerContentsDisplayDelegate;
    ColorSpace m_drawingBufferColorSpace;

    // A render buffer, and the EGLImage of its texture. Not in m_eglImages, which is the namespace of
    // the WebXR external images.
    struct DrawingBuffer {
        RefPtr<::WebGPU::Texture> texture;
        void* image { nullptr };
    };
    Vector<DrawingBuffer> m_drawingBuffers;
    uint32_t m_currentDrawingBuffer { 0 };
    // The render buffer that the layer shows.
    std::optional<uint32_t> m_displayBufferIndex;
};

} // namespace WebCore

#endif // ENABLE(WEBGL) && ENABLE(WEBGL_WEBGPU) && PLATFORM(COCOA)
