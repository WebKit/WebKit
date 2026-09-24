/*
 * Copyright (C) 2017-2025 Apple Inc. All rights reserved.
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

#if ENABLE(OFFSCREEN_CANVAS)

#include "CanvasRenderingContext.h"
#include "PlaceholderFrameIdentifier.h"
#include "PlaceholderRenderingContextSource.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class GraphicsLayerAsyncContentsDisplayDelegate;
class PlaceholderRenderingContext;

// What a placeholder's compositor layer shows. Shared with a source in the same process, so that it
// can update the layer straight from the OffscreenCanvas's thread.
class PlaceholderLayerContents final : public ThreadSafeRefCounted<PlaceholderLayerContents> {
    WTF_MAKE_TZONE_ALLOCATED(PlaceholderLayerContents);
public:
    static Ref<PlaceholderLayerContents> create() { return adoptRef(*new PlaceholderLayerContents); }

    // Shows the frame unless a newer one is already shown. On any thread.
    void copyFrame(ImageBuffer&, bool opaque, PlaceholderFrameIdentifier);
    // Like copyFrame(), for a buffer that nothing will draw into again. It reaches the compositor
    // with the next rendering update. On the main thread.
    void setFrameForNextDisplay(ImageBuffer&, bool opaque, PlaceholderFrameIdentifier);
    // On the main thread.
    void attach(GraphicsLayer&, ImageBuffer*, bool opaque, PlaceholderFrameIdentifier);

private:
    PlaceholderLayerContents() = default;

    Lock m_lock;
    RefPtr<GraphicsLayerAsyncContentsDisplayDelegate> m_delegate WTF_GUARDED_BY_LOCK(m_lock);
    PlaceholderFrameIdentifier m_frame WTF_GUARDED_BY_LOCK(m_lock);
};

// The source for an OffscreenCanvas in the same process as its placeholder, on any thread. Only the
// OffscreenCanvas holds it, so it goes away once the canvas leaves the process, and a source there
// takes over.
class LocalPlaceholderRenderingContextSource final : public PlaceholderRenderingContextSource {
    WTF_MAKE_TZONE_ALLOCATED(LocalPlaceholderRenderingContextSource);
public:
    static Ref<LocalPlaceholderRenderingContextSource> create(PlaceholderRenderingContext&);

    void setPlaceholderBuffer(ImageBuffer&, bool originClean, bool opaque) final;

private:
    explicit LocalPlaceholderRenderingContextSource(PlaceholderRenderingContext&);

    WeakPtr<PlaceholderRenderingContext> m_placeholder; // For main thread use.
    const Ref<PlaceholderLayerContents> m_layerContents;
    PlaceholderFrameIdentifier m_lastFrame; // For OffscreenCanvas holder thread use (main or worker).
};

class PlaceholderRenderingContext final : public CanvasRenderingContext {
    WTF_MAKE_TZONE_ALLOCATED(PlaceholderRenderingContext);
public:
    static std::unique_ptr<PlaceholderRenderingContext> create(HTMLCanvasElement&);
    static PlaceholderRenderingContext* fromIdentifier(PlaceholderRenderingContextIdentifier);

    ~PlaceholderRenderingContext();

    HTMLCanvasElement& NODELETE canvas() const;
    IntSize NODELETE size() const;
    void setPlaceholderBuffer(Ref<ImageBuffer>&&, PlaceholderFrameIdentifier, bool originClean, bool opaque);
    // A frame from a source in another process, whose buffer nothing will draw into again.
    void setPlaceholderBufferFromAnotherProcess(Ref<ImageBuffer>&&, PlaceholderFrameIdentifier, bool originClean, bool opaque);

    PlaceholderRenderingContextIdentifier identifier() const { return m_identifier; }
    PlaceholderLayerContents& layerContents() const { return m_layerContents; }

    RefPtr<ImageBuffer> surfaceBufferToImageBuffer(SurfaceBuffer) final;
    RefPtr<NativeImage> surfaceBufferToNativeImage(SurfaceBuffer) final;
    bool isSurfaceBufferTransparentBlack(SurfaceBuffer) const final;
    void didUpdateCanvasSizeProperties(bool) final;

private:
    PlaceholderRenderingContext(HTMLCanvasElement&);
    void setContentsToLayer(GraphicsLayer&) final;
    PixelFormat pixelFormat() const final;
    bool isOpaque() const final { return m_opaque; }

    const PlaceholderRenderingContextIdentifier m_identifier;
    const Ref<PlaceholderLayerContents> m_layerContents;
    PlaceholderFrameIdentifier m_frame;
    RefPtr<ImageBuffer> m_buffer; // Temporary until content is provided as NativeImage.
    RefPtr<NativeImage> m_bufferNativeImage;
    bool m_opaque { false };
};

}

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::PlaceholderRenderingContext, isPlaceholder())

#endif
