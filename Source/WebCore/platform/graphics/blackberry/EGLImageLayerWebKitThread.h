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

#ifndef EGLImageLayerWebKitThread_h
#define EGLImageLayerWebKitThread_h

#if USE(ACCELERATED_COMPOSITING)

#include "EGLImageLayerCompositingThreadClient.h"
#include "LayerWebKitThread.h"

namespace WebCore {

// Base class for EGLImage based layers
// The subclass must reimplement updateTextureContentsIfNeeded() to call updateFrontBuffer
// with the appropriate parameters, and make sure to call deleteFrontBuffer() from its
// destructor.
class EGLImageLayerWebKitThread : public LayerWebKitThread {
public:
    virtual ~EGLImageLayerWebKitThread();

    virtual void setNeedsDisplay();

protected:
    EGLImageLayerWebKitThread(LayerType);

    virtual void updateTextureContentsIfNeeded() = 0;
    virtual void commitPendingTextureUploads();

    // The context must be current before you call any of these
    void updateFrontBuffer(const IntSize&, unsigned backBufferTexture);
    void deleteFrontBuffer();

private:
    bool createImageIfNeeded(const IntSize&);
    void createShaderIfNeeded();
    void blitToFrontBuffer(unsigned backBufferTexture);

    RefPtr<EGLImageLayerCompositingThreadClient> m_client;
    bool m_needsDisplay;
    unsigned m_frontBufferTexture;
    unsigned m_fbo;
    unsigned m_shader;
    void* m_image;
    Vector<void*> m_garbage;
    IntSize m_size;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif // EGLImageLayerWebKitThread_h
