/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaSessionManagerInterface.h"

#include "AudioSession.h"
#include "Document.h"
#include "Logging.h"
#include "MediaSessionManagerClient.h"
#include "NowPlayingInfo.h"
#include "Page.h"
#include "PlatformMediaSession.h"
#include <algorithm>
#include <ranges>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>

#define MEDIASESSIONMANAGERINTERFACE_RELEASE_LOG(formatString, ...) \
if (willLog(WTFLogLevel::Always)) { \
    RELEASE_LOG_FORWARDABLE(Media, MediaSessionManagerInterface##formatString, ##__VA_ARGS__); \
} \

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaSessionManagerInterface);

class PageMediaSessionManagerClient final : public MediaSessionManagerClient {
    WTF_MAKE_TZONE_ALLOCATED(PageMediaSessionManagerClient);
public:
    explicit PageMediaSessionManagerClient(std::optional<PageIdentifier> pageIdentifier)
        : m_pageIdentifier(pageIdentifier) { }

private:
    void hasActiveNowPlayingSessionChanged(PlatformMediaSessionInterface*) final
    {
        if (RefPtr page = m_pageIdentifier ? Page::fromPageIdentifier(*m_pageIdentifier) : nullptr)
            page->hasActiveNowPlayingSessionChanged();
    }

    Markable<PageIdentifier> m_pageIdentifier;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(PageMediaSessionManagerClient);

MediaSessionManagerInterface::MediaSessionManagerInterface(std::optional<PageIdentifier> pageIdentifier)
    : m_pageIdentifier(pageIdentifier)
    , m_client(makeUnique<PageMediaSessionManagerClient>(pageIdentifier))
#if !RELEASE_LOG_DISABLED
    , m_stateLogTimer(makeUniqueRef<Timer>(*this, &MediaSessionManagerInterface::dumpSessionStates))
    , m_logger(AggregateLogger::create(this))
#endif
{
}

MediaSessionManagerClient& MediaSessionManagerInterface::client() const
{
    return *m_client;
}

void MediaSessionManagerInterface::setClient(std::unique_ptr<MediaSessionManagerClient>&& client)
{
    m_client = WTF::move(client);
}

MediaSessionManagerInterface::~MediaSessionManagerInterface()
{
    m_taskGroup.cancel();
}

static inline unsigned NODELETE indexFromMediaType(PlatformMediaSession::MediaType type)
{
    return static_cast<unsigned>(type);
}

Vector<WeakPtr<PlatformMediaSessionInterface>> MediaSessionManagerInterface::sessionsMatching(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& filter) const
{
    Vector<WeakPtr<PlatformMediaSessionInterface>> matchingSessions;
    for (auto& weakSession : copySessionsToVector()) {
        RefPtr session = weakSession.get();
        if (session && filter(*session))
            matchingSessions.append(weakSession);
    }
    return matchingSessions;
}

WeakPtr<PlatformMediaSessionInterface> MediaSessionManagerInterface::firstSessionMatching(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& predicate) const
{
    for (auto& weakSession : copySessionsToVector()) {
        RefPtr session = weakSession.get();
        if (session && predicate(*session))
            return weakSession;
    }
    return nullptr;
}

void MediaSessionManagerInterface::forEachMatchingSession(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& predicate, NOESCAPE const Function<void(PlatformMediaSessionInterface&)>& callback)
{
    for (auto& session : sessionsMatching(predicate)) {
        ASSERT(session);
        if (session)
            callback(*session);
    }
}

void MediaSessionManagerInterface::forEachSessionInGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier, NOESCAPE const Function<void(PlatformMediaSessionInterface&)>& callback)
{
    if (!mediaSessionGroupIdentifier)
        return;

    forEachMatchingSession([mediaSessionGroupIdentifier = *mediaSessionGroupIdentifier](auto& session) {
        return session.client().mediaSessionGroupIdentifier() == mediaSessionGroupIdentifier;
    }, [&callback](auto& session) {
        callback(session);
    });
}

void MediaSessionManagerInterface::forEachSession(NOESCAPE const Function<void(PlatformMediaSessionInterface&)>& callback)
{
    for (auto& weakSession : copySessionsToVector()) {
        if (RefPtr session = weakSession.get())
            callback(*session);
    }
}

bool MediaSessionManagerInterface::anyOfSessions(NOESCAPE const Function<bool(const PlatformMediaSessionInterface&)>& predicate) const
{
    for (auto& weakSession : copySessionsToVector()) {
        RefPtr session = weakSession.get();
        if (session && predicate(*session))
            return true;
    }

    return false;
}

