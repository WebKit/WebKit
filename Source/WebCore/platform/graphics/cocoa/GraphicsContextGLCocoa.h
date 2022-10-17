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

#if PLATFORM(MAC)
#include "ScopedHighPerformanceGPURequest.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include <memory>
#endif

namespace WebCore {

class ProcessIdentity;

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
    IOSurface* displayBuffer();
    void markDisplayBufferInUse();

    enum class PbufferAttachmentUsage { Read, Write, ReadWrite };
    // Returns a handle which, if non-null, must be released via the
    // detach call below.
    void* createPbufferAndAttachIOSurface(GCGLenum target, PbufferAttachmentUsage, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLenum type, IOSurfaceRef, GCGLuint plane);
    void destroyPbufferAndDetachIOSurface(void* handle);
#if !PLATFORM(IOS_FAMILY_SIMULATOR)
    using IOSurfaceTextureAttachment = std::optional<std::tuple<void*, unsigned, unsigned>>;
    IOSurfaceTextureAttachment attachIOSurfaceToSharedTexture(GCGLenum target, IOSurface*);
    void detachIOSurfaceFromSharedTexture(void* handle);
#endif
#if USE(MTLSHAREDEVENT_FOR_XR_FRAME_COMPLETION)
    RetainPtr<id> newSharedEventWithMachPort(mach_port_t);
    void* createSyncWithSharedEvent(const RetainPtr<id>& sharedEvent, uint64_t signalValue);
    bool destroySync(void* sync);
    void clientWaitSyncWithFlush(void* sync, uint64_t timeout);
#endif

    // GraphicsContextGLANGLE overrides.
    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() override;
#if ENABLE(VIDEO)
    bool copyTextureFromMedia(MediaPlayer&, PlatformGLObject texture, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY) final;
#endif
#if ENABLE(VIDEO)
    GraphicsContextGLCV* asCV() final;
#endif
#if ENABLE(MEDIA_STREAM)
    RefPtr<VideoFrame> paintCompositedResultsToVideoFrame() final;
#endif
    void setContextVisibility(bool) final;
    void prepareForDisplay() override;

#if PLATFORM(MAC)
    void updateContextOnDisplayReconfiguration();
#endif
protected:
    GraphicsContextGLCocoa(WebCore::GraphicsContextGLAttributes&&, ProcessIdentity&& resourceOwner);

    // GraphicsContextGLANGLE overrides.
    bool platformInitializeContext() final;
    bool platformInitialize() final;
    void invalidateKnownTextureContent(GCGLuint) final;
    bool reshapeDisplayBufferBacking() final;
    bool allocateAndBindDisplayBufferBacking();
    bool bindDisplayBufferBacking(std::unique_ptr<IOSurface> backing, void* pbuffer);

    ProcessIdentity m_resourceOwner;
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

    friend class GraphicsContextGLCVCocoa;
};

}

#endif
