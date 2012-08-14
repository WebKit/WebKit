/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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
#include "EGLImageLayerCompositingThreadClient.h"

#if USE(ACCELERATED_COMPOSITING)

#include "LayerCompositingThread.h"
#include <BlackBerryPlatformGraphics.h>
#include <BlackBerryPlatformLog.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

namespace WebCore {

static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES = 0;
static PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR = 0;

EGLImageLayerCompositingThreadClient::~EGLImageLayerCompositingThreadClient()
{
    // Someone should have called deleteTextures() by now
    ASSERT(!m_texture && !m_image);
}

void EGLImageLayerCompositingThreadClient::uploadTexturesIfNeeded(LayerCompositingThread*)
{
    if (!m_imageChanged)
        return;

    if (!glEGLImageTargetTexture2DOES) {
        glEGLImageTargetTexture2DOES = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));
        ASSERT(glEGLImageTargetTexture2DOES);
        if (!glEGLImageTargetTexture2DOES) {
            using namespace BlackBerry::Platform;
            logAlways(LogLevelWarn, "eglGetProcAddress for glEGLImageTargetTexture2DOES FAILED");
            m_imageChanged = false;
            return;
        }
    }

    if (m_image) {
        if (!m_texture) {
            m_texture = Texture::create();
            m_texture->protect();
        }

        // Connect to the new image
        glBindTexture(GL_TEXTURE_2D, m_texture->textureId());
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, m_image);
    } else if (m_texture) {
        m_texture->unprotect();
        m_texture.clear();
    }

    m_imageChanged = false;
}

void EGLImageLayerCompositingThreadClient::drawTextures(LayerCompositingThread* layer, double /*scale*/, int positionLocation, int texCoordLocation)
{
    static float upsideDown[4 * 2] = { 0, 1,  0, 0,  1, 0,  1, 1 };

    if (!m_texture)
        return;

    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, &layer->getTransformedBounds());
    glVertexAttribPointer(texCoordLocation, 2, GL_FLOAT, GL_FALSE, 0, upsideDown);
    glBindTexture(GL_TEXTURE_2D, m_texture->textureId());
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void EGLImageLayerCompositingThreadClient::deleteTextures(LayerCompositingThread*)
{
    if (m_texture) {
        m_texture->unprotect();
        m_texture.clear();
    }

    if (!m_image)
        return;

    // If the image is still with us, it was never destroyed by the EGLImageLayerWebKitThread.
    if (!eglDestroyImageKHR)
        eglDestroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));

    ASSERT(eglDestroyImageKHR);
    if (eglDestroyImageKHR)
        eglDestroyImageKHR(BlackBerry::Platform::Graphics::eglDisplay(), m_image);

    m_image = 0;
}

void EGLImageLayerCompositingThreadClient::bindContentsTexture(LayerCompositingThread*)
{
    glBindTexture(GL_TEXTURE_2D, m_texture->textureId());
}

void EGLImageLayerCompositingThreadClient::setImage(void* image)
{
    if (m_image == image)
        return;

    m_image = image;
    m_imageChanged = true;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
