/*
* Copyright (C) 2019 Apple Inc. All rights reserved.
* Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "BorderData.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {

BorderData::BorderData()
    : borderImage { Style::BorderImageData::create() }
{
}

bool BorderData::containsCurrentColor() const
{
    return edges.anyOf([](const auto& edge) {
        return edge.isVisible() && edge.color.containsCurrentColor();
    });
}

void BorderData::dump(TextStream& ts, DumpStyleValues behavior) const
{
    if (behavior == DumpStyleValues::All || left() != BorderValue())
        ts.dumpProperty("left"_s, left());
    if (behavior == DumpStyleValues::All || right() != BorderValue())
        ts.dumpProperty("right"_s, right());
    if (behavior == DumpStyleValues::All || top() != BorderValue())
        ts.dumpProperty("top"_s, top());
    if (behavior == DumpStyleValues::All || bottom() != BorderValue())
        ts.dumpProperty("bottom"_s, bottom());

    if (behavior == DumpStyleValues::All || topLeftCornerShape() != Style::CornerShapeValue(CSS::Keyword::Round { }))
        ts.dumpProperty("top-left corner shape"_s, topLeftCornerShape());
    if (behavior == DumpStyleValues::All || topRightCornerShape() != Style::CornerShapeValue(CSS::Keyword::Round { }))
        ts.dumpProperty("top-right corner shape"_s, topRightCornerShape());
    if (behavior == DumpStyleValues::All || bottomLeftCornerShape() != Style::CornerShapeValue(CSS::Keyword::Round { }))
        ts.dumpProperty("bottom-left corner shape"_s, bottomLeftCornerShape());
    if (behavior == DumpStyleValues::All || bottomRightCornerShape() != Style::CornerShapeValue(CSS::Keyword::Round { }))
        ts.dumpProperty("bottom-right corner shape"_s, bottomRightCornerShape());

    if (behavior == DumpStyleValues::All || !Style::isKnownZero(topLeftRadius()))
        ts.dumpProperty("top-left"_s, topLeftRadius());
    if (behavior == DumpStyleValues::All || !Style::isKnownZero(topRightRadius()))
        ts.dumpProperty("top-right"_s, topRightRadius());
    if (behavior == DumpStyleValues::All || !Style::isKnownZero(bottomLeftRadius()))
        ts.dumpProperty("bottom-left"_s, bottomLeftRadius());
    if (behavior == DumpStyleValues::All || !Style::isKnownZero(bottomRightRadius()))
        ts.dumpProperty("bottom-right"_s, bottomRightRadius());

    borderImage->dump(ts, behavior);
}

#if !LOG_DISABLED
void BorderData::dumpDifferences(TextStream& ts, const BorderData& other) const
{
    LOG_IF_DIFFERENT(edges);
    LOG_IF_DIFFERENT(radii);
    LOG_IF_DIFFERENT(cornerShapes);

    borderImage->dumpDifferences(ts, other.borderImage);
}
#endif

TextStream& operator<<(TextStream& ts, const BorderData& borderData)
{
    borderData.dump(ts);
    return ts;
}

} // namespace WebCore
