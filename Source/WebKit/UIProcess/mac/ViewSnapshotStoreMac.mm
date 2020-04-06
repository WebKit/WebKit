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

Ref<ViewSnapshot> ViewSnapshot::create(std::unique_ptr<WebCore::IOSurface> surface)
{
    return adoptRef(*new ViewSnapshot(WTFMove(surface)));
}

ViewSnapshot::ViewSnapshot(std::unique_ptr<WebCore::IOSurface> surface)
    : m_surface(WTFMove(surface))
{
    if (hasImage())
        ViewSnapshotStore::singleton().didAddImageToSnapshot(*this);
}

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

bool ViewSnapshot::hasImage() const
{
    return !!m_surface;
}

void ViewSnapshot::clearImage()
{
    if (!hasImage())
        return;

    ViewSnapshotStore::singleton().willRemoveImageFromSnapshot(*this);

    m_surface = nullptr;
}

WebCore::IOSurface::SurfaceState ViewSnapshot::setVolatile(bool becomeVolatile)
{
    if (ViewSnapshotStore::singleton().disableSnapshotVolatilityForTesting())
        return WebCore::IOSurface::SurfaceState::Valid;

    if (!m_surface)
        return WebCore::IOSurface::SurfaceState::Empty;

    return m_surface->setIsVolatile(becomeVolatile);
}

id ViewSnapshot::asLayerContents()
{
    if (!m_surface)
        return nullptr;

    if (setVolatile(false) != WebCore::IOSurface::SurfaceState::Valid) {
        clearImage();
        return nullptr;
    }

    return m_surface->asLayerContents();
}

RetainPtr<CGImageRef> ViewSnapshot::asImageForTesting()
{
    if (!m_surface)
        return nullptr;

    ASSERT(ViewSnapshotStore::singleton().disableSnapshotVolatilityForTesting());
    return m_surface->createImage();
}

} // namespace WebKit
