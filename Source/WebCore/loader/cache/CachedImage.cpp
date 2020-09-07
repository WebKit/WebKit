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
#include "MIMETypeRegistry.h"
#include "MemoryCache.h"
#include "RenderElement.h"
#include "SVGImage.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SharedBuffer.h"
#include "SubresourceLoader.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

#if PLATFORM(IOS_FAMILY)
#include "SystemMemory.h"
#endif

#if USE(CG)
#include "PDFDocumentImage.h"
#endif

namespace WebCore {

CachedImage::CachedImage(CachedResourceRequest&& request, const PAL::SessionID& sessionID, const CookieJar* cookieJar)
    : CachedResource(WTFMove(request), Type::ImageResource, sessionID, cookieJar)
    , m_updateImageDataCount(0)
    , m_isManuallyCached(false)
    , m_shouldPaintBrokenImage(true)
    , m_forceUpdateImageDataEnabledForTesting(false)
{
    setStatus(Unknown);
}

CachedImage::CachedImage(Image* image, const PAL::SessionID& sessionID, const CookieJar* cookieJar)
    : CachedResource(URL(), Type::ImageResource, sessionID, cookieJar)
    , m_image(image)
    , m_updateImageDataCount(0)
    , m_isManuallyCached(false)
    , m_shouldPaintBrokenImage(true)
    , m_forceUpdateImageDataEnabledForTesting(false)
{
}

CachedImage::CachedImage(const URL& url, Image* image, const PAL::SessionID& sessionID, const CookieJar* cookieJar, const String& domainForCachePartition)
    : CachedResource(url, Type::ImageResource, sessionID, cookieJar)
    , m_image(image)
    , m_updateImageDataCount(0)
    , m_isManuallyCached(true)
    , m_shouldPaintBrokenImage(true)
    , m_forceUpdateImageDataEnabledForTesting(false)
{
    m_resourceRequest.setDomainForCachePartition(domainForCachePartition);

    // Use the incoming URL in the response field. This ensures that code using the response directly,
    // such as origin checks for security, actually see something.
    m_response.setURL(url);
}

CachedImage::~CachedImage()
{
    clearImage();
}

void CachedImage::load(CachedResourceLoader& loader)
{
    m_skippingRevalidationDocument = makeWeakPtr(loader.document());
    if (loader.shouldPerformImageLoad(url()))
        CachedResource::load(loader);
    else
        setLoading(false);
}

void CachedImage::setBodyDataFrom(const CachedResource& resource)
{
    ASSERT(resource.type() == type());
    const CachedImage& image = static_cast<const CachedImage&>(resource);

    CachedResource::setBodyDataFrom(resource);

    m_image = image.m_image;
    m_imageObserver = image.m_imageObserver;
    if (m_imageObserver)
        m_imageObserver->cachedImages().add(this);

    if (m_image && is<SVGImage>(*m_image))
        m_svgImageCache = makeUnique<SVGImageCache>(&downcast<SVGImage>(*m_image));
}

void CachedImage::didAddClient(CachedResourceClient& client)
{
    if (m_data && !m_image && !errorOccurred()) {
        createImage();
        m_image->setData(m_data.copyRef(), true);
    }

    ASSERT(client.resourceClientType() == CachedImageClient::expectedType());
    if (m_image && !m_image->isNull())
        static_cast<CachedImageClient&>(client).imageChanged(this);

    if (m_image)
        m_image->startAnimationAsynchronously();

    CachedResource::didAddClient(client);
}

void CachedImage::didRemoveClient(CachedResourceClient& client)
{
    ASSERT(client.resourceClientType() == CachedImageClient::expectedType());

    m_pendingContainerContextRequests.remove(&static_cast<CachedImageClient&>(client));
    m_clientsWaitingForAsyncDecoding.remove(&static_cast<CachedImageClient&>(client));

    if (m_svgImageCache)
        m_svgImageCache->removeClientFromCache(&static_cast<CachedImageClient&>(client));

    CachedResource::didRemoveClient(client);

    static_cast<CachedImageClient&>(client).didRemoveCachedImageClient(*this);
}

bool CachedImage::isClientWaitingForAsyncDecoding(CachedImageClient& client) const
{
    return m_clientsWaitingForAsyncDecoding.contains(&client);
}

void CachedImage::addClientWaitingForAsyncDecoding(CachedImageClient& client)
{
    ASSERT(client.resourceClientType() == CachedImageClient::expectedType());
    if (m_clientsWaitingForAsyncDecoding.contains(&client))
        return;
    if (!m_clients.contains(&client)) {
        // If the <html> element does not have its own background specified, painting the root box
        // renderer uses the style of the <body> element, see RenderView::rendererForRootBackground().
        // In this case, the client we are asked to add is the root box renderer. Since we can't add
        // a client to m_clientsWaitingForAsyncDecoding unless it is one of the m_clients, we are going
        // to cancel the repaint optimization we do in CachedImage::imageFrameAvailable() by adding
        // all the m_clients to m_clientsWaitingForAsyncDecoding.
        CachedResourceClientWalker<CachedImageClient> walker(m_clients);
        while (auto* client = walker.next())
            m_clientsWaitingForAsyncDecoding.add(client);
    } else
        m_clientsWaitingForAsyncDecoding.add(&client);
}
    
void CachedImage::removeAllClientsWaitingForAsyncDecoding()
{
    if (m_clientsWaitingForAsyncDecoding.isEmpty() || !hasImage() || !is<BitmapImage>(image()))
        return;
    downcast<BitmapImage>(image())->stopAsyncDecodingQueue();
    for (auto* client : m_clientsWaitingForAsyncDecoding)
        client->imageChanged(this);
    m_clientsWaitingForAsyncDecoding.clear();
}

void CachedImage::switchClientsToRevalidatedResource()
{
    ASSERT(is<CachedImage>(resourceToRevalidate()));
    // Pending container size requests need to be transferred to the revalidated resource.
    if (!m_pendingContainerContextRequests.isEmpty()) {
        // A copy of pending size requests is needed as they are deleted during CachedResource::switchClientsToRevalidateResouce().
        ContainerContextRequests switchContainerContextRequests;
        for (auto& request : m_pendingContainerContextRequests)
            switchContainerContextRequests.set(request.key, request.value);
        CachedResource::switchClientsToRevalidatedResource();
        CachedImage& revalidatedCachedImage = downcast<CachedImage>(*resourceToRevalidate());
        for (auto& request : switchContainerContextRequests)
            revalidatedCachedImage.setContainerContextForClient(*request.key, request.value.containerSize, request.value.containerZoom, request.value.imageURL);
        return;
    }

    CachedResource::switchClientsToRevalidatedResource();
}

void CachedImage::allClientsRemoved()
{
    m_pendingContainerContextRequests.clear();
    m_clientsWaitingForAsyncDecoding.clear();
    if (m_image && !errorOccurred())
        m_image->resetAnimation();
}

std::pair<Image*, float> CachedImage::brokenImage(float deviceScaleFactor) const
{
    if (deviceScaleFactor >= 3) {
        static NeverDestroyed<Image*> brokenImageVeryHiRes(&Image::loadPlatformResource("missingImage@3x").leakRef());
        return std::make_pair(brokenImageVeryHiRes, 3);
    }

    if (deviceScaleFactor >= 2) {
        static NeverDestroyed<Image*> brokenImageHiRes(&Image::loadPlatformResource("missingImage@2x").leakRef());
        return std::make_pair(brokenImageHiRes, 2);
    }

    static NeverDestroyed<Image*> brokenImageLoRes(&Image::loadPlatformResource("missingImage").leakRef());
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

    return &Image::nullImage();
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
        return &Image::nullImage();

    if (m_image->isSVGImage()) {
        Image* image = m_svgImageCache->imageForRenderer(renderer);
        if (image != &Image::nullImage())
            return image;
    }
    return m_image.get();
}

bool CachedImage::hasSVGImage() const
{
    return m_image && m_image->isSVGImage();
}

void CachedImage::setContainerContextForClient(const CachedImageClient& client, const LayoutSize& containerSize, float containerZoom, const URL& imageURL)
{
    if (containerSize.isEmpty())
        return;
    ASSERT(containerZoom);
    if (!m_image) {
        m_pendingContainerContextRequests.set(&client, ContainerContext { containerSize, containerZoom, imageURL });
        return;
    }

    if (!m_image->isSVGImage()) {
        m_image->setContainerSize(containerSize);
        return;
    }

    m_svgImageCache->setContainerContextForClient(client, containerSize, containerZoom, imageURL);
}

FloatSize CachedImage::imageSizeForRenderer(const RenderElement* renderer, SizeType sizeType) const
{
    if (!m_image)
        return { };

    if (is<SVGImage>(*m_image) && sizeType == UsedSize)
        return m_svgImageCache->imageSizeForRenderer(renderer);

    return m_image->size(renderer ? renderer->imageOrientation() : ImageOrientation(ImageOrientation::FromImage));
}


LayoutSize CachedImage::unclampedImageSizeForRenderer(const RenderElement* renderer, float multiplier, SizeType sizeType) const
{
    LayoutSize imageSize = LayoutSize(imageSizeForRenderer(renderer, sizeType));
    if (imageSize.isEmpty() || multiplier == 1.0f)
        return imageSize;

    float widthScale = m_image->hasRelativeWidth() ? 1.0f : multiplier;
    float heightScale = m_image->hasRelativeHeight() ? 1.0f : multiplier;
    imageSize.scale(widthScale, heightScale);
    return imageSize;    
}

LayoutSize CachedImage::imageSizeForRenderer(const RenderElement* renderer, float multiplier, SizeType sizeType) const
{
    auto imageSize = unclampedImageSizeForRenderer(renderer, multiplier, sizeType);
    if (imageSize.isEmpty() || multiplier == 1.0f)
        return imageSize;

    // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
    LayoutSize minimumSize(imageSize.width() > 0 ? 1 : 0, imageSize.height() > 0 ? 1 : 0);
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

bool CachedImage::isPDFResource() const
{
    return Image::isPDFResource(m_response.mimeType(), url());
}

bool CachedImage::isPostScriptResource() const
{
    return Image::isPostScriptResource(m_response.mimeType(), url());
}

void CachedImage::clear()
{
    destroyDecodedData();
    clearImage();
    m_pendingContainerContextRequests.clear();
    m_clientsWaitingForAsyncDecoding.clear();
    setEncodedSize(0);
}

inline void CachedImage::createImage()
{
    // Create the image if it doesn't yet exist.
    if (m_image)
        return;

    m_imageObserver = CachedImageObserver::create(*this);

    m_image = Image::create(*m_imageObserver);

    if (m_image) {
        if (is<SVGImage>(*m_image))
            m_svgImageCache = makeUnique<SVGImageCache>(&downcast<SVGImage>(*m_image));

        // Send queued container size requests.
        if (m_image->usesContainerSize()) {
            for (auto& request : m_pendingContainerContextRequests)
                setContainerContextForClient(*request.key, request.value.containerSize, request.value.containerZoom, request.value.imageURL);
        }
        m_pendingContainerContextRequests.clear();
        m_clientsWaitingForAsyncDecoding.clear();
    }
}

CachedImage::CachedImageObserver::CachedImageObserver(CachedImage& image)
{
    m_cachedImages.add(&image);
}

void CachedImage::CachedImageObserver::encodedDataStatusChanged(const Image& image, EncodedDataStatus status)
{
    for (auto cachedImage : m_cachedImages)
        cachedImage->encodedDataStatusChanged(image, status);
}

void CachedImage::CachedImageObserver::decodedSizeChanged(const Image& image, long long delta)
{
    for (auto cachedImage : m_cachedImages)
        cachedImage->decodedSizeChanged(image, delta);
}

void CachedImage::CachedImageObserver::didDraw(const Image& image)
{
    for (auto cachedImage : m_cachedImages)
        cachedImage->didDraw(image);
}

bool CachedImage::CachedImageObserver::canDestroyDecodedData(const Image& image)
{
    for (auto cachedImage : m_cachedImages) {
        if (&image != cachedImage->image())
            continue;
        if (!cachedImage->canDestroyDecodedData(image))
            return false;
    }
    return true;
}

void CachedImage::CachedImageObserver::imageFrameAvailable(const Image& image, ImageAnimatingState animatingState, const IntRect* changeRect, DecodingStatus decodingStatus)
{
    for (auto cachedImage : m_cachedImages)
        cachedImage->imageFrameAvailable(image, animatingState, changeRect, decodingStatus);
}

void CachedImage::CachedImageObserver::changedInRect(const Image& image, const IntRect* rect)
{
    for (auto cachedImage : m_cachedImages)
        cachedImage->changedInRect(image, rect);
}

void CachedImage::CachedImageObserver::scheduleTimedRenderingUpdate(const Image& image)
{
    for (auto cachedImage : m_cachedImages)
        cachedImage->scheduleTimedRenderingUpdate(image);
}

inline void CachedImage::clearImage()
{
    if (!m_image)
        return;

    if (m_imageObserver) {
        m_imageObserver->cachedImages().remove(this);

        if (m_imageObserver->cachedImages().isEmpty()) {
            ASSERT(m_imageObserver->hasOneRef());
            m_image->setImageObserver(nullptr);
        }

        m_imageObserver = nullptr;
    }

    m_image = nullptr;
    m_lastUpdateImageDataTime = { };
    m_updateImageDataCount = 0;
}

void CachedImage::updateBufferInternal(SharedBuffer& data)
{
    m_data = &data;
    setEncodedSize(m_data->size());
    createImage();

    // Don't update the image with the new buffer very often. Changing the decoder
    // internal data and repainting the observers sometimes are very expensive operations.
    if (!m_forceUpdateImageDataEnabledForTesting && shouldDeferUpdateImageData())
        return;

    EncodedDataStatus encodedDataStatus = EncodedDataStatus::Unknown;

    if (isPostScriptResource()) {
#if PLATFORM(MAC) && !USE(WEBKIT_IMAGE_DECODERS)
        // Delay updating the image with the PostScript data till all the data
        // is received so it can be converted to PDF data.
        return;
#else
        // Set the encodedDataStatus to Error so loading this image will be canceled.
        encodedDataStatus = EncodedDataStatus::Error;
#endif
    } else {
        // Have the image update its data from its internal buffer. Decoding the image data
        // will be delayed until info (like size or specific image frames) are queried which
        // usually happens when the observers are repainted.
        encodedDataStatus = updateImageData(false);
    }

    if (encodedDataStatus > EncodedDataStatus::Error && encodedDataStatus < EncodedDataStatus::SizeAvailable)
        return;

    if (encodedDataStatus == EncodedDataStatus::Error || m_image->isNull()) {
        // Image decoding failed. Either we need more image data or the image data is malformed.
        error(errorOccurred() ? status() : DecodeError);
        if (m_loader && encodedDataStatus == EncodedDataStatus::Error)
            m_loader->cancel();
        if (inCache())
            MemoryCache::singleton().remove(*this);
        return;
    }

    // Tell our observers to try to draw.
    notifyObservers();
}

bool CachedImage::shouldDeferUpdateImageData() const
{
    static const double updateImageDataBackoffIntervals[] = { 0, 1, 3, 6, 15 };
    unsigned interval = m_updateImageDataCount;

    // The first time through, the chunk time will be 0 and the image will get an update.
    return (MonotonicTime::now() - m_lastUpdateImageDataTime).seconds() < updateImageDataBackoffIntervals[interval];
}

RefPtr<SharedBuffer> CachedImage::convertedDataIfNeeded(SharedBuffer* data) const
{
    if (!data || !isPostScriptResource())
        return data;
#if PLATFORM(MAC) && !USE(WEBKIT_IMAGE_DECODERS)
    return SharedBuffer::create(PDFDocumentImage::convertPostScriptDataToPDF(data->createCFData()).get());
#else
    // Loading the image should have been canceled if the system does not support converting PostScript to PDF.
    ASSERT_NOT_REACHED();
    return nullptr;
#endif
}

void CachedImage::didUpdateImageData()
{
    m_lastUpdateImageDataTime = MonotonicTime::now();
    unsigned previous = m_updateImageDataCount;
    if (previous != maxUpdateImageDataCount)
        m_updateImageDataCount += 1;
}

EncodedDataStatus CachedImage::updateImageData(bool allDataReceived)
{
    if (!m_image || !m_data)
        return EncodedDataStatus::Error;
    EncodedDataStatus result = m_image->setData(m_data.get(), allDataReceived);
    didUpdateImageData();
    return result;
}

void CachedImage::updateBuffer(SharedBuffer& data)
{
    ASSERT(dataBufferingPolicy() == DataBufferingPolicy::BufferData);
    updateBufferInternal(data);
    CachedResource::updateBuffer(data);
}

void CachedImage::updateData(const char* data, unsigned length)
{
    ASSERT(dataBufferingPolicy() == DataBufferingPolicy::DoNotBufferData);
    updateBufferInternal(SharedBuffer::create(data, length));
    CachedResource::updateData(data, length);
}

void CachedImage::finishLoading(SharedBuffer* data, const NetworkLoadMetrics& metrics)
{
    m_data = convertedDataIfNeeded(data);
    if (m_data) {
        setEncodedSize(m_data->size());
        createImage();
    }

    EncodedDataStatus encodedDataStatus = updateImageData(true);

    if (encodedDataStatus == EncodedDataStatus::Error || m_image->isNull()) {
        // Image decoding failed; the image data is malformed.
        error(errorOccurred() ? status() : DecodeError);
        if (inCache())
            MemoryCache::singleton().remove(*this);
        return;
    }

    notifyObservers();
    CachedResource::finishLoading(data, metrics);
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

void CachedImage::encodedDataStatusChanged(const Image& image, EncodedDataStatus)
{
    if (&image != m_image)
        return;

    notifyObservers();
}

void CachedImage::decodedSizeChanged(const Image& image, long long delta)
{
    if (&image != m_image)
        return;

    ASSERT(delta >= 0 || decodedSize() + delta >= 0);
    setDecodedSize(static_cast<unsigned>(decodedSize() + delta));
}

void CachedImage::didDraw(const Image& image)
{
    if (&image != m_image)
        return;
    
    MonotonicTime timeStamp = FrameView::currentPaintTimeStamp();
    if (!timeStamp) // If didDraw is called outside of a Frame paint.
        timeStamp = MonotonicTime::now();
    
    CachedResource::didAccessDecodedData(timeStamp);
}

bool CachedImage::canDestroyDecodedData(const Image& image)
{
    if (&image != m_image)
        return false;

    CachedResourceClientWalker<CachedImageClient> clientWalker(m_clients);
    while (CachedImageClient* client = clientWalker.next()) {
        if (!client->canDestroyDecodedData())
            return false;
    }

    return true;
}

void CachedImage::imageFrameAvailable(const Image& image, ImageAnimatingState animatingState, const IntRect* changeRect, DecodingStatus decodingStatus)
{
    if (&image != m_image)
        return;

    CachedResourceClientWalker<CachedImageClient> clientWalker(m_clients);
    VisibleInViewportState visibleState = VisibleInViewportState::No;

    while (CachedImageClient* client = clientWalker.next()) {
        // All the clients of animated images have to be notified. The new frame has to be drawn in all of them.
        if (animatingState == ImageAnimatingState::No && !m_clientsWaitingForAsyncDecoding.contains(client))
            continue;
        if (client->imageFrameAvailable(*this, animatingState, changeRect) == VisibleInViewportState::Yes)
            visibleState = VisibleInViewportState::Yes;
    }

    if (visibleState == VisibleInViewportState::No && animatingState == ImageAnimatingState::Yes)
        m_image->stopAnimation();

    if (decodingStatus != DecodingStatus::Partial)
        m_clientsWaitingForAsyncDecoding.clear();
}

void CachedImage::changedInRect(const Image& image, const IntRect* rect)
{
    if (&image != m_image)
        return;
    notifyObservers(rect);
}

void CachedImage::scheduleTimedRenderingUpdate(const Image& image)
{
    if (&image != m_image)
        return;

    CachedResourceClientWalker<CachedImageClient> walker(m_clients);
    while (auto* client = walker.next())
        client->scheduleTimedRenderingUpdate();
}

bool CachedImage::currentFrameKnownToBeOpaque(const RenderElement* renderer)
{
    Image* image = imageForRenderer(renderer);
    return image->currentFrameKnownToBeOpaque();
}

bool CachedImage::isOriginClean(SecurityOrigin* origin)
{
    ASSERT_UNUSED(origin, origin);
    ASSERT(this->origin());
    ASSERT(origin->toString() == this->origin()->toString());
    return !loadFailedOrCanceled() && isCORSSameOrigin();
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

bool CachedImage::canSkipRevalidation(const CachedResourceLoader& loader, const CachedResourceRequest& request) const
{
    if (options().mode != request.options().mode || options().credentials != request.options().credentials || resourceRequest().allowCookies() != request.resourceRequest().allowCookies())
        return false;

    // Skip revalidation as per https://html.spec.whatwg.org/#ignore-higher-layer-caching which defines a per-document image list.
    // This rule is loosely implemented by other browsers, we could relax it and should update it once memory cache is properly specified.
    return m_skippingRevalidationDocument && loader.document() == m_skippingRevalidationDocument;
}

} // namespace WebCore
