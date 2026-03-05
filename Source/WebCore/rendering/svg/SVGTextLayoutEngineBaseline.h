/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "FontCascade.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class RenderElement;
class RenderSVGInlineText;
class RenderStyle;
class SVGTextMetrics;

enum class AlignmentBaseline : uint8_t;

// Helper class used by SVGTextLayoutEngine to handle 'alignment-baseline' / 'dominant-baseline' and 'baseline-shift'.
class SVGTextLayoutEngineBaseline {
    WTF_MAKE_NONCOPYABLE(SVGTextLayoutEngineBaseline);
public:
    SVGTextLayoutEngineBaseline(const FontCascade&);

    float calculateBaselineShift(const RenderStyle&) const;
    float calculateAlignmentBaselineShift(bool isVerticalText, const RenderSVGInlineText& textRenderer) const;
    float calculateGlyphOrientationAngle(bool isVerticalText, const RenderStyle&, const char16_t& character) const;
    float calculateGlyphAdvanceAndOrientation(bool isVerticalText, const SVGTextMetrics&, float angle, float& xOrientationShift, float& yOrientationShift) const;

private:
    AlignmentBaseline NODELETE dominantBaselineToAlignmentBaseline(bool isVerticalText, const RenderElement& textRenderer) const;

    CheckedRef<const FontCascade> m_font;
};

} // namespace WebCore
