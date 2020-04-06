/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/Color.h>
#include <WebCore/IntPoint.h>
#include <wtf/ListHashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

#if HAVE(IOSURFACE)
#include <WebCore/IOSurface.h>
#endif

#if PLATFORM(GTK)
#include <WebCore/RefPtrCairo.h>
#endif

namespace WebKit {

class WebBackForwardListItem;
class WebPageProxy;

class ViewSnapshot : public RefCounted<ViewSnapshot> {
public:
#if HAVE(IOSURFACE)
    static Ref<ViewSnapshot> create(std::unique_ptr<WebCore::IOSurface>);
#endif
#if PLATFORM(GTK)
    static Ref<ViewSnapshot> create(RefPtr<cairo_surface_t>&&);
#endif

    ~ViewSnapshot();

    void clearImage();
    bool hasImage() const;

#if HAVE(IOSURFACE)
    id asLayerContents();
    RetainPtr<CGImageRef> asImageForTesting();
#endif

    void setRenderTreeSize(uint64_t renderTreeSize) { m_renderTreeSize = renderTreeSize; }
    uint64_t renderTreeSize() const { return m_renderTreeSize; }

    void setBackgroundColor(const WebCore::Color& color) { m_backgroundColor = color; }
    WebCore::Color backgroundColor() const { return m_backgroundColor; }

    void setViewScrollPosition(WebCore::IntPoint scrollPosition) { m_viewScrollPosition = scrollPosition; }
    WebCore::IntPoint viewScrollPosition() const { return m_viewScrollPosition; }

    void setDeviceScaleFactor(float deviceScaleFactor) { m_deviceScaleFactor = deviceScaleFactor; }
    float deviceScaleFactor() const { return m_deviceScaleFactor; }

#if HAVE(IOSURFACE)
    WebCore::IOSurface* surface() const { return m_surface.get(); }

    size_t imageSizeInBytes() const { return m_surface ? m_surface->totalBytes() : 0; }
    WebCore::IntSize size() const { return m_surface ? m_surface->size() : WebCore::IntSize(); }

    void setSurface(std::unique_ptr<WebCore::IOSurface>);

    WebCore::IOSurface::SurfaceState setVolatile(bool);
#endif

#if PLATFORM(GTK)
    cairo_surface_t* surface() const { return m_surface.get(); }

    size_t imageSizeInBytes() const;
    WebCore::IntSize size() const;
#endif

private:
#if HAVE(IOSURFACE)
    explicit ViewSnapshot(std::unique_ptr<WebCore::IOSurface>);

    std::unique_ptr<WebCore::IOSurface> m_surface;
#endif

#if PLATFORM(GTK)
    explicit ViewSnapshot(RefPtr<cairo_surface_t>&&);

    RefPtr<cairo_surface_t> m_surface;
#endif

    uint64_t m_renderTreeSize;
    float m_deviceScaleFactor;
    WebCore::Color m_backgroundColor;
    WebCore::IntPoint m_viewScrollPosition; // Scroll position at snapshot time. Integral to make comparison reliable.
};

class ViewSnapshotStore {
    WTF_MAKE_NONCOPYABLE(ViewSnapshotStore);
    friend class ViewSnapshot;
public:
    ViewSnapshotStore();
    ~ViewSnapshotStore();

    static ViewSnapshotStore& singleton();

    void recordSnapshot(WebPageProxy&, WebBackForwardListItem&);

    void discardSnapshotImages();

    void setDisableSnapshotVolatilityForTesting(bool disable) { m_disableSnapshotVolatility = disable; }
    bool disableSnapshotVolatilityForTesting() const { return m_disableSnapshotVolatility; }

private:
    void didAddImageToSnapshot(ViewSnapshot&);
    void willRemoveImageFromSnapshot(ViewSnapshot&);
    void pruneSnapshots(WebPageProxy&);

    size_t m_snapshotCacheSize { 0 };

    ListHashSet<ViewSnapshot*> m_snapshotsWithImages;
    bool m_disableSnapshotVolatility { false };
};

} // namespace WebKit
