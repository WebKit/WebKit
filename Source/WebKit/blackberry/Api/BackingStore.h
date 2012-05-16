/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef BackingStore_h
#define BackingStore_h

#include "BlackBerryGlobal.h"
#include <BlackBerryPlatformGraphics.h>

namespace WebCore {
class ChromeClientBlackBerry;
class FloatPoint;
class FrameLoaderClientBlackBerry;
class GLES2Context;
class IntRect;
}

namespace BlackBerry {
namespace Platform {
class IntRect;
}
}

namespace BlackBerry {
namespace WebKit {

class WebPage;
class WebPagePrivate;
class WebPageCompositorPrivate;
class BackingStorePrivate;
class BackingStoreClient;

class BLACKBERRY_EXPORT BackingStore {
public:
    enum ResumeUpdateOperation { None, Blit, RenderAndBlit };
    BackingStore(WebPage*, BackingStoreClient*);
    virtual ~BackingStore();

    void createSurface();

    void suspendScreenAndBackingStoreUpdates();
    void resumeScreenAndBackingStoreUpdates(ResumeUpdateOperation);

    bool isScrollingOrZooming() const;
    void setScrollingOrZooming(bool);

    void blitContents(const BlackBerry::Platform::IntRect& dstRect, const BlackBerry::Platform::IntRect& contents);
    void repaint(int x, int y, int width, int height, bool contentChanged, bool immediate);

    bool hasRenderJobs() const;
    void renderOnIdle();

    // In the defers blit mode, any blit requests will just return early, and
    // a blit job will be queued that is executed by calling blitOnIdle().
    bool defersBlit() const;
    void setDefersBlit(bool);

    bool hasBlitJobs() const;
    void blitOnIdle();

    bool isDirectRenderingToWindow() const;

    void createBackingStoreMemory();
    void releaseBackingStoreMemory();

    void drawContents(Platform::Graphics::Drawable*, const Platform::IntRect& /*contentsRect*/, const Platform::IntSize& /*destinationSize*/);

private:
    friend class BlackBerry::WebKit::BackingStoreClient;
    friend class BlackBerry::WebKit::WebPage;
    friend class BlackBerry::WebKit::WebPagePrivate; // FIXME: For now, we expose our internals to WebPagePrivate. See PR #120301.
    friend class BlackBerry::WebKit::WebPageCompositorPrivate;
    friend class WebCore::ChromeClientBlackBerry;
    friend class WebCore::FrameLoaderClientBlackBerry;
    friend class WebCore::GLES2Context;
    BackingStorePrivate *d;
};
}
}

#endif // BackingStore_h
