/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2006       Alexander Kellett <lypanov@kde.org>

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
#include "SVGImageElement.h"

#include "Attr.h"
#include "CSSPropertyNames.h"
#include "KCanvasRenderingStyle.h"
#include "RenderSVGImage.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedString.h"
#include "SVGDocument.h"
#include "SVGHelper.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "XLinkNames.h"
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasImage.h>
#include <kcanvas/KCanvasPath.h>
#include <kxmlcore/Assertions.h>

using namespace WebCore;

SVGImageElement::SVGImageElement(const QualifiedName& tagName, Document *doc)
: SVGStyledTransformableElement(tagName, doc), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired(), SVGURIReference(), m_imageLoader(this)
{
}

SVGImageElement::~SVGImageElement()
{
}

SVGAnimatedLength *SVGImageElement::x() const
{
    return lazy_create<SVGAnimatedLength>(m_x, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGImageElement::y() const
{
    return lazy_create<SVGAnimatedLength>(m_y, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedLength *SVGImageElement::width() const
{
    return lazy_create<SVGAnimatedLength>(m_width, this, LM_WIDTH, viewportElement());
}

SVGAnimatedLength *SVGImageElement::height() const
{
    return lazy_create<SVGAnimatedLength>(m_height, this, LM_HEIGHT, viewportElement());
}

SVGAnimatedPreserveAspectRatio *SVGImageElement::preserveAspectRatio() const
{
    return lazy_create<SVGAnimatedPreserveAspectRatio>(m_preserveAspectRatio, this);
}

void SVGImageElement::parseMappedAttribute(MappedAttribute *attr)
{
    const AtomicString& value = attr->value();
    if (attr->name() == SVGNames::xAttr)
        x()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::yAttr)
        y()->baseVal()->setValueAsString(value.impl());
    else if (attr->name() == SVGNames::preserveAspectRatioAttr)
        preserveAspectRatio()->baseVal()->parsePreserveAspectRatio(value.impl());
    else if (attr->name() == SVGNames::widthAttr) {
        width()->baseVal()->setValueAsString(value.impl());
        addCSSProperty(attr, CSS_PROP_WIDTH, value);
    } else if (attr->name() == SVGNames::heightAttr) {
        height()->baseVal()->setValueAsString(value.impl());
        addCSSProperty(attr, CSS_PROP_HEIGHT, value);
    } else {
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        if (SVGURIReference::parseMappedAttribute(attr)) {
            if (attr->name().matches(XLinkNames::hrefAttr))
                m_imageLoader.updateFromElement();
            return;
        }
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

RenderObject *SVGImageElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderSVGImage(this);
}

void SVGImageElement::attach()
{
    SVGStyledTransformableElement::attach();
    if (WebCore::RenderSVGImage* imageObj = static_cast<WebCore::RenderSVGImage*>(renderer()))
        imageObj->setCachedImage(m_imageLoader.image());
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

