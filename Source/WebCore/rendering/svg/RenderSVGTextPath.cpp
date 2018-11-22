/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "config.h"
#include "RenderSVGTextPath.h"

#include "FloatQuad.h"
#include "RenderBlock.h"
#include "SVGInlineTextBox.h"
#include "SVGNames.h"
#include "SVGPathData.h"
#include "SVGPathElement.h"
#include "SVGRootInlineBox.h"
#include "SVGTextPathElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGTextPath);

RenderSVGTextPath::RenderSVGTextPath(SVGTextPathElement& element, RenderStyle&& style)
    : RenderSVGInline(element, WTFMove(style))
{
}

SVGTextPathElement& RenderSVGTextPath::textPathElement() const
{
    return downcast<SVGTextPathElement>(RenderSVGInline::graphicsElement());
}

Path RenderSVGTextPath::layoutPath() const
{
    auto target = SVGURIReference::targetElementFromIRIString(textPathElement().href(), document());
    if (!is<SVGPathElement>(target.element))
        return Path();

    SVGPathElement& pathElement = downcast<SVGPathElement>(*target.element);

    Path path = pathFromGraphicsElement(&pathElement);

    // Spec:  The transform attribute on the referenced 'path' element represents a
    // supplemental transformation relative to the current user coordinate system for
    // the current 'text' element, including any adjustments to the current user coordinate
    // system due to a possible transform attribute on the current 'text' element.
    // http://www.w3.org/TR/SVG/text.html#TextPathElement
    path.transform(pathElement.animatedLocalTransform());
    return path;
}

float RenderSVGTextPath::startOffset() const
{
    return textPathElement().startOffset().valueAsPercentage();
}

bool RenderSVGTextPath::exactAlignment() const
{
    return textPathElement().spacing() == SVGTextPathSpacingExact;
}

bool RenderSVGTextPath::stretchMethod() const
{
    return textPathElement().method() == SVGTextPathMethodStretch;
}

}