void MediaSessionManagerInterface::resetRestrictions()
{
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::Video)] = MediaSessionRestriction::NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::Audio)] = MediaSessionRestriction::NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::VideoAudio)] = MediaSessionRestriction::NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::WebAudio)] = MediaSessionRestriction::NoRestrictions;
    m_restrictions[indexFromMediaType(PlatformMediaSession::MediaType::DOMMediaSession)] = MediaSessionRestriction::NoRestrictions;
}

void MediaSessionManagerInterface::resetToConsistentStateForTesting()
{
#if ENABLE(VIDEO)
    resetHaveEverRegisteredAsNowPlayingApplicationForTesting();
    resetRestrictions();
    resetSessionState();
    setWillIgnoreSystemInterruptions(true);
    applicationWillEnterForeground(false);
#endif
    setIsPlayingToAutomotiveHeadUnit(false);

#if USE(AUDIO_SESSION)
    AudioSession::singleton().setCategoryOverride(AudioSessionCategory::None);
    AudioSession::singleton().tryToSetActive(false)->whenSettled(RunLoop::mainSingleton(), [](auto&&) { });
    AudioSession::singleton().endInterruptionForTesting();
#endif
}

bool MediaSessionManagerInterface::isMediaSessionManagerGLib() const
{
    return false;
}

bool MediaSessionManagerInterface::has(PlatformMediaSession::MediaType type) const
{
    return anyOfSessions([type] (auto& session) {
        return session.mediaType() == type;
    });
}

bool MediaSessionManagerInterface::activeAudioSessionRequired() const
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    if (anyOfSessions([] (auto& session) { return session.activeAudioSessionRequired(); }))
        return true;

    return std::ranges::any_of(m_audioCaptureSources, [](auto& source) {
        return Ref { source }->isCapturingAudio();
    });
#else
    return false;
#endif
}

bool MediaSessionManagerInterface::audioSessionActivationRequired() const
{
    // Stricter than activeAudioSessionRequired(): activating the audio session interrupts other
    // applications and cannot be withdrawn once the request reaches the platform, so a session whose
    // client is not allowed to play must not cause it. Deactivation keys on
    // activeAudioSessionRequired() instead (conservative about giving the session up, strict about
    // taking it).
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    if (anyOfSessions([] (auto& session) { return session.activeAudioSessionRequired() && session.playbackPermitted(); }))
        return true;

    return std::ranges::any_of(m_audioCaptureSources, [](auto& source) {
        return Ref { source }->isCapturingAudio();
    });
#else
    return false;
#endif
}

bool MediaSessionManagerInterface::hasActiveAudioSession(PlatformMediaSessionInterface&) const
{
#if USE(AUDIO_SESSION)
    return m_becameActive;
#else
    return true;
#endif
}

bool MediaSessionManagerInterface::canProduceAudio() const
{
    return anyOfSessions([] (auto& session) {
        return session.canProduceAudio();
    });
}

void MediaSessionManagerInterface::updateNowPlayingInfoIfNecessary()
{
    scheduleSessionStatusUpdate();
}

void MediaSessionManagerInterface::updateNowPlayingInfo()
{
    updateNowPlayingInfoIfNecessary();
}

void MediaSessionManagerInterface::setNowPlayingUpdateInterval(double)
{
}

double MediaSessionManagerInterface::nowPlayingUpdateInterval()
{
    return 0;
}

void MediaSessionManagerInterface::updateAudioSessionCategoryIfNecessary()
{
    scheduleUpdateSessionState();
}

void MediaSessionManagerInterface::addNowPlayingMetadataObserver(const NowPlayingMetadataObserver& observer)
{
    ASSERT(!m_nowPlayingMetadataObservers.contains(observer));
    m_nowPlayingMetadataObservers.add(observer);
    observer(nowPlayingInfo().value_or(NowPlayingInfo { }).metadata);
}

void MediaSessionManagerInterface::removeNowPlayingMetadataObserver(const NowPlayingMetadataObserver& observer)
{
    ASSERT(m_nowPlayingMetadataObservers.contains(observer));
    m_nowPlayingMetadataObservers.remove(observer);
}

void MediaSessionManagerInterface::nowPlayingMetadataChanged(const NowPlayingMetadata& metadata)
{
    m_nowPlayingMetadataObservers.forEach([&] (auto& observer) {
        observer(metadata);
    });
}

bool MediaSessionManagerInterface::hasActiveNowPlayingSessionInGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
    bool hasActiveNowPlayingSession = false;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSessionInGroup(mediaSessionGroupIdentifier, [&](auto& session) {
        hasActiveNowPlayingSession |= session.isActiveNowPlayingSession();
    });
