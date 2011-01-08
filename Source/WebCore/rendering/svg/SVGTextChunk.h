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

#ifndef SVGTextChunk_h
#define SVGTextChunk_h

#if ENABLE(SVG)
#include "SVGRenderStyleDefs.h"
#include "SVGTextContentElement.h"

namespace WebCore {

class SVGInlineTextBox;

// A SVGTextChunk describes a range of SVGTextFragments, see the SVG spec definition of a "text chunk".
class SVGTextChunk {
public:
    SVGTextChunk(bool isVerticalText, ETextAnchor, SVGTextContentElement::SVGLengthAdjustType, float desiredTextLength);

    void calculateLength(float& length, unsigned& characters) const;
    float calculateTextAnchorShift(float length) const;

    bool isVerticalText() const { return m_isVerticalText; }
    ETextAnchor textAnchor() const { return m_textAnchor; }
    SVGTextContentElement::SVGLengthAdjustType lengthAdjust() const { return m_lengthAdjust; }
    float desiredTextLength() const { return m_desiredTextLength; }

    Vector<SVGInlineTextBox*>& boxes() { return m_boxes; }
    const Vector<SVGInlineTextBox*>& boxes() const { return m_boxes; }

    bool hasDesiredTextLength() const { return m_lengthAdjust != SVGTextContentElement::LENGTHADJUST_UNKNOWN && m_desiredTextLength > 0; } 
    bool hasTextAnchor() const { return m_textAnchor != TA_START; }

private:
    // Contains all SVGInlineTextBoxes this chunk spans.
    Vector<SVGInlineTextBox*> m_boxes;

    // writing-mode specific property.
    bool m_isVerticalText;

    // text-anchor specific properties.
    ETextAnchor m_textAnchor;

    // textLength/lengthAdjust specific properties.
    SVGTextContentElement::SVGLengthAdjustType m_lengthAdjust;
    float m_desiredTextLength;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
