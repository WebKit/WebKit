/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "TrackPrivateBase.h"
#include <wtf/Function.h>

namespace WebCore {

class VideoTrackPrivateClient : public TrackPrivateBaseClient {
public:
    virtual void selectedChanged(bool) = 0;
};

class VideoTrackPrivate : public TrackPrivateBase {
public:
    void setClient(VideoTrackPrivateClient* client) { m_client = client; }
    VideoTrackPrivateClient* client() const override { return m_client; }

    virtual void setSelected(bool selected)
    {
        if (m_selected == selected)
            return;
        m_selected = selected;
        if (m_client)
            m_client->selectedChanged(m_selected);
        if (m_selectedChangedCallback)
            m_selectedChangedCallback(*this, m_selected);
    }
    virtual bool selected() const { return m_selected; }

    enum Kind { Alternative, Captions, Main, Sign, Subtitles, Commentary, None };
    virtual Kind kind() const { return None; }

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "VideoTrackPrivate"; }
#endif

    using SelectedChangedCallback = Function<void(VideoTrackPrivate&, bool selected)>;
    void setSelectedChangedCallback(SelectedChangedCallback&& callback) { m_selectedChangedCallback = WTFMove(callback); }


protected:
    VideoTrackPrivate() = default;

private:
    VideoTrackPrivateClient* m_client { nullptr };
    bool m_selected { false };
    SelectedChangedCallback m_selectedChangedCallback;
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::VideoTrackPrivate::Kind> {
    using values = EnumValues<
        WebCore::VideoTrackPrivate::Kind,
        WebCore::VideoTrackPrivate::Kind::Alternative,
        WebCore::VideoTrackPrivate::Kind::Captions,
        WebCore::VideoTrackPrivate::Kind::Main,
        WebCore::VideoTrackPrivate::Kind::Sign,
        WebCore::VideoTrackPrivate::Kind::Subtitles,
        WebCore::VideoTrackPrivate::Kind::Commentary,
        WebCore::VideoTrackPrivate::Kind::None
    >;
};

} // namespace WTF

#endif
