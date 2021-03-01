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

#include "MediaMetadataInit.h"
#include "MediaSession.h"
#include <wtf/Optional.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

struct MediaImage;

using MediaSessionMetadata = MediaMetadataInit;

class MediaMetadata : public RefCounted<MediaMetadata> {
public:
    static ExceptionOr<Ref<MediaMetadata>> create(ScriptExecutionContext&, Optional<MediaMetadataInit>&&);
    ~MediaMetadata();

    void setMediaSession(MediaSession&);
    void resetMediaSession();

    const String& title() const { return m_metadata.title; }
    void setTitle(const String&);

    const String& artist() const { return m_metadata.artist; }
    void setArtist(const String&);

    const String& album() const { return m_metadata.album; }
    void setAlbum(const String&);

    const Vector<MediaImage>& artwork() const { return m_metadata.artwork; }
    ExceptionOr<void> setArtwork(ScriptExecutionContext&, Vector<MediaImage>&&);

    const MediaSessionMetadata& metadata() const { return m_metadata; }

private:
    MediaMetadata();
    void metadataUpdated();

    WeakPtr<MediaSession> m_session;
    MediaSessionMetadata m_metadata;
};

}

#endif
