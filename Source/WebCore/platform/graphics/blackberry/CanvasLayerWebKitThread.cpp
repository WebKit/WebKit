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
#include "CanvasLayerWebKitThread.h"

#if USE(ACCELERATED_COMPOSITING) && ENABLE(ACCELERATED_2D_CANVAS)

#include "SharedGraphicsContext3D.h"
#include <GLES2/gl2.h>
#include <SkGpuDevice.h>

namespace WebCore {

CanvasLayerWebKitThread::CanvasLayerWebKitThread(SkGpuDevice* device)
    : EGLImageLayerWebKitThread(CanvasLayer)
{
    setDevice(device);
}

CanvasLayerWebKitThread::~CanvasLayerWebKitThread()
{
    if (SharedGraphicsContext3D::get()->makeContextCurrent())
        deleteFrontBuffer();
}

void CanvasLayerWebKitThread::setDevice(SkGpuDevice* device)
{
    m_device = device;
    setNeedsDisplay();
}

void CanvasLayerWebKitThread::updateTextureContentsIfNeeded()
{
    if (!m_device)
        return;

    if (!SharedGraphicsContext3D::get()->makeContextCurrent())
        return;

    GrRenderTarget* renderTarget = reinterpret_cast<GrRenderTarget*>(m_device->accessRenderTarget());
    if (!renderTarget)
        return;

    GrTexture* texture = renderTarget->asTexture();
    if (!texture)
        return;

    updateFrontBuffer(IntSize(m_device->width(), m_device->height()), texture->getTextureHandle());
}

}

#endif // USE(ACCELERATED_COMPOSITING) && ENABLE(ACCELERATED_2D_CANVAS)
