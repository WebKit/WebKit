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

#ifndef EGLImageLayerCompositingThreadClient_h
#define EGLImageLayerCompositingThreadClient_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerCompositingThreadClient.h"
#include "Texture.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class LayerCompositingThread;

class EGLImageLayerCompositingThreadClient : public ThreadSafeRefCounted<EGLImageLayerCompositingThreadClient>, public LayerCompositingThreadClient {
public:
    static PassRefPtr<EGLImageLayerCompositingThreadClient> create()
    {
        return adoptRef(new EGLImageLayerCompositingThreadClient());
    }

    virtual ~EGLImageLayerCompositingThreadClient();

    virtual void layerCompositingThreadDestroyed(LayerCompositingThread*)
    {
        deref(); // Matched by ref() in constructor
    }

    virtual void layerVisibilityChanged(LayerCompositingThread*, bool visible) { }

    virtual void uploadTexturesIfNeeded(LayerCompositingThread*);
    virtual void drawTextures(LayerCompositingThread*, double scale, int positionLocation, int texCoordLocation);
    virtual void deleteTextures(LayerCompositingThread*);

    virtual void bindContentsTexture(LayerCompositingThread*);

    // The image must not be deleted while in our custody, however after changing image you are free to delete the old one.
    void setImage(void*);

private:
    EGLImageLayerCompositingThreadClient()
        : m_image(0)
        , m_imageChanged(false)
    {
        ref(); // Matched by deref() in layerCompositingThreadDestroyed()
    }

    void* m_image;
    bool m_imageChanged;
    RefPtr<Texture> m_texture;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif // EGLImageLayerCompositingThreadClient_h
