/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include "config.h"
#include "MediaStreamTrackPrivate.h"

#if ENABLE(MEDIA_STREAM)

#include "GraphicsContext.h"
#include "IntRect.h"
#include "Logging.h"
#include "MediaStreamTrackDataHolder.h"
#include "PlatformMediaSessionManager.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/NativePromise.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UUID.h>

#if PLATFORM(COCOA)
#include "MediaStreamTrackAudioSourceProviderCocoa.h"
#elif ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
#include "AudioSourceProviderGStreamer.h"
#else
#include "WebAudioSourceProvider.h"
#endif

namespace WebCore {

Ref<MediaStreamTrackPrivate> MediaStreamTrackPrivate::create(Ref<const Logger>&& logger, Ref<RealtimeMediaSource>&& source, std::function<void(Function<void()>&&)>&& postTask)
{
    return create(WTF::move(logger), WTF::move(source), createVersion4UUIDString(), WTF::move(postTask));
}

Ref<MediaStreamTrackPrivate> MediaStreamTrackPrivate::create(Ref<const Logger>&& logger, Ref<RealtimeMediaSource>&& source, String&& id, std::function<void(Function<void()>&&)>&& postTask)
{
    auto privateTrack = adoptRef(*new MediaStreamTrackPrivate(WTF::move(logger), WTF::move(source), WTF::move(id), WTF::move(postTask)));
    privateTrack->initialize();
    return privateTrack;
}

Ref<MediaStreamTrackPrivate> MediaStreamTrackPrivate::create(Ref<const Logger>&& logger, UniqueRef<MediaStreamTrackDataHolder>&& dataHolder, std::function<void(Function<void()>&&)>&& postTask)
{
    auto privateTrack = adoptRef(*new MediaStreamTrackPrivate(WTF::move(logger), WTF::move(dataHolder), WTF::move(postTask)));
    privateTrack->initialize();
    return privateTrack;
}

class MediaStreamTrackPrivateSourceObserverSourceProxy final : public RealtimeMediaSourceObserver, public CanMakeCheckedPtr<MediaStreamTrackPrivateSourceObserverSourceProxy> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(MediaStreamTrackPrivateSourceObserverSourceProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MediaStreamTrackPrivateSourceObserverSourceProxy);
public:
    MediaStreamTrackPrivateSourceObserverSourceProxy(WeakPtr<MediaStreamTrackPrivate>&& privateTrack, Ref<RealtimeMediaSource>&& source, std::function<void(Function<void()>&&)>&& postTask)
        : m_privateTrack(WTF::move(privateTrack))
        , m_source(WTF::move(source))
        , m_postTask(WTF::move(postTask))
    {
        ASSERT(m_postTask);
        ASSERT(isMainThread());
    }

    // RealtimeMediaSourceObserver.
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    void setDidBeginCheckedPtrDeletion() final { CanMakeCheckedPtr::setDidBeginCheckedPtrDeletion(); }

    std::function<void(Function<void()>&&)> getPostTask()
    {
        return m_postTask;
    }

    void initialize(bool interrupted, bool muted)
    {
        ASSERT(isMainThread());
        if (m_source->isEnded()) {
            sourceStopped();
            return;
        }

        if (muted != m_source->muted() || interrupted != m_source->interrupted())
            sourceMutedChanged();

        // FIXME: We should check for settings capabilities changes.

        m_isStarted = true;
        m_source->addObserver(*this);
    }

    ~MediaStreamTrackPrivateSourceObserverSourceProxy()
    {
        ASSERT(isMainThread());
        if (m_isStarted)
            m_source->removeObserver(*this);
    }

    const RealtimeMediaSourceCapabilities& capabilities()
    {
        ASSERT(isMainThread());
        return m_source->capabilities();
    }

    const RealtimeMediaSourceSettings& settings()
    {
        ASSERT(isMainThread());
        return m_source->settings();
    }

    void start()
    {
        m_source->start();
    }

    void stop()
    {
        m_source->stop();
    }

