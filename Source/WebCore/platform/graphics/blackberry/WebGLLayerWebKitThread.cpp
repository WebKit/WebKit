/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
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

#include "config.h"
#include "WebGLLayerWebKitThread.h"

#if USE(ACCELERATED_COMPOSITING) && ENABLE(WEBGL)

#include "GraphicsContext3D.h"
#include <pthread.h>

namespace WebCore {

WebGLLayerWebKitThread::WebGLLayerWebKitThread()
    : LayerWebKitThread(WebGLLayer, 0)
    , m_webGLContext(0)
    , m_needsDisplay(false)
{
    setLayerProgramShader(LayerProgramShaderRGBA);
}

WebGLLayerWebKitThread::~WebGLLayerWebKitThread()
{
    if (m_frontBufferLock)
        pthread_mutex_destroy(m_frontBufferLock);
}

void WebGLLayerWebKitThread::setNeedsDisplay()
{
    m_needsDisplay = true;
    setNeedsCommit();
}

void WebGLLayerWebKitThread::updateTextureContentsIfNeeded()
{
    // FIXME: Does WebGLLayer always need display? Or can we just return immediately if (!m_needsDisplay)?
    m_needsDisplay = false;

    if (!m_frontBufferLock)
        createFrontBufferLock();

    // Lock copied GL texture so that the UI thread won't access it while we are drawing into it.
    pthread_mutex_lock(m_frontBufferLock);
    m_webGLContext->prepareTexture();
    pthread_mutex_unlock(m_frontBufferLock);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING) && ENABLE(WEBGL)
