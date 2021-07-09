/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(MEDIA_SESSION)

#include "ActiveDOMObject.h"
#include "ExceptionOr.h"
#include "MediaPositionState.h"
#include "MediaSessionAction.h"
#include "MediaSessionActionHandler.h"
#include "MediaSessionPlaybackState.h"
#include "MediaSessionReadyState.h"
#include <wtf/Logger.h>
#include <wtf/MonotonicTime.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class HTMLMediaElement;
class MediaMetadata;
class MediaSessionCoordinator;
class MediaSessionCoordinatorPrivate;
class Navigator;
template<typename> class DOMPromiseDeferred;

class MediaSession : public RefCounted<MediaSession>, public ActiveDOMObject, public CanMakeWeakPtr<MediaSession> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<MediaSession> create(Navigator&);
    ~MediaSession();

    MediaMetadata* metadata() const { return m_metadata.get(); };
    void setMetadata(RefPtr<MediaMetadata>&&);
    void metadataUpdated();

    MediaSessionPlaybackState playbackState() const { return m_playbackState; };
    void setPlaybackState(MediaSessionPlaybackState);

    void setActionHandler(MediaSessionAction, RefPtr<MediaSessionActionHandler>&&);

    void callActionHandler(const MediaSessionActionDetails&, DOMPromiseDeferred<void>&&);

    ExceptionOr<void> setPositionState(std::optional<MediaPositionState>&&);
    std::optional<MediaPositionState> positionState() const { return m_positionState; }

    WEBCORE_EXPORT std::optional<double> currentPosition() const;

    Document* document() const;
    
#if ENABLE(MEDIA_SESSION_COORDINATOR)
    MediaSessionReadyState readyState() const { return m_readyState; };
    void setReadyState(MediaSessionReadyState);

    MediaSessionCoordinator& coordinator() const { return m_coordinator.get(); }
#endif

#if ENABLE(MEDIA_SESSION_PLAYLIST)
    const Vector<Ref<MediaMetadata>>& playlist() const { return m_playlist; }
    ExceptionOr<void> setPlaylist(ScriptExecutionContext&, Vector<RefPtr<MediaMetadata>>&&);
#endif

    bool hasActiveActionHandlers() const { return !m_actionHandlers.isEmpty(); }
    enum class TriggerGestureIndicator {
        No,
        Yes,
    };
    WEBCORE_EXPORT bool callActionHandler(const MediaSessionActionDetails&, TriggerGestureIndicator = TriggerGestureIndicator::Yes);

    const Logger& logger() const { return *m_logger.get(); }

    class Observer : public CanMakeWeakPtr<Observer> {
    public:
        virtual ~Observer() = default;

        virtual void metadataChanged(const RefPtr<MediaMetadata>&) { }
        virtual void positionStateChanged(const std::optional<MediaPositionState>&) { }
        virtual void playbackStateChanged(MediaSessionPlaybackState) { }
        virtual void actionHandlersChanged() { }

#if ENABLE(MEDIA_SESSION_COORDINATOR)
        virtual void readyStateChanged(MediaSessionReadyState) { }
#endif
    };
    void addObserver(Observer&);
    void removeObserver(Observer&);

    RefPtr<HTMLMediaElement> activeMediaElement() const;

private:
    explicit MediaSession(Navigator&);

    const void* logIdentifier() const { return m_logIdentifier; }

    void forEachObserver(const Function<void(Observer&)>&);
    void notifyMetadataObservers();
    void notifyPositionStateObservers();
    void notifyPlaybackStateObservers();
    void notifyActionHandlerObservers();
    void notifyReadyStateObservers();

    // ActiveDOMObject
    const char* activeDOMObjectName() const final { return "MediaSession"; }
    void suspend(ReasonForSuspension) final;
    void stop() final;

    WeakPtr<Navigator> m_navigator;
    RefPtr<MediaMetadata> m_metadata;
    MediaSessionPlaybackState m_playbackState { MediaSessionPlaybackState::None };
    std::optional<MediaPositionState> m_positionState;
    std::optional<double> m_lastReportedPosition;
    MonotonicTime m_timeAtLastPositionUpdate;
    HashMap<MediaSessionAction, RefPtr<MediaSessionActionHandler>, WTF::IntHash<MediaSessionAction>, WTF::StrongEnumHashTraits<MediaSessionAction>> m_actionHandlers;
    RefPtr<const Logger> m_logger;
    const void* m_logIdentifier;

    WeakHashSet<Observer> m_observers;

#if ENABLE(MEDIA_SESSION_COORDINATOR)
    MediaSessionReadyState m_readyState { MediaSessionReadyState::Havenothing };
    const Ref<MediaSessionCoordinator> m_coordinator;
#endif

#if ENABLE(MEDIA_SESSION_PLAYLIST)
    Vector<Ref<MediaMetadata>> m_playlist;
#endif
};

String convertEnumerationToString(MediaSessionPlaybackState);
String convertEnumerationToString(MediaSessionAction);

}

namespace WTF {

template<> struct LogArgument<WebCore::MediaSessionPlaybackState> {
    static String toString(WebCore::MediaSessionPlaybackState state) { return convertEnumerationToString(state); }
};

template<> struct LogArgument<WebCore::MediaSessionAction> {
    static String toString(WebCore::MediaSessionAction action) { return convertEnumerationToString(action); }
};

}

#endif // ENABLE(MEDIA_SESSION)
