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
#include "CachedImageClient.h"
#include "CachedResourceClient.h"
#include "CachedResourceClientWalker.h"
#include "CachedResourceLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameLoaderTypes.h"
#include "FrameView.h"
#include "MemoryCache.h"
#include "Page.h"
#include "RenderElement.h"
#include "ResourceBuffer.h"
#include "SVGImage.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SubresourceLoader.h"
#include <wtf/CurrentTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

#if PLATFORM(IOS)
#include "SystemMemory.h"
#endif

#if USE(CG)
#include "PDFDocumentImage.h"
#endif

#if ENABLE(DISK_IMAGE_CACHE)
#include "DiskImageCacheIOS.h"
#endif

namespace WebCore {

CachedImage::CachedImage(const ResourceRequest& resourceRequest, SessionID sessionID)
    : CachedResource(resourceRequest, ImageResource, sessionID)
    , m_image(0)
    , m_isManuallyCached(false)
    , m_shouldPaintBrokenImage(true)
{
    setStatus(Unknown);
}

CachedImage::CachedImage(Image* image, SessionID sessionID)
    : CachedResource(ResourceRequest(), ImageResource, sessionID)
    , m_image(image)
    , m_isManuallyCached(false)
    , m_shouldPaintBrokenImage(true)
{
    setStatus(Cached);
    setLoading(false);
}

CachedImage::CachedImage(const URL& url, Image* image, SessionID sessionID)
    : CachedResource(ResourceRequest(url), ImageResource, sessionID)
    , m_image(image)
    , m_isManuallyCached(false)
    , m_shouldPaintBrokenImage(true)
{
    setStatus(Cached);
    setLoading(false);
}

CachedImage::CachedImage(const URL& url, Image* image, CachedImage::CacheBehaviorType type, SessionID sessionID)
    : CachedResource(ResourceRequest(url), ImageResource, sessionID)
    , m_image(image)
    , m_isManuallyCached(type == CachedImage::ManuallyCached)
    , m_shouldPaintBrokenImage(true)
{
    setStatus(Cached);
    setLoading(false);
    if (UNLIKELY(isManuallyCached())) {
        // Use the incoming URL in the response field. This ensures that code
        // using the response directly, such as origin checks for security,
        // actually see something.
        m_response.setURL(url);
    }
}

CachedImage::~CachedImage()
{
    clearImage();
}

void CachedImage::load(CachedResourceLoader* cachedResourceLoader, const ResourceLoaderOptions& options)
{
    if (!cachedResourceLoader || cachedResourceLoader->autoLoadImages())
        CachedResource::load(cachedResourceLoader, options);
    else
        setLoading(false);
}

void CachedImage::didAddClient(CachedResourceClient* c)
{
    if (m_data && !m_image && !errorOccurred()) {
        createImage();
        m_image->setData(m_data->sharedBuffer(), true);
    }
    
    ASSERT(c->resourceClientType() == CachedImageClient::expectedType());
    if (m_image && !m_image->isNull())
        static_cast<CachedImageClient*>(c)->imageChanged(this);

    CachedResource::didAddClient(c);
}

void CachedImage::didRemoveClient(CachedResourceClient* c)
{
    ASSERT(c);
    ASSERT(c->resourceClientType() == CachedImageClient::expectedType());

    m_pendingContainerSizeRequests.remove(static_cast<CachedImageClient*>(c));

    if (m_svgImageCache)
        m_svgImageCache->removeClientFromCache(static_cast<CachedImageClient*>(c));

    CachedResource::didRemoveClient(c);
}

void CachedImage::switchClientsToRevalidatedResource()
{
    ASSERT(resourceToRevalidate());
    ASSERT(resourceToRevalidate()->isImage());
    // Pending container size requests need to be transferred to the revalidated resource.
    if (!m_pendingContainerSizeRequests.isEmpty()) {
        // A copy of pending size requests is needed as they are deleted during CachedResource::switchClientsToRevalidateResouce().
        ContainerSizeRequests switchContainerSizeRequests;
        for (ContainerSizeRequests::iterator it = m_pendingContainerSizeRequests.begin(); it != m_pendingContainerSizeRequests.end(); ++it)
            switchContainerSizeRequests.set(it->key, it->value);
        CachedResource::switchClientsToRevalidatedResource();
        CachedImage* revalidatedCachedImage = toCachedImage(resourceToRevalidate());
        for (ContainerSizeRequests::iterator it = switchContainerSizeRequests.begin(); it != switchContainerSizeRequests.end(); ++it)
            revalidatedCachedImage->setContainerSizeForRenderer(it->key, it->value.first, it->value.second);
        return;
    }

    CachedResource::switchClientsToRevalidatedResource();
}

void CachedImage::allClientsRemoved()
{
    m_pendingContainerSizeRequests.clear();
    if (m_image && !errorOccurred())
        m_image->resetAnimation();
}

std::pair<Image*, float> CachedImage::brokenImage(float deviceScaleFactor) const
{
    if (deviceScaleFactor >= 2) {
        static NeverDestroyed<Image*> brokenImageHiRes(Image::loadPlatformResource("missingImage@2x").leakRef());
        return std::make_pair(brokenImageHiRes, 2);
    }

    static NeverDestroyed<Image*> brokenImageLoRes(Image::loadPlatformResource("missingImage").leakRef());
    return std::make_pair(brokenImageLoRes, 1);
}

bool CachedImage::willPaintBrokenImage() const
{
    return errorOccurred() && m_shouldPaintBrokenImage;
}

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

