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

#if USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)

#include "LayerTreeHost.h"

#if USE(COORDINATED_GRAPHICS_THREADED)
#include "ThreadedCoordinatedLayerTreeHost.h"
#endif

using namespace WebCore;

namespace WebKit {

RefPtr<LayerTreeHost> LayerTreeHost::create(WebPage& webPage)
{
#if USE(COORDINATED_GRAPHICS_THREADED)
    return ThreadedCoordinatedLayerTreeHost::create(webPage);
#else
    UNUSED_PARAM(webPage);
    return nullptr;
#endif
}

LayerTreeHost::LayerTreeHost(WebPage& webPage)
    : m_webPage(webPage)
{
}

LayerTreeHost::~LayerTreeHost()
{
    ASSERT(!m_isValid);
}

void LayerTreeHost::setLayerFlushSchedulingEnabled(bool layerFlushingEnabled)
{
    if (m_layerFlushSchedulingEnabled == layerFlushingEnabled)
        return;

    m_layerFlushSchedulingEnabled = layerFlushingEnabled;

    if (m_layerFlushSchedulingEnabled) {
        scheduleLayerFlush();
        return;
    }

    cancelPendingLayerFlush();
}

void LayerTreeHost::pauseRendering()
{
    m_isSuspended = true;
}

void LayerTreeHost::resumeRendering()
{
    m_isSuspended = false;
    scheduleLayerFlush();
}

void LayerTreeHost::invalidate()
{
    ASSERT(m_isValid);
    m_isValid = false;
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS) || USE(TEXTURE_MAPPER)
