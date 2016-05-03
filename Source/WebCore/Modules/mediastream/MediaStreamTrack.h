/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "MediaStreamTrackPrivate.h"
#include "RealtimeMediaSource.h"
#include "ScriptWrappable.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AudioSourceProvider;
class Dictionary;
class MediaConstraintsImpl;
class MediaSourceSettings;
class MediaTrackConstraints;

class MediaStreamTrack final : public RefCounted<MediaStreamTrack>, public ActiveDOMObject, public EventTargetWithInlineData, private MediaStreamTrackPrivate::Observer {
public:
    class Observer {
    public:
        virtual ~Observer() { }
        virtual void trackDidEnd() = 0;
    };

    static Ref<MediaStreamTrack> create(ScriptExecutionContext&, MediaStreamTrackPrivate&);
    virtual ~MediaStreamTrack();

    const AtomicString& kind() const;
    const String& id() const;
    const String& label() const;

    bool enabled() const;
    void setEnabled(bool);

    bool muted() const;
    bool readonly() const;
    bool remote() const;

    enum class State { New, Live, Ended };
    State readyState() const;

    bool ended() const;

    RefPtr<MediaStreamTrack> clone();
    void stopProducingData();

    RefPtr<MediaTrackConstraints> getConstraints() const;
    RefPtr<MediaSourceSettings> getSettings() const;
    RefPtr<RealtimeMediaSourceCapabilities> getCapabilities() const;
    void applyConstraints(const Dictionary&);
    void applyConstraints(const MediaConstraints&);

    RealtimeMediaSource& source() const { return m_private->source(); }
    MediaStreamTrackPrivate& privateTrack() { return m_private.get(); }

    AudioSourceProvider* audioSourceProvider();

    void addObserver(Observer*);
    void removeObserver(Observer*);

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return MediaStreamTrackEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    using RefCounted<MediaStreamTrack>::ref;
    using RefCounted<MediaStreamTrack>::deref;

private:
    MediaStreamTrack(ScriptExecutionContext&, MediaStreamTrackPrivate&);
    explicit MediaStreamTrack(MediaStreamTrack&);

    void configureTrackRendering();

    // ActiveDOMObject API.
    void stop() final;
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    // EventTarget
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    // MediaStreamTrackPrivate::Observer
    void trackEnded(MediaStreamTrackPrivate&) override;
    void trackMutedChanged(MediaStreamTrackPrivate&) override;
    void trackSettingsChanged(MediaStreamTrackPrivate&) override;
    void trackEnabledChanged(MediaStreamTrackPrivate&) override;

    Vector<Observer*> m_observers;
    Ref<MediaStreamTrackPrivate> m_private;

    RefPtr<MediaConstraintsImpl> m_constraints;

    bool m_ended { false };
};

typedef Vector<RefPtr<MediaStreamTrack>> MediaStreamTrackVector;

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
