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

#include "BitmapImage.h"
#include "CachedImage.h"
#include "CachedResourceLoader.h"
#include "Document.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "MediaImage.h"
#include "MediaMetadataInit.h"
#include "SpaceSplitString.h"
#include <wtf/URL.h>

namespace WebCore {

ArtworkImageLoader::ArtworkImageLoader(Document& document, const String& src, ArtworkImageLoaderCallback&& callback)
    : m_document(document)
    , m_src(src)
    , m_callback(WTFMove(callback))
{
}

ArtworkImageLoader::~ArtworkImageLoader()
{
    if (m_cachedImage)
        m_cachedImage->removeClient(*this);
}

void ArtworkImageLoader::requestImageResource()
{
    ASSERT(!m_cachedImage, "Can only call requestImageResource once");
    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.contentSecurityPolicyImposition = m_document.isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;

    CachedResourceRequest request(ResourceRequest(m_document.completeURL(m_src)), options);
    request.setInitiatorType(AtomString { m_document.documentURI() });
    m_cachedImage = m_document.cachedResourceLoader().requestImage(WTFMove(request)).value_or(nullptr);

    if (m_cachedImage)
        m_cachedImage->addClient(*this);
}

void ArtworkImageLoader::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&)
{
    ASSERT_UNUSED(resource, &resource == m_cachedImage);
    if (m_cachedImage->loadFailedOrCanceled() || m_cachedImage->errorOccurred() || !m_cachedImage->image()) {
        m_callback(nullptr);
        return;
    }
    m_callback(m_cachedImage->image());
}

ExceptionOr<Ref<MediaMetadata>> MediaMetadata::create(ScriptExecutionContext& context, std::optional<MediaMetadataInit>&& init)
{
    auto metadata = adoptRef(*new MediaMetadata);
    if (init) {
        metadata->setTitle(init->title);
        metadata->setArtist(init->artist);
        metadata->setAlbum(init->album);
#if ENABLE(MEDIA_SESSION_PLAYLIST)
        metadata->setTrackIdentifier(init->trackIdentifier);
#endif
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
    m_session = session;
    refreshArtworkImage();
}

void MediaMetadata::resetMediaSession()
{
    m_session.clear();
    m_artworkLoader = nullptr;
    m_artworkImage = nullptr;
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

    refreshArtworkImage();

    metadataUpdated();
    return { };
}

void MediaMetadata::refreshArtworkImage()
{
    const auto& mediaImages = m_metadata.artwork;
    if (mediaImages.isEmpty()) {
        m_artworkLoader = nullptr;
        return;
    }
    if (!m_session || !m_session->document() || mediaImages[0].src == m_artworkImageSrc)
        return;
    // FIXME: Implement a heuristic to retrieve the "best" image.
    m_artworkImageSrc = mediaImages[0].src;
    m_artworkLoader = makeUnique<ArtworkImageLoader>(*m_session->document(), m_artworkImageSrc, [this](Image* image) {
        if (!image || !image->data())
            return;
        setArtworkImage(image);
        metadataUpdated();
    });
    m_artworkLoader->requestImageResource();
}

void MediaMetadata::setArtworkImage(Image* image)
{
    m_artworkImage = image;
}

#if ENABLE(MEDIA_SESSION_PLAYLIST)
void MediaMetadata::setTrackIdentifier(const String& identifier)
{
    if (m_metadata.trackIdentifier == identifier)
        return;

    m_metadata.trackIdentifier = identifier;
    metadataUpdated();
}
#endif

void MediaMetadata::metadataUpdated()
{
    if (m_session)
        m_session->metadataUpdated();
}

}

#endif
