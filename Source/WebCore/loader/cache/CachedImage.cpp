/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "CachedImage.h"

#include "BitmapImage.h"
#include "MemoryCache.h"
#include "CachedResourceClient.h"
#include "CachedResourceClientWalker.h"
#include "CachedResourceLoader.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "FrameLoaderTypes.h"
#include "FrameView.h"
#include "Page.h"
#include "RenderObject.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "SubresourceLoader.h"
#include <wtf/CurrentTime.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

#if USE(CG)
#include "PDFDocumentImage.h"
#endif

#if ENABLE(SVG)
#include "SVGImage.h"
#endif

using std::max;

namespace WebCore {

CachedImage::CachedImage(const ResourceRequest& resourceRequest)
    : CachedResource(resourceRequest, ImageResource)
    , m_image(0)
    , m_decodedDataDeletionTimer(this, &CachedImage::decodedDataDeletionTimerFired)
    , m_shouldPaintBrokenImage(true)
{
    setStatus(Unknown);
}

CachedImage::CachedImage(Image* image)
    : CachedResource(ResourceRequest(), ImageResource)
    , m_image(image)
    , m_decodedDataDeletionTimer(this, &CachedImage::decodedDataDeletionTimerFired)
    , m_shouldPaintBrokenImage(true)
{
    setStatus(Cached);
    setLoading(false);
}

CachedImage::~CachedImage()
{
}

void CachedImage::decodedDataDeletionTimerFired(Timer<CachedImage>*)
{
    ASSERT(!hasClients());
    destroyDecodedData();
}

void CachedImage::load(CachedResourceLoader* cachedResourceLoader, const ResourceLoaderOptions& options)
{
    if (!cachedResourceLoader || cachedResourceLoader->autoLoadImages())
        CachedResource::load(cachedResourceLoader, options);
    else
        setLoading(false);
}

void CachedImage::removeClientForRenderer(RenderObject* renderer)
{
#if ENABLE(SVG)
    if (m_svgImageCache)
        m_svgImageCache->removeRendererFromCache(renderer);
#endif
    removeClient(renderer);
}

void CachedImage::didAddClient(CachedResourceClient* c)
{
    if (m_decodedDataDeletionTimer.isActive())
        m_decodedDataDeletionTimer.stop();
    
    if (m_data && !m_image && !errorOccurred()) {
        createImage();
        m_image->setData(m_data, true);
    }
    
    ASSERT(c->resourceClientType() == CachedImageClient::expectedType());
    if (m_image && !m_image->isNull())
        static_cast<CachedImageClient*>(c)->imageChanged(this);

    CachedResource::didAddClient(c);
}

void CachedImage::allClientsRemoved()
{
    if (m_image && !errorOccurred())
        m_image->resetAnimation();
    if (double interval = memoryCache()->deadDecodedDataDeletionInterval())
        m_decodedDataDeletionTimer.startOneShot(interval);
}

pair<Image*, float> CachedImage::brokenImage(float deviceScaleFactor) const
{
    if (deviceScaleFactor >= 2) {
        DEFINE_STATIC_LOCAL(Image*, brokenImageHiRes, (Image::loadPlatformResource("missingImage@2x").leakRef()));
        return make_pair(brokenImageHiRes, 2);
    }

    DEFINE_STATIC_LOCAL(Image*, brokenImageLoRes, (Image::loadPlatformResource("missingImage").leakRef()));
    return make_pair(brokenImageLoRes, 1);
}

bool CachedImage::willPaintBrokenImage() const
{
    return errorOccurred() && m_shouldPaintBrokenImage;
}

#if ENABLE(SVG)
inline Image* CachedImage::lookupOrCreateImageForRenderer(const RenderObject* renderer)
{
    if (!m_image)
        return 0;
    if (!m_image->isSVGImage())
        return m_image.get();
    Image* useImage = m_svgImageCache->lookupOrCreateBitmapImageForRenderer(renderer);
    if (useImage == Image::nullImage())
        return m_image.get();
    return useImage;
}
#else
inline Image* CachedImage::lookupOrCreateImageForRenderer(const RenderObject*)
{
    return m_image.get();
}
#endif

Image* CachedImage::image()
{
    ASSERT(!isPurgeable());

    if (errorOccurred() && m_shouldPaintBrokenImage) {
        // Returning the 1x broken image is non-ideal, but we cannot reliably access the appropriate
        // deviceScaleFactor from here. It is critical that callers use CachedImage::brokenImage() 
        // when they need the real, deviceScaleFactor-appropriate broken image icon. 
        return brokenImage(1).first;
    }

    if (m_image)
        return m_image.get();

    return Image::nullImage();
}

Image* CachedImage::imageForRenderer(const RenderObject* renderer)
{
    ASSERT(!isPurgeable());

    if (errorOccurred() && m_shouldPaintBrokenImage) {
        // Returning the 1x broken image is non-ideal, but we cannot reliably access the appropriate
        // deviceScaleFactor from here. It is critical that callers use CachedImage::brokenImage() 
        // when they need the real, deviceScaleFactor-appropriate broken image icon. 
        return brokenImage(1).first;
    }

    if (m_image)
        return lookupOrCreateImageForRenderer(renderer);

    return Image::nullImage();
}

void CachedImage::setContainerSizeForRenderer(const RenderObject* renderer, const IntSize& containerSize, float containerZoom)
{
    if (!m_image || containerSize.isEmpty())
        return;
#if ENABLE(SVG)
    if (!m_image->isSVGImage()) {
        m_image->setContainerSize(containerSize);
        return;
    }

    // FIXME (85335): This needs to take CSS transform scale into account as well.
    float containerScale = renderer->document()->page()->deviceScaleFactor() * renderer->document()->page()->pageScaleFactor();

    m_svgImageCache->setRequestedSizeAndScales(renderer, SVGImageCache::SizeAndScales(containerSize, containerZoom, containerScale));
#else
    UNUSED_PARAM(renderer);
    UNUSED_PARAM(containerZoom);
    m_image->setContainerSize(containerSize);
#endif
}

bool CachedImage::usesImageContainerSize() const
{
    if (m_image)
        return m_image->usesContainerSize();

    return false;
}

bool CachedImage::imageHasRelativeWidth() const
{
    if (m_image)
        return m_image->hasRelativeWidth();

    return false;
}

bool CachedImage::imageHasRelativeHeight() const
{
    if (m_image)
        return m_image->hasRelativeHeight();

    return false;
}

IntSize CachedImage::imageSizeForRenderer(const RenderObject* renderer, float multiplier)
{
    ASSERT(!isPurgeable());

    if (!m_image)
        return IntSize();

    IntSize imageSize;

    if (m_image->isBitmapImage() && (renderer && renderer->shouldRespectImageOrientation() == RespectImageOrientation))
        imageSize = static_cast<BitmapImage*>(m_image.get())->sizeRespectingOrientation();
    else
        imageSize = m_image->size();

#if ENABLE(SVG)
    if (m_image->isSVGImage()) {
        SVGImageCache::SizeAndScales sizeAndScales = m_svgImageCache->requestedSizeAndScales(renderer);
        if (!sizeAndScales.size.isEmpty()) {
            imageSize.setWidth(sizeAndScales.size.width() / sizeAndScales.zoom);
            imageSize.setHeight(sizeAndScales.size.height() / sizeAndScales.zoom);
        }
    }
#endif

    if (multiplier == 1.0f)
        return imageSize;
        
    // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
    float widthScale = m_image->hasRelativeWidth() ? 1.0f : multiplier;
    float heightScale = m_image->hasRelativeHeight() ? 1.0f : multiplier;
    IntSize minimumSize(imageSize.width() > 0 ? 1 : 0, imageSize.height() > 0 ? 1 : 0);
    imageSize.scale(widthScale, heightScale);
    imageSize.clampToMinimumSize(minimumSize);
    return imageSize;
}

void CachedImage::computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio)
{
    if (m_image)
        m_image->computeIntrinsicDimensions(intrinsicWidth, intrinsicHeight, intrinsicRatio);
}

