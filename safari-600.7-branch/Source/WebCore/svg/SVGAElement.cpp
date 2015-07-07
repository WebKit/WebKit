/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGAElement.h"

#include "Attr.h"
#include "Attribute.h"
#include "Document.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderTypes.h"
#include "HTMLAnchorElement.h"
#include "HTMLParserIdioms.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "PlatformMouseEvent.h"
#include "RenderSVGInline.h"
#include "RenderSVGText.h"
#include "RenderSVGTransformableContainer.h"
#include "ResourceRequest.h"
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "SVGSMILElement.h"
#include "XLinkNames.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

using namespace HTMLNames;

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGAElement, SVGNames::targetAttr, SVGTarget, svgTarget)
DEFINE_ANIMATED_STRING(SVGAElement, XLinkNames::hrefAttr, Href, href)
DEFINE_ANIMATED_BOOLEAN(SVGAElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGAElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(svgTarget)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
    REGISTER_PARENT_ANIMATED_PROPERTIES(SVGGraphicsElement)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGAElement::SVGAElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document)
{
    ASSERT(hasTagName(SVGNames::aTag));
    registerAnimatedPropertiesForSVGAElement();
}

PassRefPtr<SVGAElement> SVGAElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new SVGAElement(tagName, document));
}

String SVGAElement::title() const
{
    // If the xlink:title is set (non-empty string), use it.
    const AtomicString& title = fastGetAttribute(XLinkNames::titleAttr);
    if (!title.isEmpty())
        return title;

    // Otherwise, use the title of this element.
    return SVGElement::title();
}

bool SVGAElement::isSupportedAttribute(const QualifiedName& attrName)
{
    static NeverDestroyed<HashSet<QualifiedName>> supportedAttributes;
    if (supportedAttributes.get().isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        SVGLangSpace::addSupportedAttributes(supportedAttributes);
        SVGExternalResourcesRequired::addSupportedAttributes(supportedAttributes);
        supportedAttributes.get().add(SVGNames::targetAttr);
    }
    return supportedAttributes.get().contains<SVGAttributeHashTranslator>(attrName);
}

void SVGAElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (!isSupportedAttribute(name)) {
        SVGGraphicsElement::parseAttribute(name, value);
        return;
    }

    if (name == SVGNames::targetAttr) {
        setSVGTargetBaseValue(value);
        return;
    }

    if (SVGURIReference::parseAttribute(name, value))
        return;
    if (SVGLangSpace::parseAttribute(name, value))
        return;
    if (SVGExternalResourcesRequired::parseAttribute(name, value))
        return;

    ASSERT_NOT_REACHED();
}

void SVGAElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGraphicsElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElementInstance::InvalidationGuard invalidationGuard(this);

    // Unlike other SVG*Element classes, SVGAElement only listens to SVGURIReference changes
    // as none of the other properties changes the linking behaviour for our <a> element.
    if (SVGURIReference::isKnownAttribute(attrName)) {
        bool wasLink = isLink();
        setIsLink(!href().isNull() && !shouldProhibitLinks(this));
        if (wasLink != isLink())
            setNeedsStyleRecalc();
    }
}

RenderPtr<RenderElement> SVGAElement::createElementRenderer(PassRef<RenderStyle> style)
{
    if (parentNode() && parentNode()->isSVGElement() && toSVGElement(parentNode())->isTextContent())
        return createRenderer<RenderSVGInline>(*this, WTF::move(style));

    return createRenderer<RenderSVGTransformableContainer>(*this, WTF::move(style));
}

void SVGAElement::defaultEventHandler(Event* event)
{
    if (isLink()) {
        if (focused() && isEnterKeyKeydownEvent(event)) {
            event->setDefaultHandled();
            dispatchSimulatedClick(event);
            return;
        }

        if (isLinkClick(event)) {
            String url = stripLeadingAndTrailingHTMLSpaces(href());

            if (url[0] == '#') {
                Element* targetElement = treeScope().getElementById(url.substringSharingImpl(1));
                if (targetElement && isSVGSMILElement(*targetElement)) {
                    toSVGSMILElement(*targetElement).beginByLinkActivation();
                    event->setDefaultHandled();
                    return;
                }
                // Only allow navigation to internal <view> anchors.
                if (targetElement && !targetElement->hasTagName(SVGNames::viewTag))
                    return;
            }

            String target = this->target();
            if (target.isEmpty() && fastGetAttribute(XLinkNames::showAttr) == "new")
                target = "_blank";
            event->setDefaultHandled();

            Frame* frame = document().frame();
            if (!frame)
                return;
            frame->loader().urlSelected(document().completeURL(url), target, event, LockHistory::No, LockBackForwardList::No, MaybeSendReferrer);
            return;
        }
    }

    SVGGraphicsElement::defaultEventHandler(event);
}

short SVGAElement::tabIndex() const
{
    // Skip the supportsFocus check in SVGElement.
    return Element::tabIndex();
}

bool SVGAElement::supportsFocus() const
{
    if (hasEditableStyle())
        return SVGGraphicsElement::supportsFocus();
    // If not a link we should still be able to focus the element if it has a tabIndex.
    return isLink() || Element::supportsFocus();
}

bool SVGAElement::isFocusable() const
{
    if (renderer() && renderer()->absoluteClippedOverflowRect().isEmpty())
        return false;
    
    return SVGElement::isFocusable();
}

bool SVGAElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name().localName() == hrefAttr || SVGGraphicsElement::isURLAttribute(attribute);
}

bool SVGAElement::isMouseFocusable() const
{
    // Links are focusable by default, but only allow links with tabindex or contenteditable to be mouse focusable.
    // https://bugs.webkit.org/show_bug.cgi?id=26856
    if (isLink())
        return Element::supportsFocus();
    
    return SVGElement::isMouseFocusable();
}

bool SVGAElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (isFocusable() && Element::supportsFocus())
        return SVGElement::isKeyboardFocusable(event);

    if (isLink())
        return document().frame()->eventHandler().tabsToLinks(event);

    return SVGElement::isKeyboardFocusable(event);
}

bool SVGAElement::canStartSelection() const
{
    if (!isLink())
        return SVGElement::canStartSelection();

    return hasEditableStyle();
}

bool SVGAElement::childShouldCreateRenderer(const Node& child) const
{
    // http://www.w3.org/2003/01/REC-SVG11-20030114-errata#linking-text-environment
    // The 'a' element may contain any element that its parent may contain, except itself.
    if (child.hasTagName(SVGNames::aTag))
        return false;
    if (parentNode() && parentNode()->isSVGElement())
        return parentNode()->childShouldCreateRenderer(child);

    return SVGElement::childShouldCreateRenderer(child);
}

bool SVGAElement::willRespondToMouseClickEvents()
{ 
    return isLink() || SVGGraphicsElement::willRespondToMouseClickEvents(); 
}

} // namespace WebCore
