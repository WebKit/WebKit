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
    m_purpose = purpose;

    if (purpose == Purpose::Initial) {
        m_changeFlags = { };
        m_changesSinceSave = ChangeFlags::all();
        return;
    }

#if USE(CG)
    // CGContextBeginTransparencyLayer() sets the CG global alpha to 1. Keep the clone's alpha in sync.
    if (purpose == Purpose::TransparencyLayer) {
        m_alpha = 1;
        m_style = std::nullopt;
        m_dropShadow = std::nullopt;
        m_compositeMode = { CompositeOperator::SourceOver, BlendMode::Normal };
        // The platform context is the one that resets these, so they are not changes to apply to it.
        m_changeFlags = { };

        // The local state stack still has to restore these on restore().
        m_changesSinceSave.add({ Change::Alpha, Change::Style, Change::DropShadow, Change::CompositeMode });
        return;
    }
#endif
    m_changeFlags = { };
    m_changesSinceSave = { };
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
        addChange(change);
    });
}

void GraphicsContextState::mergeAllChanges(const GraphicsContextState& state)
{
    forEachProperty([&](Change change, auto property) {
        if (this->*property == state.*property)
            return;
        this->*property = state.*property;
        addChange(change);
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

GraphicsContextStateStack::SmallProperties GraphicsContextStateStack::smallProperties(const GraphicsContextState& state)
{
    return {
        std::to_underlying(state.imageInterpolationQuality()),
        state.textDrawingMode().toRaw(),
        state.shouldAntialias(),
        state.shouldSmoothFonts(),
        state.shouldSubpixelQuantizeFonts(),
        state.shadowsIgnoreTransforms(),
        state.drawLuminanceMask()
    };
}

void GraphicsContextStateStack::applySmallProperties(GraphicsContextState& state, SmallProperties values)
{
    state.m_imageInterpolationQuality = static_cast<InterpolationQuality>(values.imageInterpolationQuality);
    state.m_textDrawingMode = TextDrawingModeFlags::fromRaw(values.textDrawingMode);
    state.m_shouldAntialias = values.shouldAntialias;
    state.m_shouldSmoothFonts = values.shouldSmoothFonts;
    state.m_shouldSubpixelQuantizeFonts = values.shouldSubpixelQuantizeFonts;
    state.m_shadowsIgnoreTransforms = values.shadowsIgnoreTransforms;
    state.m_drawLuminanceMask = values.drawLuminanceMask;
}

void GraphicsContextStateStack::save(GraphicsContextState& current, Purpose purpose)
{
    // We do not record unset changes, so platform context is synchronized before.
    ASSERT(!current.changes());
    // Going from Initial to first save(), we record all properties,
    // because we do not know which properties the level 1 modifies.
    ASSERT(current.purpose() != Purpose::Initial || current.changesSinceSave() == ChangeFlags::all());

    auto changes = current.changesSinceSave();

    if (changes.contains(Change::FillBrush))
        m_fillBrush.append(current.fillBrush());
    if (changes.contains(Change::StrokeBrush))
        m_strokeBrush.append(current.strokeBrush());
    if (changes.contains(Change::StrokeThickness))
        m_strokeThickness.append(current.strokeThickness());
    if (changes.contains(Change::FillRule))
        m_fillRule.append(current.fillRule());
    if (changes.contains(Change::StrokeStyle))
        m_strokeStyle.append(current.strokeStyle());
    if (changes.contains(Change::CompositeMode))
        m_compositeMode.append(current.compositeMode());
    if (changes.contains(Change::DropShadow))
        m_dropShadow.append(current.dropShadow());
    if (changes.contains(Change::Style))
        m_style.append(current.style());
    if (changes.contains(Change::Alpha))
        m_alpha.append(current.alpha());
    if (changes.containsAny(smallChanges))
        m_smallProperties.append(smallProperties(current));

    m_levels.append({ changes, current.purpose() });

    current.m_changesSinceSave = { };
    // repurpose() adds the properties it resets to the new level's changes since save, so that
    // restore() puts them back.
    current.repurpose(purpose);
}

template<typename T, typename VectorT>
static void restoreValue(GraphicsContextState::ChangeFlags changesSinceSave, GraphicsContextState::ChangeFlags pushedValues, GraphicsContextState::Change change, VectorT& values, T& current)
{
    if (changesSinceSave.contains(change)) {
        current = pushedValues.contains(change) ? values.takeLast() : values.last();
        return;
    }
    if (pushedValues.contains(change))
        values.removeLast();
}

void GraphicsContextStateStack::restore(GraphicsContextState& current)
{
    ASSERT(!m_levels.isEmpty());
    ASSERT(!current.changes());

    auto level = m_levels.takeLast();
    auto changesSinceSave = current.changesSinceSave();

    restoreValue(changesSinceSave, level.pushedValues, Change::FillBrush, m_fillBrush, current.m_fillBrush);
    restoreValue(changesSinceSave, level.pushedValues, Change::StrokeBrush, m_strokeBrush, current.m_strokeBrush);
    restoreValue(changesSinceSave, level.pushedValues, Change::StrokeThickness, m_strokeThickness, current.m_strokeThickness);
    restoreValue(changesSinceSave, level.pushedValues, Change::FillRule, m_fillRule, current.m_fillRule);
    restoreValue(changesSinceSave, level.pushedValues, Change::StrokeStyle, m_strokeStyle, current.m_strokeStyle);
    restoreValue(changesSinceSave, level.pushedValues, Change::CompositeMode, m_compositeMode, current.m_compositeMode);
    restoreValue(changesSinceSave, level.pushedValues, Change::DropShadow, m_dropShadow, current.m_dropShadow);
    restoreValue(changesSinceSave, level.pushedValues, Change::Style, m_style, current.m_style);
    restoreValue(changesSinceSave, level.pushedValues, Change::Alpha, m_alpha, current.m_alpha);
    if (changesSinceSave.containsAny(smallChanges)) {
        bool pushed = level.pushedValues.containsAny(smallChanges);
        applySmallProperties(current, pushed ? m_smallProperties.takeLast() : m_smallProperties.last());
    } else if (level.pushedValues.containsAny(smallChanges))
        m_smallProperties.removeLast();

    // The properties the level below had changed are the ones it pushed, and it will push them again
    // if it saves again.
    current.m_changesSinceSave = level.pushedValues;
    current.m_purpose = level.purpose;

    if (m_levels.isEmpty()) {
        // The outermost level pushed every property and popped them all, so nothing is left. Make
        // sure we deallocate the state stack buffers. Canvas elements will immediately save() again,
        // but that goes into inline capacity.
        ASSERT(m_fillBrush.isEmpty());
        ASSERT(m_strokeBrush.isEmpty());
        ASSERT(m_strokeThickness.isEmpty());
        ASSERT(m_fillRule.isEmpty());
        ASSERT(m_strokeStyle.isEmpty());
        ASSERT(m_compositeMode.isEmpty());
        ASSERT(m_dropShadow.isEmpty());
        ASSERT(m_style.isEmpty());
        ASSERT(m_alpha.isEmpty());
        ASSERT(m_smallProperties.isEmpty());

        m_levels.clear();
        m_fillBrush.clear();
        m_strokeBrush.clear();
        m_strokeThickness.clear();
        m_fillRule.clear();
        m_strokeStyle.clear();
        m_compositeMode.clear();
        m_dropShadow.clear();
        m_style.clear();
        m_alpha.clear();
        m_smallProperties.clear();
    }
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