void CachedImage::notifyObservers(const IntRect* changeRect)
{
    CachedResourceClientWalker<CachedImageClient> w(m_clients);
    while (CachedImageClient* c = w.next())
        c->imageChanged(this, changeRect);
}

void CachedImage::checkShouldPaintBrokenImage()
{
    if (!m_loader || m_loader->reachedTerminalState())
        return;

    m_shouldPaintBrokenImage = m_loader->frameLoader()->client()->shouldPaintBrokenImage(m_resourceRequest.url());
}

void CachedImage::clear()
{
    destroyDecodedData();
#if ENABLE(SVG)
    m_svgImageCache.clear();
#endif
    m_image = 0;
    setEncodedSize(0);
}

inline void CachedImage::createImage()
{
    // Create the image if it doesn't yet exist.
    if (m_image)
        return;
#if USE(CG) && !USE(WEBKIT_IMAGE_DECODERS)
    if (m_response.mimeType() == "application/pdf") {
        m_image = PDFDocumentImage::create();
        return;
    }
#endif
#if ENABLE(SVG)
    if (m_response.mimeType() == "image/svg+xml") {
        RefPtr<SVGImage> svgImage = SVGImage::create(this);
        m_svgImageCache = SVGImageCache::create(svgImage.get());
        m_image = svgImage.release();
        return;
    }
#endif
    m_image = BitmapImage::create(this);
}

