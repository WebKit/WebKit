/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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

#ifdef SVG_SUPPORT
#include "SVGUseElement.h"

#include "Document.h"
#include "RenderSVGContainer.h"
#include "SVGGElement.h"
#include "SVGLength.h"
#include "SVGNames.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGSVGElement.h"
#include "SVGSymbolElement.h"

namespace WebCore {

SVGUseElement::SVGUseElement(const QualifiedName& tagName, Document* doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , SVGURIReference()
    , m_x(this, LengthModeWidth)
    , m_y(this, LengthModeHeight)
    , m_width(this, LengthModeWidth)
    , m_height(this, LengthModeHeight)
{
}

SVGUseElement::~SVGUseElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGUseElement, SVGLength, Length, length, X, x, SVGNames::xAttr.localName(), m_x)
ANIMATED_PROPERTY_DEFINITIONS(SVGUseElement, SVGLength, Length, length, Y, y, SVGNames::yAttr.localName(), m_y)
ANIMATED_PROPERTY_DEFINITIONS(SVGUseElement, SVGLength, Length, length, Width, width, SVGNames::widthAttr.localName(), m_width)
ANIMATED_PROPERTY_DEFINITIONS(SVGUseElement, SVGLength, Length, length, Height, height, SVGNames::heightAttr.localName(), m_height)

void SVGUseElement::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
    
    if (attr->name() == SVGNames::xAttr)
        setXBaseValue(SVGLength(this, LengthModeWidth, value));
    else if (attr->name() == SVGNames::yAttr)
        setYBaseValue(SVGLength(this, LengthModeHeight, value));
    else if (attr->name() == SVGNames::widthAttr) {
        setWidthBaseValue(SVGLength(this, LengthModeWidth, value));
        if (width().value() < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for use attribute <width> is not allowed");
    } else if (attr->name() == SVGNames::heightAttr) {
        setHeightBaseValue(SVGLength(this, LengthModeHeight, value));
        if (height().value() < 0.0)
            document()->accessSVGExtensions()->reportError("A negative value for use attribute <height> is not allowed");
    } else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        if (SVGURIReference::parseMappedAttribute(attr))
            return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

void SVGUseElement::closeRenderer()
{
    Element* targetElement = ownerDocument()->getElementById(SVGURIReference::getTarget(href()));
    SVGElement* target = svg_dynamic_cast(targetElement);
    if (!target) {
        //document()->addForwardReference(this);
        return;
    }

    float _x = x().value(), _y = y().value();
    float _w = width().value(), _h = height().value();
    
    String wString = String::number(_w);
    String hString = String::number(_h);
    
    ExceptionCode ec = 0;
    String trans = String::format("translate(%f, %f)", _x, _y);
    if (target->hasTagName(SVGNames::symbolTag)) {
        RefPtr<SVGElement> dummy = new SVGSVGElement(SVGNames::svgTag, document());
        if (_w > 0)
            dummy->setAttribute(SVGNames::widthAttr, wString);
        if (_h > 0)
            dummy->setAttribute(SVGNames::heightAttr, hString);
        
        SVGSymbolElement* symbol = static_cast<SVGSymbolElement*>(target);
        if (symbol->hasAttribute(SVGNames::viewBoxAttr)) {
            const AtomicString& symbolViewBox = symbol->getAttribute(SVGNames::viewBoxAttr);
            dummy->setAttribute(SVGNames::viewBoxAttr, symbolViewBox);
        }
        target->cloneChildNodes(dummy.get());
        *dummy->attributes() = *target->attributes();

        RefPtr<SVGElement> dummy2 = new SVGDummyElement(SVGNames::gTag, document());
        dummy2->setAttribute(SVGNames::transformAttr, trans);
        
        appendChild(dummy2, ec);
        dummy2->appendChild(dummy, ec);
    } else if (target->hasTagName(SVGNames::svgTag)) {
        RefPtr<SVGDummyElement> dummy = new SVGDummyElement(SVGNames::gTag, document());
        dummy->setAttribute(SVGNames::transformAttr, trans);
        
        RefPtr<SVGElement> root = static_pointer_cast<SVGElement>(target->cloneNode(true));
        if (hasAttribute(SVGNames::widthAttr))
            root->setAttribute(SVGNames::widthAttr, wString);
            
        if (hasAttribute(SVGNames::heightAttr))
            root->setAttribute(SVGNames::heightAttr, hString);
            
        appendChild(dummy, ec);
        dummy->appendChild(root.release(), ec);
    } else {
        RefPtr<SVGDummyElement> dummy = new SVGDummyElement(SVGNames::gTag, document());
        dummy->setAttribute(SVGNames::transformAttr, trans);
        
        RefPtr<Node> root = target->cloneNode(true);
        
        appendChild(dummy, ec);
        dummy->appendChild(root.release(), ec);
    }

    SVGElement::closeRenderer();
}

bool SVGUseElement::hasChildNodes() const
{
    return false;
}

RenderObject* SVGUseElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderSVGContainer(this);
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
