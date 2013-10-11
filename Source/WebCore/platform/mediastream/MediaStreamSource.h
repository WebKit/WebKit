/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef MediaStreamSource_h
#define MediaStreamSource_h

#if ENABLE(MEDIA_STREAM)

#include "AudioDestinationConsumer.h"
#include "MediaConstraints.h"
#include "MediaStreamSourceCapabilities.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AudioBus;
class MediaConstraints;
class MediaStreamDescriptor;
struct MediaStreamSourceStates;

class MediaStreamSource : public RefCounted<MediaStreamSource> {
public:
    class Observer {
    public:
        virtual ~Observer() { }
        virtual void sourceStateChanged() = 0;
        virtual void sourceMutedChanged() = 0;
        virtual void sourceEnabledChanged() = 0;
        virtual bool stopped() = 0;
    };

    enum Type { Audio, Video };
    enum ReadyState { New = 0, Live = 1, Ended = 2 };

    virtual ~MediaStreamSource() { }

    bool isAudioStreamSource() const { return type() == Audio; }
    virtual bool useIDForTrackID() const { return false; }

    void reset();

    const String& id() const { return m_id; }
    Type type() const { return m_type; }
    const String& name() const { return m_name; }

    virtual RefPtr<MediaStreamSourceCapabilities> capabilities() const = 0;
    virtual const MediaStreamSourceStates& states() = 0;
    
    void setReadyState(ReadyState);
    ReadyState readyState() const { return m_readyState; }

    void addObserver(Observer*);
    void removeObserver(Observer*);

    void setConstraints(PassRefPtr<MediaConstraints>);
    MediaConstraints* constraints() const;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool);

    bool muted() const { return m_muted; }
    void setMuted(bool);

    bool readonly() const;
    void setReadonly(bool readonly) { m_readonly = readonly; }

    bool remote() const { return m_remote; }
    void setRemote(bool remote) { m_remote = remote; }

    void stop();

    MediaStreamDescriptor* stream() const { return m_stream; }
    void setStream(MediaStreamDescriptor*);
    
protected:
    MediaStreamSource(const String& id, Type, const String& name);

private:
    String m_id;
    Type m_type;
    String m_name;
    ReadyState m_readyState;
    Vector<Observer*> m_observers;
    RefPtr<MediaConstraints> m_constraints;
    MediaStreamDescriptor* m_stream;
    MediaStreamSourceStates m_states;

    bool m_enabled;
    bool m_muted;
    bool m_readonly;
    bool m_remote;
};

typedef Vector<RefPtr<MediaStreamSource> > MediaStreamSourceVector;

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // MediaStreamSource_h
