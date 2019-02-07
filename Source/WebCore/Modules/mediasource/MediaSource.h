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

#pragma once

#if ENABLE(MEDIA_SOURCE)

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "GenericEventQueue.h"
#include "MediaSourcePrivateClient.h"
#include "URLRegistry.h"
#include <wtf/LoggerHelper.h>

namespace WebCore {

class ContentType;
class HTMLMediaElement;
class SourceBuffer;
class SourceBufferList;
class SourceBufferPrivate;
class TimeRanges;

class MediaSource final
    : public MediaSourcePrivateClient
    , public ActiveDOMObject
    , public EventTargetWithInlineData
    , public URLRegistrable
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static void setRegistry(URLRegistry*);
    static MediaSource* lookup(const String& url) { return s_registry ? static_cast<MediaSource*>(s_registry->lookup(url)) : nullptr; }

    static Ref<MediaSource> create(ScriptExecutionContext&);
    virtual ~MediaSource();

    void addedToRegistry();
    void removedFromRegistry();
    void openIfInEndedState();
    bool isOpen() const;
    bool isClosed() const;
    bool isEnded() const;
    void sourceBufferDidChangeActiveState(SourceBuffer&, bool);

    enum class EndOfStreamError { Network, Decode };
    void streamEndedWithError(Optional<EndOfStreamError>);

    MediaTime duration() const final;
    void durationChanged(const MediaTime&) final;
    std::unique_ptr<PlatformTimeRanges> buffered() const final;

    bool attachToElement(HTMLMediaElement&);
    void detachFromElement(HTMLMediaElement&);
    void monitorSourceBuffers() override;
    bool isSeeking() const { return m_pendingSeekTime.isValid(); }
    Ref<TimeRanges> seekable();
    ExceptionOr<void> setLiveSeekableRange(double start, double end);
    ExceptionOr<void> clearLiveSeekableRange();

    ExceptionOr<void> setDuration(double);
    ExceptionOr<void> setDurationInternal(const MediaTime&);
    MediaTime currentTime() const;

    enum class ReadyState { Closed, Open, Ended };
    ReadyState readyState() const { return m_readyState; }
    ExceptionOr<void> endOfStream(Optional<EndOfStreamError>);

    HTMLMediaElement* mediaElement() const { return m_mediaElement; }

    SourceBufferList* sourceBuffers() { return m_sourceBuffers.get(); }
    SourceBufferList* activeSourceBuffers() { return m_activeSourceBuffers.get(); }
    ExceptionOr<Ref<SourceBuffer>> addSourceBuffer(const String& type);
    ExceptionOr<void> removeSourceBuffer(SourceBuffer&);
    static bool isTypeSupported(const String& type);

    ScriptExecutionContext* scriptExecutionContext() const final;

    using RefCounted::ref;
    using RefCounted::deref;

    bool hasPendingActivity() const final;

    static const MediaTime& currentTimeFudgeFactor();
    static bool contentTypeShouldGenerateTimestamps(const ContentType&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const final { return "MediaSource"; }
    WTFLogChannel& logChannel() const final;
    void setLogIdentifier(const void*) final;
#endif

private:
    explicit MediaSource(ScriptExecutionContext&);

    void suspend(ReasonForSuspension) final;
    void resume() final;
    void stop() final;
    bool canSuspendForDocumentSuspension() const final;
    const char* activeDOMObjectName() const final;

    void setPrivateAndOpen(Ref<MediaSourcePrivate>&&) final;
    void seekToTime(const MediaTime&) final;

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    EventTargetInterface eventTargetInterface() const final;

    URLRegistry& registry() const final;

    void setReadyState(ReadyState);
    void onReadyStateChange(ReadyState oldState, ReadyState newState);

    Vector<PlatformTimeRanges> activeRanges() const;

    ExceptionOr<Ref<SourceBufferPrivate>> createSourceBufferPrivate(const ContentType&);
    void scheduleEvent(const AtomicString& eventName);

    bool hasBufferedTime(const MediaTime&);
    bool hasCurrentTime();
    bool hasFutureTime();

    void regenerateActiveSourceBuffers();

    void completeSeek();

    static URLRegistry* s_registry;

    RefPtr<MediaSourcePrivate> m_private;
    RefPtr<SourceBufferList> m_sourceBuffers;
    RefPtr<SourceBufferList> m_activeSourceBuffers;
    mutable std::unique_ptr<PlatformTimeRanges> m_buffered;
    std::unique_ptr<PlatformTimeRanges> m_liveSeekable;
    HTMLMediaElement* m_mediaElement { nullptr };
    MediaTime m_duration;
    MediaTime m_pendingSeekTime;
    ReadyState m_readyState { ReadyState::Closed };
    GenericEventQueue m_asyncEventQueue;
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier { nullptr };
#endif
};

String convertEnumerationToString(MediaSource::EndOfStreamError);
String convertEnumerationToString(MediaSource::ReadyState);

} // namespace WebCore

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::MediaSource::EndOfStreamError> {
    static String toString(const WebCore::MediaSource::EndOfStreamError error)
    {
        return convertEnumerationToString(error);
    }
};

template <>
struct LogArgument<WebCore::MediaSource::ReadyState> {
    static String toString(const WebCore::MediaSource::ReadyState state)
    {
        return convertEnumerationToString(state);
    }
};

} // namespace WTF

#endif