#else
    UNUSED_PARAM(mediaSessionGroupIdentifier);
#endif

    return hasActiveNowPlayingSession;
}

void MediaSessionManagerInterface::enqueueTaskOnMainThread(Function<void()>&& task)
{
    callOnMainThread(CancellableTask(m_taskGroup, [task = WTF::move(task)] () mutable {
        task();
    }));
}

void MediaSessionManagerInterface::beginInterruption(PlatformMediaSession::InterruptionType type)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_currentInterruption = type;
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([type] (auto& session) {
        session.beginInterruption(type);
    });
#endif
    scheduleUpdateSessionState();
}

void MediaSessionManagerInterface::endInterruption(PlatformMediaSession::EndInterruptionFlags flags)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_currentInterruption = { };
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([flags] (auto& session) {
        session.endInterruption(flags);
    });
#else
    UNUSED_PARAM(flags);
#endif
}

void MediaSessionManagerInterface::applicationWillEnterForeground(bool suspendedUnderLock)
{
    ALWAYS_LOG(LOGIDENTIFIER, suspendedUnderLock);

    if (!m_isApplicationInBackground)
        return;

    m_isApplicationInBackground = false;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachMatchingSession([&](auto& session) {
        return (suspendedUnderLock && restrictions(session.mediaType()).contains( MediaSessionRestriction::SuspendedUnderLockPlaybackRestricted)) || restrictions(session.mediaType()).contains( MediaSessionRestriction::BackgroundProcessPlaybackRestricted);
    }, [](auto& session) {
        session.endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    });
#endif
}

void MediaSessionManagerInterface::applicationDidEnterBackground(bool suspendedUnderLock)
{
    ALWAYS_LOG(LOGIDENTIFIER, suspendedUnderLock);

    if (m_isApplicationInBackground)
        return;

    m_isApplicationInBackground = true;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([&] (auto& session) {
        if (suspendedUnderLock && restrictions(session.mediaType()).contains(MediaSessionRestriction::SuspendedUnderLockPlaybackRestricted))
            session.beginInterruption(PlatformMediaSession::InterruptionType::SuspendedUnderLock);
        else if (restrictions(session.mediaType()).contains(MediaSessionRestriction::BackgroundProcessPlaybackRestricted))
            session.beginInterruption(PlatformMediaSession::InterruptionType::EnteringBackground);
    });
#endif
}

void MediaSessionManagerInterface::applicationWillBecomeInactive()
{
    ALWAYS_LOG(LOGIDENTIFIER);

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachMatchingSession([&](auto& session) {
        return restrictions(session.mediaType()).contains(MediaSessionRestriction::InactiveProcessPlaybackRestricted);
    }, [](auto& session) {
        session.beginInterruption(PlatformMediaSession::InterruptionType::ProcessInactive);
    });
#endif
}

void MediaSessionManagerInterface::applicationDidBecomeActive()
{
    ALWAYS_LOG(LOGIDENTIFIER);

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachMatchingSession([&](auto& session) {
        return restrictions(session.mediaType()).contains(MediaSessionRestriction::InactiveProcessPlaybackRestricted);
    }, [](auto& session) {
        session.endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    });
#endif
}

void MediaSessionManagerInterface::processWillSuspend()
{
    if (m_processIsSuspended)
        return;
    m_processIsSuspended = true;

    ALWAYS_LOG(LOGIDENTIFIER);

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([&] (auto& session) {
        session.client().processIsSuspendedChanged();
    });
#endif

    if (std::ranges::any_of(m_audioCaptureSources, [](auto& source) { return Ref { source }->isCapturingAudio(); }))
        MEDIASESSIONMANAGERINTERFACE_RELEASE_LOG(ProcessWillSuspendWhileCapturing);


#if USE(AUDIO_SESSION)
    maybeDeactivateAudioSession(ShouldCheckRequiredSession::No);
#endif
}

void MediaSessionManagerInterface::processDidResume()
{
    if (!m_processIsSuspended)
        return;
    m_processIsSuspended = false;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([&] (auto& session) {
        session.client().processIsSuspendedChanged();
    });
#endif

#if USE(AUDIO_SESSION)
    if (!m_becameActive)
        maybeActivateAudioSession();
#endif
}

void MediaSessionManagerInterface::stopAllMediaPlaybackForProcess()
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([] (auto& session) {
        session.stopSession();
    });
#endif
}