    if (!m_image)
        return Image::nullImage();

    if (m_image->isSVGImage()) {
        Image* image = m_svgImageCache->imageForRenderer(renderer);
        if (image != Image::nullImage())
            return image;
    }
    return m_image.get();
}

void CachedImage::setContainerSizeForRenderer(const CachedImageClient* renderer, const LayoutSize& containerSize, float containerZoom)
{
    if (containerSize.isEmpty())
        return;
    ASSERT(renderer);
    ASSERT(containerZoom);
    if (!m_image) {
        m_pendingContainerSizeRequests.set(renderer, SizeAndZoom(containerSize, containerZoom));
        return;
    }

    if (!m_image->isSVGImage()) {
        m_image->setContainerSize(containerSize);
        return;
    }

    m_svgImageCache->setContainerSizeForRenderer(renderer, containerSize, containerZoom);
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

LayoutSize CachedImage::imageSizeForRenderer(const RenderObject* renderer, float multiplier, SizeType sizeType)
{
    ASSERT(!isPurgeable());

    if (!m_image)
        return LayoutSize();

    LayoutSize imageSize(m_image->size());

#if ENABLE(CSS_IMAGE_ORIENTATION)
    if (renderer && m_image->isBitmapImage()) {
        ImageOrientationDescription orientationDescription(renderer->shouldRespectImageOrientation(), renderer->style().imageOrientation());
        if (orientationDescription.respectImageOrientation() == RespectImageOrientation)
            imageSize = LayoutSize(toBitmapImage(m_image.get())->sizeRespectingOrientation(orientationDescription));
    }
#else
    if (m_image->isBitmapImage() && (renderer && renderer->shouldRespectImageOrientation() == RespectImageOrientation))
#if !PLATFORM(IOS)
        imageSize = LayoutSize(toBitmapImage(m_image.get())->sizeRespectingOrientation());
#else
    {
        // On iOS, the image may have been subsampled to accommodate our size restrictions. However
        // we should tell the renderer what the original size was.
        imageSize = LayoutSize(toBitmapImage(m_image.get())->originalSizeRespectingOrientation());
    } else if (m_image->isBitmapImage())
        imageSize = LayoutSize(toBitmapImage(m_image.get())->originalSize());
#endif // !PLATFORM(IOS)
#endif // ENABLE(CSS_IMAGE_ORIENTATION)

    else if (m_image->isSVGImage() && sizeType == UsedSize) {
        imageSize = LayoutSize(m_svgImageCache->imageSizeForRenderer(renderer));
    }

    if (multiplier == 1.0f)
        return imageSize;
        
    // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
    float widthScale = m_image->hasRelativeWidth() ? 1.0f : multiplier;
    float heightScale = m_image->hasRelativeHeight() ? 1.0f : multiplier;
    LayoutSize minimumSize(imageSize.width() > 0 ? 1 : 0, imageSize.height() > 0 ? 1 : 0);
    imageSize.scale(widthScale, heightScale);
    imageSize.clampToMinimumSize(minimumSize);
    ASSERT(multiplier != 1.0f || (imageSize.width().fraction() == 0.0f && imageSize.height().fraction() == 0.0f));
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

    m_shouldPaintBrokenImage = m_loader->frameLoader()->client().shouldPaintBrokenImage(m_resourceRequest.url());
}

void CachedImage::clear()
{
    destroyDecodedData();
    clearImage();
    m_pendingContainerSizeRequests.clear();
    setEncodedSize(0);
}

inline void CachedImage::createImage()
{
    // Create the image if it doesn't yet exist.
    if (m_image)
        return;
#if USE(CG) && !USE(WEBKIT_IMAGE_DECODERS)
    else if (m_response.mimeType() == "application/pdf")
        m_image = PDFDocumentImage::create(this);
#endif
    else if (m_response.mimeType() == "image/svg+xml") {
        RefPtr<SVGImage> svgImage = SVGImage::create(this);
        m_svgImageCache = std::make_unique<SVGImageCache>(svgImage.get());
        m_image = svgImage.release();
    } else
        m_image = BitmapImage::create(this);

    if (m_image) {
        // Send queued container size requests.
        if (m_image->usesContainerSize()) {
            for (ContainerSizeRequests::iterator it = m_pendingContainerSizeRequests.begin(); it != m_pendingContainerSizeRequests.end(); ++it)
                setContainerSizeForRenderer(it->key, it->value.first, it->value.second);
        }
        m_pendingContainerSizeRequests.clear();
    }
}

inline void CachedImage::clearImage()
{
    // If our Image has an observer, it's always us so we need to clear the back pointer
    // before dropping our reference.
    if (m_image)
        m_image->setImageObserver(0);
    m_image.clear();
}

void CachedImage::addIncrementalDataBuffer(ResourceBuffer* data)
{
    m_data = data;
    if (!data)
        return;

    createImage();

    // Have the image update its data from its internal buffer.
    // It will not do anything now, but will delay decoding until
    // queried for info (like size or specific image frames).
    bool sizeAvailable = m_image->setData(m_data->sharedBuffer(), false);
    if (!sizeAvailable)
        return;

    if (m_image->isNull()) {
        // Image decoding failed. Either we need more image data or the image data is malformed.
        error(errorOccurred() ? status() : DecodeError);
        if (inCache())
            memoryCache()->remove(this);
        return;
    }

    // Go ahead and tell our observers to try to draw.
    // Each chunk from the network causes observers to repaint, which will
    // force that chunk to decode.
    // It would be nice to only redraw the decoded band of the image, but with the current design
    // (decoding delayed until painting) that seems hard.
    notifyObservers();

    setEncodedSize(m_image->data() ? m_image->data()->size() : 0);
}

void CachedImage::addDataBuffer(ResourceBuffer* data)
{
    ASSERT(m_options.dataBufferingPolicy == BufferData);
    addIncrementalDataBuffer(data);
}

void CachedImage::addData(const char* data, unsigned length)
{
    ASSERT(m_options.dataBufferingPolicy == DoNotBufferData);
    addIncrementalDataBuffer(ResourceBuffer::create(data, length).get());
}

void CachedImage::finishLoading(ResourceBuffer* data)
{
    m_data = data;
    if (!m_image && data)
        createImage();

    if (m_image)
        m_image->setData(m_data->sharedBuffer(), true);

    if (!m_image || m_image->isNull()) {
        // Image decoding failed; the image data is malformed.
        error(errorOccurred() ? status() : DecodeError);
        if (inCache())
            memoryCache()->remove(this);
        return;
    }

    notifyObservers();
    if (m_image)
        setEncodedSize(m_image->data() ? m_image->data()->size() : 0);
    CachedResource::finishLoading(data);
}

void CachedImage::error(CachedResource::Status status)
{
    checkShouldPaintBrokenImage();
    clear();
    CachedResource::error(status);
    notifyObservers();
}

void CachedImage::responseReceived(const ResourceResponse& response)
{
    if (!m_response.isNull())
        clear();
    CachedResource::responseReceived(response);
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
        timeStamp = monotonicallyIncreasingTime();
    
    CachedResource::didAccessDecodedData(timeStamp);
}

void CachedImage::animationAdvanced(const Image* image)
{
    if (!image || image != m_image)
        return;
    CachedResourceClientWalker<CachedImageClient> clientWalker(m_clients);
    while (CachedImageClient* client = clientWalker.next())
        client->newImageAnimationFrameAvailable(*this);
}

void CachedImage::changedInRect(const Image* image, const IntRect& rect)
{
    if (!image || image != m_image)
        return;
    notifyObservers(&rect);
}

bool CachedImage::currentFrameKnownToBeOpaque(const RenderElement* renderer)
{
    Image* image = imageForRenderer(renderer);
    if (image->isBitmapImage())
        image->nativeImageForCurrentFrame(); // force decode
    return image->currentFrameKnownToBeOpaque();
}

#if ENABLE(DISK_IMAGE_CACHE)
bool CachedImage::canUseDiskImageCache() const
{
    if (isLoading() || errorOccurred())
        return false;

    if (!m_data)
        return false;

    if (isPurgeable())
        return false;

    if (m_data->size() < diskImageCache().minimumImageSize())
        return false;

    // "Cache-Control: no-store" resources may be marked as such because they may
    // contain sensitive information. We should not write these resources to disk.
    if (m_response.cacheControlContainsNoStore())
        return false;

    // Testing shows that PDF images did not work when memory mapped.
    // However, SVG images and Bitmap images were fine. See:
    // <rdar://problem/8591834> Disk Image Cache should support PDF Images
    if (m_response.mimeType() == "application/pdf")
        return false;

    return true;
}

void CachedImage::useDiskImageCache()
{
    ASSERT(canUseDiskImageCache());
    ASSERT(!isUsingDiskImageCache());
    m_data->sharedBuffer()->allowToBeMemoryMapped();
}
#endif

bool CachedImage::isOriginClean(SecurityOrigin* securityOrigin)
{
    if (!image()->hasSingleSecurityOrigin())
        return false;
    if (passesAccessControlCheck(securityOrigin))
        return true;
    return !securityOrigin->taintsCanvas(response().url());
}

bool CachedImage::mustRevalidateDueToCacheHeaders(CachePolicy policy) const
{
    if (UNLIKELY(isManuallyCached())) {
        // Do not revalidate manually cached images. This mechanism is used as a
        // way to efficiently share an image from the client to content and
        // the URL for that image may not represent a resource that can be
        // retrieved by standard means. If the manual caching SPI is used, it is
        // incumbent on the client to only use valid resources.
        return false;
    }
    return CachedResource::mustRevalidateDueToCacheHeaders(policy);
}

} // namespace WebCore
