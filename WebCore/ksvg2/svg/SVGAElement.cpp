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

#if ENABLE(SVG)

#include "SVGAElement.h"

#include "Attr.h"
#include "Document.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "MouseEvent.h"
#include "RenderSVGContainer.h"
#include "ResourceRequest.h"
#include "SVGNames.h"
#include "XLinkNames.h"
#include "csshelper.h"

namespace WebCore {

SVGAElement::SVGAElement(const QualifiedName& tagName, Document *doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGURIReference()
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
{
}

SVGAElement::~SVGAElement()
{
}

String SVGAElement::title() const
{
    return getAttribute(XLinkNames::titleAttr);
}

ANIMATED_PROPERTY_DEFINITIONS(SVGAElement, String, String, string, Target, target, SVGNames::targetAttr.localName(), m_target)

void SVGAElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::targetAttr)
        setTargetBaseValue(attr->value());
    else {
        if (SVGURIReference::parseMappedAttribute(attr)) {
            bool wasLink = m_isLink;
            m_isLink = !attr->isNull();
            if (wasLink != m_isLink)
                setChanged();
            return;
        }
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

RenderObject* SVGAElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    return new (arena) RenderSVGContainer(this);
}

void SVGAElement::defaultEventHandler(Event* evt)
{
    // TODO : should use CLICK instead
    if ((evt->type() == EventNames::mouseupEvent && m_isLink)) {
        MouseEvent* e = static_cast<MouseEvent*>(evt);

        if (e && e->button() == 2) {
            SVGStyledTransformableElement::defaultEventHandler(evt);
            return;
        }

        String url = parseURL(href());

        String target = getAttribute(SVGNames::targetAttr);
        if (e && e->button() == 1)
            target = "_blank";

        if (!evt->defaultPrevented())
            if (document() && document()->frame())
                document()->frame()->loader()->urlSelected(document()->completeURL(url), target, evt);

        evt->setDefaultHandled();
    }

    SVGStyledTransformableElement::defaultEventHandler(evt);
}

} // namespace WebCore

// vim:ts=4:noet
#endif // ENABLE(SVG)
