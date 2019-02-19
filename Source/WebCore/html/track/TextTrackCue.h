/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#include "Document.h"
#include <wtf/JSONValues.h>
#include <wtf/MediaTime.h>

namespace WebCore {

class TextTrack;

class TextTrackCue : public RefCounted<TextTrackCue>, public EventTargetWithInlineData {
public:
    static const AtomicString& cueShadowPseudoId();

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

    bool isActive();
    virtual void setIsActive(bool);

    virtual bool isOrderedBefore(const TextTrackCue*) const;
    virtual bool isPositionedAbove(const TextTrackCue* cue) const { return isOrderedBefore(cue); }

    bool hasEquivalentStartTime(const TextTrackCue&) const;

    enum CueType { Data, Generic, WebVTT };
    virtual CueType cueType() const = 0;
    virtual bool isRenderable() const { return false; }

    enum CueMatchRules { MatchAllFields, IgnoreDuration };
    virtual bool isEqual(const TextTrackCue&, CueMatchRules) const;
    virtual bool doesExtendCue(const TextTrackCue&) const;

    void willChange();
    virtual void didChange();

    String toJSONString() const;
    String debugString() const;

    using RefCounted::ref;
    using RefCounted::deref;

protected:
    TextTrackCue(ScriptExecutionContext&, const MediaTime& start, const MediaTime& end);

    Document& ownerDocument() { return downcast<Document>(m_scriptExecutionContext); }

    virtual void toJSON(JSON::Object&) const;

private:
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    using EventTarget::dispatchEvent;
    void dispatchEvent(Event&) final;

    EventTargetInterface eventTargetInterface() const final { return TextTrackCueEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return &m_scriptExecutionContext; }

    virtual bool cueContentsMatch(const TextTrackCue&) const;

    String m_id;
    MediaTime m_startTime;
    MediaTime m_endTime;
    int m_processingCueChanges { 0 };

    TextTrack* m_track { nullptr };

    ScriptExecutionContext& m_scriptExecutionContext;

    bool m_isActive : 1;
    bool m_pauseOnExit : 1;
};

} // namespace WebCore

namespace WTF {

template<typename> struct LogArgument;

template<> struct LogArgument<WebCore::TextTrackCue> {
    static String toString(const WebCore::TextTrackCue& cue) { return cue.toJSONString(); }
};

}

#endif
