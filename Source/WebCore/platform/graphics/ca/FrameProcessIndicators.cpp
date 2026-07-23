/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "FrameProcessIndicators.h"

#if USE(CA)

#include "ComplexTextController.h"
#include "FontCascade.h"
#include "FontCascadeDescription.h"
#include "FontSelector.h"
#include "GraphicsLayerCA.h"
#include "TextRun.h"
#include <wtf/ProcessID.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

static constexpr float padding = 3;
static constexpr float fontSize = 12;
static constexpr float borderWidth = 4;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameProcessIndicators);

FrameProcessIndicators::FrameProcessIndicators(GraphicsLayerCA& graphicsLayer)
    : m_graphicsLayer(graphicsLayer)
    , m_backgroundColor(borderColor())
    , m_text(String::number(getCurrentProcessID()))
    , m_borderLayer(graphicsLayer.createPlatformCALayer(PlatformCALayer::LayerType::LayerTypeLayer, nullptr))
    , m_indicatorLayer(graphicsLayer.createPlatformCALayer(PlatformCALayer::LayerType::LayerTypeWebLayer, this))
{
    m_borderLayer->setName(MAKE_STATIC_STRING_IMPL("frame process border"));
    m_borderLayer->setAnchorPoint({ });
    m_borderLayer->setBorderColor(m_backgroundColor);
    m_borderLayer->setBorderWidth(borderWidth);
    m_borderLayer->setUserInteractionEnabled(false);

    m_indicatorLayer->setName(MAKE_STATIC_STRING_IMPL("frame process indicator"));
    m_indicatorLayer->setAnchorPoint({ });
    m_indicatorLayer->setBounds({ { }, size() });
    m_indicatorLayer->setContentsScale(graphicsLayer.m_layer->contentsScale());
    m_indicatorLayer->setUserInteractionEnabled(false);
    m_indicatorLayer->setNeedsDisplay();

    updateGeometry(graphicsLayer.m_layer->bounds());
}

FrameProcessIndicators::~FrameProcessIndicators()
{
    m_indicatorLayer->setOwner(nullptr);
}

void FrameProcessIndicators::appendLayers(PlatformCALayerList& list) const
{
    list.append(m_borderLayer);
    list.append(m_indicatorLayer);
}

void FrameProcessIndicators::updateGeometry(const FloatRect& bounds)
{
    auto location = bounds.location();
    m_borderLayer->setPosition(location);
    m_borderLayer->setBounds({ { }, bounds.size() });
    m_indicatorLayer->setPosition({ bounds.maxX() - m_indicatorLayer->bounds().width(), location.y(), 1 });
}

void FrameProcessIndicators::updateContentsScale(float contentsScale)
{
    if (contentsScale == m_indicatorLayer->contentsScale())
        return;

    m_indicatorLayer->setContentsScale(contentsScale);
    m_indicatorLayer->setNeedsDisplay();
}

Color FrameProcessIndicators::borderColor()
{
    auto hash = intHash(static_cast<uint32_t>(getCurrentProcessID()));
    uint8_t r = (hash >> 0) & 0xFF;
    uint8_t g = (hash >> 8) & 0xFF;
    uint8_t b = (hash >> 16) & 0xFF;
    return SRGBA<uint8_t> { r, g, b }.colorWithAlphaByte(192);
}

FontCascade FrameProcessIndicators::makeFont()
{
    FontCascadeDescription fontDescription;
    fontDescription.setOneFamily("Helvetica"_s);
    fontDescription.setSpecifiedSize(fontSize);
    fontDescription.setComputedSize(fontSize);

    FontCascade font(WTF::move(fontDescription));
    font.update(nullptr);
    return font;
}

FloatSize FrameProcessIndicators::size() const
{
    auto font = makeFont();
    auto [textTop, textBottom] = textVerticalBounds(font);
    return { 2 * padding + font.width(TextRun(m_text)), 2 * padding + textBottom - textTop };
}

std::pair<float, float> FrameProcessIndicators::textVerticalBounds(const FontCascade& font) const
{
    return ComplexTextController::enclosingGlyphBoundsForTextRun(font, TextRun(m_text));
}

void FrameProcessIndicators::platformCALayerPaintContents(PlatformCALayer* layer, GraphicsContext& context, const FloatRect&, OptionSet<GraphicsLayerPaintBehavior>)
{
    auto font = makeFont();
    auto textTop = textVerticalBounds(font).first;
    auto textColor = m_backgroundColor.luminance() > 0.5 ? Color::black : Color::white;
    auto indicatorSize = layer->bounds().size();

    context.setFillColor(m_backgroundColor);
    context.fillRect(FloatRect { 0, borderWidth, indicatorSize.width() - borderWidth, indicatorSize.height() - borderWidth });
    context.setFillColor(textColor);
    context.drawText(font, TextRun(m_text), { padding, padding - textTop });
}

float FrameProcessIndicators::platformCALayerDeviceScaleFactor() const
{
    return m_graphicsLayer ? m_graphicsLayer->deviceScaleFactor() : 1;
}

} // namespace WebCore

#endif // USE(CA)
