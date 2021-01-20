/*
 * Copyright (C) 2011-2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TileCoverageMap_h
#define TileCoverageMap_h

#include "FloatRect.h"
#include "IntRect.h"
#include "PlatformCALayer.h"
#include "PlatformCALayerClient.h"
#include "Timer.h"
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

class FloatRect;
class IntPoint;
class IntRect;
class TileController;

class TileCoverageMap : public PlatformCALayerClient {
    WTF_MAKE_NONCOPYABLE(TileCoverageMap); WTF_MAKE_FAST_ALLOCATED;
public:
    TileCoverageMap(const TileController&);
    ~TileCoverageMap();

    void update();
    void setPosition(const FloatPoint& position) { m_position = position; }

    PlatformCALayer& layer() { return m_layer; }

    void setDeviceScaleFactor(float);

    void setNeedsUpdate();

private:
    // PlatformCALayerClient
    GraphicsLayer::CompositingCoordinatesOrientation platformCALayerContentsOrientation() const override { return GraphicsLayer::CompositingCoordinatesOrientation::TopDown; }
    bool platformCALayerContentsOpaque() const override { return true; }
    bool platformCALayerDrawsContent() const override { return true; }
    void platformCALayerPaintContents(PlatformCALayer*, GraphicsContext&, const FloatRect&, GraphicsLayerPaintBehavior) override;
    float platformCALayerDeviceScaleFactor() const override;

    void updateTimerFired();
    
    const TileController& m_controller;
    
    Timer m_updateTimer;

    Ref<PlatformCALayer> m_layer;
    Ref<PlatformCALayer> m_visibleViewportIndicatorLayer;
    Ref<PlatformCALayer> m_layoutViewportIndicatorLayer;
    Ref<PlatformCALayer> m_coverageRectIndicatorLayer;

    FloatPoint m_position;
};

}

#endif
