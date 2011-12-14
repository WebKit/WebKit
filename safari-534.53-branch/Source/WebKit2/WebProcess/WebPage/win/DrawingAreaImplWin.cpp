/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DrawingAreaImpl.h"

#include "ShareableBitmap.h"
#include "UpdateInfo.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WindowGeometry.h"
#include <WebCore/GraphicsContext.h>

using namespace WebCore;

namespace WebKit {

void DrawingAreaImpl::scheduleChildWindowGeometryUpdate(const WindowGeometry& geometry)
{
#if USE(ACCELERATED_COMPOSITING)
    if (m_layerTreeHost) {
        m_layerTreeHost->scheduleChildWindowGeometryUpdate(geometry);
        return;
    }
#endif

    // FIXME: This should be a Messages::DrawingAreaProxy, and DrawingAreaProxy should pass the
    // data off to the WebPageProxy.
    m_webPage->send(Messages::WebPageProxy::ScheduleChildWindowGeometryUpdate(geometry));
}

PassOwnPtr<GraphicsContext> DrawingAreaImpl::createGraphicsContext(ShareableBitmap* bitmap)
{
    HDC bitmapDC = bitmap->windowsContext();
    // FIXME: We should really be checking m_webPage->draws[Transparent]Background() to determine
    // whether to have an alpha channel or not. But currently we always render into a non-layered
    // window, so the alpha channel doesn't matter anyway.
    return adoptPtr(new GraphicsContext(bitmapDC, false));
}

} // namespace WebKit

