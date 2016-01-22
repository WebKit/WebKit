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
#include "SVGImage.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SharedBuffer.h"
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

namespace WebCore {

CachedImage::CachedImage(const ResourceRequest& resourceRequest, SessionID sessionID)
    : CachedResource(resourceRequest, ImageResource, sessionID)
    , m_image(nullptr)
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

void CachedImage::load(CachedResourceLoader& cachedResourceLoader, const ResourceLoaderOptions& options)
{
    if (cachedResourceLoader.shouldPerformImageLoad(url()))
        CachedResource::load(cachedResourceLoader, options);
    else
        setLoading(false);
}

void CachedImage::didAddClient(CachedResourceClient* client)
{
    if (m_data && !m_image && !errorOccurred()) {
        createImage();
        m_image->setData(m_data, true);
    }
    
    ASSERT(client->resourceClientType() == CachedImageClient::expectedType());
    if (m_image && !m_image->isNull())
        static_cast<CachedImageClient*>(client)->imageChanged(this);

    CachedResource::didAddClient(client);
}

void CachedImage::didRemoveClient(CachedResourceClient* client)
{
    ASSERT(client);
    ASSERT(client->resourceClientType() == CachedImageClient::expectedType());

    m_pendingContainerSizeRequests.remove(static_cast<CachedImageClient*>(client));

    if (m_svgImageCache)
        m_svgImageCache->removeClientFromCache(static_cast<CachedImageClient*>(client));

    CachedResource::didRemoveClient(client);
}

void CachedImage::switchClientsToRevalidatedResource()
{
    ASSERT(is<CachedImage>(resourceToRevalidate()));
    // Pending container size requests need to be transferred to the revalidated resource.
    if (!m_pendingContainerSizeRequests.isEmpty()) {
        // A copy of pending size requests is needed as they are deleted during CachedResource::switchClientsToRevalidateResouce().
        ContainerSizeRequests switchContainerSizeRequests;
        for (auto& request : m_pendingContainerSizeRequests)
            switchContainerSizeRequests.set(request.key, request.value);
        CachedResource::switchClientsToRevalidatedResource();
        CachedImage& revalidatedCachedImage = downcast<CachedImage>(*resourceToRevalidate());
        for (auto& request : switchContainerSizeRequests)
            revalidatedCachedImage.setContainerSizeForRenderer(request.key, request.value.first, request.value.second);
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
    if (deviceScaleFactor >= 3) {
        static NeverDestroyed<Image*> brokenImageVeryHiRes(Image::loadPlatformResource("missingImage@3x").leakRef());
        return std::make_pair(brokenImageVeryHiRes, 3);
    }

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
    if (!m_image)
        return LayoutSize();

    LayoutSize imageSize(m_image->size());

#if ENABLE(CSS_IMAGE_ORIENTATION)
    if (renderer && is<BitmapImage>(*m_image)) {
        ImageOrientationDescription orientationDescription(renderer->shouldRespectImageOrientation(), renderer->style().imageOrientation());
        if (orientationDescription.respectImageOrientation() == RespectImageOrientation)
            imageSize = LayoutSize(downcast<BitmapImage>(*m_image).sizeRespectingOrientation(orientationDescription));
    }
#else
    if (is<BitmapImage>(*m_image) && (renderer && renderer->shouldRespectImageOrientation() == RespectImageOrientation))
        imageSize = LayoutSize(downcast<BitmapImage>(*m_image).sizeRespectingOrientation());
#endif // ENABLE(CSS_IMAGE_ORIENTATION)

    else if (is<SVGImage>(*m_image) && sizeType == UsedSize) {
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

    m_shouldPaintBrokenImage = m_loader->frameLoader()->client().shouldPaintBrokenImage(url());
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
        RefPtr<SVGImage> svgImage = SVGImage::create(*this, url());
        m_svgImageCache = std::make_unique<SVGImageCache>(svgImage.get());
        m_image = svgImage.release();
    } else {
        m_image = BitmapImage::create(this);
        downcast<BitmapImage>(*m_image).setAllowSubsampling(m_loader && m_loader->frameLoader()->frame().settings().imageSubsamplingEnabled());
    }

    if (m_image) {
        // Send queued container size requests.
        if (m_image->usesContainerSize()) {
            for (auto& request : m_pendingContainerSizeRequests)
                setContainerSizeForRenderer(request.key, request.value.first, request.value.second);
        }
        m_pendingContainerSizeRequests.clear();
    }
}

inline void CachedImage::clearImage()
{
    // If our Image has an observer, it's always us so we need to clear the back pointer
    // before dropping our reference.
    if (m_image)
        m_image->setImageObserver(nullptr);
    m_image = nullptr;
}

void CachedImage::addIncrementalDataBuffer(SharedBuffer& data)
{
    m_data = &data;

    createImage();

    // Have the image update its data from its internal buffer.
    // It will not do anything now, but will delay decoding until
    // queried for info (like size or specific image frames).
    bool sizeAvailable = m_image->setData(&data, false);
    if (!sizeAvailable)
        return;

    if (m_image->isNull()) {
        // Image decoding failed. Either we need more image data or the image data is malformed.
        error(errorOccurred() ? status() : DecodeError);
        if (inCache())
            MemoryCache::singleton().remove(*this);
        return;
    }

    // Tell our observers to try to draw.
    // Each chunk from the network causes observers to repaint, which will force that chunk to decode.
    // It would be nice to only redraw the decoded band of the image, but with the current design
    // (decoding delayed until painting) that seems hard.
    notifyObservers();

    setEncodedSize(m_image->data() ? m_image->data()->size() : 0);
}

void CachedImage::addDataBuffer(SharedBuffer& data)
{
    ASSERT(dataBufferingPolicy() == BufferData);
    addIncrementalDataBuffer(data);
    CachedResource::addDataBuffer(data);
}

void CachedImage::addData(const char* data, unsigned length)
{
    ASSERT(dataBufferingPolicy() == DoNotBufferData);
    addIncrementalDataBuffer(*SharedBuffer::create(data, length));
    CachedResource::addData(data, length);
}

void CachedImage::finishLoading(SharedBuffer* data)
{
    m_data = data;
    if (!m_image && data)
        createImage();

    if (m_image)
        m_image->setData(data, true);

    if (!m_image || m_image->isNull()) {
        // Image decoding failed; the image data is malformed.
        error(errorOccurred() ? status() : DecodeError);
        if (inCache())
            MemoryCache::singleton().remove(*this);
        return;
    }

    notifyObservers();
    if (m_image)
        setEncodedSize(m_image->data() ? m_image->data()->size() : 0);
    CachedResource::finishLoading(data);
}

void CachedImage::didReplaceSharedBufferContents()
{
    if (m_image) {
        // Let the Image know that the SharedBuffer has been rejigged, so it can let go of any references to the heap-allocated resource buffer.
        // FIXME(rdar://problem/24275617): It would be better if we could somehow tell the Image's decoder to swap in the new contents without destroying anything.
        m_image->destroyDecodedData(true);
    }
    CachedResource::didReplaceSharedBufferContents();
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
    if (canDeleteImage && !isLoading() && !hasClients()) {
        m_image = nullptr;
        setDecodedSize(0);
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
    return image->currentFrameKnownToBeOpaque();
}

bool CachedImage::isOriginClean(SecurityOrigin* securityOrigin)
{
    if (!image()->hasSingleSecurityOrigin())
        return false;
    if (passesAccessControlCheck(*securityOrigin))
        return true;
    return !securityOrigin->taintsCanvas(responseForSameOriginPolicyChecks().url());
}

CachedResource::RevalidationDecision CachedImage::makeRevalidationDecision(CachePolicy cachePolicy) const
{
    if (UNLIKELY(isManuallyCached())) {
        // Do not revalidate manually cached images. This mechanism is used as a
        // way to efficiently share an image from the client to content and
        // the URL for that image may not represent a resource that can be
        // retrieved by standard means. If the manual caching SPI is used, it is
        // incumbent on the client to only use valid resources.
        return RevalidationDecision::No;
    }
    return CachedResource::makeRevalidationDecision(cachePolicy);
}

} // namespace WebCore