bool MediaSessionManagerInterface::mediaPlaybackIsPaused(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
    bool mediaPlaybackIsPaused = false;
    forEachSessionInGroup(mediaSessionGroupIdentifier, [&mediaPlaybackIsPaused](auto& session) {
        if (session.state() == PlatformMediaSession::State::Paused)
            mediaPlaybackIsPaused = true;
    });
    return mediaPlaybackIsPaused;
}

void MediaSessionManagerInterface::pauseAllMediaPlaybackForGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.pauseSession();
    });
}

void MediaSessionManagerInterface::suspendAllMediaPlaybackForGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.beginInterruption(PlatformMediaSession::InterruptionType::PlaybackSuspended);
    });
#else
    UNUSED_PARAM(mediaSessionGroupIdentifier);
#endif
}

void MediaSessionManagerInterface::resumeAllMediaPlaybackForGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    });
#else
    UNUSED_PARAM(mediaSessionGroupIdentifier);
#endif
}

void MediaSessionManagerInterface::suspendAllMediaBufferingForGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.suspendBuffering();
    });
}

void MediaSessionManagerInterface::resumeAllMediaBufferingForGroup(std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier)
{
    forEachSessionInGroup(mediaSessionGroupIdentifier, [](auto& session) {
        session.resumeBuffering();
    });
}

void MediaSessionManagerInterface::addRestriction(PlatformMediaSession::MediaType type, MediaSessionRestrictions restriction)
{
    m_restrictions[indexFromMediaType(type)].add(restriction);
}

void MediaSessionManagerInterface::removeRestriction(PlatformMediaSession::MediaType type, MediaSessionRestrictions restriction)
{
    m_restrictions[indexFromMediaType(type)].remove(restriction);
}

MediaSessionRestrictions MediaSessionManagerInterface::restrictions(PlatformMediaSession::MediaType type)
{
    return m_restrictions[indexFromMediaType(type)];
}

Ref<GenericPromise> MediaSessionManagerInterface::sessionWillBeginPlayback(PlatformMediaSessionInterface& session)
{
    assertIsMainThread();

    auto stateAtStart = session.state();

    GenericPromise::Producer producer;
    Ref promise = producer.promise();
    m_currentPlaybackAdmission = protect(m_currentPlaybackAdmission)->whenSettled(RunLoop::mainSingleton(), [this, protectedThis = Ref { *this }, weakSession = WeakPtr { session }, stateAtStart, producer = WTF::move(producer)](auto) mutable -> Ref<GenericPromise> {
        RefPtr session = weakSession.get();
        if (!session) {
            producer.reject();
            return GenericPromise::createAndResolve();
        }

        // A pause() call between clientWillBeginPlayback() starting and this callback running means the
        // client no longer intends to play — this turn must not touch setCurrentSession or evict anyone on
        // its behalf. That gap exists on every call, not just a contended one: this link is always deferred
        // by a run-loop turn, whether or not another admission preceded it in the chain. Resolve, not
        // reject: commitPlaybackAdmission() handles the same race by skipping the claim to Playing while still
        // settling successfully, and the play promise must settle the same way regardless of which of the two
        // checks caught the race.
        if (!session->admissionStillValid()) {
            producer.resolve();
            return GenericPromise::createAndResolve();
        }

        return startSessionAdmission(*session, stateAtStart)->whenSettled(RunLoop::mainSingleton(), [producer = WTF::move(producer)](auto&& result) mutable {
            if (result)
                producer.resolve();
            else
                producer.reject();
            return GenericPromise::createAndResolve();
        });
    });
    return promise;
}

void MediaSessionManagerInterface::sessionDidCompleteAdmission(PlatformMediaSessionInterface&)
{
}

Ref<GenericPromise> MediaSessionManagerInterface::startSessionAdmission(PlatformMediaSessionInterface& session, PlatformMediaSessionState stateAtStart)
{
    ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier());

    setCurrentSession(session);

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    auto sessionType = session.mediaType();
    auto restrictions = this->restrictions(sessionType);
    if (stateAtStart == PlatformMediaSession::State::Interrupted && restrictions & MediaSessionRestriction::InterruptedPlaybackNotPermitted) {
        ALWAYS_LOG(LOGIDENTIFIER, session.logIdentifier(), " returning false because stateAtStart is Interrupted, and InterruptedPlaybackNotPermitted");
        return GenericPromise::createAndReject();
    }

    // Capture whether the session is already interrupted now. If activation is async and an
    // interruption begins while activation is in flight, we must not consume that interruption
    // when the callback fires.
    bool sessionWasAlreadyInterrupted = session.state() == PlatformMediaSession::State::Interrupted;

    auto logSiteIdentifier = LOGIDENTIFIER;
    auto completeWillBeginPlayback = [this, protectedThis = Ref { *this }, weakSession = WeakPtr { session }, stateAtStart,
#if !RELEASE_LOG_DISABLED
        sessionLogId = session.logIdentifier(),
#endif
        sessionWasAlreadyInterrupted, logSiteIdentifier](bool activated) {
        if (!activated) {
            ALWAYS_LOG(logSiteIdentifier, " returning false, failed to activate AudioSession");
            return GenericPromise::createAndReject();
        }
        if (m_currentInterruption && sessionWasAlreadyInterrupted)
            endInterruption(PlatformMediaSession::EndInterruptionFlags::NoFlags);

        if (RefPtr session = weakSession.get()) {
            if (session->commitPlaybackAdmission(stateAtStart)) {
                enforceConcurrentPlaybackRestriction(*session);
                sessionDidCompleteAdmission(*session);
            }
        }
        ALWAYS_LOG(logSiteIdentifier, sessionLogId, " returning true");
        return GenericPromise::createAndResolve();
    };

