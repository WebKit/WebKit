/*
 * Copyright (C) 2014 Haiku, Inc. All rights reserved.
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
#include "GraphicsLayer.h"

#include "GraphicsLayerFactory.h"
#include "NotImplemented.h"


namespace WebCore {

class GraphicsLayerHaiku: public GraphicsLayer {
public:
    GraphicsLayerHaiku(Type type, GraphicsLayerClient& client);
    ~GraphicsLayerHaiku();

    void setNeedsDisplay();
    void setNeedsDisplayInRect(const FloatRect&, ShouldClipToLayer);
};

GraphicsLayerHaiku::GraphicsLayerHaiku(Type type, GraphicsLayerClient& client)
    : GraphicsLayer(type, client)
{
}

GraphicsLayerHaiku::~GraphicsLayerHaiku()
{
    // Call this cleanup method before destroying our vtable.
    willBeDestroyed();
}

void GraphicsLayerHaiku::setNeedsDisplay()
{
    notImplemented();
}

void GraphicsLayerHaiku::setNeedsDisplayInRect(const FloatRect&, ShouldClipToLayer)
{
    notImplemented();
}

Ref<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient& client, Type type)
{
    if (!factory)
        return adoptRef(* new GraphicsLayerHaiku(type, client));
    return factory->createGraphicsLayer(type, client);
}

}
