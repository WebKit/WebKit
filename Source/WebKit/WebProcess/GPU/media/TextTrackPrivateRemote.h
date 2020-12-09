/*
* Copyright (C) 2020 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
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

#if ENABLE(GPU_PROCESS)

#include "DataReference.h"
#include "TextTrackPrivateRemoteConfiguration.h"
#include "TrackPrivateRemoteIdentifier.h"
#include <WebCore/InbandTextTrackPrivate.h>

namespace IPC {
class Connection;
}

namespace WebCore {
class InbandGenericCue;
class ISOWebVTTCue;
}

namespace WebKit {

class MediaPlayerPrivateRemote;

class TextTrackPrivateRemote final : public WebCore::InbandTextTrackPrivate {
    WTF_MAKE_NONCOPYABLE(TextTrackPrivateRemote)
public:

    static Ref<TextTrackPrivateRemote> create(IPC::Connection& connection, TrackPrivateRemoteIdentifier idendifier, TextTrackPrivateRemoteConfiguration&& configuration)
    {
        return adoptRef(*new TextTrackPrivateRemote(connection, idendifier, WTFMove(configuration)));
    }

    void addDataCue(MediaTime&& start, MediaTime&& end, IPC::DataReference&&);

#if ENABLE(DATACUE_VALUE)
    using SerializedPlatformDataCueValue = WebCore::SerializedPlatformDataCueValue;
    void addDataCueWithType(MediaTime&& start, MediaTime&& end, SerializedPlatformDataCueValue&&, String&&);
    void updateDataCue(MediaTime&& start, MediaTime&& end, SerializedPlatformDataCueValue&&);
    void removeDataCue(MediaTime&& start, MediaTime&& end, SerializedPlatformDataCueValue&&);
#endif

    using InbandGenericCue = WebCore::InbandGenericCue;
    void addGenericCue(Ref<InbandGenericCue>);
    void updateGenericCue(Ref<InbandGenericCue>);
    void removeGenericCue(Ref<InbandGenericCue>);

    using ISOWebVTTCue = WebCore::ISOWebVTTCue;
    void parseWebVTTFileHeader(String&&);
    void parseWebVTTCueData(const IPC::DataReference&);
    void parseWebVTTCueDataStruct(ISOWebVTTCue&&);

    void updateConfiguration(TextTrackPrivateRemoteConfiguration&&);

    AtomString id() const final { return m_id; }
    AtomString label() const final { return m_label; }
    AtomString language() const final { return m_language; }
    int trackIndex() const final { return m_trackIndex; }

    using TextTrackCueFormat = WebCore::InbandTextTrackPrivate::CueFormat;
    TextTrackCueFormat cueFormat() const final { return m_format; }

    using TextTrackKind = WebCore::InbandTextTrackPrivate::Kind;
    TextTrackKind kind() const final { return m_kind; }

    using TextTrackMode = WebCore::InbandTextTrackPrivate::Mode;
    void setMode(TextTrackMode) final;
    TextTrackMode mode() const final { return m_mode; }

    bool isClosedCaptions() const final { return m_isClosedCaptions; }
    bool isSDH() const final { return m_isSDH; }
    bool containsOnlyForcedSubtitles() const final { return m_containsOnlyForcedSubtitles; }
    bool isMainProgramContent() const final { return m_isMainProgramContent; }
    bool isEasyToRead() const final { return m_isEasyToRead; }
    bool isDefault() const final { return m_isDefault; }

private:
    TextTrackPrivateRemote(IPC::Connection&, TrackPrivateRemoteIdentifier, TextTrackPrivateRemoteConfiguration&&);

    IPC::Connection& m_connection;
    AtomString m_id;
    AtomString m_label;
    AtomString m_language;
    int m_trackIndex { -1 };
    MediaTime m_startTimeVariance { MediaTime::zeroTime() };
    TrackPrivateRemoteIdentifier m_idendifier;

    TextTrackCueFormat m_format { TextTrackCueFormat::Generic };
    TextTrackKind m_kind { TextTrackKind::None };
    TextTrackMode m_mode { TextTrackMode::Disabled };
    bool m_isClosedCaptions { false };
    bool m_isSDH { false };
    bool m_containsOnlyForcedSubtitles { false };
    bool m_isMainProgramContent { true };
    bool m_isEasyToRead { false };
    bool m_isDefault { false };
};

} // namespace WebKit

#endif
