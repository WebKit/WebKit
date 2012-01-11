/*
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
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
 */

#ifndef SVGTextMetricsBuilder_h
#define SVGTextMetricsBuilder_h

#if ENABLE(SVG)
#include "SVGTextMetrics.h"
#include <wtf/Vector.h>

namespace WebCore {

class RenderObject;
class RenderSVGInlineText;
struct WidthIterator;

class SVGTextMetricsBuilder {
    WTF_MAKE_NONCOPYABLE(SVGTextMetricsBuilder);
public:
    SVGTextMetricsBuilder();
    void measureAllCharactersOfRenderer(RenderSVGInlineText*, Vector<SVGTextMetrics>&);

private:
    void advance();
    void advanceSimpleText(bool startsSurrogatePair);
    void advanceComplexText(bool startsSurrogatePair);
    void measureTextRenderer(RenderSVGInlineText*, const UChar*& lastCharacter, Vector<SVGTextMetrics>*);
    void walkTreeUntilSpecificLeafIsReached(RenderObject*, RenderSVGInlineText* stopAtElement, const UChar*& lastCharacter, Vector<SVGTextMetrics>&);

    RenderSVGInlineText* m_text;
    const UChar* m_characters;
    unsigned m_textLength;
    unsigned m_textPosition;
    bool m_finished;
    bool m_isComplexText;
    bool m_preserveWhiteSpace;
    SVGTextMetrics m_currentMetrics;
    float m_totalWidth;

    // Simple text only.
    OwnPtr<WidthIterator> m_simpleWidthIterator;

    // Complex text only.
    SVGTextMetrics m_complexStartToCurrentMetrics;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
