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

#include "RenderStyle.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/PointerComparison.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

bool BorderData::containsCurrentColor() const
{
    return m_edges.anyOf([](const auto& edge) {
        return edge.isVisible() && edge.color().containsCurrentColor();
    });
}

bool BorderData::isEquivalentForPainting(const BorderData& other, bool currentColorDiffers) const
{
    if (this == &other) {
        ASSERT(currentColorDiffers);
        return !containsCurrentColor();
    }

    if (*this != other)
        return false;

    if (!currentColorDiffers)
        return true;

    return !containsCurrentColor();
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

    if (behavior == DumpStyleValues::All || topLeftCornerShape() != Style::CornerShapeValue::round())
        ts.dumpProperty("top-left corner shape"_s, topLeftCornerShape());
    if (behavior == DumpStyleValues::All || topRightCornerShape() != Style::CornerShapeValue::round())
        ts.dumpProperty("top-right corner shape"_s, topRightCornerShape());
    if (behavior == DumpStyleValues::All || bottomLeftCornerShape() != Style::CornerShapeValue::round())
        ts.dumpProperty("bottom-left corner shape"_s, bottomLeftCornerShape());
    if (behavior == DumpStyleValues::All || bottomRightCornerShape() != Style::CornerShapeValue::round())
        ts.dumpProperty("bottom-right corner shape"_s, bottomRightCornerShape());

    ts.dumpProperty("image"_s, image());

    if (behavior == DumpStyleValues::All || !Style::isZero(topLeftRadius()))
        ts.dumpProperty("top-left"_s, topLeftRadius());
    if (behavior == DumpStyleValues::All || !Style::isZero(topRightRadius()))
        ts.dumpProperty("top-right"_s, topRightRadius());
    if (behavior == DumpStyleValues::All || !Style::isZero(bottomLeftRadius()))
        ts.dumpProperty("bottom-left"_s, bottomLeftRadius());
    if (behavior == DumpStyleValues::All || !Style::isZero(bottomRightRadius()))
        ts.dumpProperty("bottom-right"_s, bottomRightRadius());
}

TextStream& operator<<(TextStream& ts, const BorderData& borderData)
{
    borderData.dump(ts);
    return ts;
}

} // namespace WebCore
