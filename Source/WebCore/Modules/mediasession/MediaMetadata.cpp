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

#include "config.h"
#include "MediaMetadata.h"

#if ENABLE(MEDIA_SESSION)

#include "Document.h"
#include "MediaImage.h"
#include "MediaMetadataInit.h"
#include <wtf/URL.h>

namespace WebCore {

ExceptionOr<Ref<MediaMetadata>> MediaMetadata::create(ScriptExecutionContext& context, Optional<MediaMetadataInit>&& init)
{
    auto metadata = adoptRef(*new MediaMetadata);
    if (init) {
        metadata->setTitle(init->title);
        metadata->setArtist(init->artist);
        metadata->setAlbum(init->album);
        auto possibleException = metadata->setArtwork(context, WTFMove(init->artwork));
        if (possibleException.hasException())
            return Exception { possibleException.exception() };
    }
    return metadata;
}

MediaMetadata::MediaMetadata() = default;
MediaMetadata::~MediaMetadata() = default;

void MediaMetadata::setMediaSession(MediaSession& session)
{
    m_session = makeWeakPtr(session);
}

void MediaMetadata::resetMediaSession()
{
    m_session.clear();
}

void MediaMetadata::setTitle(const String& title)
{
    if (m_metadata.title == title)
        return;

    m_metadata.title = title;
    metadataUpdated();
}

void MediaMetadata::setArtist(const String& artist)
{
    if (m_metadata.artist == artist)
        return;

    m_metadata.artist = artist;
    metadataUpdated();
}

void MediaMetadata::setAlbum(const String& album)
{
    if (m_metadata.album == album)
        return;

    m_metadata.album = album;
    metadataUpdated();
}

ExceptionOr<void> MediaMetadata::setArtwork(ScriptExecutionContext& context, Vector<MediaImage>&& artwork)
{
    Vector<MediaImage> resolvedArtwork;
    resolvedArtwork.reserveInitialCapacity(artwork.size());
    for (auto& image : artwork) {
        auto resolvedSrc = context.completeURL(image.src);
        if (!resolvedSrc.isValid())
            return Exception { TypeError };
        resolvedArtwork.uncheckedAppend(MediaImage { resolvedSrc.string(), image.sizes, image.type });
    }

    m_metadata.artwork = WTFMove(resolvedArtwork);
    metadataUpdated();
    return { };
}

void MediaMetadata::metadataUpdated()
{
    if (m_session)
        m_session->metadataUpdated();
}

}

#endif
