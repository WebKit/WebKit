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

#pragma once

#if ENABLE(WEBGL)

#include "GraphicsContextGLANGLE.h"
#include "IOSurface.h"
#include "ProcessIdentity.h"
#include <array>

#if PLATFORM(MAC)
#include "ScopedHighPerformanceGPURequest.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include <memory>
#endif

OBJC_CLASS MTLSharedEventListener;

namespace WebCore {

class GraphicsLayerContentsDisplayDelegate;

#if ENABLE(VIDEO)
class GraphicsContextGLCVCocoa;
#endif

#if ENABLE(MEDIA_STREAM)
class ImageRotationSessionVT;
#endif

class WEBCORE_EXPORT GraphicsContextGLCocoa : public GraphicsContextGLANGLE {
public:
    static RefPtr<GraphicsContextGLCocoa> create(WebCore::GraphicsContextGLAttributes&&, ProcessIdentity&& resourceOwner);
    ~GraphicsContextGLCocoa();
    IOSurface* displayBufferSurface();

    std::tuple<GCGLenum, GCGLenum> externalImageTextureBindingPoint() final;

    enum class PbufferAttachmentUsage { Read, Write, ReadWrite };
    // Returns a handle which, if non-null, must be released via the
    // detach call below.
    void* createPbufferAndAttachIOSurface(GCGLenum target, PbufferAttachmentUsage, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLenum type, IOSurfaceRef, GCGLuint plane);
    void destroyPbufferAndDetachIOSurface(void* handle);

    std::optional<EGLImageAttachResult> createAndBindEGLImage(GCGLenum, EGLImageSource) final;

    RetainPtr<id> newSharedEventWithMachPort(mach_port_t);
    GCEGLSync createEGLSync(ExternalEGLSyncEvent) final;
    // Short term support for in-process WebGL.
    GCEGLSync createEGLSync(id, uint64_t);

    bool enableRequiredWebXRExtensions() final;

    void waitUntilWorkScheduled();

    // GraphicsContextGLANGLE overrides.
    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() override;
#if ENABLE(VIDEO)
    bool copyTextureFromMedia(MediaPlayer&, PlatformGLObject texture, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY) final;
#endif
#if ENABLE(VIDEO)
    GraphicsContextGLCV* asCV() final;
#endif
#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
    RefPtr<VideoFrame> surfaceBufferToVideoFrame(SurfaceBuffer) final;
#endif
    RefPtr<PixelBuffer> readCompositedResults() final;
    void setContextVisibility(bool) final;
    void setDrawingBufferColorSpace(const DestinationColorSpace&) final;
    void prepareForDisplay() override;

    void withBufferAsNativeImage(SurfaceBuffer, Function<void(NativeImage&)>) override;

#if PLATFORM(MAC)
    void updateContextOnDisplayReconfiguration();
#endif

    // Prepares current frame for display. The `finishedSignal` will be invoked once the frame has finished rendering.
    void prepareForDisplayWithFinishedSignal(Function<void()> finishedSignal);

protected:
    GraphicsContextGLCocoa(WebCore::GraphicsContextGLAttributes&&, ProcessIdentity&& resourceOwner);

    // GraphicsContextGLANGLE overrides.
    bool platformInitializeContext() final;
    bool platformInitialize() final;
    void invalidateKnownTextureContent(GCGLuint) final;
    bool reshapeDrawingBuffer() final;

    // IOSurface backing store for an image of a texture.
    // When preserveDrawingBuffer == false, this is the drawing buffer backing store.
    // When preserveDrawingBuffer == true, this is blitted to during display prepare.
    struct IOSurfacePbuffer {
        std::unique_ptr<IOSurface> surface;
        void* pbuffer { nullptr };
        operator bool() const { return !!surface; }
    };
    IOSurfacePbuffer& drawingBuffer();
    IOSurfacePbuffer& displayBuffer();
    IOSurfacePbuffer& surfaceBuffer(SurfaceBuffer);
    bool bindNextDrawingBuffer();
    void freeDrawingBuffers();

    // Inserts new fence that will invoke `signal` from a background thread when completed.
    // If not possible, calls the `signal`.
    void insertFinishedSignalOrInvoke(Function<void()> signal);

    ProcessIdentity m_resourceOwner;
    DestinationColorSpace m_drawingBufferColorSpace;
#if ENABLE(VIDEO)
    std::unique_ptr<GraphicsContextGLCVCocoa> m_cv;
#endif
#if PLATFORM(MAC)
    bool m_switchesGPUOnDisplayReconfiguration { false };
    ScopedHighPerformanceGPURequest m_highPerformanceGPURequest;
#endif
#if ENABLE(MEDIA_STREAM)
    std::unique_ptr<ImageRotationSessionVT> m_mediaSampleRotationSession;
    IntSize m_mediaSampleRotationSessionSize;
#endif
    RetainPtr<MTLSharedEventListener> m_finishedMetalSharedEventListener;
    RetainPtr<id> m_finishedMetalSharedEvent; // FIXME: Remove all C++ includees and use id<MTLSharedEvent>.

    static constexpr size_t maxReusedDrawingBuffers { 3 };
    size_t m_currentDrawingBufferIndex { 0 };
    std::array<IOSurfacePbuffer, maxReusedDrawingBuffers> m_drawingBuffers;
    friend class GraphicsContextGLCVCocoa;
};

}

#endif