#if USE(AUDIO_SESSION)
    if (!AudioSession::singleton().isActive() && session.canProduceAudio()) {
        // This session is about to produce audio and the audio session is not
        // currently active. Activate before completing so that the active state
        // is visible to JS observers (e.g., the 'playing' event handler reading
        // internals.audioSessionActive()) by the time play() resolves.
        //
        // Complete the playback admission FIRST — completeWillBeginPlayback()
        // drives the session to State::Playing (via clientWillBeginPlayback's
        // completion) and fires the 'playing' event. Then deactivate only if the
        // session was actually REMOVED while activation was in flight (hasNoSession),
        // e.g. an iframe GC'd before the reply — mirroring removeSession()'s own
        // check. Do NOT key this on !activeAudioSessionRequired(): the callback runs
        // a runloop turn later, and a still-alive session can be transiently
        // "not required" (Autoplaying, or canProduceAudio flipping during a
        // Video->VideoAudio transition), which would spuriously tear down an active
        // session and make internals.audioSessionActive() observably false at the
        // 'playing' event (Release-timing race). Consistent with B1.
        return AudioSession::singleton().tryToSetActive(true)->whenSettled(RunLoop::mainSingleton(),
            [this, protectedThis = Ref { *this }, completeWillBeginPlayback = WTF::move(completeWillBeginPlayback)](auto&& result) {
                if (result)
                    m_becameActive = true;
                Ref promise = completeWillBeginPlayback(result.has_value());
                if (result && hasNoSession())
                    maybeDeactivateAudioSession();
                return promise;
            });
    }
#endif

    if (!activeAudioSessionRequired() || hasActiveAudioSession(session))
        return completeWillBeginPlayback(true);

    return maybeActivateAudioSession()->whenSettled(RunLoop::mainSingleton(),
        [completeWillBeginPlayback = WTF::move(completeWillBeginPlayback)](auto&& result) {
            return completeWillBeginPlayback(result.has_value());
        });
#else
    return GenericPromise::createAndReject();
#endif
}

void MediaSessionManagerInterface::enforceConcurrentPlaybackRestriction(PlatformMediaSessionInterface& newSession)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    auto restrictions = this->restrictions(newSession.mediaType());
    if (!restrictions.contains(MediaSessionRestriction::ConcurrentPlaybackNotPermitted))
        return;

    // Only the current session claims exclusivity. A session that stopped being current while its
    // admission was in flight, or whose mediaType changed after another session started, would
    // otherwise pause the session that started last.
    if (currentSession() != &newSession)
        return;

    forEachMatchingSession([&newSession](auto& otherSession) {
        bool isOther = &otherSession == &newSession;
        // Only sessions that reached Playing are candidates. A session with preparingToPlay() set hasn't
        // earned that yet — whether it's merely queued behind another admission (sessionWillBeginPlayback()
        // serializes those) or a call from outside that chain (canProduceAudioChanged()) catches it mid-
        // admission — and evicting it here preempts its own admission, which is the only place that should
        // decide its fate.
        bool isPlaying = otherSession.state() == PlatformMediaSession::State::Playing;
        bool canConcurrent = otherSession.canPlayConcurrently(newSession);
        if (isOther)
            return false;
        if (!isPlaying)
            return false;
        return !canConcurrent;
    }, [](auto& oneSession) {
        oneSession.pauseSession();
    });
#else
    UNUSED_PARAM(newSession);
#endif
}

