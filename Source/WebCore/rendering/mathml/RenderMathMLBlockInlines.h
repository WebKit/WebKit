/**
 * Copyright (C) 2010 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2012 David Barton (dbarton@mathscribe.com). All rights reserved.
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

#pragma once

#if ENABLE(MATHML)

#include "RenderBoxInlines.h"
#include "RenderMathMLBlock.h"
#include "RenderTableInlines.h"
#include "StyleInheritedData.h"

namespace WebCore {

inline RenderMathMLTable::RenderMathMLTable(MathMLElement& element, RenderStyle&& style)
    : RenderTable(Type::MathMLTable, element, WTFMove(style))
    , m_mathMLStyle(MathMLStyle::create())
{
    ASSERT(isRenderMathMLTable());
}

inline LayoutUnit RenderMathMLBlock::ascentForChild(const RenderBox& child)
{
    return child.firstLineBaseline().value_or(child.logicalHeight().toInt());
}

inline LayoutUnit RenderMathMLBlock::mirrorIfNeeded(LayoutUnit horizontalOffset, const RenderBox& child) const
{
    return mirrorIfNeeded(horizontalOffset, child.logicalWidth());
}

inline LayoutUnit RenderMathMLBlock::ruleThicknessFallback() const
{
    // This function returns a value for the default rule thickness (TeX's \xi_8) to be used as a fallback when we lack a MATH table.
    // This arbitrary value of 0.05em was used in early WebKit MathML implementations for the thickness of the fraction bars.
    // Note that Gecko has a slower but more accurate version that measures the thickness of U+00AF MACRON to be more accurate and otherwise fallback to some arbitrary value.
    return LayoutUnit(0.05f * style().fontCascade().size());
}

} // namespace WebCore

#endif // ENABLE(MATHML)
