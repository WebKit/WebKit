/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DisplayStyle.h"

#include "BorderData.h"
#include "FillLayer.h"
#include "RenderStyle.h"
#include "ShadowData.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Display {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(Style);

static RefPtr<FillLayer> deepCopy(const FillLayer& layer)
{
    RefPtr<FillLayer> firstLayer;
    FillLayer* currCopiedLayer = nullptr;

    for (auto* currLayer = &layer; currLayer; currLayer = currLayer->next()) {
        RefPtr<FillLayer> layerCopy = currLayer->copy();

        if (!firstLayer) {
            firstLayer = layerCopy;
            currCopiedLayer = layerCopy.get();
        } else {
            auto nextCopiedLayer = layerCopy.get();
            currCopiedLayer->setNext(WTFMove(layerCopy));
            currCopiedLayer = nextCopiedLayer;
        }
    }
    return firstLayer;
}

static std::unique_ptr<ShadowData> deepCopy(const ShadowData* shadow, const RenderStyle& style)
{
    std::unique_ptr<ShadowData> firstShadow;
    ShadowData* currCopiedShadow = nullptr;

    for (auto* currShadow = shadow; currShadow; currShadow = currShadow->next()) {
        auto shadowCopy = makeUnique<ShadowData>(*currShadow);
        shadowCopy->setColor(style.colorByApplyingColorFilter(shadowCopy->color()));
        
        if (!firstShadow) {
            currCopiedShadow = shadowCopy.get();
            firstShadow = WTFMove(shadowCopy);
        } else {
            auto nextCopiedShadow = shadowCopy.get();
            currCopiedShadow->setNext(WTFMove(shadowCopy));
            currCopiedShadow = nextCopiedShadow;
        }
    }
    return firstShadow;
}

Style::Style(const RenderStyle& style)
    : Style(style, &style)
{
}

Style::Style(const RenderStyle& style, const RenderStyle* styleForBackground)
    : m_overflowX(style.overflowX())
    , m_overflowY(style.overflowY())
    , m_fontCascade(style.fontCascade())
    , m_whiteSpace(style.whiteSpace())
    , m_tabSize(style.tabSize())
    , m_opacity(style.opacity())
{
    // FIXME: Is currentColor resolved here?
    m_color = style.visitedDependentColorWithColorFilter(CSSPropertyColor);

    if (styleForBackground)
        setupBackground(*styleForBackground);

    m_boxShadow = deepCopy(style.boxShadow(), style);

    if (!style.hasAutoUsedZIndex())
        m_zIndex = style.usedZIndex();

    setIsPositioned(style.position() != PositionType::Static);
    setIsFloating(style.floating() != Float::None);
    setHasTransform(style.hasTransform());
}

void Style::setupBackground(const RenderStyle& style)
{
    m_backgroundColor = style.visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor);
    m_backgroundLayers = deepCopy(style.backgroundLayers());
}

bool Style::hasBackground() const
{
    return m_backgroundColor.isVisible() || hasBackgroundImage();
}

bool Style::hasBackgroundImage() const
{
    return m_backgroundLayers && m_backgroundLayers->hasImage();
}

bool Style::backgroundHasOpaqueTopLayer() const
{
    auto* fillLayer = backgroundLayers();
    if (!fillLayer)
        return false;

    if (fillLayer->clip() != FillBox::Border)
        return false;

    // FIXME: Check for overflow clip and local attachment.
    // FIXME: Check for repeated, opaque and renderable image.

    // If there is only one layer and no image, check whether the background color is opaque.
    if (!fillLayer->next() && !fillLayer->hasImage()) {
        if (backgroundColor().isOpaque())
            return true;
    }

    return false;
}

bool Style::autoWrap() const
{
    return RenderStyle::autoWrap(whiteSpace());
}

bool Style::preserveNewline() const
{
    return RenderStyle::preserveNewline(whiteSpace());
}

bool Style::collapseWhiteSpace() const
{
    return RenderStyle::collapseWhiteSpace(whiteSpace());
}

} // namespace Display
} // namespace WebCore

