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

#ifndef SVGTextLayoutAttributesBuilder_h
#define SVGTextLayoutAttributesBuilder_h

#if ENABLE(SVG)
#include "SVGTextLayoutAttributes.h"
#include <wtf/Vector.h>

namespace WebCore {

class RenderObject;
class RenderSVGText;

// SVGTextLayoutAttributesBuilder performs the first layout phase for SVG text.
//
// It extracts the x/y/dx/dy/rotate values from the SVGTextPositioningElements in the DOM,
// measures all characters in the RenderSVGText subtree and extracts kerning/ligature information.
// These values are propagated to the corresponding RenderSVGInlineText renderers.
// The first layout phase only extracts the relevant information needed in RenderBlockLineLayout
// to create the InlineBox tree based on text chunk boundaries & BiDi information.
// The second layout phase is carried out by SVGTextLayoutEngine.

class SVGTextLayoutAttributesBuilder : public Noncopyable {
public:
    SVGTextLayoutAttributesBuilder();
    void buildLayoutAttributesForTextSubtree(RenderSVGText*);

private:
    struct LayoutScope {
        LayoutScope()
            : textContentStart(0)
            , textContentLength(0)
        {
        }

        unsigned textContentStart;
        unsigned textContentLength;
        SVGTextLayoutAttributes attributes;
    };

    void buildLayoutScope(LayoutScope&, RenderObject*, unsigned textContentStart, unsigned textContentLength) const;
    void buildLayoutScopes(RenderObject*, unsigned& atCharacter, UChar& lastCharacter);
    void buildOutermostLayoutScope(RenderSVGText*, unsigned textLength);
    void propagateLayoutAttributes(RenderObject*, unsigned& atCharacter, UChar& lastCharacter) const;

    enum LayoutValueType {
        XValue,
        YValue,
        DxValue,
        DyValue,
        RotateValue
    };

    float nextLayoutValue(LayoutValueType, unsigned atCharacter) const;
    void assignLayoutAttributesForCharacter(SVGTextLayoutAttributes&, SVGTextMetrics&, unsigned valueListPosition) const;
    void assignEmptyLayoutAttributesForCharacter(SVGTextLayoutAttributes&) const;

private:
    Vector<LayoutScope> m_scopes;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