void MediaSessionManagerInterface::sessionWillEndPlayback(PlatformMediaSessionInterface& pausingSession, DelayCallingUpdateNowPlaying)
{
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    MEDIASESSIONMANAGERINTERFACE_RELEASE_LOG(SessionWillEndPlayback, pausingSession.logIdentifier());
#endif

    auto& sessions = this->sessions();
    auto sessionCount = sessions.computeSize();
    if (sessionCount < 2)
        return;

    RefPtr<PlatformMediaSessionInterface> firstPausedSession;
    for (auto it = sessions.begin(); it != sessions.end(); ++it) {
        RefPtr session = *it.get();
        // A session whose own admission is in flight must stay ahead of the pausing session: it is
        // likely still at the front of the list, and enforceConcurrentPlaybackRestriction()'s guard
        // relies on currentSession() still naming it once its admission completes.
        if (&pausingSession == session.get() || session->isPlayingOrPreparingToPlay())
            continue;

        firstPausedSession = session.get();
        break;
    }

    if (firstPausedSession) {
        sessions.remove(pausingSession);
        sessions.insertBefore(*firstPausedSession, pausingSession);
    } else
        sessions.appendOrMoveToLast(pausingSession);
}

void MediaSessionManagerInterface::sessionStateChanged(PlatformMediaSessionInterface& session)
{
    // Call updateSessionState() synchronously if the new state is Playing to ensure
    // the audio session is active and has the correct category before playback starts.
    if (session.state() == PlatformMediaSession::State::Playing)
        updateSessionState();
    else
        scheduleUpdateSessionState();

#if !RELEASE_LOG_DISABLED
    scheduleStateLog();
#endif
}

void MediaSessionManagerInterface::sessionCanProduceAudioChanged()
{
    MEDIASESSIONMANAGERINTERFACE_RELEASE_LOG(SessionCanProduceAudioChanged);

    // Activate synchronously so isActive() reflects the newly-audible
    // state immediately. The WebContent process always holds a RemoteAudioSession
    // (GPU process is always enabled), whose tryToSetActive sets m_active
    // optimistically+synchronously — so internals.audioSessionActive() (and any
    // 'playing'-handler read) sees the correct state without waiting for the
    // debounced task below, which can otherwise land after the 'playing' event.
    // No-op unless activeAudioSessionRequired(); redundant with the deferred call,
    // which coalesces at RemoteAudioSession's IPC FIFO.
    maybeActivateAudioSession();

    if (m_alreadyScheduledSessionStatedUpdate)
        return;

    m_alreadyScheduledSessionStatedUpdate = true;
    enqueueTaskOnMainThread([this, protectedThis = Ref { *this }] {
        m_alreadyScheduledSessionStatedUpdate = false;
        maybeActivateAudioSession();
        updateSessionState();
    });
}

void MediaSessionManagerInterface::sessionIsPlayingToWirelessPlaybackTargetChanged(PlatformMediaSessionInterface& session)
{
    if (!m_isApplicationInBackground || !(restrictions(session.mediaType()).contains(MediaSessionRestriction::BackgroundProcessPlaybackRestricted)))
        return;

    if (session.state() != PlatformMediaSession::State::Interrupted)
        session.beginInterruption(PlatformMediaSession::InterruptionType::EnteringBackground);
}

void MediaSessionManagerInterface::setIsPlayingToAutomotiveHeadUnit(bool isPlayingToAutomotiveHeadUnit)
{
    if (isPlayingToAutomotiveHeadUnit == m_isPlayingToAutomotiveHeadUnit)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, isPlayingToAutomotiveHeadUnit);
    m_isPlayingToAutomotiveHeadUnit = isPlayingToAutomotiveHeadUnit;
}

void MediaSessionManagerInterface::addAudioCaptureSource(AudioCaptureSource& source)
{
    ASSERT(!m_audioCaptureSources.contains(source));
    m_audioCaptureSources.add(source);
    updateSessionState();
}


void MediaSessionManagerInterface::removeAudioCaptureSource(AudioCaptureSource& source)
{
    m_audioCaptureSources.remove(source);
    scheduleUpdateSessionState();
}

Ref<GenericPromise> MediaSessionManagerInterface::audioCaptureSourceStateChanged(IsCaptureStarting isCaptureStarting)
{
    updateSessionState();
#if USE(AUDIO_SESSION)
    // Activation and deactivation are requested synchronously, so a caller that does not wait on the
    // returned promise still observes AudioSession::isActive() as soon as this returns
    // (RemoteAudioSession reflects the requested state optimistically). The promise settles once the
    // activation has completed, for callers that need an active session — the getUserMedia promise and
    // the track unmute event.
    if (isCaptureStarting == IsCaptureStarting::Yes)
        return maybeActivateAudioSession();

    maybeDeactivateAudioSession();
#else
    UNUSED_PARAM(isCaptureStarting);
#endif
    return GenericPromise::createAndResolve();
}

