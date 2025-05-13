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

#include "config.h"
#include "DropShadowFilterOperationWithStyleColor.h"

#include "AnimationUtilities.h"
#include "ColorBlending.h"
#include "RenderStyle.h"

namespace WebCore {
namespace Style {

Ref<DropShadowFilterOperation> DropShadowFilterOperationWithStyleColor::createEquivalentWithResolvedColor(const RenderStyle& style)
{
    auto resolvedColor = m_styleColor.resolveColor(style.color());
    return DropShadowFilterOperation::create(location(), stdDeviation(), resolvedColor);
}

bool DropShadowFilterOperationWithStyleColor::operator==(const FilterOperation& operation) const
{
    if (!isSameType(operation))
        return false;
    auto& other = downcast<DropShadowFilterOperationWithStyleColor>(operation);
    return nonColorEqual(other) && m_styleColor == other.m_styleColor;
}

RefPtr<FilterOperation> DropShadowFilterOperationWithStyleColor::blend(const FilterOperation* from, const BlendingContext& context, bool blendToPassthrough)
{
    if (from && !from->isSameType(*this))
        return this;

    auto toColor = m_styleColor.resolveColor(context.toCurrentColor);

    if (blendToPassthrough) {
        return DropShadowFilterOperationWithStyleColor::create(
            WebCore::blend(m_location, IntPoint(), context),
            WebCore::blend(m_stdDeviation, 0, context),
            WebCore::blend(toColor, WebCore::Color::transparentBlack, context));
    }

    auto* fromOperation = downcast<DropShadowFilterOperationWithStyleColor>(from);
    IntPoint fromLocation = fromOperation ? fromOperation->location() : IntPoint();
    int fromStdDeviation = fromOperation ? fromOperation->stdDeviation() : 0;

    auto fromColor = fromOperation ? fromOperation->styleColor().resolveColor(context.fromCurrentColor) : WebCore::Color::transparentBlack;

    return DropShadowFilterOperationWithStyleColor::create(
        WebCore::blend(fromLocation, m_location, context),
        std::max(WebCore::blend(fromStdDeviation, m_stdDeviation, context), 0),
        WebCore::blend(fromColor, toColor, context));
}

void DropShadowFilterOperationWithStyleColor::dump(TextStream& ts) const
{
    ts << "drop-shadow("_s << x() << ' ' << y() << ' ' << location() << ' ';
    ts << styleColor() << ')';
}

}
}
