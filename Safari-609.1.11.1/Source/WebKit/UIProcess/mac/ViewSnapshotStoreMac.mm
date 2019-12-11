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

#import "config.h"
#import "ViewSnapshotStore.h"

#import <CoreGraphics/CoreGraphics.h>
#import <WebCore/IOSurface.h>

#if PLATFORM(IOS_FAMILY)
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#endif

namespace WebKit {
using namespace WebCore;

#if !HAVE(IOSURFACE) && HAVE(CORE_ANIMATION_RENDER_SERVER)
CAContext *ViewSnapshotStore::snapshottingContext()
{
    static CAContext *context;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSDictionary *options = @{
            kCAContextDisplayName: @"WebKitSnapshotting",
            kCAContextIgnoresHitTest: @YES,
            kCAContextDisplayId : @20000
        };
        context = [[CAContext remoteContextWithOptions:options] retain];
    });

    return context;
}
#endif

#if HAVE(IOSURFACE)
Ref<ViewSnapshot> ViewSnapshot::create(std::unique_ptr<WebCore::IOSurface> surface)
{
    return adoptRef(*new ViewSnapshot(WTFMove(surface)));
}
#else
Ref<ViewSnapshot> ViewSnapshot::create(uint32_t slotID, IntSize size, size_t imageSizeInBytes)
{
    return adoptRef(*new ViewSnapshot(slotID, size, imageSizeInBytes));
}
#endif

#if HAVE(IOSURFACE)
ViewSnapshot::ViewSnapshot(std::unique_ptr<WebCore::IOSurface> surface)
    : m_surface(WTFMove(surface))
#else
ViewSnapshot::ViewSnapshot(uint32_t slotID, IntSize size, size_t imageSizeInBytes)
    : m_slotID(slotID)
    , m_imageSizeInBytes(imageSizeInBytes)
    , m_size(size)
#endif
{
    if (hasImage())
        ViewSnapshotStore::singleton().didAddImageToSnapshot(*this);
}

#if HAVE(IOSURFACE)
void ViewSnapshot::setSurface(std::unique_ptr<WebCore::IOSurface> surface)
{
    ASSERT(!m_surface);
    if (!surface) {
        clearImage();
        return;
    }

    m_surface = WTFMove(surface);
    ViewSnapshotStore::singleton().didAddImageToSnapshot(*this);
}
#endif

bool ViewSnapshot::hasImage() const
{
#if HAVE(IOSURFACE)
    return !!m_surface;
#else
    return m_slotID;
#endif
}

void ViewSnapshot::clearImage()
{
    if (!hasImage())
        return;

    ViewSnapshotStore::singleton().willRemoveImageFromSnapshot(*this);

#if HAVE(IOSURFACE)
    m_surface = nullptr;
#else
#if HAVE(CORE_ANIMATION_RENDER_SERVER)
    [ViewSnapshotStore::snapshottingContext() deleteSlot:m_slotID];
#endif
    m_slotID = 0;
    m_imageSizeInBytes = 0;
#endif
}

#if HAVE(IOSURFACE)
WebCore::IOSurface::SurfaceState ViewSnapshot::setVolatile(bool becomeVolatile)
{
    if (ViewSnapshotStore::singleton().disableSnapshotVolatilityForTesting())
        return WebCore::IOSurface::SurfaceState::Valid;

    if (!m_surface)
        return WebCore::IOSurface::SurfaceState::Empty;

    return m_surface->setIsVolatile(becomeVolatile);
}
#endif

id ViewSnapshot::asLayerContents()
{
#if HAVE(IOSURFACE)
    if (!m_surface)
        return nullptr;

    if (setVolatile(false) != WebCore::IOSurface::SurfaceState::Valid) {
        clearImage();
        return nullptr;
    }

    return m_surface->asLayerContents();
#else
    return [CAContext objectForSlot:m_slotID];
#endif
}

RetainPtr<CGImageRef> ViewSnapshot::asImageForTesting()
{
#if HAVE(IOSURFACE)
    if (!m_surface)
        return nullptr;

    ASSERT(ViewSnapshotStore::singleton().disableSnapshotVolatilityForTesting());
    return m_surface->createImage();
#else
    // FIXME: Implement this in the slot case.
    return nullptr;
#endif
}

} // namespace WebKit
