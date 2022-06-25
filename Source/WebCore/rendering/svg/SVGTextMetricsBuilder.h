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

#pragma once

#include "SVGTextLayoutAttributes.h"
#include "TextRun.h"
#include "WidthIterator.h"

namespace WebCore {

class RenderElement;
class RenderSVGInlineText;
class RenderSVGText;
struct MeasureTextData;

class SVGTextMetricsBuilder {
    WTF_MAKE_NONCOPYABLE(SVGTextMetricsBuilder);
public:
    SVGTextMetricsBuilder();
    void measureTextRenderer(RenderSVGInlineText&);
    void buildMetricsAndLayoutAttributes(RenderSVGText&, RenderSVGInlineText* stopAtLeaf, SVGCharacterDataMap& allCharactersMap);

private:
    bool advance();
    void advanceSimpleText();
    void advanceComplexText();
    bool currentCharacterStartsSurrogatePair() const;

    void initializeMeasurementWithTextRenderer(RenderSVGInlineText&);
    void walkTree(RenderElement&, RenderSVGInlineText* stopAtLeaf, MeasureTextData*);
    void measureTextRenderer(RenderSVGInlineText&, MeasureTextData*);

    RenderSVGInlineText* m_text;
    TextRun m_run;
    unsigned m_textPosition;
    bool m_isComplexText;
    SVGTextMetrics m_currentMetrics;
    float m_totalWidth;

    // Simple text only.
    std::unique_ptr<WidthIterator> m_simpleWidthIterator;

    // Complex text only.
    SVGTextMetrics m_complexStartToCurrentMetrics;
};

} // namespace WebCore
