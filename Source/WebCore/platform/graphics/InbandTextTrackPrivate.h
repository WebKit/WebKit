/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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

#include "InbandTextTrackPrivateClient.h"

namespace WebCore {

class InbandTextTrackPrivate : public TrackPrivateBase {
public:
    enum class CueFormat : uint8_t {
        Data,
        Generic,
        WebVTT
    };
    static Ref<InbandTextTrackPrivate> create(CueFormat format) { return adoptRef(*new InbandTextTrackPrivate(format)); }
    virtual ~InbandTextTrackPrivate() = default;

    InbandTextTrackPrivateClient* client() const override { return m_client; }
    void setClient(InbandTextTrackPrivateClient* client) { m_client = client; }

    enum class Mode : uint8_t {
        Disabled,
        Hidden,
        Showing
    };
    virtual void setMode(Mode mode) { m_mode = mode; };
    virtual InbandTextTrackPrivate::Mode mode() const { return m_mode; }

    enum class Kind : uint8_t {
        Subtitles,
        Captions,
        Descriptions,
        Chapters,
        Metadata,
        Forced,
        None
    };
    virtual Kind kind() const { return Kind::Subtitles; }

    virtual bool isClosedCaptions() const { return false; }
    virtual bool isSDH() const { return false; }
    virtual bool containsOnlyForcedSubtitles() const { return false; }
    virtual bool isMainProgramContent() const { return true; }
    virtual bool isEasyToRead() const { return false; }
    virtual bool isDefault() const { return false; }
    AtomString label() const override { return emptyAtom(); }
    AtomString language() const override { return emptyAtom(); }
    AtomString id() const override { return emptyAtom(); }
    virtual AtomString inBandMetadataTrackDispatchType() const { return emptyAtom(); }

    virtual int textTrackIndex() const { return 0; }

    virtual CueFormat cueFormat() const { return m_format; }

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const override { return "InbandTextTrackPrivate"; }
#endif

protected:
    InbandTextTrackPrivate(CueFormat format)
        : m_format(format)
    {
    }

private:
    CueFormat m_format;
    InbandTextTrackPrivateClient* m_client { nullptr };
    Mode m_mode { Mode::Disabled };
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::InbandTextTrackPrivate::CueFormat> {
    using values = EnumValues<
        WebCore::InbandTextTrackPrivate::CueFormat,
        WebCore::InbandTextTrackPrivate::CueFormat::Data,
        WebCore::InbandTextTrackPrivate::CueFormat::Generic,
        WebCore::InbandTextTrackPrivate::CueFormat::WebVTT
    >;
};

template<> struct EnumTraits<WebCore::InbandTextTrackPrivate::Mode> {
    using values = EnumValues<
        WebCore::InbandTextTrackPrivate::Mode,
        WebCore::InbandTextTrackPrivate::Mode::Disabled,
        WebCore::InbandTextTrackPrivate::Mode::Hidden,
        WebCore::InbandTextTrackPrivate::Mode::Showing
    >;
};

template<> struct EnumTraits<WebCore::InbandTextTrackPrivate::Kind> {
    using values = EnumValues<
        WebCore::InbandTextTrackPrivate::Kind,
        WebCore::InbandTextTrackPrivate::Kind::Subtitles,
        WebCore::InbandTextTrackPrivate::Kind::Captions,
        WebCore::InbandTextTrackPrivate::Kind::Descriptions,
        WebCore::InbandTextTrackPrivate::Kind::Chapters,
        WebCore::InbandTextTrackPrivate::Kind::Metadata,
        WebCore::InbandTextTrackPrivate::Kind::Forced,
        WebCore::InbandTextTrackPrivate::Kind::None
    >;
};

} // namespace WTF

#endif // ENABLE(VIDEO)

