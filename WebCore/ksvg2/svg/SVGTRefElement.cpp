/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
#if SVG_SUPPORT
#include "Text.h"
#include "SVGTRefElement.h"
#include "SVGNames.h"
#include "XLinkNames.h"
#include "SVGAnimatedString.h"
#include "SVGDocument.h"
#include "RenderInline.h"

using namespace WebCore;

SVGTRefElement::SVGTRefElement(const QualifiedName& tagName, Document *doc)
: SVGTextPositioningElement(tagName, doc), SVGURIReference()
{
}

SVGTRefElement::~SVGTRefElement()
{
}

void SVGTRefElement::updateReferencedText()
{
    String targetId = SVGURIReference::getTarget(String(href()->baseVal()).deprecatedString());
    Element *targetElement = ownerDocument()->getElementById(targetId.impl());
    SVGElement *target = svg_dynamic_cast(targetElement);
    if (target) {
        ExceptionCode ignore = 0;
        setTextContent(target->textContent().impl(), ignore);
    }
}

void SVGTRefElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    if (attr->name().matches(XLinkNames::hrefAttr))
        updateReferencedText();

    SVGTextPositioningElement::attributeChanged(attr, preserveDecls);
}

void SVGTRefElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (SVGURIReference::parseMappedAttribute(attr)) {
        updateReferencedText();
        return;
    }

    SVGTextPositioningElement::parseMappedAttribute(attr);
}

bool SVGTRefElement::childShouldCreateRenderer(WebCore::Node *child) const
{
    if (child->isTextNode() || child->hasTagName(SVGNames::tspanTag) ||
        child->hasTagName(SVGNames::trefTag))
        return true;
    return false;
}

RenderObject *SVGTRefElement::createRenderer(RenderArena *arena, RenderStyle *)
{
    return new (arena) RenderInline(this);
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

