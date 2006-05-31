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
#include "SVGStyledElement.h"

#include "Attr.h"
#include "Document.h"
#include "KCanvasCreator.h"
#include "KCanvasRenderingStyle.h"
#include "KRenderingDevice.h"
#include "PlatformString.h"
#include "RenderView.h"
#include "RenderPath.h"
#include "SVGAnimatedString.h"
#include "SVGDOMImplementation.h"
#include "SVGElement.h"
#include "SVGHelper.h"
#include "SVGMatrix.h"
#include "SVGNames.h"
#include "SVGRenderStyle.h"
#include "SVGSVGElement.h"
#include "ksvg.h"
#include "ksvgcssproperties.h"

#include <wtf/Assertions.h>

namespace WebCore {

// Defined in CSSGrammar.y, but not in any header, so just declare it here for now.
int getPropertyID(const char* str, int len);

using namespace SVGNames;

SVGStyledElement::SVGStyledElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
    , m_updateVectorial(false)
{
}

SVGStyledElement::~SVGStyledElement()
{
}

SVGAnimatedString* SVGStyledElement::className() const
{
    return lazy_create(m_className, (SVGStyledElement*)0); // TODO: use notification context?
}

RenderObject* SVGStyledElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    RefPtr<KCanvasPath> pathData = toPathData();
    if (!pathData)
        return 0;
    return renderingDevice()->createItem(arena, style, this, pathData.get());
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
    // For most cases we need to handle vectorial data changes (ie. rect x changed)
    if (!ownerDocument()->parsing()) {
        // TODO: Use a more optimized way of updating, means not calling updateCanvasItem() here!
        const_cast<SVGStyledElement*>(this)->m_updateVectorial = true;
        const_cast<SVGStyledElement*>(this)->updateCanvasItem();
    }
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

void SVGStyledElement::updateCanvasItem()
{
    if (!m_updateVectorial || !renderer() || !renderer()->isRenderPath())
        return;
    
    RenderPath* renderPath = static_cast<RenderPath*>(renderer());
    bool renderSection = false;
    
    SVGElement* parentElement = svg_dynamic_cast(parentNode());
    if (parentElement && parentElement->renderer() && parentElement->isStyled()
        && parentElement->childShouldCreateRenderer(this))
        renderSection = true;

    renderPath->setPath(toPathData());

    if (renderSection)
        renderPath->setNeedsLayout(true);

    m_updateVectorial = false;
}

const SVGStyledElement* SVGStyledElement::pushAttributeContext(const SVGStyledElement*)
{
    if (view())
        static_cast<RenderPath*>(renderer())->setPath(toPathData());

    return 0;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

