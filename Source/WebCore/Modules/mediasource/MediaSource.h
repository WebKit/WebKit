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
#include "MediaPlayer.h"
#include "MediaPromiseTypes.h"
#include "MediaSourceInit.h"
#include "MediaSourcePrivateClient.h"
#include "URLRegistry.h"
#include <optional>
#include <wtf/LoggerHelper.h>
#include <wtf/NativePromise.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AudioTrack;
class AudioTrackPrivate;
class ContentType;
class InbandTextTrackPrivate;
class HTMLMediaElement;
class MediaSourceClientImpl;
class MediaSourceHandle;
class SourceBuffer;
class SourceBufferList;
class SourceBufferPrivate;
class TextTrack;
class TimeRanges;
class VideoTrack;
class VideoTrackPrivate;
template<typename> class ExceptionOr;

enum class MediaSourceReadyState { Closed, Open, Ended };

class MediaSource
    : public RefCounted<MediaSource>
    , public ActiveDOMObject
    , public EventTarget
    , public URLRegistrable
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
    , private Logger::Observer
#endif
{
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(MediaSource);
public:
    static void setRegistry(URLRegistry*);
    static MediaSource* lookup(const String& url) { return s_registry ? downcast<MediaSource>(s_registry->lookup(url)) : nullptr; }

    static Ref<MediaSource> create(ScriptExecutionContext&, MediaSourceInit&&);
    virtual ~MediaSource();

    // ContextDestructionObserver.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
    USING_CAN_MAKE_WEAKPTR(ActiveDOMObject);

    static bool enabledForContext(ScriptExecutionContext&);

    void addedToRegistry();
    void removedFromRegistry();
    void openIfInEndedState();
    void openIfDeferredOpen();
    bool isOpen() const;
    virtual void monitorSourceBuffers();
    bool isClosed() const;
    bool isEnded() const;
    void sourceBufferDidChangeActiveState(SourceBuffer&, bool);
    MediaTime duration() const;
    PlatformTimeRanges buffered() const;

    enum class EndOfStreamError { Network, Decode };
    void streamEndedWithError(std::optional<EndOfStreamError>);

    bool attachToElement(WeakPtr<HTMLMediaElement>&&);
    void elementIsShuttingDown();
    void detachFromElement();
    bool isSeeking() const { return !!m_pendingSeekTarget; }
    PlatformTimeRanges seekable();
    ExceptionOr<void> setLiveSeekableRange(double start, double end);
    ExceptionOr<void> clearLiveSeekableRange();

    ExceptionOr<void> setDuration(double);
    ExceptionOr<void> setDurationInternal(const MediaTime&);
    MediaTime currentTime() const;

    using ReadyState = MediaSourceReadyState;
    ReadyState readyState() const;
    ExceptionOr<void> endOfStream(std::optional<EndOfStreamError>);

    Ref<SourceBufferList> sourceBuffers() const;
    Ref<SourceBufferList> activeSourceBuffers() const;
    ExceptionOr<Ref<SourceBuffer>> addSourceBuffer(const String& type);
    ExceptionOr<void> removeSourceBuffer(SourceBuffer&);
    static bool isTypeSupported(ScriptExecutionContext&, const String& type);

#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    Ref<MediaSourceHandle> handle();
    static bool canConstructInDedicatedWorker(ScriptExecutionContext&);
    void registerTransferredHandle(MediaSourceHandle&);
#endif
    bool detachable() const { return m_detachable; }

    ScriptExecutionContext* scriptExecutionContext() const final;
    using ActiveDOMObject::protectedScriptExecutionContext;

    static const MediaTime& currentTimeFudgeFactor();
    static bool contentTypeShouldGenerateTimestamps(const ContentType&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    uint64_t logIdentifier() const final { return m_logIdentifier; }
    ASCIILiteral logClassName() const final { return "MediaSource"_s; }
    WTFLogChannel& logChannel() const final;
    void setLogIdentifier(uint64_t);

    Ref<Logger> logger(ScriptExecutionContext&);
    void didLogMessage(const WTFLogChannel&, WTFLogLevel, Vector<JSONLogValue>&&) final;
#endif

    virtual bool isManaged() const { return false; }
    virtual bool streaming() const { return false; }
    void memoryPressure();

    void setAsSrcObject(bool);

    // Called by SourceBuffer.
    void sourceBufferBufferedChanged();
    void sourceBufferReceivedFirstInitializationSegmentChanged();
    void sourceBufferActiveTrackFlagChanged(bool);
    void setMediaPlayerReadyState(MediaPlayer::ReadyState);
    void incrementDroppedFrameCount();
    void addAudioTrackToElement(Ref<AudioTrack>&&);
    void addTextTrackToElement(Ref<TextTrack>&&);
    void addVideoTrackToElement(Ref<VideoTrack>&&);
    void addAudioTrackMirrorToElement(Ref<AudioTrackPrivate>&&, bool enabled);
    void addTextTrackMirrorToElement(Ref<InbandTextTrackPrivate>&&);
    void addVideoTrackMirrorToElement(Ref<VideoTrackPrivate>&&, bool selected);

    Ref<MediaSourcePrivateClient> client() const;

protected:
    MediaSource(ScriptExecutionContext&, MediaSourceInit&&);

    bool isBuffered(const PlatformTimeRanges&) const;

    void scheduleEvent(const AtomString& eventName);
    void notifyElementUpdateMediaState() const;
    void ensureWeakOnHTMLMediaElementContext(Function<void(HTMLMediaElement&)>&&) const;

    virtual void elementDetached() { }

    RefPtr<MediaSourcePrivate> protectedPrivate() const;

    WeakPtr<HTMLMediaElement> m_mediaElement;
    bool m_detachable { false };

private:
    friend class MediaSourceClientImpl;

    // ActiveDOMObject.
    void stop() final;
    bool virtualHasPendingActivity() const final;

    static bool isTypeSupported(ScriptExecutionContext&, const String& type, Vector<ContentType>&& contentTypesRequiringHardwareSupport);

    void setPrivate(RefPtr<MediaSourcePrivate>&&);
    void setPrivateAndOpen(Ref<MediaSourcePrivate>&&);
    void reOpen();
    void open();

    void removeSourceBufferWithOptionalDestruction(SourceBuffer&, bool withDestruction);

    Ref<MediaTimePromise> waitForTarget(const SeekTarget&);
    using RendererType = MediaSourcePrivateClient::RendererType;
    void failedToCreateRenderer(RendererType);

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    enum EventTargetInterfaceType eventTargetInterface() const final;

    // URLRegistrable.
    URLRegistry& registry() const final;
    RegistrableType registrableType() const final { return RegistrableType::MediaSource; }

    void setReadyState(ReadyState);
    void onReadyStateChange(ReadyState oldState, ReadyState newState);

    Vector<PlatformTimeRanges> activeRanges() const;

    ExceptionOr<Ref<SourceBufferPrivate>> createSourceBufferPrivate(const ContentType&);

    void regenerateActiveSourceBuffers();
    void updateBufferedIfNeeded(bool forced = false);

    bool hasBufferedTime(const MediaTime&);
    bool hasCurrentTime();
    bool hasFutureTime();

    void completeSeek();

    static URLRegistry* s_registry;

    const Ref<SourceBufferList> m_sourceBuffers;
    const Ref<SourceBufferList> m_activeSourceBuffers;
    std::optional<SeekTarget> m_pendingSeekTarget;
    std::optional<MediaTimePromise::AutoRejectProducer> m_seekTargetPromise;
    bool m_openDeferred { false };
    bool m_sourceopenPending { false };
    bool m_isAttached { false };
    std::optional<ReadyState> m_readyStateBeforeDetached;
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    RefPtr<MediaSourceHandle> m_handle;
#endif

#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    uint64_t m_logIdentifier { 0 };
#endif
    std::atomic<uint64_t> m_associatedRegistryCount { 0 };
    RefPtr<MediaSourcePrivate> m_private;
    const Ref<MediaSourceClientImpl> m_client;
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

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MediaSource)
    static bool isType(const WebCore::URLRegistrable& registrable) { return registrable.registrableType() == WebCore::URLRegistrable::RegistrableType::MediaSource; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
