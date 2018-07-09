/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "FloatRect.h"
#include "IntSize.h"
#include "PlatformLayer.h"
#include "VideoFullscreenLayerManager.h"
#include <wtf/Function.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

class VideoFullscreenLayerManagerObjC final : public VideoFullscreenLayerManager {
    WTF_MAKE_FAST_ALLOCATED;
public:
    VideoFullscreenLayerManagerObjC();

    PlatformLayer *videoInlineLayer() const final { return m_videoInlineLayer.get(); }
    PlatformLayer *videoFullscreenLayer() const final { return m_videoFullscreenLayer.get(); }
    FloatRect videoFullscreenFrame() const final { return m_videoFullscreenFrame; }
    void setVideoLayer(PlatformLayer *, IntSize contentSize) final;
    void setVideoFullscreenLayer(PlatformLayer *, WTF::Function<void()>&& completionHandler, NativeImagePtr) final;
    void updateVideoFullscreenInlineImage(NativeImagePtr) final;
    void setVideoFullscreenFrame(FloatRect) final;
    void didDestroyVideoLayer() final;

    bool requiresTextTrackRepresentation() const final;
    void setTextTrackRepresentation(TextTrackRepresentation*) final;
    void syncTextTrackBounds() final;

private:
    RetainPtr<PlatformLayer> m_textTrackRepresentationLayer;
    RetainPtr<PlatformLayer> m_videoInlineLayer;
    RetainPtr<PlatformLayer> m_videoFullscreenLayer;
    RetainPtr<PlatformLayer> m_videoLayer;
    FloatRect m_videoFullscreenFrame;
    FloatRect m_videoInlineFrame;
};

}

