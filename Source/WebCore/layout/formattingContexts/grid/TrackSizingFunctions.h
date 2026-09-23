/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#include "StyleGridTrackBreadth.h"
#include "StyleGridTrackSize.h"
#include "StyleZoomPrimitives.h"

namespace WebCore {
namespace Layout {

// https://drafts.csswg.org/css-grid-2/#typedef-track-size
// fit-content() is only ever a max track sizing function.
class MaxTrackSizingFunction {
public:
    MaxTrackSizingFunction(Style::GridTrackBreadth breadth)
        : m_value(WTF::move(breadth)) { }

    MaxTrackSizingFunction(Style::GridTrackSize::FitContent fitContent)
        : m_value(WTF::move(fitContent)) { }

    FORWARD_VARIANT_FUNCTIONS(MaxTrackSizingFunction, m_value)

    // Absent for a fit-content() maximum.
    std::optional<Style::GridTrackBreadth> tryBreadth() const
    {
        if (auto* breadth = std::get_if<Style::GridTrackBreadth>(&m_value))
            return *breadth;
        return { };
    }

    bool isAuto() const
    {
        auto breadth = tryBreadth();
        return breadth && breadth->isAuto();
    }

    bool isFlex() const
    {
        auto breadth = tryBreadth();
        return breadth && breadth->isFlex();
    }

    Style::GridTrackBreadth::Flex flex() const
    {
        ASSERT(isFlex());
        return tryBreadth()->flex();
    }

    bool isFitContent() const
    {
        return std::holds_alternative<Style::GridTrackSize::FitContent>(m_value);
    }

    Style::GridTrackSize::FitContent fitContent() const
    {
        ASSERT(isFitContent());
        return std::get<Style::GridTrackSize::FitContent>(m_value);
    }

    bool isContentSized() const
    {
        auto breadth = tryBreadth();
        return !breadth || breadth->isContentSized();
    }

private:
    Variant<Style::GridTrackBreadth, Style::GridTrackSize::FitContent> m_value;
};

struct TrackSizingFunctions {
    // https://drafts.csswg.org/css-grid-1/#extra-space
    // The resolved fit-content() argument, which a fit-content() track may not grow past.
    std::optional<LayoutUnit> fitContentLimit(LayoutUnit availableSpace) const
    {
        if (!max.isFitContent())
            return { };
        auto fitContent = max.fitContent();
        if (auto fixedArgument = fitContent->value.tryFixed())
            return Style::evaluate<LayoutUnit>(*fixedArgument, zoom);
        return Style::evaluate<LayoutUnit>(fitContent->value, availableSpace, zoom);
    }

    Style::GridTrackBreadth min { CSS::Keyword::Auto { } };
    MaxTrackSizingFunction max { Style::GridTrackBreadth { CSS::Keyword::Auto { } } };
    Style::ZoomFactor zoom;
};

} // namespace Layout
} // namespace WebCore
