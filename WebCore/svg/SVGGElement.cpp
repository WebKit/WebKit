/*
    Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGGElement.h"

#include "RenderSVGHiddenContainer.h"
#include "RenderSVGTransformableContainer.h"

namespace WebCore {

SVGGElement::SVGGElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
{
}

SVGGElement::~SVGGElement()
{
}

void SVGGElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (SVGTests::parseMappedAttribute(attr))
        return;
    if (SVGLangSpace::parseMappedAttribute(attr))
        return;
    if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
        return;

    SVGStyledTransformableElement::parseMappedAttribute(attr);
}

void SVGGElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledTransformableElement::svgAttributeChanged(attrName);

    RenderObject* renderer = this->renderer();
    if (!renderer)
        return;

    if (SVGStyledTransformableElement::isKnownAttribute(attrName)) {
        renderer->setNeedsTransformUpdate();
        renderer->setNeedsLayout(true);
        return;
    }

    if (SVGTests::isKnownAttribute(attrName)
        || SVGLangSpace::isKnownAttribute(attrName)
        || SVGExternalResourcesRequired::isKnownAttribute(attrName))
        renderer->setNeedsLayout(true);
}

void SVGGElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGStyledTransformableElement::synchronizeProperty(attrName);

    if (attrName == anyQName() || SVGExternalResourcesRequired::isKnownAttribute(attrName))
        synchronizeExternalResourcesRequired();
}

RenderObject* SVGGElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    // SVG 1.1 testsuite explicitely uses constructs like <g display="none"><linearGradient>
    // We still have to create renderers for the <g> & <linearGradient> element, though the
    // subtree may be hidden - we only want the resource renderers to exist so they can be
    // referenced from somewhere else.
    if (style->display() == NONE)
        return new (arena) RenderSVGHiddenContainer(this);

    return new (arena) RenderSVGTransformableContainer(this);
}

bool SVGGElement::rendererIsNeeded(RenderStyle*)
{
    return parentNode() && parentNode()->isSVGElement(); 
}

}

#endif // ENABLE(SVG)
