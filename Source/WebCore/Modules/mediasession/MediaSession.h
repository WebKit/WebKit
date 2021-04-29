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
#include "EventTarget.h"
#include "GenericEventQueue.h"
#include "MediaPositionState.h"
#include "MediaSessionAction.h"
#include "MediaSessionActionHandler.h"
#include "MediaSessionPlaybackState.h"
#include "MediaSessionReadyState.h"
#include <wtf/Logger.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Optional.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class HTMLMediaElement;
class MediaMetadata;
class MediaSessionCoordinator;
class Navigator;

class MediaSession : public RefCounted<MediaSession>, public ActiveDOMObject, public EventTargetWithInlineData {
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

    ExceptionOr<void> setPositionState(Optional<MediaPositionState>&&);
    Optional<MediaPositionState> positionState() const { return m_positionState; }

    WEBCORE_EXPORT Optional<double> currentPosition() const;

    Document* document() const;
    
#if ENABLE(MEDIA_SESSION_COORDINATOR)
    MediaSessionReadyState readyState() const { return m_readyState; };
    void setReadyState(MediaSessionReadyState);

    MediaSessionCoordinator* coordinator() const { return m_coordinator.get(); }
    WEBCORE_EXPORT void setCoordinator(MediaSessionCoordinator*);
#endif

#if ENABLE(MEDIA_SESSION_PLAYLIST)
    const Vector<Ref<MediaMetadata>>& playlist() const { return m_playlist; }
    ExceptionOr<void> setPlaylist(ScriptExecutionContext&, Vector<RefPtr<MediaMetadata>>&&);
#endif

    bool hasActiveActionHandlers() const { return !m_actionHandlers.isEmpty(); }
    WEBCORE_EXPORT bool callActionHandler(const MediaSessionActionDetails&);

    const Logger& logger() const { return *m_logger.get(); }

    // EventTarget
    using RefCounted::ref;
    using RefCounted::deref;

    class Observer : public CanMakeWeakPtr<Observer> {
    public:
        virtual ~Observer() = default;

        virtual void metadataChanged(const RefPtr<MediaMetadata>&) { }
        virtual void positionStateChanged(const Optional<MediaPositionState>&) { }
        virtual void playbackStateChanged(MediaSessionPlaybackState) { }
        virtual void actionHandlersChanged() { }

#if ENABLE(MEDIA_SESSION_COORDINATOR)
        virtual void readyStateChanged(MediaSessionReadyState) { }
#endif
    };
    void addObserver(Observer&);
    void removeObserver(Observer&);

private:
    explicit MediaSession(Navigator&);

    const void* logIdentifier() const { return m_logIdentifier; }

    void forEachObserver(const Function<void(Observer&)>&);
    void notifyMetadataObservers();
    void notifyPositionStateObservers();
    void notifyPlaybackStateObservers();
    void notifyActionHandlerObservers();
    void notifyReadyStateObservers();

    // EventTarget
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    EventTargetInterface eventTargetInterface() const final { return MediaSessionEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }

    // ActiveDOMObject
    const char* activeDOMObjectName() const final { return "MediaSession"; }
    bool virtualHasPendingActivity() const final;

    RefPtr<HTMLMediaElement> activeMediaElement() const;

    WeakPtr<Navigator> m_navigator;
    RefPtr<MediaMetadata> m_metadata;
    MediaSessionPlaybackState m_playbackState { MediaSessionPlaybackState::None };
    Optional<MediaPositionState> m_positionState;
    Optional<double> m_lastReportedPosition;
    MonotonicTime m_timeAtLastPositionUpdate;
    HashMap<MediaSessionAction, RefPtr<MediaSessionActionHandler>, WTF::IntHash<MediaSessionAction>, WTF::StrongEnumHashTraits<MediaSessionAction>> m_actionHandlers;
    RefPtr<const Logger> m_logger;
    const void* m_logIdentifier;

    WeakHashSet<Observer> m_observers;
    UniqueRef<MainThreadGenericEventQueue> m_asyncEventQueue;

#if ENABLE(MEDIA_SESSION_COORDINATOR)
    MediaSessionReadyState m_readyState { MediaSessionReadyState::HaveNothing };
    RefPtr<MediaSessionCoordinator> m_coordinator;
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
