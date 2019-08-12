/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 ChangSeok Oh <shivamidow@gmail.com>
 * Copyright (C) 2013 Adobe Systems Inc. All right reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "InlineIterator.h"
#include "LineInfo.h"
#include "LineInlineHeaders.h"
#include <wtf/Vector.h>

namespace WebCore {

class RenderText;
class TextLayout;

struct RenderTextInfo {
    RenderText* text { nullptr };
    std::unique_ptr<TextLayout, TextLayoutDeleter> layout;
    LazyLineBreakIterator lineBreakIterator;
    const FontCascade* font { nullptr };
};

class LineBreaker {
public:
    friend class BreakingContext;

    explicit LineBreaker(RenderBlockFlow& block)
        : m_block(block)
    {
        reset();
    }

    InlineIterator nextLineBreak(InlineBidiResolver&, LineInfo&, RenderTextInfo&, FloatingObject* lastFloatFromPreviousLine, unsigned consecutiveHyphenatedLines, WordMeasurements&);

    bool lineWasHyphenated() { return m_hyphenated; }
    const Vector<RenderBox*>& positionedObjects() { return m_positionedObjects; }
    Clear clear() { return m_clear; }

private:
    void reset();

    void skipTrailingWhitespace(InlineIterator&, const LineInfo&);
    void skipLeadingWhitespace(InlineBidiResolver&, LineInfo&, FloatingObject* lastFloatFromPreviousLine, LineWidth&);

    FloatingObject* insertFloatingObject(RenderBox& floatBox) { return m_block.insertFloatingObject(floatBox); }
    bool positionNewFloatOnLine(const FloatingObject& newFloat, FloatingObject* lastFloatFromPreviousLine, LineInfo& lineInfo, LineWidth& width)
    {
        return m_block.complexLineLayout()->positionNewFloatOnLine(newFloat, lastFloatFromPreviousLine, lineInfo, width);
    }

    RenderBlockFlow& m_block;
    bool m_hyphenated;
    Clear m_clear;
    Vector<RenderBox*> m_positionedObjects;
};

} // namespace WebCore
