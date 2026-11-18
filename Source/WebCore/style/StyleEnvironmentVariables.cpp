/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "StyleEnvironmentVariables.h"

#include "CSSCustomPropertyValue.h"
#include "CSSParserTokenRange.h"
#include "CSSVariableData.h"
#include "DocumentPage.h"
#include "ElementInlines.h"
#include "Settings.h"
#include "StyleCustomProperty.h"
#include "StyleDocumentScope.h"
#include "StyleLinkParameters.h"
#include "StyleScope.h"
#include "ViewportSegments.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/ZippedRange.h>

namespace WebCore {
namespace Style {

WTF_MAKE_TZONE_ALLOCATED_IMPL(EnvironmentVariables);

EnvironmentVariables::EnvironmentVariables(Document& document)
    : m_document(document)
{
}

const EnvironmentVariables::Values& EnvironmentVariables::userAgentDefinedValues() const
{
    if (!m_values) {
        const_cast<EnvironmentVariables&>(*this).buildValues();
    }
    return *m_values;
}

RefPtr<const CustomProperty> EnvironmentVariables::valueForName(const AtomString& name, std::span<const unsigned> indices) const
{
    if (indices.empty()) {
        if (auto value = m_linkParameterValues.get(name))
            return value;
    }

    auto it = userAgentDefinedValues().find(name);
    if (it == userAgentDefinedValues().end())
        return nullptr;

    // The number of indices must match the variable's dimensions, and each index must be in bounds.
    auto& variable = it->value;
    if (indices.size() != variable.dimensions.size())
        return nullptr;

    size_t flatIndex = 0;
    for (auto [index, dimension] : zippedRange(indices, variable.dimensions)) {
        if (index >= dimension)
            return nullptr;
        flatIndex = flatIndex * dimension + index;
    }

    ASSERT(flatIndex < variable.values.size());
    return variable.values[flatIndex].ptr();
}

const AtomString& EnvironmentVariables::nameForVariable(UADefinedVariable variable) const
{
    static MainThreadNeverDestroyed<const AtomString> safeAreaInsetTopName("safe-area-inset-top"_s);
    static MainThreadNeverDestroyed<const AtomString> safeAreaInsetRightName("safe-area-inset-right"_s);
    static MainThreadNeverDestroyed<const AtomString> safeAreaInsetBottomName("safe-area-inset-bottom"_s);
    static MainThreadNeverDestroyed<const AtomString> safeAreaInsetLeftName("safe-area-inset-left"_s);
    static MainThreadNeverDestroyed<const AtomString> fullscreenInsetTopName("fullscreen-inset-top"_s);
    static MainThreadNeverDestroyed<const AtomString> fullscreenInsetLeftName("fullscreen-inset-left"_s);
    static MainThreadNeverDestroyed<const AtomString> fullscreenInsetBottomName("fullscreen-inset-bottom"_s);
    static MainThreadNeverDestroyed<const AtomString> fullscreenInsetRightName("fullscreen-inset-right"_s);
    static MainThreadNeverDestroyed<const AtomString> fullscreenAutoHideDurationName("fullscreen-auto-hide-duration"_s);

    switch (variable) {
    case UADefinedVariable::SafeAreaInsetTop:
        return safeAreaInsetTopName;
    case UADefinedVariable::SafeAreaInsetRight:
        return safeAreaInsetRightName;
    case UADefinedVariable::SafeAreaInsetBottom:
        return safeAreaInsetBottomName;
    case UADefinedVariable::SafeAreaInsetLeft:
        return safeAreaInsetLeftName;
    case UADefinedVariable::FullscreenInsetTop:
        return fullscreenInsetTopName;
    case UADefinedVariable::FullscreenInsetLeft:
        return fullscreenInsetLeftName;
    case UADefinedVariable::FullscreenInsetBottom:
        return fullscreenInsetBottomName;
    case UADefinedVariable::FullscreenInsetRight:
        return fullscreenInsetRightName;
    case UADefinedVariable::FullscreenAutoHideDuration:
        return fullscreenAutoHideDurationName;
    }

    return nullAtom();
}

void EnvironmentVariables::setValueForVariable(UADefinedVariable variable, Ref<CSSVariableData>&& data)
{
    if (!m_values)
        buildValues();

    auto& name = nameForVariable(variable);
    m_values->set(name, Variable { { }, { CustomProperty::createForVariableData(name, WTF::move(data)) } });
}

void EnvironmentVariables::setLinkParameters(const LinkParameters& parameters)
{
    m_linkParameterValues.clear();

    for (auto& parameter : parameters) {
        auto& name = parameter->spec.name();
        auto set = [&](Ref<CSSVariableData>&& value) {
            m_linkParameterValues.set(name, CustomProperty::createForVariableData(name, WTF::move(value)));
        };

        WTF::switchOn(parameter->resolved,
            [&](LinkParameter::Unresolved) { set(parameter->value.value.copyRef()); },
            [](LinkParameter::Invalid) { },
            [&](const Ref<CSSVariableData>& resolved) { set(resolved.copyRef()); }
        );
    }

    Ref<Document> document = m_document.get();
    document->styleScope().invalidateMatchedDeclarationsCache();
    if (RefPtr documentElement = document->documentElement())
        documentElement->invalidateStyleForSubtree();
}

void EnvironmentVariables::buildValues()
{
    m_values = Values { };

    updateSafeAreaInsetVariables();
    updateFullscreenVariables();
    updateViewportSegmentVariables();
}

static Ref<CSSVariableData> variableDataForPositivePixelLength(float lengthInPx)
{
    ASSERT(lengthInPx >= 0);

    CSSParserToken token(lengthInPx, NumberValueType, NoSign, { });
    token.convertToDimensionWithUnit(CSSUnitType::Px);

    Vector<CSSParserToken> tokens { token };
    CSSParserTokenRange tokenRange(tokens);
    return CSSVariableData::create(tokenRange);
}

static Ref<CSSVariableData> variableDataForPositiveDuration(Seconds durationInSeconds)
{
    ASSERT(durationInSeconds >= 0_s);

    CSSParserToken token(durationInSeconds.value(), NumberValueType, NoSign, { });
    token.convertToDimensionWithUnit(CSSUnitType::S);

    Vector<CSSParserToken> tokens { token };
    CSSParserTokenRange tokenRange(tokens);
    return CSSVariableData::create(tokenRange);
}

void EnvironmentVariables::updateSafeAreaInsetVariables()
{
    RefPtr page = m_document->page();
    FloatBoxExtent unobscuredSafeAreaInsets = page ? page->unobscuredSafeAreaInsets() : FloatBoxExtent();
    setValueForVariable(UADefinedVariable::SafeAreaInsetTop, variableDataForPositivePixelLength(unobscuredSafeAreaInsets.top()));
    setValueForVariable(UADefinedVariable::SafeAreaInsetRight, variableDataForPositivePixelLength(unobscuredSafeAreaInsets.right()));
    setValueForVariable(UADefinedVariable::SafeAreaInsetBottom, variableDataForPositivePixelLength(unobscuredSafeAreaInsets.bottom()));
    setValueForVariable(UADefinedVariable::SafeAreaInsetLeft, variableDataForPositivePixelLength(unobscuredSafeAreaInsets.left()));
}

void EnvironmentVariables::didChangeSafeAreaInsets()
{
    updateSafeAreaInsetVariables();
    protect(m_document)->invalidateMatchedPropertiesCacheAndForceStyleRecalc();
}

void EnvironmentVariables::updateFullscreenVariables()
{
    RefPtr page = m_document->page();
    FloatBoxExtent fullscreenInsets = page ? page->fullscreenInsets() : FloatBoxExtent();
    setValueForVariable(UADefinedVariable::FullscreenInsetTop, variableDataForPositivePixelLength(fullscreenInsets.top()));
    setValueForVariable(UADefinedVariable::FullscreenInsetRight, variableDataForPositivePixelLength(fullscreenInsets.right()));
    setValueForVariable(UADefinedVariable::FullscreenInsetBottom, variableDataForPositivePixelLength(fullscreenInsets.bottom()));
    setValueForVariable(UADefinedVariable::FullscreenInsetLeft, variableDataForPositivePixelLength(fullscreenInsets.left()));

    Seconds fullscreenAutoHideDuration = page ? page->fullscreenAutoHideDuration() : 0_s;
    setValueForVariable(UADefinedVariable::FullscreenAutoHideDuration, variableDataForPositiveDuration(fullscreenAutoHideDuration));
}

void EnvironmentVariables::didChangeFullscreenInsets()
{
    updateFullscreenVariables();
    protect(m_document)->invalidateMatchedPropertiesCacheAndForceStyleRecalc();
}

void EnvironmentVariables::setFullscreenAutoHideDuration(Seconds duration)
{
    setValueForVariable(UADefinedVariable::FullscreenAutoHideDuration, variableDataForPositiveDuration(duration));
    protect(m_document)->invalidateMatchedPropertiesCacheAndForceStyleRecalc();
}

void EnvironmentVariables::updateViewportSegmentVariables()
{
    if (!m_values)
        return;

    struct ViewportSegmentVariable {
        ASCIILiteral name;
        float (*length)(const FloatRect&);
    };
    static constexpr std::array viewportSegmentVariables {
        ViewportSegmentVariable { "viewport-segment-width"_s, [](const FloatRect& rect) { return rect.width(); } },
        ViewportSegmentVariable { "viewport-segment-height"_s, [](const FloatRect& rect) { return rect.height(); } },
        ViewportSegmentVariable { "viewport-segment-top"_s, [](const FloatRect& rect) { return rect.y(); } },
        ViewportSegmentVariable { "viewport-segment-right"_s, [](const FloatRect& rect) { return rect.maxX(); } },
        ViewportSegmentVariable { "viewport-segment-bottom"_s, [](const FloatRect& rect) { return rect.maxY(); } },
        ViewportSegmentVariable { "viewport-segment-left"_s, [](const FloatRect& rect) { return rect.x(); } },
    };

    for (auto& variable : viewportSegmentVariables)
        m_values->remove(AtomString { variable.name });

    if (!m_document->settings().viewportSegmentsAPIEnabled())
        return;

    RefPtr page = m_document->page();
    if (!page)
        return;

    const auto& segments = page->viewportSegments();

    // Not segmented; env() variables are undefined.
    if (segments.columns == 1 && segments.rows == 1)
        return;

    // The segments come from the UI process; ignore them if they are inconsistent.
    if (segments.segmentRects.size() != static_cast<size_t>(segments.columns) * segments.rows)
        return;

    // https://drafts.csswg.org/css-env-1/#viewport-segments
    // The variables are two-dimensional, indexed by column then row, e.g. env(viewport-segment-top 1 0).
    for (auto& [literal, length] : viewportSegmentVariables) {
        AtomString name { literal };
        Variable variable { { segments.columns, segments.rows }, { } };
        variable.values.reserveInitialCapacity(segments.segmentRects.size());
        for (unsigned x = 0; x < segments.columns; ++x) {
            for (unsigned y = 0; y < segments.rows; ++y) {
                // segmentRects is row-major.
                auto& rect = segments.segmentRects[y * segments.columns + x];
                variable.values.append(CustomProperty::createForVariableData(name, variableDataForPositivePixelLength(length(rect))));
            }
        }
        m_values->set(name, WTF::move(variable));
    }
}

void EnvironmentVariables::didChangeViewportSegments()
{
    updateViewportSegmentVariables();
    protect(m_document)->invalidateMatchedPropertiesCacheAndForceStyleRecalc();
}

}
}
