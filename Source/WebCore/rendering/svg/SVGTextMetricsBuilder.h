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

namespace WebCore {

class ComplexTextController;
class RenderElement;
class RenderSVGInlineText;
class RenderSVGText;
struct MeasureTextData;
struct WidthIterator;

class SVGTextMetricsBuilder {
    WTF_MAKE_NONCOPYABLE(SVGTextMetricsBuilder);
public:
    SVGTextMetricsBuilder();
    void measureTextRenderer(RenderSVGText&, RenderSVGInlineText* stopAtLeaf);
    void buildMetricsAndLayoutAttributes(RenderSVGText&, RenderSVGInlineText* stopAtLeaf, SVGCharacterDataMap& allCharactersMap);

private:
    template<typename Iterator> bool advance(Iterator&);
    void advanceIterator(WidthIterator&);
    void advanceIterator(ComplexTextController&);
    bool NODELETE currentCharacterStartsSurrogatePair() const;

    void initializeMeasurementWithTextRenderer(RenderSVGInlineText&);
    void walkTree(RenderElement&, RenderSVGInlineText* stopAtLeaf, MeasureTextData&);
    std::tuple<unsigned, char16_t> measureTextRenderer(RenderSVGInlineText&, const MeasureTextData&, std::tuple<unsigned, char16_t>);

    template<typename Iterator> std::tuple<unsigned, char16_t> measureTextRendererWithIterator(Iterator&, RenderSVGInlineText&, const MeasureTextData&, std::tuple<unsigned, char16_t>);

    SingleThreadWeakPtr<RenderSVGInlineText> m_text;
    TextRun m_run;
    unsigned m_textPosition { 0 };
    bool m_isComplexText { false };
    bool m_canUseSimplifiedTextMeasuring { false };
    SVGTextMetrics m_currentMetrics;
    float m_totalWidth { 0 };

    // Complex text only.
    SVGTextMetrics m_complexStartToCurrentMetrics;
};

} // namespace WebCore
