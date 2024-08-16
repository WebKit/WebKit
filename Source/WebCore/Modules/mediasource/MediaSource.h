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
#include "HTMLMediaElement.h"
#include "MediaPlayer.h"
#include "MediaPromiseTypes.h"
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
class MediaSourceClientImpl;
class MediaSourceHandle;
class SourceBuffer;
class SourceBufferList;
class SourceBufferPrivate;
class TextTrack;
class TimeRanges;
class VideoTrack;
class VideoTrackPrivate;

enum class MediaSourceReadyState { Closed, Open, Ended };

class MediaSource
    : public RefCounted<MediaSource>
    , public CanMakeWeakPtr<MediaSource>
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
    static MediaSource* lookup(const String& url) { return s_registry ? static_cast<MediaSource*>(s_registry->lookup(url)) : nullptr; }

    static Ref<MediaSource> create(ScriptExecutionContext&);
    virtual ~MediaSource();

    using CanMakeWeakPtr<MediaSource>::weakPtrFactory;
    using CanMakeWeakPtr<MediaSource>::WeakValueType;
    using CanMakeWeakPtr<MediaSource>::WeakPtrImplType;

    // ActiveDOMObject.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

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
    void detachFromElement();
    bool isSeeking() const { return !!m_pendingSeekTarget; }
    Ref<TimeRanges> seekable();
    ExceptionOr<void> setLiveSeekableRange(double start, double end);
    ExceptionOr<void> clearLiveSeekableRange();

    ExceptionOr<void> setDuration(double);
    ExceptionOr<void> setDurationInternal(const MediaTime&);
    MediaTime currentTime() const;

    using ReadyState = MediaSourceReadyState;
    ReadyState readyState() const;
    ExceptionOr<void> endOfStream(std::optional<EndOfStreamError>);

    SourceBufferList* sourceBuffers() { return m_sourceBuffers.get(); }
    SourceBufferList* activeSourceBuffers() { return m_activeSourceBuffers.get(); }
    ExceptionOr<Ref<SourceBuffer>> addSourceBuffer(const String& type);
    ExceptionOr<void> removeSourceBuffer(SourceBuffer&);
    static bool isTypeSupported(ScriptExecutionContext&, const String& type);

#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    Ref<MediaSourceHandle> handle();
    static bool canConstructInDedicatedWorker(ScriptExecutionContext&);
    void registerTransferredHandle(MediaSourceHandle&);
#endif

    ScriptExecutionContext* scriptExecutionContext() const final;

    static const MediaTime& currentTimeFudgeFactor();
    static bool contentTypeShouldGenerateTimestamps(const ContentType&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    ASCIILiteral logClassName() const final { return "MediaSource"_s; }
    WTFLogChannel& logChannel() const final;
    void setLogIdentifier(const void*);

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
    explicit MediaSource(ScriptExecutionContext&);

    bool isBuffered(const PlatformTimeRanges&) const;

    void scheduleEvent(const AtomString& eventName);
    void notifyElementUpdateMediaState() const;
    void ensureWeakOnHTMLMediaElementContext(Function<void(HTMLMediaElement&)>&&) const;

    virtual void elementDetached() { }

    RefPtr<MediaSourcePrivate> m_private;
    WeakPtr<HTMLMediaElement> m_mediaElement;

private:
    friend class MediaSourceClientImpl;

    // ActiveDOMObject.
    void stop() final;
    bool virtualHasPendingActivity() const final;

    static bool isTypeSupported(ScriptExecutionContext&, const String& type, Vector<ContentType>&& contentTypesRequiringHardwareSupport);

    void setPrivateAndOpen(Ref<MediaSourcePrivate>&&);
    Ref<MediaTimePromise> waitForTarget(const SeekTarget&);
    Ref<MediaPromise> seekToTime(const MediaTime&);
    using RendererType = MediaSourcePrivateClient::RendererType;
    void failedToCreateRenderer(RendererType);

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    enum EventTargetInterfaceType eventTargetInterface() const final;

    URLRegistry& registry() const final;

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

    RefPtr<SourceBufferList> m_sourceBuffers;
    RefPtr<SourceBufferList> m_activeSourceBuffers;
    std::optional<SeekTarget> m_pendingSeekTarget;
    std::optional<MediaTimePromise::AutoRejectProducer> m_seekTargetPromise;
    bool m_openDeferred { false };
    bool m_sourceopenPending { false };
    bool m_isAttached { false };
#if ENABLE(MEDIA_SOURCE_IN_WORKERS)
    RefPtr<MediaSourceHandle> m_handle;
#endif

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier { nullptr };
#endif
    std::atomic<uint64_t> m_associatedRegistryCount { 0 };
    Ref<MediaSourceClientImpl> m_client;
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