    void requestToEnd()
    {
        m_shouldPreventSourceFromEnding = false;
        m_source->requestToEnd(*this);
    }

    void setMuted(bool muted)
    {
        m_source->setMuted(muted);
    }

    void applyConstraints(const MediaConstraints& constraints, RealtimeMediaSource::ApplyConstraintsHandler&& completionHandler)
    {
        m_source->applyConstraints(constraints, WTF::move(completionHandler));
    }

    void postTask(Function<void()>&& task)
    {
        m_postTask(WTF::move(task));
    }

private:
    void sourceStarted() final
    {
        sendToMediaStreamTrackPrivate([] (auto& privateTrack) {
            privateTrack.sourceStarted();
        });
    }

    void sourceStopped() final
    {
        sendToMediaStreamTrackPrivate([captureDidFail = m_source->captureDidFail()] (auto& privateTrack) {
            privateTrack.sourceStopped(captureDidFail);
        });
    }

    void sourceMutedChanged() final
    {
        sendToMediaStreamTrackPrivate([muted = m_source->muted(), interrupted = m_source->interrupted()] (auto& privateTrack) {
            privateTrack.sourceMutedChanged(interrupted, muted);
        });
    }

    void sourceSettingsChanged() final
    {
        sendToMediaStreamTrackPrivate([settings = crossThreadCopy(m_source->settings()), capabilities = crossThreadCopy(m_source->capabilities())] (auto& privateTrack) mutable {
            privateTrack.sourceSettingsChanged(WTF::move(settings), WTF::move(capabilities));
        });
    }

    void sourceConfigurationChanged() final
    {
        sendToMediaStreamTrackPrivate([name = crossThreadCopy(m_source->name()), settings = crossThreadCopy(m_source->settings()), capabilities = crossThreadCopy(m_source->capabilities())] (auto& privateTrack) mutable {
            privateTrack.sourceConfigurationChanged(WTF::move(name), WTF::move(settings), WTF::move(capabilities));
        });
    }

    void hasStartedProducingData() final
    {
        sendToMediaStreamTrackPrivate([] (auto& privateTrack) {
            privateTrack.hasStartedProducingData();
        });
    }

    void sendToMediaStreamTrackPrivate(Function<void(MediaStreamTrackPrivate&)>&& task)
    {
        m_postTask([task = WTF::move(task), privateTrack = m_privateTrack] () mutable {
            if (RefPtr protectedPrivateTrack = privateTrack.get())
                task(*protectedPrivateTrack);
        });
    }

    bool preventSourceFromEnding() { return m_shouldPreventSourceFromEnding; }

    WeakPtr<MediaStreamTrackPrivate> m_privateTrack;
    const Ref<RealtimeMediaSource> m_source;
    std::function<void(Function<void()>&&)> m_postTask;
    bool m_shouldPreventSourceFromEnding { true };
    bool m_isStarted { false };
};

MediaStreamTrackPrivateSourceObserver::MediaStreamTrackPrivateSourceObserver(Ref<RealtimeMediaSource>&& source, std::function<void(Function<void()>&&)>&& postTask)
    : m_source(WTF::move(source))
    , m_postTask(WTF::move(postTask))
{
    ASSERT(m_postTask || isMainThread());
    if (!m_postTask)
        m_postTask = [](Function<void()>&& function) { function(); };
}

MediaStreamTrackPrivateSourceObserver::~MediaStreamTrackPrivateSourceObserver() = default;

void MediaStreamTrackPrivateSourceObserver::initialize(MediaStreamTrackPrivate& privateTrack)
{
    ensureOnMainThread([this, protectedThis = Ref { *this }, privateTrack = WeakPtr { privateTrack }, postTask = m_postTask, source = m_source, interrupted = privateTrack.interrupted(), muted = privateTrack.muted()] () mutable {
        lazyInitialize(m_sourceProxy, makeUnique<MediaStreamTrackPrivateSourceObserverSourceProxy>(WTF::move(privateTrack), WTF::move(source), WTF::move(postTask)));
        m_sourceProxy->initialize(interrupted, muted);
    });
}

