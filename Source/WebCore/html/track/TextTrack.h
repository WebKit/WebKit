/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2012 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "TrackBase.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class HTMLMediaElement;
class TextTrack;
class TextTrackCue;
class TextTrackCueList;

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

class TextTrack : public TrackBase {
public:
    static PassRefPtr<TextTrack> create(ScriptExecutionContext* context, TextTrackClient* client, const AtomicString& kind, const AtomicString& label, const AtomicString& language)
    {
        return adoptRef(new TextTrack(context, client, kind, label, language, AddTrack));
    }
    virtual ~TextTrack();

    void setMediaElement(HTMLMediaElement* element) { m_mediaElement = element; }
    HTMLMediaElement* mediaElement() { return m_mediaElement; }

    AtomicString kind() const { return m_kind; }
    void setKind(const AtomicString&);

    static const AtomicString& subtitlesKeyword();
    static const AtomicString& captionsKeyword();
    static const AtomicString& descriptionsKeyword();
    static const AtomicString& chaptersKeyword();
    static const AtomicString& metadataKeyword();
    static bool isValidKindKeyword(const AtomicString&);

    AtomicString label() const { return m_label; }
    void setLabel(const AtomicString& label) { m_label = label; }

    AtomicString language() const { return m_language; }
    void setLanguage(const AtomicString& language) { m_language = language; }

    static const AtomicString& disabledKeyword();
    static const AtomicString& hiddenKeyword();
    static const AtomicString& showingKeyword();

    virtual AtomicString mode() const;
    virtual void setMode(const AtomicString&);

    bool showingByDefault() const { return m_showingByDefault; }
    void setShowingByDefault(bool showing) { m_showingByDefault = showing; }

    enum ReadinessState { NotLoaded = 0, Loading = 1, Loaded = 2, FailedToLoad = 3 };
    ReadinessState readinessState() const { return m_readinessState; }
    void setReadinessState(ReadinessState state) { m_readinessState = state; }

    TextTrackCueList* cues();
    TextTrackCueList* activeCues() const;

    void clearClient() { m_client = 0; }
    TextTrackClient* client() { return m_client; }

    void addCue(PassRefPtr<TextTrackCue>);
    void removeCue(TextTrackCue*, ExceptionCode&);

    void cueWillChange(TextTrackCue*);
    void cueDidChange(TextTrackCue*);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(cuechange);

    enum TextTrackType { TrackElement, AddTrack, InBand };
    TextTrackType trackType() const { return m_trackType; }
    
    enum WebVTTNodeType {WebVTTNodeTypeNone, WebVTTNodeTypeFuture, WebVTTNodeTypePast};

    int trackIndex();
    void invalidateTrackIndex();

    bool isRendered();
    int trackIndexRelativeToRenderedTracks();

    bool hasBeenConfigured() const { return m_hasBeenConfigured; }
    void setHasBeenConfigured(bool flag) { m_hasBeenConfigured = flag; }

    virtual bool isDefault() const { return false; }
    virtual void setIsDefault(bool) { }

    void removeAllCues();

protected:
    TextTrack(ScriptExecutionContext*, TextTrackClient*, const AtomicString& kind, const AtomicString& label, const AtomicString& language, TextTrackType);

    RefPtr<TextTrackCueList> m_cues;

private:
    TextTrackCueList* ensureTextTrackCueList();
    HTMLMediaElement* m_mediaElement;
    AtomicString m_kind;
    AtomicString m_label;
    AtomicString m_language;
    AtomicString m_mode;
    TextTrackClient* m_client;
    TextTrackType m_trackType;
    ReadinessState m_readinessState;
    int m_trackIndex;
    int m_renderedTrackIndex;
    bool m_showingByDefault;
    bool m_hasBeenConfigured;
};

} // namespace WebCore

#endif
#endif