int MediaSessionManagerInterface::countActiveAudioCaptureSources()
{
    int count = 0;
    for (Ref source : m_audioCaptureSources) {
        if (source->wantsToCaptureAudio())
            ++count;
    }
    return count;
}

void MediaSessionManagerInterface::processDidReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType command, const PlatformMediaSession::RemoteCommandArgument& argument)
{
#if ENABLE(VIDEO) || ENABLE(audio)
    RefPtr<PlatformMediaSessionInterface> activeSession;
    for (auto& weakSession : copySessionsToVector()) {
        RefPtr session = weakSession.get();
        if (!session || !session->canReceiveRemoteControlCommands())
            continue;

        if (session->isNowPlayingEligible()) {
            activeSession = WTF::move(session);
            break;
        }
        if (!activeSession)
            activeSession = WTF::move(session);
    }

    if (activeSession)
        activeSession->didReceiveRemoteControlCommand(command, argument);
#else
    UNUSED_PARAM(command);
    UNUSED_PARAM(argument);
#endif
}

void MediaSessionManagerInterface::processSystemWillSleep()
{
    if (m_currentInterruption)
        return;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([] (auto& session) {
        session.beginInterruption(PlatformMediaSession::InterruptionType::SystemSleep);
    });
#endif
}

void MediaSessionManagerInterface::processSystemDidWake()
{
    if (m_currentInterruption)
        return;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([] (auto& session) {
        session.endInterruption(PlatformMediaSession::EndInterruptionFlags::MayResumePlaying);
    });
#endif
}

void MediaSessionManagerInterface::addSession(PlatformMediaSessionInterface& session)
{
#if !RELEASE_LOG_DISABLED && (ENABLE(VIDEO) || ENABLE(WEB_AUDIO))
    m_logger->addLogger(protect(session.logger()));
    MEDIASESSIONMANAGERINTERFACE_RELEASE_LOG(AddSession, session.logIdentifier());
#endif

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    if (m_currentInterruption)
        session.beginInterruption(*m_currentInterruption);
#else
    UNUSED_PARAM(session);
#endif

    scheduleUpdateSessionState();
}

void MediaSessionManagerInterface::removeSession(PlatformMediaSessionInterface& session)
{
    UNUSED_PARAM(session);

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    MEDIASESSIONMANAGERINTERFACE_RELEASE_LOG(RemoveSession, session.logIdentifier());
#endif

    if (hasNoSession())
        maybeDeactivateAudioSession();

#if !RELEASE_LOG_DISABLED && (ENABLE(VIDEO) || ENABLE(WEB_AUDIO))
    m_logger->removeLogger(protect(session.logger()));
#endif

    scheduleUpdateSessionState();
}

bool MediaSessionManagerInterface::hasNoSession() const
{
    return sessions().isEmptyIgnoringNullReferences();
}

bool MediaSessionManagerInterface::computeSupportsSeeking() const
{
    if (RefPtr activeSession = currentSession())
        return activeSession->supportsSeeking();

    return false;
}

void MediaSessionManagerInterface::maybeDeactivateAudioSession(ShouldCheckRequiredSession shouldCheckRequiredSession)
{
#if USE(AUDIO_SESSION)
    if (!m_becameActive || !shouldDeactivateAudioSession())
        return;

    if (shouldCheckRequiredSession == ShouldCheckRequiredSession::Yes && activeAudioSessionRequired()) {
        ALWAYS_LOG(LOGIDENTIFIER, "skipping AudioSession deactivation, capture or media session still requires it");
        return;
    }

    // Honor an explicit category override (e.g., navigator.audioSession.type =
    // "play-and-record"): the page has expressed intent that the session remain
    // in that category, which implies keeping it active.
    if (AudioSession::singleton().categoryOverride() != AudioSession::CategoryType::None)
        return;

    // Don't tear down activation while an interruption is in progress —
    // the system audio session is in an unstable state and a follow-up
    // tryToSetActive(true) (e.g. from getUserMedia recall after the
    // interruption ends) may fail with the session in a corrupt state.
    if (AudioSession::singleton().isInterrupted())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, "tried to set inactive AudioSession");
    AudioSession::singleton().tryToSetActive(false)->whenSettled(RunLoop::mainSingleton(), [](auto&&) { });
    m_becameActive = false;
#else
    UNUSED_PARAM(shouldCheckRequiredSession);
#endif
}

