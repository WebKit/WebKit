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
#include "StyleInheritedFlags.h"

#include "StyleComputedStyle+DifferenceLogging.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"

namespace WebCore {
namespace Style {

static_assert(!(static_cast<unsigned>(maxTextTransformValue) >> TextTransformBits));

#if !LOG_DISABLED
void InheritedFlags::dumpDifferences(TextStream& ts, const InheritedFlags& other) const
{
    if (*this == other)
        return;

    LOG_IF_DIFFERENT(writingMode);

    LOG_IF_DIFFERENT_WITH_CAST(WhiteSpaceCollapse, whiteSpaceCollapse);
    LOG_IF_DIFFERENT_WITH_CAST(TextWrapMode, textWrapMode);
    LOG_IF_DIFFERENT_WITH_CAST(TextAlign, textAlign);
    LOG_IF_DIFFERENT_WITH_CAST(TextWrapStyle, textWrapStyle);

    LOG_IF_DIFFERENT_WITH_FROM_RAW(TextTransform, textTransform);
    LOG_IF_DIFFERENT_WITH_FROM_RAW(TextDecorationLine, textDecorationLineInEffect);

    LOG_IF_DIFFERENT_WITH_CAST(PointerEvents, pointerEvents);
    LOG_IF_DIFFERENT_WITH_CAST(Visibility, visibility);
    LOG_IF_DIFFERENT_WITH_CAST(CursorType, cursorType);

#if ENABLE(CURSOR_VISIBILITY)
    LOG_IF_DIFFERENT_WITH_CAST(CursorVisibility, cursorVisibility);
#endif

    LOG_IF_DIFFERENT_WITH_CAST(ListStylePosition, listStylePosition);
    LOG_IF_DIFFERENT_WITH_CAST(EmptyCell, emptyCells);
    LOG_IF_DIFFERENT_WITH_CAST(BorderCollapse, borderCollapse);
    LOG_IF_DIFFERENT_WITH_CAST(CaptionSide, captionSide);
    LOG_IF_DIFFERENT_WITH_CAST(BoxDirection, boxDirection);
    LOG_IF_DIFFERENT_WITH_CAST(Order, rtlOrdering);
    LOG_IF_DIFFERENT_WITH_CAST(bool, hasExplicitlySetColor);
    LOG_IF_DIFFERENT_WITH_CAST(PrintColorAdjust, printColorAdjust);
    LOG_IF_DIFFERENT_WITH_CAST(InsideLink, insideLink);

#if ENABLE(TEXT_AUTOSIZING)
    LOG_IF_DIFFERENT_WITH_CAST(unsigned, autosizeStatus);
#endif
}
#endif

} // namespace Style
} // namespace WebCore
