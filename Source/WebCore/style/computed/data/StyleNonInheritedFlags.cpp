/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "StyleNonInheritedFlags.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

static_assert(PublicPseudoIDBits == allPublicPseudoElementTypes.size());
// Value zero is used to indicate no pseudo-element.
static_assert(!((std::to_underlying(PseudoElementType::HighestEnumValue) + 1) >> PseudoElementTypeBits));

#if !LOG_DISABLED
void NonInheritedFlags::dumpDifferences(TextStream& ts, const NonInheritedFlags& other) const
{
    if (*this == other)
        return;

    LOG_IF_DIFFERENT_WITH_CAST(Style::DisplayType, display);
    LOG_IF_DIFFERENT_WITH_CAST(Style::DisplayType, originalDisplay);
    LOG_IF_DIFFERENT_WITH_CAST(Overflow, overflowX);
    LOG_IF_DIFFERENT_WITH_CAST(Overflow, overflowY);
    LOG_IF_DIFFERENT_WITH_CAST(Clear, clear);
    LOG_IF_DIFFERENT_WITH_CAST(PositionType, position);
    LOG_IF_DIFFERENT_WITH_CAST(UnicodeBidi, unicodeBidi);
    LOG_IF_DIFFERENT_WITH_CAST(Float, floating);

    LOG_IF_DIFFERENT(usesViewportUnits);
    LOG_IF_DIFFERENT(usesContainerUnits);
    LOG_IF_DIFFERENT(useTreeCountingFunctions);

    LOG_IF_DIFFERENT_WITH_FROM_RAW(TextDecorationLine, textDecorationLine);

    LOG_IF_DIFFERENT(hasExplicitlyInheritedProperties);
    LOG_IF_DIFFERENT(disallowsFastPathInheritance);

    LOG_IF_DIFFERENT(emptyState);
    LOG_IF_DIFFERENT(firstChildState);
    LOG_IF_DIFFERENT(lastChildState);
    LOG_IF_DIFFERENT(isLink);

    LOG_IF_DIFFERENT_WITH_CAST(PseudoId, pseudoElementType);
    LOG_IF_DIFFERENT_WITH_CAST(unsigned, pseudoBits);
}
#endif

} // namespace Style
} // namespace WebCore
