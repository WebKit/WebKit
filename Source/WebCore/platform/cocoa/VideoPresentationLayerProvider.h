/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "PlatformView.h"

OBJC_CLASS WebAVPlayerLayer;
OBJC_CLASS WebAVPlayerLayerView;

namespace WebCore {

class VideoPresentationLayerProvider {
public:
    WEBCORE_EXPORT virtual ~VideoPresentationLayerProvider();

    PlatformView *layerHostView() const { return m_layerHostView.get(); }
    void setLayerHostView(RetainPtr<PlatformView>&& layerHostView) { m_layerHostView = WTFMove(layerHostView); }

    WebAVPlayerLayer *playerLayer() const { return m_playerLayer.get(); }
    virtual void setPlayerLayer(RetainPtr<WebAVPlayerLayer>&& layer) { m_playerLayer = WTFMove(layer); }

#if PLATFORM(IOS_FAMILY)
    WebAVPlayerLayerView *playerLayerView() const { return m_playerLayerView.get(); }
    void setPlayerLayerView(RetainPtr<WebAVPlayerLayerView>&& playerLayerView) { m_playerLayerView = WTFMove(playerLayerView); }

    PlatformView *videoView() const { return m_videoView.get(); }
    void setVideoView(RetainPtr<PlatformView>&& videoView) { m_videoView = WTFMove(videoView); }
#endif

protected:
    WEBCORE_EXPORT VideoPresentationLayerProvider();

private:
    RetainPtr<PlatformView> m_layerHostView;
    RetainPtr<WebAVPlayerLayer> m_playerLayer;

#if PLATFORM(IOS_FAMILY)
    RetainPtr<WebAVPlayerLayerView> m_playerLayerView;
    RetainPtr<PlatformView> m_videoView;
#endif
};

}

#endif
