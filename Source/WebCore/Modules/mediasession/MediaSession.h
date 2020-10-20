/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "MediaPositionState.h"
#include "MediaSessionAction.h"
#include "MediaSessionActionHandler.h"
#include "MediaSessionPlaybackState.h"
#include <wtf/MonotonicTime.h>
#include <wtf/Optional.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class MediaMetadata;
class Navigator;

class MediaSession : public RefCounted<MediaSession>, public CanMakeWeakPtr<MediaSession> {
public:
    static Ref<MediaSession> create(Navigator&);
    ~MediaSession();

    MediaMetadata* metadata() const { return m_metadata.get(); };
    void setMetadata(RefPtr<MediaMetadata>&&);

    MediaSessionPlaybackState playbackState() const { return m_playbackState; };
    void setPlaybackState(MediaSessionPlaybackState);

    void setActionHandler(MediaSessionAction, RefPtr<MediaSessionActionHandler>&&);
    ExceptionOr<void> setPositionState(Optional<MediaPositionState>&&);
    WEBCORE_EXPORT Optional<double> currentPosition() const;

    void metadataUpdated();
    void actionHandlersUpdated();
    bool hasActionHandler(MediaSessionAction) const;
    WEBCORE_EXPORT RefPtr<MediaSessionActionHandler> handlerForAction(MediaSessionAction) const;

private:
    explicit MediaSession(Navigator&);

    WeakPtr<Navigator> m_navigator;
    RefPtr<MediaMetadata> m_metadata;
    MediaSessionPlaybackState m_playbackState { MediaSessionPlaybackState::None };
    Optional<MediaPositionState> m_positionState;
    Optional<double> m_lastReportedPosition;
    MonotonicTime m_timeAtLastPositionUpdate;
    HashMap<MediaSessionAction, RefPtr<MediaSessionActionHandler>, WTF::IntHash<MediaSessionAction>, WTF::StrongEnumHashTraits<MediaSessionAction>> m_actionHandlers;
};

}

#endif // ENABLE(MEDIA_SESSION)
