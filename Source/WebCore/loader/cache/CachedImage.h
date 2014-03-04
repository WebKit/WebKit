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

#ifndef CachedImage_h
#define CachedImage_h

#include "CachedResource.h"
#include "ImageObserver.h"
#include "IntRect.h"
#include "IntSizeHash.h"
#include "LayoutSize.h"
#include "SVGImageCache.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

class CachedImageClient;
class CachedResourceLoader;
class FloatSize;
class MemoryCache;
class RenderElement;
class RenderObject;
class SecurityOrigin;

struct Length;

class CachedImage final : public CachedResource, public ImageObserver {
    friend class MemoryCache;

public:
    enum CacheBehaviorType { AutomaticallyCached, ManuallyCached };

    CachedImage(const ResourceRequest&);
    CachedImage(Image*);
    CachedImage(const URL&, Image*, CacheBehaviorType = AutomaticallyCached);
    virtual ~CachedImage();

    Image* image(); // Returns the nullImage() if the image is not available yet.
    Image* imageForRenderer(const RenderObject*); // Returns the nullImage() if the image is not available yet.
    bool hasImage() const { return m_image.get(); }
    bool currentFrameKnownToBeOpaque(const RenderElement*); // Side effect: ensures decoded image is in cache, therefore should only be called when about to draw the image.

    std::pair<Image*, float> brokenImage(float deviceScaleFactor) const; // Returns an image and the image's resolution scale factor.
    bool willPaintBrokenImage() const; 

    bool canRender(const RenderObject* renderer, float multiplier) { return !errorOccurred() && !imageSizeForRenderer(renderer, multiplier).isEmpty(); }

    void setContainerSizeForRenderer(const CachedImageClient*, const IntSize&, float);
    bool usesImageContainerSize() const;
    bool imageHasRelativeWidth() const;
    bool imageHasRelativeHeight() const;

    virtual void addDataBuffer(ResourceBuffer*) override;
    virtual void finishLoading(ResourceBuffer*) override;

    enum SizeType {
        UsedSize,
        IntrinsicSize
    };
    // This method takes a zoom multiplier that can be used to increase the natural size of the image by the zoom.
    LayoutSize imageSizeForRenderer(const RenderObject*, float multiplier, SizeType = UsedSize); // returns the size of the complete image.
    void computeIntrinsicDimensions(Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio);

    bool isManuallyCached() const { return m_isManuallyCached; }
    virtual bool mustRevalidateDueToCacheHeaders(CachePolicy) const;

#if ENABLE(DISK_IMAGE_CACHE)
    virtual bool canUseDiskImageCache() const override;
    virtual void useDiskImageCache() override;
#endif

    bool isOriginClean(SecurityOrigin*);

private:
    virtual void load(CachedResourceLoader*, const ResourceLoaderOptions&) override;

    void clear();

    void createImage();
    void clearImage();
    bool canBeDrawn() const;
    // If not null, changeRect is the changed part of the image.
    void notifyObservers(const IntRect* changeRect = 0);
    virtual PurgePriority purgePriority() const override { return PurgeFirst; }
    void checkShouldPaintBrokenImage();

    virtual void switchClientsToRevalidatedResource() override;
    virtual bool mayTryReplaceEncodedData() const override { return true; }

    virtual void didAddClient(CachedResourceClient*) override;
    virtual void didRemoveClient(CachedResourceClient*) override;

    virtual void allClientsRemoved() override;
    virtual void destroyDecodedData() override;

    virtual void addData(const char* data, unsigned length) override;
    virtual void error(CachedResource::Status) override;
    virtual void responseReceived(const ResourceResponse&) override;

    // For compatibility, images keep loading even if there are HTTP errors.
    virtual bool shouldIgnoreHTTPStatusCodeErrors() const override { return true; }

    virtual bool stillNeedsLoad() const override { return !errorOccurred() && status() == Unknown && !isLoading(); }

    // ImageObserver
    virtual void decodedSizeChanged(const Image*, int delta) override;
    virtual void didDraw(const Image*) override;

    virtual void animationAdvanced(const Image*) override;
    virtual void changedInRect(const Image*, const IntRect&) override;

    void addIncrementalDataBuffer(ResourceBuffer*);

    typedef std::pair<IntSize, float> SizeAndZoom;
    typedef HashMap<const CachedImageClient*, SizeAndZoom> ContainerSizeRequests;
    ContainerSizeRequests m_pendingContainerSizeRequests;

    RefPtr<Image> m_image;
    std::unique_ptr<SVGImageCache> m_svgImageCache;
    unsigned char m_isManuallyCached : 1;
    unsigned char m_shouldPaintBrokenImage : 1;
};

CACHED_RESOURCE_TYPE_CASTS(CachedImage, CachedResource, CachedResource::ImageResource)

}

#endif
