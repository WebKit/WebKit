/*
 * Copyright (C) 2011 Igalia S.L.
 * Copyright (C) 2012 Collabora Ltd.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#ifndef GraphicsContext3DPrivate_h
#define GraphicsContext3DPrivate_h

#include "GraphicsContext3D.h"
#include <wtf/PassOwnPtr.h>

#if USE(ACCELERATED_COMPOSITING) && USE(CLUTTER)
#include <clutter/clutter.h>
#endif

namespace WebCore {

class GraphicsContext3DPrivate {
public:
    static PassOwnPtr<GraphicsContext3DPrivate> create(GraphicsContext3D*, HostWindow*);
    ~GraphicsContext3DPrivate();
    bool makeContextCurrent();
    PlatformGraphicsContext3D platformContext();
    ClutterActor* platformLayer() { return m_layer; }

#if USE(ACCELERATED_COMPOSITING) && USE(CLUTTER)
    virtual void paintToGraphicsLayerActor(ClutterActor*, const FloatRect& target, const TransformationMatrix&, float opacity);
#endif

private:
    GraphicsContext3DPrivate(GraphicsContext3D*, HostWindow*);

    GraphicsContext3D* m_context;
    HostWindow* m_window;
    ClutterActor* m_layer;
};

}

#endif // GraphicsContext3DPrivate_h
