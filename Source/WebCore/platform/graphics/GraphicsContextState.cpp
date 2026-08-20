/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "GraphicsContextState.h"

#include <wtf/MathExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

GraphicsContextState::GraphicsContextState(const ChangeFlags& changeFlags, InterpolationQuality imageInterpolationQuality)
    : m_changeFlags(changeFlags)
    , m_imageInterpolationQuality(imageInterpolationQuality)
{
}

void GraphicsContextState::repurpose(Purpose purpose)
{
    if (purpose == Purpose::Initial)
        m_changeFlags = { };

#if USE(CG)
    // CGContextBeginTransparencyLayer() sets the CG global alpha to 1. Keep the clone's alpha in sync.
    if (purpose == Purpose::TransparencyLayer) {
        m_alpha = 1;
        m_style = std::nullopt;
        m_dropShadow = std::nullopt;
        m_compositeMode = { CompositeOperator::SourceOver, BlendMode::Normal };
    }
#endif

    m_purpose = purpose;
}

GraphicsContextState GraphicsContextState::clone(Purpose purpose) const
{
    auto clone = *this;
    clone.repurpose(purpose);
    return clone;
}

template<typename Functor>
constexpr void GraphicsContextState::forEachProperty(NOESCAPE const Functor& functor)
{
    functor(Change::FillBrush, &GraphicsContextState::m_fillBrush);
    functor(Change::FillRule, &GraphicsContextState::m_fillRule);
    functor(Change::StrokeBrush, &GraphicsContextState::m_strokeBrush);
    functor(Change::StrokeThickness, &GraphicsContextState::m_strokeThickness);
    functor(Change::StrokeStyle, &GraphicsContextState::m_strokeStyle);
    functor(Change::CompositeMode, &GraphicsContextState::m_compositeMode);
    functor(Change::DropShadow, &GraphicsContextState::m_dropShadow);
    functor(Change::Style, &GraphicsContextState::m_style);
    functor(Change::Alpha, &GraphicsContextState::m_alpha);
    functor(Change::ImageInterpolationQuality, &GraphicsContextState::m_imageInterpolationQuality);
    functor(Change::TextDrawingMode, &GraphicsContextState::m_textDrawingMode);
    functor(Change::ShouldAntialias, &GraphicsContextState::m_shouldAntialias);
    functor(Change::ShouldSmoothFonts, &GraphicsContextState::m_shouldSmoothFonts);
    functor(Change::ShouldSubpixelQuantizeFonts, &GraphicsContextState::m_shouldSubpixelQuantizeFonts);
    functor(Change::ShadowsIgnoreTransforms, &GraphicsContextState::m_shadowsIgnoreTransforms);
    functor(Change::DrawLuminanceMask, &GraphicsContextState::m_drawLuminanceMask);
}

void GraphicsContextState::mergeLastChanges(const GraphicsContextState& state)
{
    auto changes = state.changes();
    if (!changes)
        return;
    forEachProperty([&](Change change, auto property) {
        if (!changes.contains(change) || this->*property == state.*property)
            return;
        this->*property = state.*property;
        m_changeFlags.add(change);
    });
}

void GraphicsContextState::mergeAllChanges(const GraphicsContextState& state)
{
    forEachProperty([&](Change change, auto property) {
        if (this->*property == state.*property)
            return;
        this->*property = state.*property;
        m_changeFlags.add(change);
    });
}

void GraphicsContextState::filterLastChangesForMatching(const GraphicsContextState& state)
{
    if (!m_changeFlags)
        return;
    forEachProperty([&](Change change, auto property) {
        if (m_changeFlags.contains(change) && this->*property == state.*property)
            m_changeFlags.remove(change);
    });
}

void GraphicsContextState::copyLastChangesFrom(const GraphicsContextState& state)
{
    auto changes = state.m_changeFlags;
    if (!changes)
        return;
    forEachProperty([&](Change change, auto property) {
        if (changes.contains(change))
            this->*property = state.*property;
    });
}

static ASCIILiteral stateChangeName(GraphicsContextState::Change change)
{
    switch (change) {
    case GraphicsContextState::Change::FillBrush:
        return "fill-brush"_s;

    case GraphicsContextState::Change::FillRule:
        return "fill-rule"_s;

    case GraphicsContextState::Change::StrokeBrush:
        return "stroke-brush"_s;

    case GraphicsContextState::Change::StrokeThickness:
        return "stroke-thickness"_s;

    case GraphicsContextState::Change::StrokeStyle:
        return "stroke-style"_s;

    case GraphicsContextState::Change::CompositeMode:
        return "composite-mode"_s;

    case GraphicsContextState::Change::DropShadow:
        return "drop-shadow"_s;

    case GraphicsContextState::Change::Style:
        return "style"_s;

    case GraphicsContextState::Change::Alpha:
        return "alpha"_s;

    case GraphicsContextState::Change::ImageInterpolationQuality:
        return "image-interpolation-quality"_s;

    case GraphicsContextState::Change::TextDrawingMode:
        return "text-drawing-mode"_s;

    case GraphicsContextState::Change::ShouldAntialias:
        return "should-antialias"_s;

    case GraphicsContextState::Change::ShouldSmoothFonts:
        return "should-smooth-fonts"_s;

    case GraphicsContextState::Change::ShouldSubpixelQuantizeFonts:
        return "should-subpixel-quantize-fonts"_s;

    case GraphicsContextState::Change::ShadowsIgnoreTransforms:
        return "shadows-ignore-transforms"_s;

    case GraphicsContextState::Change::DrawLuminanceMask:
        return "draw-luminance-mask"_s;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

TextStream& GraphicsContextState::dump(TextStream& ts) const
{
    constexpr auto numberOfProperties = [] {
        size_t count = 0;
        forEachProperty([&](Change, auto) { ++count; });
        return count;
    };
    // DrawLuminanceMask is the highest Change bit, so its index plus one is the number of changes.
    static_assert(numberOfProperties() == WTF::ctz(std::to_underlying(Change::DrawLuminanceMask)) + 1,
        "forEachProperty() must list every Change enumerator");

    ts.dumpProperty("change-flags"_s, m_changeFlags);
    if (m_changeFlags) {
        forEachProperty([&](Change change, auto property) {
            if (m_changeFlags.contains(change))
                ts.dumpProperty(stateChangeName(change), this->*property);
        });
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, GraphicsContextState::Change change)
{
    ts << stateChangeName(change);
    return ts;
}

TextStream& operator<<(TextStream& ts, const GraphicsContextState& state)
{
    return state.dump(ts);
}

} // namespace WebCore
