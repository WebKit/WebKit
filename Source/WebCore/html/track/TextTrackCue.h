/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(VIDEO)

#include "DocumentFragment.h"
#include "HTMLElement.h"
#include <wtf/JSONValues.h>
#include <wtf/MediaTime.h>

namespace WebCore {

class TextTrack;
class TextTrackCue;

class TextTrackCueBox : public HTMLElement {
    WTF_MAKE_ISO_ALLOCATED(TextTrackCueBox);
public:
    static Ref<TextTrackCueBox> create(Document&, TextTrackCue&);

    TextTrackCue* getCue() const;
    virtual void applyCSSProperties(const IntSize&) { }

protected:
    void initialize();

    TextTrackCueBox(Document&, TextTrackCue&);
    ~TextTrackCueBox() { }

private:

    WeakPtr<TextTrackCue> m_cue;
};

class TextTrackCue : public RefCounted<TextTrackCue>, public EventTargetWithInlineData, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(TextTrackCue);
public:
    static const AtomString& cueShadowPseudoId();
    static const AtomString& cueBackdropShadowPseudoId();
    static const AtomString& cueBoxShadowPseudoId();

    static ExceptionOr<Ref<TextTrackCue>> create(Document&, double start, double end, DocumentFragment&);

    TextTrack* track() const;
    void setTrack(TextTrack*);

    const String& id() const { return m_id; }
    void setId(const String&);

    double startTime() const { return startMediaTime().toDouble(); }
    void setStartTime(double);

    double endTime() const { return endMediaTime().toDouble(); }
    void setEndTime(double);

    bool pauseOnExit() const { return m_pauseOnExit; }
    void setPauseOnExit(bool);

    MediaTime startMediaTime() const { return m_startTime; }
    void setStartTime(const MediaTime&);

    MediaTime endMediaTime() const { return m_endTime; }
    void setEndTime(const MediaTime&);

    bool isActive() const;
    virtual void setIsActive(bool);

    virtual bool isOrderedBefore(const TextTrackCue*) const;
    virtual bool isPositionedAbove(const TextTrackCue* cue) const { return isOrderedBefore(cue); }

    bool hasEquivalentStartTime(const TextTrackCue&) const;

    enum CueType { Generic, Data, ConvertedToWebVTT, WebVTT };
    virtual CueType cueType() const { return CueType::Generic; }
    virtual bool isRenderable() const;

    enum CueMatchRules { MatchAllFields, IgnoreDuration };
    bool isEqual(const TextTrackCue&, CueMatchRules) const;

    void willChange();
    virtual void didChange();

    virtual RefPtr<TextTrackCueBox> getDisplayTree(const IntSize& videoSize, int fontSize);
    virtual void removeDisplayTree();

    virtual RefPtr<DocumentFragment> getCueAsHTML();

    String toJSONString() const;

    using RefCounted::ref;
    using RefCounted::deref;

    virtual void recalculateStyles() { m_displayTreeNeedsUpdate = true; }
    virtual void setFontSize(int fontSize, const IntSize& videoSize, bool important);
    virtual void updateDisplayTree(const MediaTime&) { }

    unsigned cueIndex() const;

protected:
    TextTrackCue(Document&, const MediaTime& start, const MediaTime& end);

    Document& ownerDocument() { return m_document; }

    virtual bool cueContentsMatch(const TextTrackCue&) const;
    virtual void toJSON(JSON::Object&) const;

private:
    TextTrackCue(Document&, const MediaTime& start, const MediaTime& end, Ref<DocumentFragment>&&);

    // EventTarget
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    using EventTarget::dispatchEvent;
    void dispatchEvent(Event&) final;
    EventTargetInterface eventTargetInterface() const final { return TextTrackCueEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final;

    // ActiveDOMObject
    const char* activeDOMObjectName() const final;

    void rebuildDisplayTree();

    String m_id;
    MediaTime m_startTime;
    MediaTime m_endTime;
    int m_processingCueChanges { 0 };

    TextTrack* m_track { nullptr };

    Document& m_document;

    RefPtr<DocumentFragment> m_cueNode;
    RefPtr<TextTrackCueBox> m_displayTree;

    int m_fontSize { 0 };
    bool m_fontSizeIsImportant { false };

    bool m_isActive { false };
    bool m_pauseOnExit { false };
    bool m_displayTreeNeedsUpdate { true };
};

#ifndef NDEBUG
TextStream& operator<<(TextStream&, const TextTrackCue&);
#endif

} // namespace WebCore

namespace WTF {

template<typename> struct LogArgument;

template<> struct LogArgument<WebCore::TextTrackCue> {
    static String toString(const WebCore::TextTrackCue& cue) { return cue.toJSONString(); }
};

}

#endif
