/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
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

#pragma once

#include "CachedResource.h"
#include "Image.h"
#include "ImageObserver.h"
#include "IntRect.h"
#include "IntSizeHash.h"
#include "LayoutSize.h"
#include "SVGImageCache.h"
#include <wtf/HashMap.h>

namespace WebCore {

class CachedImageClient;
class CachedResourceLoader;
class FloatSize;
class MemoryCache;
class RenderElement;
class RenderObject;
class SecurityOrigin;

struct Length;

class CachedImage final : public CachedResource {
    friend class MemoryCache;

public:
    CachedImage(CachedResourceRequest&&, const PAL::SessionID&, const CookieJar*);
    CachedImage(Image*, const PAL::SessionID&, const CookieJar*);
    // Constructor to use for manually cached images.
    CachedImage(const URL&, Image*, const PAL::SessionID&, const CookieJar*, const String& domainForCachePartition);
    virtual ~CachedImage();

    WEBCORE_EXPORT Image* image(); // Returns the nullImage() if the image is not available yet.
    WEBCORE_EXPORT Image* imageForRenderer(const RenderObject*); // Returns the nullImage() if the image is not available yet.
    bool hasImage() const { return m_image.get(); }
    bool currentFrameKnownToBeOpaque(const RenderElement*);

    std::pair<Image*, float> brokenImage(float deviceScaleFactor) const; // Returns an image and the image's resolution scale factor.
    bool willPaintBrokenImage() const;

    bool canRender(const RenderElement* renderer, float multiplier) { return !errorOccurred() && !imageSizeForRenderer(renderer, multiplier).isEmpty(); }

    void setContainerContextForClient(const CachedImageClient&, const LayoutSize&, float, const URL&);
    bool usesImageContainerSize() const { return m_image && m_image->usesContainerSize(); }
    bool imageHasRelativeWidth() const { return m_image && m_image->hasRelativeWidth(); }
    bool imageHasRelativeHeight() const { return m_image && m_image->hasRelativeHeight(); }

    void updateBuffer(SharedBuffer&) override;
    void finishLoading(SharedBuffer*) override;

    enum SizeType {
        UsedSize,
        IntrinsicSize
    };
    // This method takes a zoom multiplier that can be used to increase the natural size of the image by the zoom.
    LayoutSize imageSizeForRenderer(const RenderElement*, float multiplier, SizeType = UsedSize); // returns the size of the complete image.
    void computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio);

    bool isManuallyCached() const { return m_isManuallyCached; }
    RevalidationDecision makeRevalidationDecision(CachePolicy) const override;
    void load(CachedResourceLoader&) override;

    bool isOriginClean(SecurityOrigin*);

    bool isClientWaitingForAsyncDecoding(CachedImageClient&) const;
    void addClientWaitingForAsyncDecoding(CachedImageClient&);
    void removeAllClientsWaitingForAsyncDecoding();

    void setForceUpdateImageDataEnabledForTesting(bool enabled) { m_forceUpdateImageDataEnabledForTesting =  enabled; }
    
private:
    void clear();

    CachedImage(CachedImage&, const ResourceRequest&, PAL::SessionID);

    void setBodyDataFrom(const CachedResource&) final;

    bool isPDFResource() const;
    bool isPostScriptResource() const;

    void createImage();
    void clearImage();
    // If not null, changeRect is the changed part of the image.
    void notifyObservers(const IntRect* changeRect = nullptr);
    void checkShouldPaintBrokenImage();

    void switchClientsToRevalidatedResource() final;
    bool mayTryReplaceEncodedData() const final { return true; }

    void didAddClient(CachedResourceClient&) final;
    void didRemoveClient(CachedResourceClient&) final;

    void allClientsRemoved() override;
    void destroyDecodedData() override;

    bool shouldDeferUpdateImageData() const;
    RefPtr<SharedBuffer> convertedDataIfNeeded(SharedBuffer* data) const;
    void didUpdateImageData();
    EncodedDataStatus updateImageData(bool allDataReceived);
    void updateData(const char* data, unsigned length) override;
    void error(CachedResource::Status) override;
    void responseReceived(const ResourceResponse&) override;

    // For compatibility, images keep loading even if there are HTTP errors.
    bool shouldIgnoreHTTPStatusCodeErrors() const override { return true; }

    bool stillNeedsLoad() const override { return !errorOccurred() && status() == Unknown && !isLoading(); }

    class CachedImageObserver final : public RefCounted<CachedImageObserver>, public ImageObserver {
    public:
        static Ref<CachedImageObserver> create(CachedImage& image) { return adoptRef(*new CachedImageObserver(image)); }
        HashSet<CachedImage*>& cachedImages() { return m_cachedImages; }
        const HashSet<CachedImage*>& cachedImages() const { return m_cachedImages; }

    private:
        explicit CachedImageObserver(CachedImage&);

        // ImageObserver API
        URL sourceUrl() const override { return !m_cachedImages.isEmpty() ? (*m_cachedImages.begin())->url() : URL(); }
        String mimeType() const override { return !m_cachedImages.isEmpty() ? (*m_cachedImages.begin())->mimeType() : emptyString(); }
        long long expectedContentLength() const override { return !m_cachedImages.isEmpty() ? (*m_cachedImages.begin())->expectedContentLength() : 0; }

        void encodedDataStatusChanged(const Image&, EncodedDataStatus) final;
        void decodedSizeChanged(const Image&, long long delta) final;
        void didDraw(const Image&) final;

        bool canDestroyDecodedData(const Image&) final;
        void imageFrameAvailable(const Image&, ImageAnimatingState, const IntRect* changeRect = nullptr, DecodingStatus = DecodingStatus::Invalid) final;
        void changedInRect(const Image&, const IntRect*) final;

        HashSet<CachedImage*> m_cachedImages;
    };

    void encodedDataStatusChanged(const Image&, EncodedDataStatus);
    void decodedSizeChanged(const Image&, long long delta);
    void didDraw(const Image&);
    bool canDestroyDecodedData(const Image&);
    void imageFrameAvailable(const Image&, ImageAnimatingState, const IntRect* changeRect = nullptr, DecodingStatus = DecodingStatus::Invalid);
    void changedInRect(const Image&, const IntRect*);

    void updateBufferInternal(SharedBuffer&);

    void didReplaceSharedBufferContents() override;

    struct ContainerContext {
        LayoutSize containerSize;
        float containerZoom;
        URL imageURL;
    };

    using ContainerContextRequests = HashMap<const CachedImageClient*, ContainerContext>;
    ContainerContextRequests m_pendingContainerContextRequests;

    HashSet<CachedImageClient*> m_clientsWaitingForAsyncDecoding;

    RefPtr<CachedImageObserver> m_imageObserver;
    RefPtr<Image> m_image;
    std::unique_ptr<SVGImageCache> m_svgImageCache;

    MonotonicTime m_lastUpdateImageDataTime;

    unsigned m_updateImageDataCount { 0 };
    bool m_isManuallyCached { false };
    bool m_shouldPaintBrokenImage { true };
    bool m_forceUpdateImageDataEnabledForTesting { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CACHED_RESOURCE(CachedImage, CachedResource::Type::ImageResource)