size_t CachedImage::maximumDecodedImageSize()
{
    if (!m_loader || m_loader->reachedTerminalState())
        return 0;
    Settings* settings = m_loader->frameLoader()->frame()->settings();
    return settings ? settings->maximumDecodedImageSize() : 0;
}

void CachedImage::data(PassRefPtr<SharedBuffer> data, bool allDataReceived)
{
    m_data = data;

    createImage();

    bool sizeAvailable = false;

    // Have the image update its data from its internal buffer.
    // It will not do anything now, but will delay decoding until 
    // queried for info (like size or specific image frames).
    sizeAvailable = m_image->setData(m_data, allDataReceived);

    // Go ahead and tell our observers to try to draw if we have either
    // received all the data or the size is known.  Each chunk from the
    // network causes observers to repaint, which will force that chunk
    // to decode.
    if (sizeAvailable || allDataReceived) {
        size_t maxDecodedImageSize = maximumDecodedImageSize();
        IntSize s = m_image->size();
        size_t estimatedDecodedImageSize = s.width() * s.height() * 4; // no overflow check
        if (m_image->isNull() || (maxDecodedImageSize > 0 && estimatedDecodedImageSize > maxDecodedImageSize)) {
            error(errorOccurred() ? status() : DecodeError);
            if (inCache())
                memoryCache()->remove(this);
            return;
        }
        
        // It would be nice to only redraw the decoded band of the image, but with the current design
        // (decoding delayed until painting) that seems hard.
        notifyObservers();

        if (m_image)
            setEncodedSize(m_image->data() ? m_image->data()->size() : 0);
    }
    
    if (allDataReceived) {
        setLoading(false);
        checkNotify();
    }
}

void CachedImage::error(CachedResource::Status status)
{
    checkShouldPaintBrokenImage();
    clear();
    setStatus(status);
    ASSERT(errorOccurred());
    m_data.clear();
    notifyObservers();
    setLoading(false);
    checkNotify();
}

void CachedImage::setResponse(const ResourceResponse& response)
{
    if (!m_response.isNull())
        clear();
    CachedResource::setResponse(response);
}

void CachedImage::destroyDecodedData()
{
    bool canDeleteImage = !m_image || (m_image->hasOneRef() && m_image->isBitmapImage());
    if (isSafeToMakePurgeable() && canDeleteImage && !isLoading()) {
        // Image refs the data buffer so we should not make it purgeable while the image is alive. 
        // Invoking addClient() will reconstruct the image object.
        m_image = 0;
        setDecodedSize(0);
        if (!MemoryCache::shouldMakeResourcePurgeableOnEviction())
            makePurgeable(true);
    } else if (m_image && !errorOccurred())
        m_image->destroyDecodedData();
}

void CachedImage::decodedSizeChanged(const Image* image, int delta)
{
    if (!image || image != m_image)
        return;
    
    setDecodedSize(decodedSize() + delta);
}

void CachedImage::didDraw(const Image* image)
{
    if (!image || image != m_image)
        return;
    
    double timeStamp = FrameView::currentPaintTimeStamp();
    if (!timeStamp) // If didDraw is called outside of a Frame paint.
        timeStamp = currentTime();
    
    CachedResource::didAccessDecodedData(timeStamp);
}

bool CachedImage::shouldPauseAnimation(const Image* image)
{
    if (!image || image != m_image)
        return false;
    
    CachedResourceClientWalker<CachedImageClient> w(m_clients);
    while (CachedImageClient* c = w.next()) {
        if (c->willRenderImage(this))
            return false;
    }

    return true;
}

void CachedImage::animationAdvanced(const Image* image)
{
    if (!image || image != m_image)
        return;
    notifyObservers();
}

void CachedImage::changedInRect(const Image* image, const IntRect& rect)
{
    if (!image || image != m_image)
        return;
#if ENABLE(SVG)
    // We have to update the cached ImageBuffers if the underlying content changed.
    if (image->isSVGImage()) {
        m_svgImageCache->imageContentChanged();
        return;
    }
#endif
    notifyObservers(&rect);
}

} // namespace WebCore
