/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Collabora Ltd.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#include "GraphicsLayerClutter.h"

#include "FloatRect.h"
#include "GraphicsLayerFactory.h"
#include "NotImplemented.h"

namespace WebCore {

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient* client)
{
    if (!factory)
        return adoptPtr(new GraphicsLayerClutter(client));

    return factory->createGraphicsLayer(client);
}

PassOwnPtr<GraphicsLayer> GraphicsLayer::create(GraphicsLayerClient* client)
{
    return adoptPtr(new GraphicsLayerClutter(client));
}

GraphicsLayerClutter::GraphicsLayerClutter(GraphicsLayerClient* client)
    : GraphicsLayer(client)
{
    // ClutterRectangle will be used to show the debug border.
    m_layer = adoptGRef(clutter_rectangle_new());
}

GraphicsLayerClutter::~GraphicsLayerClutter()
{
    willBeDestroyed();
}

ClutterActor* GraphicsLayerClutter::platformLayer() const
{
    return m_layer.get();
}

void GraphicsLayerClutter::setNeedsDisplay()
{
    notImplemented();
    addRepaintRect(FloatRect(FloatPoint(), m_size));
}

void GraphicsLayerClutter::setNeedsDisplayInRect(const FloatRect& rect)
{
    notImplemented();
    addRepaintRect(rect);
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
