/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

#include <WebCore/WebMediaSessionManagerClient.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakObjCPtr.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS WebView;

namespace WebCore {
class FloatRect;
class MediaPlaybackTarget;
class Page;
enum class MediaPlaybackTargetMockState : uint8_t;
}

class WebMediaPlaybackTargetPicker : public WebCore::WebMediaSessionManagerClient {
    WTF_MAKE_TZONE_ALLOCATED(WebMediaPlaybackTargetPicker);
public:
    static std::unique_ptr<WebMediaPlaybackTargetPicker> create(WebView *, WebCore::Page&);

    explicit WebMediaPlaybackTargetPicker(WebView *, WebCore::Page&);
    virtual ~WebMediaPlaybackTargetPicker() = default;

    void addPlaybackTargetPickerClient(WebCore::PlaybackTargetClientContextIdentifier);
    void removePlaybackTargetPickerClient(WebCore::PlaybackTargetClientContextIdentifier);
    void showPlaybackTargetPicker(WebCore::PlaybackTargetClientContextIdentifier, const WebCore::FloatRect&, bool hasVideo);
    void playbackTargetPickerClientStateDidChange(WebCore::PlaybackTargetClientContextIdentifier, WebCore::MediaProducerMediaStateFlags);
    void setMockMediaPlaybackTargetPickerEnabled(bool);
    void setMockMediaPlaybackTargetPickerState(const String&, WebCore::MediaPlaybackTargetMockState);
    void mockMediaPlaybackTargetPickerDismissPopup();

    void invalidate();

private:
    // WebMediaSessionManagerClient
    void setPlaybackTarget(WebCore::PlaybackTargetClientContextIdentifier, Ref<WebCore::MediaPlaybackTarget>&&) final;
    void externalOutputDeviceAvailableDidChange(WebCore::PlaybackTargetClientContextIdentifier, bool) final;
    void setShouldPlayToPlaybackTarget(WebCore::PlaybackTargetClientContextIdentifier, bool) final;
    void playbackTargetPickerWasDismissed(WebCore::PlaybackTargetClientContextIdentifier) final;
    RetainPtr<CocoaView> platformView() const final;

    WeakPtr<WebCore::Page> m_page;
    WeakObjCPtr<WebView> m_webView;
};

#endif
