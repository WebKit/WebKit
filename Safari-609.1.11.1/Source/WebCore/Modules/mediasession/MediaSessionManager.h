/*
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_SESSION)

#include "MediaSessionInterruptionProvider.h"
#include <wtf/HashSet.h>

namespace WebCore {

class MediaSession;

class MediaSessionManager : public MediaSessionInterruptionProviderClient {
    friend class NeverDestroyed<MediaSessionManager>;
public:
    WEBCORE_EXPORT static MediaSessionManager& singleton();

    WEBCORE_EXPORT void togglePlayback();
    WEBCORE_EXPORT void skipToNextTrack();
    WEBCORE_EXPORT void skipToPreviousTrack();

    WEBCORE_EXPORT void didReceiveStartOfInterruptionNotification(MediaSessionInterruptingCategory) override;
    WEBCORE_EXPORT void didReceiveEndOfInterruptionNotification(MediaSessionInterruptingCategory) override;

private:
    friend class MediaSession;

    MediaSessionManager();

    bool hasActiveMediaElements() const;

    void addMediaSession(MediaSession&);
    void removeMediaSession(MediaSession&);

    RefPtr<MediaSessionInterruptionProvider> m_interruptionProvider;
    HashSet<MediaSession*> m_sessions;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_SESSION)
