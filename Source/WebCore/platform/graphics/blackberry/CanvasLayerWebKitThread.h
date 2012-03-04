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

#ifndef CanvasLayerWebKitThread_h
#define CanvasLayerWebKitThread_h

#if USE(ACCELERATED_COMPOSITING) && ENABLE(SKIA_GPU_CANVAS)

#include "LayerWebKitThread.h"

namespace WebCore {

class HTMLCanvasElement;

class CanvasLayerWebKitThread : public LayerWebKitThread {
public:
    static PassRefPtr<CanvasLayerWebKitThread> create(HTMLCanvasElement* canvas)
    {
        return adoptRef(new CanvasLayerWebKitThread(canvas));
    }

    virtual ~CanvasLayerWebKitThread();

    void setCanvas(HTMLCanvasElement*);

    virtual void setNeedsDisplay();

protected:
    virtual void updateTextureContentsIfNeeded();

private:
    CanvasLayerWebKitThread(HTMLCanvasElement*);
    bool m_needsDisplay;
};

}

#endif // USE(ACCELERATED_COMPOSITING) && ENABLE(SKIA_GPU_CANVAS)

#endif // CanvasLayerWebKitThread_h