void MediaStreamTrackPrivateSourceObserver::start()
{
    ensureOnMainThread([protectedThis = Ref { *this }] {
        protectedThis->m_sourceProxy->start();
    });
}

void MediaStreamTrackPrivateSourceObserver::stop()
{
    ensureOnMainThread([protectedThis = Ref { *this }] {
        protectedThis->m_sourceProxy->stop();
    });
}

void MediaStreamTrackPrivateSourceObserver::requestToEnd()
{
    ensureOnMainThread([protectedThis = Ref { *this }] {
        protectedThis->m_sourceProxy->requestToEnd();
    });
}

void MediaStreamTrackPrivateSourceObserver::setMuted(bool muted)
{
    ensureOnMainThread([protectedThis = Ref { *this }, muted] {
        protectedThis->m_sourceProxy->setMuted(muted);
    });
}

void MediaStreamTrackPrivateSourceObserver::close()
{
    auto callbacks = std::exchange(m_applyConstraintsCallbacks, { });
    for (auto& callback : callbacks.values())
        callback(RealtimeMediaSource::ApplyConstraintsError { MediaConstraintType::Unknown, "applyConstraint cancelled"_s }, { }, { });
}

void MediaStreamTrackPrivateSourceObserver::applyConstraints(const MediaConstraints& constraints, ApplyConstraintsHandler&& completionHandler)
{
    m_applyConstraintsCallbacks.add(++m_applyConstraintsCallbacksIdentifier, WTF::move(completionHandler));
    ensureOnMainThread([this, protectedThis = Ref { *this }, constraints = crossThreadCopy(constraints), identifier = m_applyConstraintsCallbacksIdentifier] () mutable {
        m_sourceProxy->applyConstraints(constraints, [weakObserver = WeakPtr { *m_sourceProxy }, protectedThis = WTF::move(protectedThis), identifier] (auto&& result) mutable {
            if (!weakObserver)
                return;
            weakObserver->postTask([protectedThis = WTF::move(protectedThis), result = crossThreadCopy(WTF::move(result)), settings = crossThreadCopy(weakObserver->settings()), capabilities = crossThreadCopy(weakObserver->capabilities()), identifier] () mutable {
                if (auto callback = protectedThis->m_applyConstraintsCallbacks.take(identifier))
                    callback(WTF::move(result), WTF::move(settings), WTF::move(capabilities));
            });
        });
    });
}

MediaStreamTrackPrivate::MediaStreamTrackPrivate(Ref<const Logger>&& trackLogger, Ref<RealtimeMediaSource>&& source, String&& id, std::function<void(Function<void()>&&)>&& postTask)
    : m_sourceObserver(MediaStreamTrackPrivateSourceObserver::create(WTF::move(source), WTF::move(postTask)))
    , m_id(WTF::move(id))
    , m_label(m_sourceObserver->source().name())
    , m_type(m_sourceObserver->source().type())
    , m_deviceType(m_sourceObserver->source().deviceType())
    , m_isCaptureTrack(m_sourceObserver->source().isCaptureSource())
    , m_captureDidFail(m_sourceObserver->source().captureDidFail())
    , m_logger(WTF::move(trackLogger))
#if !RELEASE_LOG_DISABLED
    , m_logIdentifier(uniqueLogIdentifier())
#endif
    , m_isProducingData(m_sourceObserver->source().isProducingData())
    , m_isMuted(m_sourceObserver->source().muted())
    , m_isInterrupted(m_sourceObserver->source().interrupted())
    , m_settings(m_sourceObserver->source().settings())
    , m_capabilities(m_sourceObserver->source().capabilities())
#if ASSERT_ENABLED
    , m_creationThreadId(isMainThread() ? 0 : Thread::currentSingleton().uid())
#endif
{
    UNUSED_PARAM(trackLogger);
    ALWAYS_LOG(LOGIDENTIFIER);
    if (!isMainThread())
        return;

#if !RELEASE_LOG_DISABLED
    m_sourceObserver->source().setLogger(m_logger.copyRef(), m_logIdentifier);
#endif
}

