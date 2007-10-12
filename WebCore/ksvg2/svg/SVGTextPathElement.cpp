/*
    Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>

    This file is part of the KDE project

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGTextPathElement.h"

#include "AffineTransform.h"
#include "FloatRect.h"
#include "RenderSVGTextPath.h"
#include "SVGLengthList.h"
#include "SVGPathElement.h"
#include "SVGRenderStyle.h"
#include "SVGTextPathElement.h"
#include "SVGTransformList.h"

namespace WebCore {

SVGTextPathElement::SVGTextPathElement(const QualifiedName& tagName, Document* doc)
    : SVGTextContentElement(tagName, doc)
    , SVGURIReference()
    , m_startOffset(this, LengthModeOther)
    , m_method(SVG_TEXTPATH_METHODTYPE_ALIGN)
    , m_spacing(SVG_TEXTPATH_SPACINGTYPE_EXACT)
{
}

SVGTextPathElement::~SVGTextPathElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGTextPathElement, SVGLength, Length, length, StartOffset, startOffset, SVGNames::startOffsetAttr.localName(), m_startOffset)
ANIMATED_PROPERTY_DEFINITIONS(SVGTextPathElement, int, Enumeration, enumeration, Method, method, SVGNames::methodAttr.localName(), m_method)
ANIMATED_PROPERTY_DEFINITIONS(SVGTextPathElement, int, Enumeration, enumeration, Spacing, spacing, SVGNames::spacingAttr.localName(), m_spacing)

void SVGTextPathElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();

    if (attr->name() == SVGNames::startOffsetAttr)
        setStartOffsetBaseValue(SVGLength(this, LengthModeOther, value));
    else if (attr->name() == SVGNames::methodAttr) {
        if (value == "align")
            setSpacingBaseValue(SVG_TEXTPATH_METHODTYPE_ALIGN);
        else if(value == "stretch")
            setSpacingBaseValue(SVG_TEXTPATH_METHODTYPE_STRETCH);
    } else if (attr->name() == SVGNames::spacingAttr) {
        if (value == "auto")
            setMethodBaseValue(SVG_TEXTPATH_SPACINGTYPE_AUTO);
        else if (value == "exact")
            setMethodBaseValue(SVG_TEXTPATH_SPACINGTYPE_EXACT);
    } else {
        if (SVGURIReference::parseMappedAttribute(attr))
            return;
        SVGTextContentElement::parseMappedAttribute(attr);
    }
}

RenderObject* SVGTextPathElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    return new (arena) RenderSVGTextPath(this);
}

bool SVGTextPathElement::childShouldCreateRenderer(Node* child) const
{
    if (child->isTextNode() || child->hasTagName(SVGNames::trefTag) ||
        child->hasTagName(SVGNames::tspanTag) || child->hasTagName(SVGNames::textPathTag))
        return true;

    return false;
}

void SVGTextPathElement::insertedIntoDocument()
{
    SVGElement::insertedIntoDocument();

    String id = SVGURIReference::getTarget(href());
    Element* targetElement = ownerDocument()->getElementById(id);
    if (!targetElement) {
        document()->accessSVGExtensions()->addPendingResource(id, this);
        return;
    }

    buildPendingResource();
}

void SVGTextPathElement::buildPendingResource()
{
    // FIXME: Real logic here!
    if (attached())
        detach();
    
    ASSERT(!attached());
    attach();
}

void SVGTextPathElement::attach()
{ 
    SVGStyledElement::attach();

    if (!renderer())
        return;

    String id = SVGURIReference::getTarget(href());
    Element* targetElement = ownerDocument()->getElementById(id);
    ASSERT(targetElement);

    if (!targetElement->hasTagName(SVGNames::pathTag))
        return;

    SVGPathElement* pathElement = static_cast<SVGPathElement*>(targetElement);

    RenderSVGTextPath* textPath = static_cast<RenderSVGTextPath*>(renderer());
    ASSERT(textPath);

    textPath->setStartOffset(m_startOffset.valueAsPercentage());
    textPath->setExactAlignment(m_method == SVG_TEXTPATH_SPACINGTYPE_EXACT);
    textPath->setStretchMethod(m_spacing == SVG_TEXTPATH_METHODTYPE_STRETCH);

    // TODO: Catch transform updates :-)
    Path pathData = pathElement->toPathData();
    pathData.transform(pathElement->localMatrix());
    textPath->setLayoutPath(pathData);
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
