/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "GenericEventQueue.h"
#include "MediaSourcePrivate.h"
#include "MediaSourcePrivateClient.h"
#include "ScriptWrappable.h"
#include "SourceBuffer.h"
#include "SourceBufferList.h"
#include "URLRegistry.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class GenericEventQueue;

class MediaSource : public MediaSourcePrivateClient, public ActiveDOMObject, public EventTargetWithInlineData, public ScriptWrappable, public URLRegistrable {
public:
    static void setRegistry(URLRegistry*);
    static MediaSource* lookup(const String& url) { return s_registry ? static_cast<MediaSource*>(s_registry->lookup(url)) : 0; }

    static const AtomicString& openKeyword();
    static const AtomicString& closedKeyword();
    static const AtomicString& endedKeyword();

    static PassRefPtr<MediaSource> create(ScriptExecutionContext&);
    virtual ~MediaSource();

    void addedToRegistry();
    void removedFromRegistry();
    void openIfInEndedState();
    bool isOpen() const;
    bool isClosed() const;
    bool isEnded() const;
    void sourceBufferDidChangeAcitveState(SourceBuffer*, bool);
    void streamEndedWithError(const AtomicString& error, ExceptionCode&);

    // MediaSourcePrivateClient
    virtual void setPrivateAndOpen(PassRef<MediaSourcePrivate>) override;
    virtual MediaTime duration() const override;
    virtual std::unique_ptr<PlatformTimeRanges> buffered() const override;
    virtual void seekToTime(const MediaTime&) override;

    bool attachToElement(HTMLMediaElement*);
    void close();
    void monitorSourceBuffers();
    bool isSeeking() const { return m_pendingSeekTime.isValid(); }
    void completeSeek();

    void setDuration(double, ExceptionCode&);
    void setDurationInternal(const MediaTime&);
    MediaTime currentTime() const;
    const AtomicString& readyState() const { return m_readyState; }
    void setReadyState(const AtomicString&);
    void endOfStream(ExceptionCode&);
    void endOfStream(const AtomicString& error, ExceptionCode&);

    HTMLMediaElement* mediaElement() const { return m_mediaElement; }

    // MediaSource.idl methods
    SourceBufferList* sourceBuffers() { return m_sourceBuffers.get(); }
    SourceBufferList* activeSourceBuffers() { return m_activeSourceBuffers.get(); }
    SourceBuffer* addSourceBuffer(const String& type, ExceptionCode&);
    void removeSourceBuffer(SourceBuffer*, ExceptionCode&);
    static bool isTypeSupported(const String& type);

    // ActiveDOMObject interface
    virtual bool hasPendingActivity() const override;
    virtual void stop() override;

    // EventTarget interface
    virtual ScriptExecutionContext* scriptExecutionContext() const override final;
    virtual void refEventTarget() override final { ref(); }
    virtual void derefEventTarget() override final { deref(); }
    virtual EventTargetInterface eventTargetInterface() const override;

    // URLRegistrable interface
    virtual URLRegistry& registry() const override;

    using RefCounted<MediaSourcePrivateClient>::ref;
    using RefCounted<MediaSourcePrivateClient>::deref;

protected:
    explicit MediaSource(ScriptExecutionContext&);

    void onReadyStateChange(const AtomicString& oldState, const AtomicString& newState);
    Vector<PlatformTimeRanges> activeRanges() const;

    RefPtr<SourceBufferPrivate> createSourceBufferPrivate(const ContentType&, ExceptionCode&);
    void scheduleEvent(const AtomicString& eventName);
    GenericEventQueue& asyncEventQueue() { return m_asyncEventQueue; }

    void regenerateActiveSourceBuffers();

    static URLRegistry* s_registry;

    RefPtr<MediaSourcePrivate> m_private;
    RefPtr<SourceBufferList> m_sourceBuffers;
    RefPtr<SourceBufferList> m_activeSourceBuffers;
    HTMLMediaElement* m_mediaElement;
    MediaTime m_duration;
    MediaTime m_pendingSeekTime;
    AtomicString m_readyState;
    GenericEventQueue m_asyncEventQueue;
};

}

#endif

#endif
