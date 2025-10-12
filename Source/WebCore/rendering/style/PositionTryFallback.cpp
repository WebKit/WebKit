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
#include "PositionTryFallback.h"

#include "StyleProperties.h"
#include "StylePropertiesInlines.h"

namespace WebCore {
namespace Style {

PositionTryFallback::~PositionTryFallback() = default;
bool operator==(const PositionTryFallback& lhs, const PositionTryFallback& rhs)
{
    RefPtr lhsPositionAreaProperties = lhs.positionAreaProperties;
    RefPtr rhsPositionAreaProperties = rhs.positionAreaProperties;

    if (lhsPositionAreaProperties && rhsPositionAreaProperties) {
        if (lhsPositionAreaProperties == rhsPositionAreaProperties)
            return true;

        auto lhsPositionArea = lhsPositionAreaProperties->getPropertyCSSValue(CSSPropertyPositionArea);
        ASSERT(lhsPositionArea);

        auto rhsPositionArea = rhsPositionAreaProperties->getPropertyCSSValue(CSSPropertyPositionArea);
        ASSERT(rhsPositionArea);

        return *lhsPositionArea == *rhsPositionArea;
    }

    if (!lhsPositionAreaProperties && !rhsPositionAreaProperties)
        return lhs.positionTryRuleName == rhs.positionTryRuleName && lhs.tactics == rhs.tactics;

    // If we got here, lhs and rhs don't have the same type (e.g comparing position-area with rule + tactics)
    return false;
}

TextStream& operator<<(TextStream& ts, const PositionTryFallback::Tactic& tactic)
{
    switch (tactic) {
    case PositionTryFallback::Tactic::FlipBlock:
        ts << "flip-block"_s;
        break;
    case PositionTryFallback::Tactic::FlipInline:
        ts << "flip-inline"_s;
        break;
    case PositionTryFallback::Tactic::FlipStart:
        ts << "flip-start"_s;
        break;
    }

    return ts;
}

TextStream& operator<<(TextStream& ts, const PositionTryFallback& fallback)
{
    ts << "(";

    if (RefPtr positionAreaProperties = fallback.positionAreaProperties) {
        ts << "type: PositionArea ";

        auto positionAreaString = positionAreaProperties->getPropertyValue(CSSPropertyPositionArea);
        ASSERT(!positionAreaString.isEmpty());
        ts << "positionArea: " << positionAreaString;
    } else {
        ts << "type: RuleAndTactic ";

        if (fallback.positionTryRuleName)
            ts << "ruleName: " << *fallback.positionTryRuleName << " ";

        ts << "tactics: [";
        auto separator = ""_s;
        for (const auto& tactic : fallback.tactics)
            ts << std::exchange(separator, ", "_s) << tactic;
        ts << "]";
    }

    ts << ")";
    return ts;
}

TextStream& operator<<(TextStream& ts, const Vector<PositionTryFallback>& positionTryFallbacks)
{
    if (positionTryFallbacks.isEmpty()) {
        ts << "none"_s;
        return ts;
    }
    auto separator = ""_s;
    for (auto& item : positionTryFallbacks) {
        ts << std::exchange(separator, ", "_s);
        ts << item;
    }
    return ts;
}

}
}
