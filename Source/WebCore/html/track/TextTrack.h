/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011-2021 Apple Inc.  All rights reserved.
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

#include "ContextDestructionObserver.h"
#include "PlatformTimeRanges.h"
#include "TextTrackCue.h"
#include "TrackBase.h"

namespace WebCore {

class ScriptExecutionContext;
class TextTrack;
class TextTrackCueList;
class VTTRegion;
class VTTRegionList;

class TextTrackClient {
public:
    virtual ~TextTrackClient() = default;
    virtual void textTrackKindChanged(TextTrack&) = 0;
    virtual void textTrackModeChanged(TextTrack&) = 0;
    virtual void textTrackAddCues(TextTrack&, const TextTrackCueList&) = 0;
    virtual void textTrackRemoveCues(TextTrack&, const TextTrackCueList&) = 0;
    virtual void textTrackAddCue(TextTrack&, TextTrackCue&) = 0;
    virtual void textTrackRemoveCue(TextTrack&, TextTrackCue&) = 0;
};

class TextTrack : public TrackBase, public EventTargetWithInlineData, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(TextTrack);
public:
    static Ref<TextTrack> create(Document*, TextTrackClient*, const AtomString& kind, const AtomString& id, const AtomString& label, const AtomString& language);
    virtual ~TextTrack();

    static TextTrack& captionMenuOffItem();
    static TextTrack& captionMenuAutomaticItem();

    static const AtomString& subtitlesKeyword();
    static bool isValidKindKeyword(const AtomString&);

    enum class Kind { Subtitles, Captions, Descriptions, Chapters, Metadata, Forced };
    Kind kind() const;

    Kind kindForBindings() const;
    void setKindForBindings(Kind);

    const AtomString& kindKeyword() const;
    void setKindKeywordIgnoringASCIICase(StringView);

    virtual AtomString inBandMetadataTrackDispatchType() const { return emptyString(); }

    enum class Mode { Disabled, Hidden, Showing };
    Mode mode() const;
    virtual void setMode(Mode);

    enum ReadinessState { NotLoaded = 0, Loading = 1, Loaded = 2, FailedToLoad = 3 };
    ReadinessState readinessState() const { return m_readinessState; }
    void setReadinessState(ReadinessState state) { m_readinessState = state; }

    TextTrackCueList* cues();
    TextTrackCueList* activeCues() const;

    TextTrackCueList* cuesInternal() const { return m_cues.get(); }

    void clearClient() override { m_client = nullptr; }

    ExceptionOr<void> addCue(Ref<TextTrackCue>&&);
    virtual ExceptionOr<void> removeCue(TextTrackCue&);

    VTTRegionList* regions();
    void addRegion(Ref<VTTRegion>&&);
    ExceptionOr<void> removeRegion(VTTRegion&);

    void cueWillChange(TextTrackCue&);
    void cueDidChange(TextTrackCue&);

    enum TextTrackType { TrackElement, AddTrack, InBand };
    TextTrackType trackType() const { return m_trackType; }

    virtual bool isClosedCaptions() const { return false; }
    virtual bool isSDH() const { return false; }
    virtual bool containsOnlyForcedSubtitles() const;
    virtual bool isMainProgramContent() const;
    virtual bool isEasyToRead() const { return false; }

    int trackIndex();
    void invalidateTrackIndex();

    bool isRendered();
    int trackIndexRelativeToRenderedTracks();

    bool hasBeenConfigured() const { return m_hasBeenConfigured; }
    void setHasBeenConfigured(bool flag) { m_hasBeenConfigured = flag; }

    virtual bool isDefault() const { return false; }

    void removeAllCues();

#if ENABLE(MEDIA_SOURCE)
    void setLanguage(const AtomString&) override;
#endif

    virtual bool isInband() const { return false; }

    virtual MediaTime startTimeVariance() const { return MediaTime::zeroTime(); }

    using RefCounted::ref;
    using RefCounted::deref;

    const std::optional<Vector<String>>& styleSheets() const { return m_styleSheets; }

    virtual bool shouldPurgeCuesFromUnbufferedRanges() const { return false; }
    virtual void removeCuesNotInTimeRanges(PlatformTimeRanges&);

protected:
    TextTrack(ScriptExecutionContext*, TextTrackClient*, const AtomString& kind, const AtomString& id, const AtomString& label, const AtomString& language, TextTrackType);

    Document& document() const;
    TextTrackClient* client() { return m_client; }

    bool hasCue(TextTrackCue&, TextTrackCue::CueMatchRules = TextTrackCue::MatchAllFields);
    void setKind(Kind);

    RefPtr<TextTrackCueList> m_cues;
    std::optional<Vector<String>> m_styleSheets;

private:
    EventTargetInterface eventTargetInterface() const final { return TextTrackEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }

    bool enabled() const override;

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // ActiveDOMObject
    const char* activeDOMObjectName() const final;

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const override { return "TextTrack"; }
#endif

    VTTRegionList& ensureVTTRegionList();
    RefPtr<VTTRegionList> m_regions;

    TextTrackCueList& ensureTextTrackCueList();

    Mode m_mode { Mode::Disabled };
    Kind m_kind { Kind::Subtitles };
    TextTrackClient* m_client;
    TextTrackType m_trackType;
    ReadinessState m_readinessState { NotLoaded };
    std::optional<int> m_trackIndex;
    std::optional<int> m_renderedTrackIndex;
    bool m_hasBeenConfigured { false };
};

inline auto TextTrack::mode() const -> Mode
{
    return m_mode;
}

inline auto TextTrack::kind() const -> Kind
{
    return m_kind;
}

inline auto TextTrack::kindForBindings() const -> Kind
{
    return kind();
}

#if !ENABLE(MEDIA_SOURCE)

inline void TextTrack::setKindForBindings(Kind)
{
    // FIXME: We are using kindForBindings only to implement this empty function, preserving the
    // behavior of doing nothing when trying to set the kind, originally implemented in a custom setter.
    // Once we no longer need this special case, we should remove kindForBindings and setKindForBindings.
}

#else

inline void TextTrack::setKindForBindings(Kind kind)
{
    setKind(kind);
}

#endif

String convertEnumerationToString(TextTrack::Mode); // Defined in JSTextTrack.cpp
String convertEnumerationToString(TextTrack::Kind);

} // namespace WebCore

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::TextTrack::Kind> {
    static String toString(const WebCore::TextTrack::Kind kind)
    {
        return convertEnumerationToString(kind);
    }
};

template <>
struct LogArgument<WebCore::TextTrack::Mode> {
    static String toString(const WebCore::TextTrack::Mode mode)
    {
        return convertEnumerationToString(mode);
    }
};

template<> struct DefaultHash<WebCore::TextTrack::Kind> : IntHash<WebCore::TextTrack::Kind> { };
template<> struct HashTraits<WebCore::TextTrack::Kind> : StrongEnumHashTraits<WebCore::TextTrack::Kind> { };

} // namespace WTF

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::TextTrack)
    static bool isType(const WebCore::TrackBase& track) { return track.type() == WebCore::TrackBase::TextTrack; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
