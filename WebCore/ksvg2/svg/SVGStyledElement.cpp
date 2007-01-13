/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifdef SVG_SUPPORT
#include "SVGStyledElement.h"

#include "Attr.h"
#include "cssstyleselector.h"
#include "Document.h"
#include "HTMLNames.h"
#include "ksvgcssproperties.h"
#include "PlatformString.h"
#include "RenderView.h"
#include "RenderPath.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "SVGSVGElement.h"

#include <wtf/Assertions.h>

namespace WebCore {

// Defined in CSSGrammar.y, but not in any header, so just declare it here for now.
int getPropertyID(const char* str, int len);

using namespace SVGNames;

SVGStyledElement::SVGStyledElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
{
}

SVGStyledElement::~SVGStyledElement()
{
}

ANIMATED_PROPERTY_DEFINITIONS(SVGStyledElement, String, String, string, ClassName, className, HTMLNames::classAttr.localName(), m_className)

RenderObject* SVGStyledElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    // The path data is set upon the first layout() call.
    return new (arena) RenderPath(style, this);
}

void SVGStyledElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    // id and class are handled by StyledElement
    DeprecatedString qProp = attr->name().localName().deprecatedString();
    int propId = getPropertyID(qProp.ascii(), qProp.length());
    if (propId == 0)
        propId = SVG::getSVGCSSPropertyID(qProp.ascii(), qProp.length());
    if (propId > 0) {
        addCSSProperty(attr, propId, value);
        setChanged();
        return;
    }
    
    SVGElement::parseMappedAttribute(attr);
}

void SVGStyledElement::notifyAttributeChange() const
{
    // no-op
}

void SVGStyledElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    // FIXME: Eventually subclasses from SVGElement should implement
    // attributeChanged() instead of notifyAttributeChange()
    // This is a quick fix to allow dynamic updates of SVG elements
    // but will result in slower dynamic-update performance than necessary.
    SVGElement::attributeChanged(attr, preserveDecls);
    notifyAttributeChange();
}

RenderView* SVGStyledElement::view() const
{
    return static_cast<RenderView*>(document()->renderer());
}

void SVGStyledElement::rebuildRenderer() const
{
    if (!renderer() || !renderer()->isRenderPath())
        return;
    
    RenderPath* renderPath = static_cast<RenderPath*>(renderer());    
    SVGElement* parentElement = svg_dynamic_cast(parentNode());
    if (parentElement && parentElement->renderer() && parentElement->isStyled()
        && parentElement->childShouldCreateRenderer(const_cast<SVGStyledElement*>(this)))
        renderPath->setNeedsLayout(true);
}

}

#endif // SVG_SUPPORT

// vim:ts=4:noet
