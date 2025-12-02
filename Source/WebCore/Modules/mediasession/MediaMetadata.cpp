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
#include "DocumentResourceLoader.h"
#include "ExceptionOr.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "IntSize.h"
#include "MediaImage.h"
#include "MediaMetadataInit.h"
#include "SpaceSplitString.h"
#include <ranges>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ArtworkImageLoader);

Ref<ArtworkImageLoader> ArtworkImageLoader::create(Document& document, const String& src, ArtworkImageLoaderCallback&& callback)
{
    return adoptRef(*new ArtworkImageLoader(document, src, WTFMove(callback)));
}

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
    RefPtr document = m_document.get();
    options.contentSecurityPolicyImposition = document->isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;

    CachedResourceRequest request(ResourceRequest(document->completeURL(m_src)), options);
    request.setInitiatorType(AtomString { document->documentURI() });
    m_cachedImage = document->protectedCachedResourceLoader()->requestImage(WTFMove(request)).value_or(nullptr);

    if (m_cachedImage)
        m_cachedImage->addClient(*this);
}

void ArtworkImageLoader::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess)
{
    ASSERT_UNUSED(resource, &resource == m_cachedImage);
    if (m_cachedImage->loadFailedOrCanceled() || m_cachedImage->errorOccurred() || !m_cachedImage->image()) {
        m_callback(nullptr);
        return;
    }
    Ref image = *m_cachedImage->image();
    image->subresourcesAreFinished(nullptr, [image, callback = std::exchange(m_callback, { })]() mutable {
        callback(image.ptr());
    });
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

Ref<MediaMetadata> MediaMetadata::create(MediaSession& session, Vector<URL>&& images)
{
    auto metadata = adoptRef(*new MediaMetadata);
    metadata->m_defaultImages = WTFMove(images);
    metadata->setMediaSession(session);
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
    ASSERT(!m_defaultImages.size());
    Vector<MediaImage> resolvedArtwork;
    resolvedArtwork.reserveInitialCapacity(artwork.size());
    for (auto& image : artwork) {
        auto resolvedSrc = context.completeURL(image.src);
        if (!resolvedSrc.isValid())
            return Exception { ExceptionCode::TypeError };
        resolvedArtwork.append(MediaImage { resolvedSrc.string(), image.sizes, image.type });
    }

    m_metadata.artwork = WTFMove(resolvedArtwork);
    refreshArtworkImage();

    metadataUpdated();
    return { };
}

// We attempt to give a score to the image dimensions. This score is between 0 and 1.
// A negative score indicates an invalid image
// A score of 0 is for images smaller than the minimum size
// A score of 1 indicates an image of ideal size with an aspect ratio of 1 (square)
// The closer to the ideal size, the higher the score.
static float imageDimensionsScore(int width, int height, int minimumSize, int idealSize)
{

    IntSize size { width, height };
    if (size.isEmpty())
        return -1;

    if (size.maxDimension() <= minimumSize)
        return 0; // Ignore images that are too small.

    // We account for the aspect ratio in scoring the "best" artwork.
    // We prefer artwork's images with a square AR.
    double longEdge = size.maxDimension();
    double shortEdge = size.minDimension();
    auto aspectRatioCoefficient = shortEdge / longEdge;

    if (longEdge < idealSize)
        return aspectRatioCoefficient * (0.8 * (longEdge - minimumSize) / (idealSize - minimumSize) + 0.2);
    return aspectRatioCoefficient * (1.0 * idealSize / longEdge);
}

void MediaMetadata::refreshArtworkImage()
{
    static_assert(s_minimumSize < s_idealSize);

    m_artworkLoader = nullptr;

    if (!m_session)
        return;

    m_artworkImageSrc = String();
    m_artworkImage = nullptr;

    size_t numArtworks = m_defaultImages.size() ? m_defaultImages.size() : m_metadata.artwork.size();
    if (!numArtworks)
        return;

    // First look into the artwork's sizes attributes to attempt to determine the best score.
    Vector<Pair> artworks(numArtworks, [&](size_t index) -> Pair {
        if (m_defaultImages.size())
            return { -1, m_defaultImages[index].string() };
        auto size = [&](const String& sizes) -> IntSize {
            if (sizes.isEmpty())
                return { };
            if (equalIgnoringASCIICase(sizes, "any"_s))
                return { s_idealSize, s_idealSize }; // We prefer image tagged with "any" size.
            IntSize size;
            for (auto element : StringView(sizes).split(' ')) {
                if (element.isEmpty())
                    continue;
                auto posX = element.findIgnoringASCIICase("x"_s);
                if (posX == notFound || !posX)
                    return { };
                std::optional<uint32_t> width = parseInteger<uint32_t>(element.left(posX));
                std::optional<uint32_t> height = parseInteger<uint32_t>(element.right(posX));
                if (!width || !height)
                    return { };

                IntSize newSize { int(*width), int(*height) };
                if (size.maxDimension() < newSize.maxDimension())
                    size = newSize;
            }
            return size;
        }(m_metadata.artwork[index].sizes);
        return { imageDimensionsScore(size.width(), size.height(), s_minimumSize, s_idealSize), m_metadata.artwork[index].src };
    });

    std::ranges::sort(artworks, std::ranges::greater { }, &Pair::score);

    tryNextArtworkImage(0, WTFMove(artworks));
}

void MediaMetadata::tryNextArtworkImage(uint32_t index, Vector<Pair>&& artworks)
{
    if (!m_session)
        return;
    RefPtr document = m_session->document();
    if (!document)
        return;

    String artworkImageSrc = artworks[index].src;

    m_artworkLoader = ArtworkImageLoader::create(*document, artworkImageSrc, [this, index, artworkImageSrc, artworks = WTFMove(artworks)](Image* image) mutable {
        if (image && image->data() && image->width() && image->height()) {
            IntSize size { int(image->width()), int(image->height()) };
            float imageScore = imageDimensionsScore(size.width(), size.height(), s_minimumSize, s_idealSize);
            if (!index || (m_artworkImage && (imageDimensionsScore(m_artworkImage->width(), m_artworkImage->height(), s_minimumSize, s_idealSize) < imageScore))) {
                m_artworkImageSrc = artworkImageSrc;
                setArtworkImage(image);
                metadataUpdated();
            }
            // If selection from `sizes` attribute yielded a valid image, or we have downloaded an image bigger than the ideal size we stop.
            if (artworks[index].score >= 0 || size.maxDimension() >= s_idealSize)
                return;
        }

        if (++index < artworks.size())
            tryNextArtworkImage(index, WTFMove(artworks));
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
        m_session->metadataUpdated(*this);
}

}

#endif
