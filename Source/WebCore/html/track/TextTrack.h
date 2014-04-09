/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2012, 2013 Apple Inc.  All rights reserved.
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

#ifndef TextTrack_h
#define TextTrack_h

#if ENABLE(VIDEO_TRACK)

#include "ExceptionCode.h"
#include "TextTrackCue.h"
#include "TrackBase.h"
#include "VTTCue.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

#if USE(PLATFORM_TEXT_TRACK_MENU)
#include "PlatformTextTrack.h"
#endif

namespace WebCore {

class ScriptExecutionContext;
class TextTrack;
class TextTrackCueList;
#if ENABLE(WEBVTT_REGIONS)
class VTTRegion;
class VTTRegionList;
#endif

class TextTrackClient {
public:
    virtual ~TextTrackClient() { }
    virtual void textTrackKindChanged(TextTrack*) = 0;
    virtual void textTrackModeChanged(TextTrack*) = 0;
    virtual void textTrackAddCues(TextTrack*, const TextTrackCueList*) = 0;
    virtual void textTrackRemoveCues(TextTrack*, const TextTrackCueList*) = 0;
    virtual void textTrackAddCue(TextTrack*, PassRefPtr<TextTrackCue>) = 0;
    virtual void textTrackRemoveCue(TextTrack*, PassRefPtr<TextTrackCue>) = 0;
};

class TextTrack : public TrackBase, public EventTargetWithInlineData
#if USE(PLATFORM_TEXT_TRACK_MENU)
    , public PlatformTextTrackClient
#endif
    {
public:
    static PassRefPtr<TextTrack> create(ScriptExecutionContext* context, TextTrackClient* client, const AtomicString& kind, const AtomicString& id, const AtomicString& label, const AtomicString& language)
    {
        return adoptRef(new TextTrack(context, client, kind, id, label, language, AddTrack));
    }
    virtual ~TextTrack();

    virtual EventTargetInterface eventTargetInterface() const override final { return TextTrackEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const override final { return m_scriptExecutionContext; }

    static TextTrack* captionMenuOffItem();
    static TextTrack* captionMenuAutomaticItem();

    static const AtomicString& subtitlesKeyword();
    static const AtomicString& captionsKeyword();
    static const AtomicString& descriptionsKeyword();
    static const AtomicString& chaptersKeyword();
    static const AtomicString& metadataKeyword();
    static const AtomicString& forcedKeyword();
    virtual const AtomicString& defaultKindKeyword() const override { return subtitlesKeyword(); }
    static bool isValidKindKeyword(const AtomicString&);

    static const AtomicString& disabledKeyword();
    static const AtomicString& hiddenKeyword();
    static const AtomicString& showingKeyword();

    virtual void setKind(const AtomicString&) override;

    virtual AtomicString inBandMetadataTrackDispatchType() const { return emptyString(); }

    AtomicString mode() const { return m_mode; }
    virtual void setMode(const AtomicString&);

    enum ReadinessState { NotLoaded = 0, Loading = 1, Loaded = 2, FailedToLoad = 3 };
    ReadinessState readinessState() const { return m_readinessState; }
    void setReadinessState(ReadinessState state) { m_readinessState = state; }

    TextTrackCueList* cues();
    TextTrackCueList* activeCues() const;

    virtual void clearClient() override { m_client = 0; }
    TextTrackClient* client() { return m_client; }

    void addCue(PassRefPtr<TextTrackCue>, ExceptionCode&);
    virtual void removeCue(TextTrackCue*, ExceptionCode&);

    bool hasCue(VTTCue*, VTTCue::CueMatchRules = VTTCue::MatchAllFields);

#if ENABLE(VIDEO_TRACK) && ENABLE(WEBVTT_REGIONS)
    VTTRegionList* regions();
    void addRegion(PassRefPtr<VTTRegion>);
    void removeRegion(VTTRegion*, ExceptionCode&);
#endif

    void cueWillChange(TextTrackCue*);
    void cueDidChange(TextTrackCue*);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(cuechange);

    enum TextTrackType { TrackElement, AddTrack, InBand };
    TextTrackType trackType() const { return m_trackType; }

    virtual bool isClosedCaptions() const { return false; }
    virtual bool isSDH() const { return false; }
    virtual bool containsOnlyForcedSubtitles() const { return false; }
    virtual bool isMainProgramContent() const;
    virtual bool isEasyToRead() const { return false; }

    int trackIndex();
    void invalidateTrackIndex();

    bool isRendered();
    int trackIndexRelativeToRenderedTracks();

    bool hasBeenConfigured() const { return m_hasBeenConfigured; }
    void setHasBeenConfigured(bool flag) { m_hasBeenConfigured = flag; }

    virtual bool isDefault() const { return false; }
    virtual void setIsDefault(bool) { }

    void removeAllCues();

#if USE(PLATFORM_TEXT_TRACK_MENU)
    PassRefPtr<PlatformTextTrack> platformTextTrack();
#endif

#if ENABLE(MEDIA_SOURCE)
    virtual void setLanguage(const AtomicString&) override;
#endif

    using RefCounted<TrackBase>::ref;
    using RefCounted<TrackBase>::deref;

protected:
    TextTrack(ScriptExecutionContext*, TextTrackClient*, const AtomicString& kind, const AtomicString& id, const AtomicString& label, const AtomicString& language, TextTrackType);

    RefPtr<TextTrackCueList> m_cues;

private:
    virtual bool isValidKind(const AtomicString&) const override;

    virtual bool enabled() const override;

    virtual void refEventTarget() override final { ref(); }
    virtual void derefEventTarget() override final { deref(); }

#if ENABLE(VIDEO_TRACK) && ENABLE(WEBVTT_REGIONS)
    VTTRegionList* ensureVTTRegionList();
    RefPtr<VTTRegionList> m_regions;
#endif

#if USE(PLATFORM_TEXT_TRACK_MENU)
    virtual TextTrack* publicTrack() override { return this; }

    RefPtr<PlatformTextTrack> m_platformTextTrack;
#endif

    TextTrackCueList* ensureTextTrackCueList();

    ScriptExecutionContext* m_scriptExecutionContext;
    AtomicString m_mode;
    TextTrackClient* m_client;
    TextTrackType m_trackType;
    ReadinessState m_readinessState;
    int m_trackIndex;
    int m_renderedTrackIndex;
    bool m_hasBeenConfigured;
};

inline TextTrack* toTextTrack(TrackBase* track)
{
    ASSERT_WITH_SECURITY_IMPLICATION(track->type() == TrackBase::TextTrack);
    return static_cast<TextTrack*>(track);
}

} // namespace WebCore

#endif
#endif
