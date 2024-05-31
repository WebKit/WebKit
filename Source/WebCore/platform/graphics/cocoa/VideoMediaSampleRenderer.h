/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "ProcessIdentity.h"
#include "SampleMap.h"
#include <wtf/Function.h>
#include <wtf/Ref.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>

OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_PROTOCOL(WebSampleBufferVideoRendering);
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
typedef struct __CVBuffer* CVPixelBufferRef;
#endif

namespace WebCore {

class WebCoreDecompressionSession;

class VideoMediaSampleRenderer : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<VideoMediaSampleRenderer> {
public:
    static Ref<VideoMediaSampleRenderer> create(WebSampleBufferVideoRendering *renderer) { return adoptRef(*new VideoMediaSampleRenderer(renderer)); }
    ~VideoMediaSampleRenderer();

    bool isReadyForMoreMediaData() const;
    void requestMediaDataWhenReady(Function<void()>&&);
    void enqueueSample(CMSampleBufferRef, bool displaying = true);
    void stopRequestingMediaData();

    void flush();

    void expectMinimumUpcomingSampleBufferPresentationTime(const MediaTime&);
    void resetUpcomingSampleBufferPresentationTimeExpectations();

    WebSampleBufferVideoRendering *renderer() const { return m_renderer.get(); }
    AVSampleBufferDisplayLayer *displayLayer() const;
#if HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
    RetainPtr<CVPixelBufferRef> copyDisplayedPixelBuffer() const;
    CGRect bounds() const;
#endif

    void setResourceOwner(const ProcessIdentity& resourceOwner) { m_resourceOwner = resourceOwner; }

private:
    VideoMediaSampleRenderer(WebSampleBufferVideoRendering *);
    void resetReadyForMoreSample();
    void initializeDecompressionSession();

    RetainPtr<WebSampleBufferVideoRendering> m_renderer;
    RefPtr<WebCoreDecompressionSession> m_decompressionSession;
    bool m_displayLayerReadyForMoreSample { false };
    bool m_decompressionSessionReadyForMoreSample { false };
    Function<void()> m_readyForMoreSampleFunction;
    uint32_t m_decodePending { 0 };
    bool m_wasNotDisplaying { false };
    bool m_requestMediaDataWhenReadySet { false };
    std::optional<uint32_t> m_currentCodec;
    std::optional<MediaTime> m_minimumUpcomingPresentationTime;

    ProcessIdentity m_resourceOwner;
};

} // namespace WebCore
