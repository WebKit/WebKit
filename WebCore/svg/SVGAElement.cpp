/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>
                  2007 Eric Seidel <eric@webkit.org>

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGAElement.h"

#include "Attr.h"
#include "CSSHelper.h"
#include "Document.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "KeyboardEvent.h"
#include "MappedAttribute.h"
#include "MouseEvent.h"
#include "PlatformMouseEvent.h"
#include "RenderSVGInline.h"
#include "RenderSVGTransformableContainer.h"
#include "ResourceRequest.h"
#include "SVGNames.h"
#include "SVGSMILElement.h"
#include "XLinkNames.h"

namespace WebCore {

SVGAElement::SVGAElement(const QualifiedName& tagName, Document *doc)
    : SVGStyledTransformableElement(tagName, doc)
    , SVGURIReference()
    , SVGTests()
    , SVGLangSpace()
    , SVGExternalResourcesRequired()
    , m_target(this, SVGNames::targetAttr)
    , m_href(this, XLinkNames::hrefAttr)
{
}

SVGAElement::~SVGAElement()
{
}

String SVGAElement::title() const
{
    return getAttribute(XLinkNames::titleAttr);
}

void SVGAElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == SVGNames::targetAttr)
        setTargetBaseValue(attr->value());
    else {
        if (SVGURIReference::parseMappedAttribute(attr))
            return;
        if (SVGTests::parseMappedAttribute(attr))
            return;
        if (SVGLangSpace::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;
        SVGStyledTransformableElement::parseMappedAttribute(attr);
    }
}

void SVGAElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGStyledTransformableElement::svgAttributeChanged(attrName);

    // Unlike other SVG*Element classes, SVGAElement only listens to SVGURIReference changes
    // as none of the other properties changes the linking behaviour for our <a> element.
    if (SVGURIReference::isKnownAttribute(attrName)) {
        bool wasLink = isLink();
        setIsLink(!href().isNull());

        if (wasLink != isLink())
            setNeedsStyleRecalc();
    }
}

RenderObject* SVGAElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    if (static_cast<SVGElement*>(parent())->isTextContent())
        return new (arena) RenderSVGInline(this);

    return new (arena) RenderSVGTransformableContainer(this);
}

void SVGAElement::defaultEventHandler(Event* evt)
{
    if (isLink() && (evt->type() == eventNames().clickEvent || (evt->type() == eventNames().keydownEvent && focused()))) {
        MouseEvent* e = 0;
        if (evt->type() == eventNames().clickEvent && evt->isMouseEvent())
            e = static_cast<MouseEvent*>(evt);
        
        KeyboardEvent* k = 0;
        if (evt->type() == eventNames().keydownEvent && evt->isKeyboardEvent())
            k = static_cast<KeyboardEvent*>(evt);
        
        if (e && e->button() == RightButton) {
            SVGStyledTransformableElement::defaultEventHandler(evt);
            return;
        }
        
        if (k) {
            if (k->keyIdentifier() != "Enter") {
                SVGStyledTransformableElement::defaultEventHandler(evt);
                return;
            }
            evt->setDefaultHandled();
            dispatchSimulatedClick(evt);
            return;
        }
        
        String target = this->target();
        if (e && e->button() == MiddleButton)
            target = "_blank";
        else if (target.isEmpty()) // if target is empty, default to "_self" or use xlink:target if set
            target = (getAttribute(XLinkNames::showAttr) == "new") ? "_blank" : "_self";

        if (!evt->defaultPrevented()) {
            String url = deprecatedParseURL(href());
#if ENABLE(SVG_ANIMATION)
            if (url.startsWith("#")) {
                Element* targetElement = document()->getElementById(url.substring(1));
                if (SVGSMILElement::isSMILElement(targetElement)) {
                    SVGSMILElement* timed = static_cast<SVGSMILElement*>(targetElement);
                    timed->beginByLinkActivation();
                    evt->setDefaultHandled();
                    SVGStyledTransformableElement::defaultEventHandler(evt);
                    return;
                }
            }
#endif
            if (document()->frame())
                document()->frame()->loader()->urlSelected(document()->completeURL(url), target, evt, false, false, true);
        }

        evt->setDefaultHandled();
    }

    SVGStyledTransformableElement::defaultEventHandler(evt);
}

bool SVGAElement::supportsFocus() const
{
    if (isContentEditable())
        return SVGStyledTransformableElement::supportsFocus();
    return true;
}

bool SVGAElement::isFocusable() const
{
    if (renderer() && renderer()->absoluteClippedOverflowRect().isEmpty())
        return false;
    
    return SVGElement::isFocusable();
}

bool SVGAElement::isMouseFocusable() const
{
    return false;
}

bool SVGAElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (!isFocusable())
        return false;
    
    if (!document()->frame())
        return false;
    
    return document()->frame()->eventHandler()->tabsToLinks(event);
}

bool SVGAElement::childShouldCreateRenderer(Node* child) const
{
    // http://www.w3.org/2003/01/REC-SVG11-20030114-errata#linking-text-environment
    // The 'a' element may contain any element that its parent may contain, except itself.
    if (child->hasTagName(SVGNames::aTag))
        return false;
    if (parent() && parent()->isSVGElement())
        return static_cast<SVGElement*>(parent())->childShouldCreateRenderer(child);

    return SVGElement::childShouldCreateRenderer(child);
}

} // namespace WebCore

#endif // ENABLE(SVG)
