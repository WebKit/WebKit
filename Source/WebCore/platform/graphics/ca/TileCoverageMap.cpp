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

#include "config.h"
#include "TileCoverageMap.h"

#include "GraphicsContext.h"
#include "TileController.h"
#include "TileGrid.h"

namespace WebCore {

TileCoverageMap::TileCoverageMap(const TileController& controller)
    : m_controller(controller)
    , m_updateTimer(*this, &TileCoverageMap::updateTimerFired)
    , m_layer(controller.rootLayer().createCompatibleLayer(PlatformCALayer::LayerTypeSimpleLayer, this))
    , m_visibleViewportIndicatorLayer(controller.rootLayer().createCompatibleLayer(PlatformCALayer::LayerTypeLayer, nullptr))
    , m_layoutViewportIndicatorLayer(controller.rootLayer().createCompatibleLayer(PlatformCALayer::LayerTypeLayer, nullptr))
    , m_coverageRectIndicatorLayer(controller.rootLayer().createCompatibleLayer(PlatformCALayer::LayerTypeLayer, nullptr))
    , m_position(FloatPoint(0, controller.topContentInset()))
{
    m_layer.get().setOpacity(0.75);
    m_layer.get().setAnchorPoint(FloatPoint3D());
    m_layer.get().setBorderColor(Color::black);
    m_layer.get().setBorderWidth(1);
    m_layer.get().setPosition(FloatPoint(2, 2));
    m_layer.get().setContentsScale(m_controller.deviceScaleFactor());

    m_visibleViewportIndicatorLayer.get().setName("visible viewport indicator");
    m_visibleViewportIndicatorLayer.get().setBorderWidth(2);
    m_visibleViewportIndicatorLayer.get().setAnchorPoint(FloatPoint3D());
    m_visibleViewportIndicatorLayer.get().setBorderColor(Color::red.colorWithAlpha(200));

    m_layoutViewportIndicatorLayer.get().setName("layout viewport indicator");
    m_layoutViewportIndicatorLayer.get().setBorderWidth(2);
    m_layoutViewportIndicatorLayer.get().setAnchorPoint(FloatPoint3D());
    m_layoutViewportIndicatorLayer.get().setBorderColor(SRGBA<uint8_t> { 0, 128, 128, 200 });
    
    m_coverageRectIndicatorLayer.get().setName("coverage indicator");
    m_coverageRectIndicatorLayer.get().setAnchorPoint(FloatPoint3D());
    m_coverageRectIndicatorLayer.get().setBackgroundColor(SRGBA<uint8_t> { 64, 64, 64, 50 });

    m_layer.get().appendSublayer(m_coverageRectIndicatorLayer);
    m_layer.get().appendSublayer(m_visibleViewportIndicatorLayer);
    
    if (m_controller.layoutViewportRect())
        m_layer.get().appendSublayer(m_layoutViewportIndicatorLayer);

    update();
}

TileCoverageMap::~TileCoverageMap()
{
    m_layer.get().setOwner(nullptr);
}

void TileCoverageMap::setNeedsUpdate()
{
    if (!m_updateTimer.isActive())
        m_updateTimer.startOneShot(0_s);
}

void TileCoverageMap::updateTimerFired()
{
    update();
}

void TileCoverageMap::update()
{
    FloatRect containerBounds = m_controller.bounds();
    FloatRect visibleRect = m_controller.visibleRect();
    FloatRect coverageRect = m_controller.coverageRect();
    visibleRect.contract(4, 4); // Layer is positioned 2px from top and left edges.

    float widthScale = 1;
    float scale = 1;
    if (!containerBounds.isEmpty()) {
        widthScale = std::min<float>(visibleRect.width() / containerBounds.width(), 0.1);
        float visibleHeight = visibleRect.height() - std::min(m_controller.topContentInset(), visibleRect.y());
        scale = std::min(widthScale, visibleHeight / containerBounds.height());
    }

    float indicatorScale = scale * m_controller.tileGrid().scale();

    FloatRect mapBounds = containerBounds;
    mapBounds.scale(indicatorScale);

    m_layer.get().setPosition(m_position + FloatPoint(2, 2));
    m_layer.get().setBounds(mapBounds);
    m_layer.get().setNeedsDisplay();

    visibleRect.scale(indicatorScale);
    visibleRect.expand(2, 2);
    m_visibleViewportIndicatorLayer->setPosition(visibleRect.location());
    m_visibleViewportIndicatorLayer->setBounds(FloatRect(FloatPoint(), visibleRect.size()));

    if (auto layoutViewportRect = m_controller.layoutViewportRect()) {
        FloatRect layoutRect = layoutViewportRect.value();
        layoutRect.scale(indicatorScale);
        layoutRect.expand(2, 2);
        m_layoutViewportIndicatorLayer->setPosition(layoutRect.location());
        m_layoutViewportIndicatorLayer->setBounds(FloatRect(FloatPoint(), layoutRect.size()));

        if (!m_layoutViewportIndicatorLayer->superlayer())
            m_layer.get().appendSublayer(m_layoutViewportIndicatorLayer);
    } else if (m_layoutViewportIndicatorLayer->superlayer())
        m_layoutViewportIndicatorLayer->removeFromSuperlayer();

    coverageRect.scale(indicatorScale);
    coverageRect.expand(2, 2);
    m_coverageRectIndicatorLayer->setPosition(coverageRect.location());
    m_coverageRectIndicatorLayer->setBounds(FloatRect(FloatPoint(), coverageRect.size()));

    Color visibleRectIndicatorColor;
    switch (m_controller.indicatorMode()) {
    case SynchronousScrollingBecauseOfLackOfScrollingCoordinatorIndication:
        visibleRectIndicatorColor = SRGBA<uint8_t> { 200, 80, 255 };
        break;
    case SynchronousScrollingBecauseOfStyleIndication:
        visibleRectIndicatorColor = Color::red;
        break;
    case SynchronousScrollingBecauseOfEventHandlersIndication:
        visibleRectIndicatorColor = Color::yellow;
        break;
    case AsyncScrollingIndication:
        visibleRectIndicatorColor = SRGBA<uint8_t> { 0, 200, 0 };
        break;
    }

    m_visibleViewportIndicatorLayer.get().setBorderColor(visibleRectIndicatorColor);
}

void TileCoverageMap::platformCALayerPaintContents(PlatformCALayer* platformCALayer, GraphicsContext& context, const FloatRect&, GraphicsLayerPaintBehavior)
{
    ASSERT_UNUSED(platformCALayer, platformCALayer == m_layer.ptr());
    m_controller.tileGrid().drawTileMapContents(context.platformContext(), m_layer.get().bounds());
}

float TileCoverageMap::platformCALayerDeviceScaleFactor() const
{
    return m_controller.rootLayer().owner()->platformCALayerDeviceScaleFactor();
}

void TileCoverageMap::setDeviceScaleFactor(float deviceScaleFactor)
{
    m_layer.get().setContentsScale(deviceScaleFactor);
}

}
