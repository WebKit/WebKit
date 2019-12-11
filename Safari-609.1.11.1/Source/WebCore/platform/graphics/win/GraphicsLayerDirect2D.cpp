/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GraphicsLayerDirect2D.h"

#if USE(DIRECT2D)

#include "DisplayList.h"
#include "GraphicsLayerFactory.h"
#include "NotImplemented.h"
#include <limits.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/WTFString.h>


#if COMPILER(MSVC)
// See https://msdn.microsoft.com/en-us/library/1wea5zwe.aspx
#pragma warning(disable: 4701)
#endif

namespace WebCore {

Ref<GraphicsLayer> GraphicsLayer::create(GraphicsLayerFactory* factory, GraphicsLayerClient& client, Type layerType)
{
    if (factory) {
        auto layer = factory->createGraphicsLayer(layerType, client);
        layer->initialize(layerType);
        return layer;
    }

    // FIXME: Return a real Direct2D graphics layer
    return adoptRef(*new GraphicsLayerDirect2D(layerType, client));
}

GraphicsLayerDirect2D::GraphicsLayerDirect2D(Type layerType, GraphicsLayerClient& client)
    : GraphicsLayer(layerType, client)
{
}

void GraphicsLayerDirect2D::initialize(Type layerType)
{
}

GraphicsLayerDirect2D::~GraphicsLayerDirect2D()
{
    willBeDestroyed();
}

void GraphicsLayerDirect2D::setNeedsDisplay()
{
    if (!drawsContent())
        return;

    notImplemented();
}

void GraphicsLayerDirect2D::setNeedsDisplayInRect(const FloatRect& r, ShouldClipToLayer shouldClip)
{
    if (!drawsContent())
        return;

    notImplemented();
}

} // namespace WebCore

#endif
