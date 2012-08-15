/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef MediaSource_h
#define MediaSource_h

#if ENABLE(MEDIA_SOURCE)

#include "ContextDestructionObserver.h"
#include "MediaPlayer.h"
#include "SourceBuffer.h"
#include "SourceBufferList.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class MediaSource : public RefCounted<MediaSource>, public EventTarget, public ContextDestructionObserver {
public:
    static const String& openKeyword()
    {
        DEFINE_STATIC_LOCAL(const String, open, ("open"));
        return open;
    }

    static const String& closedKeyword()
    {
        DEFINE_STATIC_LOCAL(const String, closed, ("closed"));
        return closed;
    }

    static const String& endedKeyword()
    {
        DEFINE_STATIC_LOCAL(const String, ended, ("ended"));
        return ended;
    }

    static PassRefPtr<MediaSource> create(ScriptExecutionContext*);
    virtual ~MediaSource() { }

    SourceBufferList* sourceBuffers();
    SourceBufferList* activeSourceBuffers();

    SourceBuffer* addSourceBuffer(const String& type, ExceptionCode&);
    void removeSourceBuffer(SourceBuffer*, ExceptionCode&);

    const String& readyState() const;
    void setReadyState(const String&);

    void endOfStream(const String& error, ExceptionCode&);

    void setMediaPlayer(MediaPlayer* player) { m_player = player; }
    
    PassRefPtr<TimeRanges> buffered(const String& id, ExceptionCode&) const;
    void append(const String& id, PassRefPtr<Uint8Array> data, ExceptionCode&);
    void abort(const String& id, ExceptionCode&);
    bool setTimestampOffset(const String& id, double, ExceptionCode&);

    // EventTarget interface
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ScriptExecutionContext* scriptExecutionContext() const OVERRIDE;

    using RefCounted<MediaSource>::ref;
    using RefCounted<MediaSource>::deref;

private:
    explicit MediaSource(ScriptExecutionContext*);

    virtual EventTargetData* eventTargetData() OVERRIDE;
    virtual EventTargetData* ensureEventTargetData() OVERRIDE;

    virtual void refEventTarget() OVERRIDE { ref(); }
    virtual void derefEventTarget() OVERRIDE { deref(); }

    EventTargetData m_eventTargetData;

    String m_readyState;
    MediaPlayer* m_player;

    RefPtr<SourceBufferList> m_sourceBuffers;
    RefPtr<SourceBufferList> m_activeSourceBuffers;
};

} // namespace WebCore

#endif
#endif
