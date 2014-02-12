/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) 2012, 2013 Apple Inc.  All rights reserved.
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

#ifndef TextTrackCue_h
#define TextTrackCue_h

#if ENABLE(VIDEO_TRACK)

#include "EventTarget.h"
#include "HTMLElement.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class TextTrack;

class TextTrackCue : public RefCounted<TextTrackCue>, public EventTargetWithInlineData {
public:
    static PassRefPtr<TextTrackCue> create(ScriptExecutionContext&, double start, double end, const String& content);

    static const AtomicString& cueShadowPseudoId()
    {
        DEFINE_STATIC_LOCAL(const AtomicString, cue, ("cue", AtomicString::ConstructFromLiteral));
        return cue;
    }

    virtual ~TextTrackCue();

    TextTrack* track() const;
    void setTrack(TextTrack*);

    const String& id() const { return m_id; }
    void setId(const String&);

    double startTime() const { return m_startTime; }
    void setStartTime(double, ExceptionCode&);

    double endTime() const { return m_endTime; }
    void setEndTime(double, ExceptionCode&);

    bool pauseOnExit() const { return m_pauseOnExit; }
    void setPauseOnExit(bool);

    int cueIndex();
    void invalidateCueIndex();

    using EventTarget::dispatchEvent;
    virtual bool dispatchEvent(PassRefPtr<Event>) override;

    bool isActive();
    virtual void setIsActive(bool);

    virtual EventTargetInterface eventTargetInterface() const override final { return TextTrackCueEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const override final { return &m_scriptExecutionContext; }

    virtual bool isOrderedBefore(const TextTrackCue*) const;

    enum CueType {
        Data,
        Generic,
        WebVTT
    };
    virtual CueType cueType() const = 0;
    virtual bool isRenderable() const { return false; }

    void willChange();
    virtual void didChange();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(enter);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(exit);

    using RefCounted<TextTrackCue>::ref;
    using RefCounted<TextTrackCue>::deref;

protected:
    TextTrackCue(ScriptExecutionContext&, double start, double end);

    Document& ownerDocument() { return toDocument(m_scriptExecutionContext); }

private:

    virtual void refEventTarget() override final { ref(); }
    virtual void derefEventTarget() override final { deref(); }

    String m_id;
    double m_startTime;
    double m_endTime;
    int m_cueIndex;
    int m_processingCueChanges;

    TextTrack* m_track;

    ScriptExecutionContext& m_scriptExecutionContext;

    bool m_isActive;
    bool m_pauseOnExit;
};

} // namespace WebCore

#endif
#endif
