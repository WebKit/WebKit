/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

#include <WebCore/MediaPlaybackTarget.h>
#include <WebCore/MediaPlaybackTargetContext.h>
#include <WebCore/WebMediaSessionManagerClient.h>
#include <wtf/Ref.h>

namespace WebCore {
class FloatRect;
class MediaPlaybackTarget;
class Page;
}

class WebMediaPlaybackTargetPicker : public WebCore::WebMediaSessionManagerClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<WebMediaPlaybackTargetPicker> create(WebCore::Page&);

    explicit WebMediaPlaybackTargetPicker(WebCore::Page&);
    virtual ~WebMediaPlaybackTargetPicker() { }

    void addPlaybackTargetPickerClient(uint64_t);
    void removePlaybackTargetPickerClient(uint64_t);
    void showPlaybackTargetPicker(uint64_t, const WebCore::FloatRect&, bool hasVideo);
    void playbackTargetPickerClientStateDidChange(uint64_t, WebCore::MediaProducer::MediaStateFlags);
    void setMockMediaPlaybackTargetPickerEnabled(bool);
    void setMockMediaPlaybackTargetPickerState(const String&, WebCore::MediaPlaybackTargetContext::State);

    // WebMediaSessionManagerClient
    void setPlaybackTarget(uint64_t, Ref<WebCore::MediaPlaybackTarget>&&) override;
    void externalOutputDeviceAvailableDidChange(uint64_t, bool) override;
    void setShouldPlayToPlaybackTarget(uint64_t, bool) override;

    void invalidate();

private:
    WebCore::Page* m_page;
};

#endif
