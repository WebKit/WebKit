/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef RealtimeMediaSource_h
#define RealtimeMediaSource_h

#if ENABLE(MEDIA_STREAM)

#include "MediaConstraints.h"
#include "RealtimeMediaSourceCapabilities.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AudioBus;
class MediaConstraints;
class MediaStreamPrivate;
class RealtimeMediaSourceStates;

class RealtimeMediaSource : public RefCounted<RealtimeMediaSource> {
public:
    class Observer {
    public:
        virtual ~Observer() { }
        
        // Source state changes.
        virtual void sourceStopped() = 0;
        virtual void sourceMutedChanged() = 0;

        // Observer state queries.
        virtual bool preventSourceFromStopping() = 0;
    };

    virtual ~RealtimeMediaSource() { }

    bool isAudioStreamSource() const { return type() == Audio; }

    const String& id() const { return m_id; }

    enum Type { None, Audio, Video };
    Type type() const { return m_type; }

    virtual const String& name() const { return m_name; }
    virtual void setName(const String& name) { m_name = name; }

    virtual RefPtr<RealtimeMediaSourceCapabilities> capabilities() const = 0;
    virtual const RealtimeMediaSourceStates& states() = 0;
    
    bool stopped() const { return m_stopped; }

    virtual bool muted() const { return m_muted; }
    virtual void setMuted(bool);

    virtual bool readonly() const;
    virtual void setReadonly(bool readonly) { m_readonly = readonly; }

    virtual bool remote() const { return m_remote; }
    virtual void setRemote(bool remote) { m_remote = remote; }

    void addObserver(Observer*);
    void removeObserver(Observer*);

    virtual void startProducingData() { }
    virtual void stopProducingData() { }

    void stop(Observer* callingObserver = nullptr);
    void requestStop(Observer* callingObserver = nullptr);

    void reset();

protected:
    RealtimeMediaSource(const String& id, Type, const String& name);

private:
    String m_id;
    Type m_type;
    String m_name;
    bool m_stopped;
    Vector<Observer*> m_observers;

    bool m_muted;
    bool m_readonly;
    bool m_remote;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // RealtimeMediaSource_h
