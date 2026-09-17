/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004-2025 Apple Inc. All rights reserved.

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

#include <WebCore/CachedResource.h>
#include <WebCore/FloatSize.h>
#include <WebCore/Image.h>
#include <WebCore/ImageObserver.h>
#include <WebCore/IntRect.h>
#include <WebCore/LayoutSize.h>
#include <WebCore/StyleLinkParameters.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebCore {

class CachedImageClient;
class CachedResourceLoader;
class WeakPtrImplWithEventTargetData;
class MemoryCache;
class NativeImage;
class SecurityOrigin;

class CachedImage final : public CachedResource {
    friend class MemoryCache;

public:
    CachedImage(CachedResourceRequest&&, PAL::SessionID, const CookieJar*);
    CachedImage(Image*, PAL::SessionID, const CookieJar*);
    virtual ~CachedImage();

    WEBCORE_EXPORT Image* image() const; // Returns the nullImage() if the image is not available yet.
    bool hasImage() const { return m_image.get(); }
    bool currentFrameKnownToBeOpaque();
    bool currentFrameIsComplete();

    std::pair<WeakPtr<BitmapImage>, float> brokenImage(float deviceScaleFactor) const; // Returns an image and the image's resolution scale factor.
    bool NODELETE willPaintBrokenImage() const;

    bool canRender(ImageOrientation, float multiplier);

    void setAllowsOrientationOverride(bool b) { m_allowsOrientationOverride = b; }
    bool allowsOrientationOverride() const { return m_allowsOrientationOverride; }


    void updateBuffer(const FragmentedSharedBuffer&) override;
    void finishLoading(const FragmentedSharedBuffer*, const NetworkLoadMetrics&) override;

    NaturalDimensions naturalDimensions(ImageOrientation = ImageOrientation::Orientation::FromImage) const;

    bool hasHDRContent() const;

    bool isManuallyCached() const { return m_isManuallyCached; }
    RevalidationDecision makeRevalidationDecision(CachePolicy) const override;
    void load(CachedResourceLoader&) override;

    bool NODELETE isOriginClean(SecurityOrigin*);

    bool NODELETE isClientWaitingForAsyncDecoding(const CachedImageClient&) const;
    void addClientWaitingForAsyncDecoding(CachedImageClient&);
    void removeAllClientsWaitingForAsyncDecoding();

    bool hasRendererClients() const;

    void setForceUpdateImageDataEnabledForTesting(bool enabled) { m_forceUpdateImageDataEnabledForTesting =  enabled; }

    bool stillNeedsLoad() const override { return !errorOccurred() && status() == Unknown && !isLoading(); }
    bool canSkipRevalidation(const CachedResourceLoader&, const CachedResourceRequest&) const;

    bool isVisibleInViewport(const Document&) const;
    bool allowsAnimation(const Image&) const;

private:

    void clear();

    void setBodyDataFrom(const CachedResource&) final;

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
    RefPtr<SharedBuffer> convertedDataIfNeeded(const FragmentedSharedBuffer* data) const;
    void didUpdateImageData();
    EncodedDataStatus updateImageData(bool allDataReceived);
    void updateData(const SharedBuffer&) override;
    void error(CachedResource::Status) override;
    void responseReceived(ResourceResponse&&) override;

    // For compatibility, images keep loading even if there are HTTP errors.
    bool shouldIgnoreHTTPStatusCodeErrors() const override { return true; }

    class CachedImageObserver final : public ImageObserver {
    public:
        static Ref<CachedImageObserver> create(CachedImage& image) { return adoptRef(*new CachedImageObserver(image)); }
        WeakHashSet<CachedImage>& cachedImages() LIFETIME_BOUND { return m_cachedImages; }
        const WeakHashSet<CachedImage>& cachedImages() const LIFETIME_BOUND { return m_cachedImages; }

    private:
        explicit CachedImageObserver(CachedImage&);

        // ImageObserver API
        URL sourceUrl() const override { return !m_cachedImages.isEmptyIgnoringNullReferences() ? (*m_cachedImages.begin()).url() : URL(); }
        String mimeType() const override { return !m_cachedImages.isEmptyIgnoringNullReferences() ? (*m_cachedImages.begin()).mimeType() : emptyString(); }
        unsigned numberOfClients() const override { return !m_cachedImages.isEmptyIgnoringNullReferences() ? (*m_cachedImages.begin()).numberOfClients() : 0; }
        long long expectedContentLength() const override { return !m_cachedImages.isEmptyIgnoringNullReferences() ? (*m_cachedImages.begin()).expectedContentLength() : 0; }

        void encodedDataStatusChanged(const Image&, EncodedDataStatus) final;
        void decodedSizeChanged(const Image&, long long delta) final;
        void didDraw(const Image&) final;

        bool canDestroyDecodedData(const Image&) const final;
        void imageFrameAvailable(const Image&, ImageAnimatingState, const IntRect* changeRect = nullptr, DecodingStatus = DecodingStatus::Invalid) final;
        void changedInRect(const Image&, const IntRect*) final;
        void imageContentChanged(const Image&) final;
        void scheduleRenderingUpdate(const Image&) final;

        bool allowsAnimation(const Image&) const final;
        const Settings* settings() final { return !m_cachedImages.isEmptyIgnoringNullReferences() ? (*m_cachedImages.begin()).m_settings.get() : nullptr; }
        bool useSystemDarkAppearance() const final { return !m_cachedImages.isEmptyIgnoringNullReferences() && m_cachedImages.begin()->useSystemDarkAppearance(); }

        WeakHashSet<CachedImage> m_cachedImages;
    };

    void encodedDataStatusChanged(const Image&, EncodedDataStatus);
    void decodedSizeChanged(const Image&, long long delta);
    void didDraw(const Image&);
    bool canDestroyDecodedData(const Image&) const;
    void imageFrameAvailable(const Image&, ImageAnimatingState, const IntRect* changeRect = nullptr, DecodingStatus = DecodingStatus::Invalid);
    void changedInRect(const Image&, const IntRect*);
    void imageContentChanged(const Image&);
    void scheduleRenderingUpdate(const Image&);
    bool useSystemDarkAppearance() const;

    void updateBufferInternal(const FragmentedSharedBuffer&);

    void didReplaceSharedBufferContents() override;

    SingleThreadWeakHashSet<CachedImageClient> m_clientsWaitingForAsyncDecoding;

    RefPtr<CachedImageObserver> m_imageObserver;
    RefPtr<Image> m_image;

#if ENABLE(AX_CUSTOM_COLOR_MODE)
    std::optional<bool> m_axCustomColorModeShouldAdjust;
    RefPtr<NativeImage> m_axCustomColorModeAdjustedTile;
    FloatSize m_axCustomColorModeAdjustedTileSize;
#endif

    MonotonicTime m_lastUpdateImageDataTime;

    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_skippingRevalidationDocument;
    RefPtr<const Settings> m_settings;

    static constexpr unsigned maxUpdateImageDataCount = 4;
    unsigned m_updateImageDataCount : 3;
    bool m_isManuallyCached : 1;
    bool m_shouldPaintBrokenImage : 1;
    bool m_forceUpdateImageDataEnabledForTesting : 1;
    bool m_allowsOrientationOverride : 1;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CACHED_RESOURCE(CachedImage, CachedResource::Type::ImageResource)
