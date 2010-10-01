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
class RenderSVGInlineText;
class RenderSVGText;

class SVGTextLayoutAttributesBuilder : public Noncopyable {
public:
    SVGTextLayoutAttributesBuilder();
    void buildLayoutAttributesForTextSubtree(RenderSVGText*);

private:
    struct LayoutScope {
        LayoutScope()
            : isVerticalWritingMode(false)
            , textContentStart(0)
            , textContentLength(0)
        {
        }

        bool isVerticalWritingMode;
        unsigned textContentStart;
        unsigned textContentLength;
        SVGTextLayoutAttributes attributes;
    };

    void buildLayoutScopes(RenderObject*, unsigned& atCharacter);
    void buildLayoutScope(LayoutScope&, RenderObject*, unsigned textContentStart, unsigned textContentLength);
    void buildLayoutAttributesFromScopes();
    void propagateLayoutAttributes(RenderObject*, unsigned& atCharacter);
    void measureCharacters(RenderSVGInlineText*, SVGTextLayoutAttributes&);

private:
    Vector<LayoutScope> m_scopes;
    SVGTextLayoutAttributes m_attributes;
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
