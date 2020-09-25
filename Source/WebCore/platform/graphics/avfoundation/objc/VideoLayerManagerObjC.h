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
#include "NativeImage.h"
#include "PlatformLayer.h"
#include "WebVideoContainerLayer.h"
#include <wtf/Function.h>
#include <wtf/LoggerHelper.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

class TextTrackRepresentation;

class VideoLayerManagerObjC final : public LoggerHelper {
    WTF_MAKE_NONCOPYABLE(VideoLayerManagerObjC);
    WTF_MAKE_FAST_ALLOCATED;
public:
    VideoLayerManagerObjC(const Logger&, const void*);

    PlatformLayer *videoInlineLayer() const { return m_videoInlineLayer.get(); }

    void setVideoLayer(PlatformLayer *, IntSize contentSize);
    void didDestroyVideoLayer();

#if ENABLE(VIDEO_PRESENTATION_MODE)
    PlatformLayer *videoFullscreenLayer() const { return m_videoFullscreenLayer.get(); }
    void setVideoFullscreenLayer(PlatformLayer *, WTF::Function<void()>&& completionHandler, NativeImagePtr);
    FloatRect videoFullscreenFrame() const { return m_videoFullscreenFrame; }
    void setVideoFullscreenFrame(FloatRect);
    void updateVideoFullscreenInlineImage(NativeImagePtr);
#endif

    bool requiresTextTrackRepresentation() const;
    void setTextTrackRepresentationLayer(PlatformLayer*);
    void syncTextTrackBounds();

private:
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const final { return "VideoLayerManagerObjC"; }
    WTFLogChannel& logChannel() const final;

    Ref<const Logger> m_logger;
    const void* m_logIdentifier;

    RetainPtr<WebVideoContainerLayer> m_videoInlineLayer;
#if ENABLE(VIDEO_PRESENTATION_MODE)
    RetainPtr<PlatformLayer> m_videoFullscreenLayer;
    FloatRect m_videoFullscreenFrame;
#endif
    RetainPtr<PlatformLayer> m_textTrackRepresentationLayer;

    RetainPtr<PlatformLayer> m_videoLayer;
};

}
