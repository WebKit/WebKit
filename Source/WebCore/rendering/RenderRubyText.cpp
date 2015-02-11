/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "RenderRubyText.h"
#include "RenderRubyRun.h"

namespace WebCore {

RenderRubyText::RenderRubyText(Element& element, PassRef<RenderStyle> style)
    : RenderBlockFlow(element, WTF::move(style))
{
}

RenderRubyText::~RenderRubyText()
{
}

RenderRubyRun* RenderRubyText::rubyRun() const
{
    ASSERT(parent());
    return toRenderRubyRun(parent());
}

bool RenderRubyText::isChildAllowed(const RenderObject& child, const RenderStyle&) const
{
    return child.isInline();
}

ETextAlign RenderRubyText::textAlignmentForLine(bool endsWithSoftBreak) const
{
    ETextAlign textAlign = style().textAlign();
    // FIXME: This check is bogus since user can set the initial value.
    if (textAlign != RenderStyle::initialTextAlign())
        return RenderBlockFlow::textAlignmentForLine(endsWithSoftBreak);

    // The default behavior is to allow ruby text to expand if it is shorter than the ruby base.
    return JUSTIFY;
}

void RenderRubyText::adjustInlineDirectionLineBounds(int expansionOpportunityCount, float& logicalLeft, float& logicalWidth) const
{
    ETextAlign textAlign = style().textAlign();
    // FIXME: This check is bogus since user can set the initial value.
    if (textAlign != RenderStyle::initialTextAlign())
        return RenderBlockFlow::adjustInlineDirectionLineBounds(expansionOpportunityCount, logicalLeft, logicalWidth);

    int maxPreferredLogicalWidth = this->maxPreferredLogicalWidth();
    if (maxPreferredLogicalWidth >= logicalWidth)
        return;

    // Inset the ruby text by half the inter-ideograph expansion amount, but no more than a full-width
    // ruby character on each side.
    float inset = (logicalWidth - maxPreferredLogicalWidth) / (expansionOpportunityCount + 1);
    if (expansionOpportunityCount)
        inset = std::min<float>(2 * style().fontSize(), inset);

    logicalLeft += inset / 2;
    logicalWidth -= inset;
}

bool RenderRubyText::avoidsFloats() const
{
    return true;
}

bool RenderRubyText::canBreakBefore(const LazyLineBreakIterator& iterator) const
{
    // FIXME: It would be nice to improve this so that it isn't just hard-coded, but lookahead in this
    // case is particularly problematic.

    if (!iterator.priorContextLength())
        return true;
    UChar ch = iterator.lastCharacter();
    ULineBreak lineBreak = (ULineBreak)u_getIntPropertyValue(ch, UCHAR_LINE_BREAK);
    // UNICODE LINE BREAKING ALGORITHM
    // http://www.unicode.org/reports/tr14/
    // And Requirements for Japanese Text Layout, 3.1.7 Characters Not Starting a Line
    // http://www.w3.org/TR/2012/NOTE-jlreq-20120403/#characters_not_starting_a_line
    switch (lineBreak) {
    case U_LB_NONSTARTER:
    case U_LB_CLOSE_PARENTHESIS:
    case U_LB_CLOSE_PUNCTUATION:
    case U_LB_EXCLAMATION:
    case U_LB_BREAK_SYMBOLS:
    case U_LB_INFIX_NUMERIC:
    case U_LB_ZWSPACE:
    case U_LB_WORD_JOINER:
        return false;
    default:
        break;
    }
    // Special care for Requirements for Japanese Text Layout
    switch (ch) {
    case 0x2019: // RIGHT SINGLE QUOTATION MARK
    case 0x201D: // RIGHT DOUBLE QUOTATION MARK
    case 0x00BB: // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
    case 0x2010: // HYPHEN
    case 0x2013: // EN DASH
    case 0x300C: // LEFT CORNER BRACKET
        return false;
    default:
        break;
    }
    return true;
}

} // namespace WebCore
