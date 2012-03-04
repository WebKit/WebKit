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

#if USE(ACCELERATED_COMPOSITING) && ENABLE(SKIA_GPU_CANVAS)

#include "HTMLCanvasElement.h"
#include "PlatformContextSkia.h"

#include <BlackBerryPlatformGraphics.h>
#include <GLES2/gl2.h>
#include <GrGLTexture.h>
#include <SkGpuDevice.h>

namespace WebCore {

CanvasLayerWebKitThread::CanvasLayerWebKitThread(HTMLCanvasElement* canvas)
    : LayerWebKitThread(Layer, 0)
{
    setCanvas(canvas);
}

CanvasLayerWebKitThread::~CanvasLayerWebKitThread()
{
    if (m_texID) {
        BlackBerry::Platform::Graphics::makeSharedResourceContextCurrent(BlackBerry::Platform::Graphics::GLES2);
        glDeleteTextures(1, &m_texID);
    }
}

void CanvasLayerWebKitThread::setCanvas(HTMLCanvasElement* canvas)
{
    m_canvas = canvas;
    setNeedsTexture(isDrawable() && canvas);
    setLayerProgramShader(LayerProgramShaderRGBA);
    setNeedsDisplay();
}

void CanvasLayerWebKitThread::setNeedsDisplay()
{
    m_needsDisplay = true;
    setNeedsCommit();
}

void CanvasLayerWebKitThread::updateTextureContentsIfNeeded()
{
    if (!m_needsDisplay)
        return;

    m_needsDisplay = false;

    if (!m_canvas || !m_canvas->drawingContext())
        return;

    SkGpuDevice* gpuDevice = static_cast<SkGpuDevice*>(m_canvas->drawingContext()->platformContext()->canvas()->getDevice());
    gpuDevice->makeRenderTargetCurrent();
    // We know this is a GrGLTexture
    if (GrGLTexture* deviceTexture = (GrGLTexture*)gpuDevice->accessTexture()) {
        m_texWidth = deviceTexture->allocWidth();
        m_texHeight = deviceTexture->allocHeight();
    }

    GLint previousTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);

    if (!m_texID) {
        glGenTextures(1, &m_texID);
        glBindTexture(GL_TEXTURE_2D, m_texID);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_canvas->width(), m_canvas->height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

        createFrontBufferLock();
    }

    pthread_mutex_lock(m_frontBufferLock);
    glBindTexture(GL_TEXTURE_2D, m_texID);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, m_texWidth, m_texHeight, 0);
    glBindTexture(GL_TEXTURE_2D, previousTexture);
    pthread_mutex_unlock(m_frontBufferLock);
}

}

#endif // USE(ACCELERATED_COMPOSITING) && ENABLE(SKIA_GPU_CANVAS)