MediaStreamTrackPrivate::MediaStreamTrackPrivate(Ref<const Logger>&& logger, UniqueRef<MediaStreamTrackDataHolder>&& dataHolder, std::function<void(Function<void()>&&)>&& postTask)
    : m_sourceObserver(MediaStreamTrackPrivateSourceObserver::create(WTF::move(dataHolder->source), WTF::move(postTask)))
    , m_id(WTF::move(dataHolder->trackId))
    , m_label(WTF::move(dataHolder->label))
    , m_type(dataHolder->type)
    , m_deviceType(dataHolder->deviceType)
    , m_isCaptureTrack(false)
    , m_isEnabled(dataHolder->isEnabled)
    , m_isEnded(dataHolder->isEnded)
    , m_captureDidFail(false)
    , m_contentHint(dataHolder->contentHint)
    , m_logger(WTF::move(logger))
#if !RELEASE_LOG_DISABLED
    , m_logIdentifier(uniqueLogIdentifier())
#endif
    , m_isProducingData(dataHolder->isProducingData)
    , m_isMuted(dataHolder->isMuted)
    , m_isInterrupted(dataHolder->isInterrupted)
    , m_settings(WTF::move(dataHolder->settings))
    , m_capabilities(WTF::move(dataHolder->capabilities))
#if ASSERT_ENABLED
    , m_creationThreadId(isMainThread() ? 0 : Thread::currentSingleton().uid())
#endif
{
}

void MediaStreamTrackPrivate::initialize()
{
    m_sourceObserver->initialize(*this);
}

MediaStreamTrackPrivate::~MediaStreamTrackPrivate()
{
    ASSERT(isOnCreationThread());

    ALWAYS_LOG(LOGIDENTIFIER);

    m_sourceObserver->close();
}

#if ASSERT_ENABLED
bool MediaStreamTrackPrivate::isOnCreationThread()
{
    return m_creationThreadId ? m_creationThreadId == Thread::currentSingleton().uid() : isMainThread();
}
#endif

void MediaStreamTrackPrivate::updateLabelIfRemoteTrack()
{
    if (!isMainThread() || !(protect(source())->isIncomingAudioSource() || protect(source())->isIncomingVideoSource()))
        return;

    m_label = makeString(m_label, " - "_s, m_id);
}


void MediaStreamTrackPrivate::forEachObserver(NOESCAPE const Function<void(MediaStreamTrackPrivateObserver&)>& apply)
{
    ASSERT(isOnCreationThread());
    ASSERT(!m_observers.hasNullReferences());
    Ref protectedThis { *this };
    m_observers.forEach(apply);
}

void MediaStreamTrackPrivate::addObserver(MediaStreamTrackPrivateObserver& observer)
{
    ASSERT(isOnCreationThread());
    m_observers.add(observer);
}

void MediaStreamTrackPrivate::removeObserver(MediaStreamTrackPrivateObserver& observer)
{
    ASSERT(isOnCreationThread());
    m_observers.remove(observer);
}

void MediaStreamTrackPrivate::setContentHint(MediaStreamTrackHintValue hintValue)
{
    m_contentHint = hintValue;
}

void MediaStreamTrackPrivate::startProducingData()
{
    m_sourceObserver->start();
}

void MediaStreamTrackPrivate::stopProducingData()
{
    m_sourceObserver->stop();
}

void MediaStreamTrackPrivate::dataFlowStarted()
{
    forEachObserver([this](auto& observer) {
        observer.dataFlowStarted(*this);
    });
}

void MediaStreamTrackPrivate::setIsInBackground(bool value)
{
    ASSERT(isMainThread());
    m_sourceObserver->source().setIsInBackground(value);
}

void MediaStreamTrackPrivate::setMuted(bool muted)
{
    ASSERT(isOnCreationThread());
    m_isMuted = muted;

    m_sourceObserver->setMuted(muted);
}

