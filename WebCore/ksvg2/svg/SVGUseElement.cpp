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
#include <kdom/Namespace.h>
#include <kdom/core/Attr.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGGElement.h"
#include "SVGSVGElement.h"
#include "SVGUseElement.h"
#include "SVGSymbolElement.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedString.h"
#include "KCanvasRenderingStyle.h"
#include "SVGAnimatedPreserveAspectRatio.h"

#include <kcanvas/KCanvas.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include "KCanvasRenderingStyle.h"
#include <kcanvas/device/KRenderingDevice.h>
#include "Document.h"

using namespace WebCore;

SVGUseElement::SVGUseElement(const QualifiedName& tagName, Document *doc)
: SVGStyledTransformableElement(tagName, doc), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGURIReference()
{
}

SVGUseElement::~SVGUseElement()
{
}

SVGAnimatedLength *SVGUseElement::x() const
{
    return lazy_create<SVGAnimatedLength>(m_x, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGUseElement::y() const
{
    return lazy_create<SVGAnimatedLength>(m_y, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLength *SVGUseElement::width() const
{
    return lazy_create<SVGAnimatedLength>(m_width, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGUseElement::height() const
{
    return lazy_create<SVGAnimatedLength>(m_height, this, LM_HEIGHT, viewportElement());
}

void SVGUseElement::parseMappedAttribute(MappedAttribute *attr)
{
    const AtomicString& value = attr->value();
    
    if (attr->name() == SVGNames::xAttr)
        x()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::yAttr)
        y()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::widthAttr)
        width()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::heightAttr)
        height()->baseVal()->setValueAsString(value.impl());
    else {
        if(SVGTests::parseMappedAttribute(attr)) return;
        if(SVGLangSpace::parseMappedAttribute(attr)) return;
        if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;
        if(SVGURIReference::parseMappedAttribute(attr)) return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

void SVGUseElement::closeRenderer()
{
    DeprecatedString ref = String(href()->baseVal()).deprecatedString();
    String targetId = SVGURIReference::getTarget(ref);
    Element *targetElement = ownerDocument()->getElementById(targetId.impl());
    SVGElement *target = svg_dynamic_cast(targetElement);
    if (!target)
    {
        //getDocument()->addForwardReference(this);
        return;
    }

    float _x = x()->baseVal()->value(), _y = y()->baseVal()->value();
    float _w = width()->baseVal()->value(), _h = height()->baseVal()->value();
    
    String wString = DeprecatedString::number(_w);
    String hString = DeprecatedString::number(_h);
    
    ExceptionCode ec;
    DeprecatedString trans = DeprecatedString::fromLatin1("translate(%1, %2)").arg(_x).arg(_y);
    if(target->hasTagName(SVGNames::symbolTag))
    {
        RefPtr<SVGElement> dummy = new SVGSVGElement(SVGNames::svgTag, getDocument());
        if(_w > 0)
            dummy->setAttribute(SVGNames::widthAttr, wString.impl());
        if(_h > 0)
            dummy->setAttribute(SVGNames::heightAttr, hString.impl());
        
        SVGSymbolElement *symbol = static_cast<SVGSymbolElement *>(target);
        if (symbol->hasAttribute(SVGNames::viewBoxAttr)) {
            const AtomicString& symbolViewBox = symbol->getAttribute(SVGNames::viewBoxAttr);
            dummy->setAttribute(SVGNames::viewBoxAttr, symbolViewBox);
        }
        target->cloneChildNodes(dummy.get());

        RefPtr<SVGElement> dummy2 = new SVGDummyElement(SVGNames::gTag, getDocument());
        dummy2->setAttribute(SVGNames::transformAttr, String(trans));
        
        appendChild(dummy2, ec);
        dummy2->appendChild(dummy, ec);
    }
    else if(target->hasTagName(SVGNames::svgTag))
    {
        RefPtr<SVGDummyElement> dummy = new SVGDummyElement(SVGNames::gTag, getDocument());
        dummy->setAttribute(SVGNames::transformAttr, String(trans));
        
        RefPtr<SVGElement> root = static_pointer_cast<SVGElement>(target->cloneNode(true));
        if(hasAttribute(SVGNames::widthAttr))
            root->setAttribute(SVGNames::widthAttr, wString.impl());
            
        if(hasAttribute(SVGNames::heightAttr))
            root->setAttribute(SVGNames::heightAttr, hString.impl());
            
        appendChild(dummy, ec);
        dummy->appendChild(root.release(), ec);
    }
    else
    {
        RefPtr<SVGDummyElement> dummy = new SVGDummyElement(SVGNames::gTag, getDocument());
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

RenderObject *SVGUseElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return renderingDevice()->createContainer(arena, style, this);
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

