/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "TextSizeAdjustment.h"

#if ENABLE(TEXT_AUTOSIZING)

#include "RenderStyle.h"

namespace WebCore {

AutosizeStatus::AutosizeStatus(OptionSet<Fields> fields)
    : m_fields(fields)
{
}

bool AutosizeStatus::contains(Fields fields) const
{
    return m_fields.contains(fields);
}

AutosizeStatus AutosizeStatus::updateStatus(RenderStyle& style)
{
    OptionSet<Fields> result = style.autosizeStatus().fields();
    if (style.hasOutOfFlowPosition())
        result.add(Fields::FoundOutOfFlowPosition);
    switch (style.display()) {
    case DisplayType::InlineBlock:
        result.add(Fields::FoundInlineBlock);
        break;
    case DisplayType::None:
        result.add(Fields::FoundDisplayNone);
        break;
    default: // FIXME: Add more cases.
        break;
    }
    if (style.height().isFixed())
        result.add(Fields::FoundFixedHeight);
    style.setAutosizeStatus(result);
    return result;
}

bool AutosizeStatus::shouldSkipSubtree() const
{
    return m_fields.containsAny({ Fields::FoundOutOfFlowPosition, Fields::FoundInlineBlock, Fields::FoundFixedHeight, Fields::FoundDisplayNone });
}

float AutosizeStatus::idempotentTextSize(float specifiedSize, float pageScale)
{
    // This describes a piecewise curve when the page scale is 2/3.
    FloatPoint points[] = { {0.0f, 0.0f}, {6.0f, 9.0f}, {14.0f, 17.0f} };

    // When the page scale is 1, the curve should be the identity.
    // Linearly interpolate between the curve above and identity based on the page scale.
    // Beware that depending on the specific values picked in the curve, this interpolation might change the shape of the curve for very small pageScales.
    pageScale = std::min(std::max(pageScale, 0.5f), 1.0f);
    auto scalePoint = [&](FloatPoint point) {
        float fraction = 3.0f - 3.0f * pageScale;
        point.setY(point.x() + (point.y() - point.x()) * fraction);
        return point;
    };

    if (specifiedSize <= 0)
        return 0;

    float result = scalePoint(points[WTF_ARRAY_LENGTH(points) - 1]).y();
    for (size_t i = 1; i < WTF_ARRAY_LENGTH(points); ++i) {
        if (points[i].x() < specifiedSize)
            continue;
        auto leftPoint = scalePoint(points[i - 1]);
        auto rightPoint = scalePoint(points[i]);
        float fraction = (specifiedSize - leftPoint.x()) / (rightPoint.x() - leftPoint.x());
        result = leftPoint.y() + fraction * (rightPoint.y() - leftPoint.y());
        break;
    }

    return std::max(std::round(result), specifiedSize);
}

}

#endif