void MediaStreamTrackPrivate::setEnabled(bool enabled)
{
    ASSERT(isOnCreationThread());
    if (m_isEnabled == enabled)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, enabled);

    // Always update the enabled state regardless of the track being ended.
    m_isEnabled = enabled;

    forEachObserver([this](auto& observer) {
        observer.trackEnabledChanged(*this);
    });
}

void MediaStreamTrackPrivate::endTrack()
{
    ASSERT(isOnCreationThread());
    if (m_isEnded)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    // Set m_isEnded to true before telling the source it can stop, so if this is the
    // only track using the source and it does stop, we will only call each observer's
    // trackEnded method once.
    m_isEnded = true;
    updateReadyState();

    m_sourceObserver->requestToEnd();

    forEachObserver([this](auto& observer) {
        observer.trackEnded(*this);
    });
}

Ref<MediaStreamTrackPrivate> MediaStreamTrackPrivate::clone()
{
    ASSERT(isOnCreationThread());

    auto postTask = m_sourceObserver->getPostTask();
    auto clonedMediaStreamTrackPrivate = create(m_logger.copyRef(), toDataHolder(ShouldClone::Yes), WTF::move(postTask));

    ALWAYS_LOG(LOGIDENTIFIER, clonedMediaStreamTrackPrivate->logIdentifier());

    clonedMediaStreamTrackPrivate->m_isCaptureTrack = this->m_isCaptureTrack;
    clonedMediaStreamTrackPrivate->m_captureDidFail = this->m_captureDidFail;
    clonedMediaStreamTrackPrivate->updateReadyState();

    if (m_isProducingData && !m_isMuted && !m_isInterrupted)
        clonedMediaStreamTrackPrivate->startProducingData();

    return clonedMediaStreamTrackPrivate;
}

RealtimeMediaSource& MediaStreamTrackPrivate::source()
{
    ASSERT(isMainThread());
    return m_sourceObserver->source();
}

const RealtimeMediaSource& MediaStreamTrackPrivate::source() const
{
    ASSERT(isMainThread());
    return m_sourceObserver->source();
}

RealtimeMediaSource& MediaStreamTrackPrivate::sourceForProcessor()
{
    ASSERT(isOnCreationThread());
    return m_sourceObserver->source();
}

bool MediaStreamTrackPrivate::hasSource(const RealtimeMediaSource* source) const
{
    ASSERT(isMainThread());
    return &m_sourceObserver->source() == source;
}

Ref<RealtimeMediaSource::PhotoCapabilitiesNativePromise> MediaStreamTrackPrivate::getPhotoCapabilities()
{
    ASSERT(isMainThread());
    return m_sourceObserver->source().getPhotoCapabilities();
}

Ref<RealtimeMediaSource::PhotoSettingsNativePromise> MediaStreamTrackPrivate::getPhotoSettings()
{
    ASSERT(isMainThread());
    return m_sourceObserver->source().getPhotoSettings();
}

Ref<RealtimeMediaSource::TakePhotoNativePromise> MediaStreamTrackPrivate::takePhoto(PhotoSettings&& settings)
{
    ASSERT(isMainThread());
    return m_sourceObserver->source().takePhoto(WTF::move(settings));
}

void MediaStreamTrackPrivate::applyConstraints(const MediaConstraints& constraints, RealtimeMediaSource::ApplyConstraintsHandler&& completionHandler)
{
    MediaStreamTrackPrivateSourceObserver::ApplyConstraintsHandler callback = [weakThis = WeakPtr { *this }, completionHandler = WTF::move(completionHandler)] (auto&& result, auto&& settings, auto&& capabilities) mutable {
        if (RefPtr protectedThis = weakThis.get()) {
            protectedThis->m_settings = WTF::move(settings);
            protectedThis->m_capabilities = WTF::move(capabilities);
        }
        completionHandler(WTF::move(result));
    };
    m_sourceObserver->applyConstraints(constraints, WTF::move(callback));
}