Ref<GenericPromise> MediaSessionManagerInterface::maybeActivateAudioSession()
{
#if USE(AUDIO_SESSION)
    if (!audioSessionActivationRequired()) {
        MEDIASESSIONMANAGERINTERFACE_RELEASE_LOG(MaybeActivateAudioSessionActiveSessionNotRequired);
        // Do NOT deactivate here. This is an "activate or no-op" function, matching
        // the pre-async contract (7136b90d351d). Tearing the session down on any
        // re-evaluation where nothing currently "requires" it caused permanent audio
        // loss on pages whose audible content sits in Autoplaying rather than Playing
        // (e.g. YouTube DASH audio elements), because activeAudioSessionRequired()
        // excludes Autoplaying (PlatformMediaSession::activeAudioSessionRequired).
        // rdar://181517699 (REGRESSION 316303@main). Deactivation stays conservative,
        // driven only by the lifecycle callers (processWillSuspend / removeSession /
        // audioCaptureSourceStateChanged) via maybeDeactivateAudioSession().
        return GenericPromise::createAndResolve();
    }

    // Set m_becameActive optimistically so that an interleaved
    // maybeDeactivateAudioSession (e.g. from processWillSuspend) doesn't bail
    // out before the activation IPC's reply arrives. Revert on failure.
    // Mirrors the optimistic configuration().isActive update in
    // RemoteAudioSession::tryToSetActiveInternal.
    bool previousBecameActive = m_becameActive;
    m_becameActive = true;
    auto logSiteIdentifier = LOGIDENTIFIER;

    // Each call gets its own promise. RemoteAudioSession's FIFO chain
    // coalesces same-state IPCs at the IPC layer, so issuing tryToSetActive(true)
    // per call is harmless. The MSMI-level dedup via m_activationWaiters was
    // removed because it could stall a fresh activation request when a previous
    // activation's callback hadn't yet fired (e.g. processDidResume right after
    // processWillSuspend's deactivate — the still-pending earlier waiter would
    // block the resume's tryToSetActive(true) optimistic update from running).
    return AudioSession::singleton().tryToSetActive(true)->whenSettled(RunLoop::mainSingleton(),
        [this, protectedThis = Ref { *this }, previousBecameActive, logSiteIdentifier](auto&& result) mutable -> Ref<GenericPromise> {
            if (!result) {
                m_becameActive = previousBecameActive;
                ALWAYS_LOG(logSiteIdentifier, "failed to activate AudioSession");
                return GenericPromise::createAndReject();
            }

            ALWAYS_LOG(logSiteIdentifier, "successfully activated AudioSession");

            // Handle the race: session may have been removed while activation was in
            // flight. Key on hasNoSession() (genuine removal, as removeSession() does),
            // NOT !activeAudioSessionRequired(): this callback runs a runloop turn later,
            // and a still-alive session can be transiently not-required (Autoplaying, or
            // canProduceAudio flipping during a Video->VideoAudio transition). Keying on
            // "not required" would spuriously deactivate an active session mid-playback
            // (Release-timing race → internals.audioSessionActive() false at 'playing').
            if (hasNoSession())
                maybeDeactivateAudioSession();

            return GenericPromise::createAndResolve();
        });
#else
    return GenericPromise::createAndResolve();
#endif
}

void MediaSessionManagerInterface::scheduleUpdateSessionState()
{
    if (m_hasScheduledSessionStateUpdate)
        return;

    m_hasScheduledSessionStateUpdate = true;
    enqueueTaskOnMainThread([this, protectedThis = Ref { * this }] {
        updateSessionState();
        m_hasScheduledSessionStateUpdate = false;
    });
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaSessionManagerInterface::logChannel() const
{
    return LogMedia;
}

void MediaSessionManagerInterface::scheduleStateLog()
{
    if (m_stateLogTimer->isActive())
        return;

    static constexpr Seconds StateLogDelay { 5_s };
    m_stateLogTimer->startOneShot(StateLogDelay);
}

void MediaSessionManagerInterface::dumpSessionStates()
{
    StringBuilder builder;

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    forEachSession([&](auto& session) {
        builder.append('(', hex(session.logIdentifier()), "): "_s, session.description(), "\n"_s);
    });
#endif

    ALWAYS_LOG(LOGIDENTIFIER, " Sessions:\n", builder.toString());
}
#endif

bool MediaSessionManagerInterface::willLog(WTFLogLevel level) const
{
#if !RELEASE_LOG_DISABLED
    return m_logger->willLog(logChannel(), level);
#else
    UNUSED_PARAM(level);
    return false;
#endif
}


} // namespace WebCore