#if ENABLE(WEB_AUDIO)
RefPtr<WebAudioSourceProvider> MediaStreamTrackPrivate::createAudioSourceProvider()
{
    ASSERT(isMainThread());
    ALWAYS_LOG(LOGIDENTIFIER);

#if PLATFORM(COCOA)
    return MediaStreamTrackAudioSourceProviderCocoa::create(*this);
#elif USE(GSTREAMER)
    return AudioSourceProviderGStreamer::create(*this);
#else
    return nullptr;
#endif
}
#endif

void MediaStreamTrackPrivate::sourceStarted()
{
    ASSERT(isOnCreationThread());
    ALWAYS_LOG(LOGIDENTIFIER);

    m_isProducingData = true;
    forEachObserver([this](auto& observer) {
        observer.trackStarted(*this);
    });
}

void MediaStreamTrackPrivate::sourceStopped(bool captureDidFail)
{
    ASSERT(isOnCreationThread());
    m_isProducingData = false;

    if (m_isEnded)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_isEnded = true;
    m_captureDidFail = captureDidFail;
    updateReadyState();

    forEachObserver([this](auto& observer) {
        observer.trackEnded(*this);
    });
}

void MediaStreamTrackPrivate::sourceMutedChanged(bool interrupted, bool muted)
{
    ASSERT(isOnCreationThread());
    ALWAYS_LOG(LOGIDENTIFIER);

    m_isInterrupted = interrupted;
    m_isMuted = muted;
    forEachObserver([this](auto& observer) {
        observer.trackMutedChanged(*this);
    });
}

void MediaStreamTrackPrivate::sourceSettingsChanged(RealtimeMediaSourceSettings&& settings, RealtimeMediaSourceCapabilities&& capabilities)
{
    ASSERT(isOnCreationThread());
    ALWAYS_LOG(LOGIDENTIFIER);

    m_settings = WTF::move(settings);
    m_capabilities = WTF::move(capabilities);
    forEachObserver([this](auto& observer) {
        observer.trackSettingsChanged(*this);
    });
}
void MediaStreamTrackPrivate::sourceConfigurationChanged(String&& label, RealtimeMediaSourceSettings&& settings, RealtimeMediaSourceCapabilities&& capabilities)
{
    ASSERT(isOnCreationThread());
    ALWAYS_LOG(LOGIDENTIFIER);

    m_label = WTF::move(label);
    m_settings = WTF::move(settings);
    m_capabilities = WTF::move(capabilities);
    forEachObserver([this](auto& observer) {
        observer.trackConfigurationChanged(*this);
    });
}

void MediaStreamTrackPrivate::hasStartedProducingData()
{
    ASSERT(isOnCreationThread());
    if (m_hasStartedProducingData)
        return;
    ALWAYS_LOG(LOGIDENTIFIER);
    m_hasStartedProducingData = true;
    updateReadyState();
}

void MediaStreamTrackPrivate::updateReadyState()
{
    ASSERT(isOnCreationThread());
    ReadyState state = ReadyState::None;

    if (m_isEnded)
        state = ReadyState::Ended;
    else if (m_hasStartedProducingData)
        state = ReadyState::Live;

    if (state == m_readyState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, state == ReadyState::Ended ? "Ended" : "Live");

    m_readyState = state;
    forEachObserver([this](auto& observer) {
        observer.readyStateChanged(*this);
    });
}

UniqueRef<MediaStreamTrackDataHolder> MediaStreamTrackPrivate::toDataHolder(ShouldClone shouldClone)
{
    return makeUniqueRef<MediaStreamTrackDataHolder>(
        shouldClone == ShouldClone::Yes ? createVersion4UUIDString() : m_id.isolatedCopy(),
        m_label.isolatedCopy(),
        m_type,
        m_deviceType,
        m_isEnabled,
        m_isEnded,
        m_contentHint,
        m_isProducingData,
        m_isMuted,
        m_isInterrupted,
        m_settings.isolatedCopy(),
        m_capabilities.isolatedCopy(),
        shouldClone == ShouldClone::Yes ? m_sourceObserver->source().clone() : Ref { m_sourceObserver->source() });
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaStreamTrackPrivate::logChannel() const
{
    return LogWebRTC;
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
