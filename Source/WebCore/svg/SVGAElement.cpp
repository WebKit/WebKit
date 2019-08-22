/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
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

#include "Document.h"
#include "EventHandler.h"
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
#include "SVGNames.h"
#include "SVGSMILElement.h"
#include "XLinkNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGAElement);

inline SVGAElement::SVGAElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document)
    , SVGExternalResourcesRequired(this)
    , SVGURIReference(this)
{
    ASSERT(hasTagName(SVGNames::aTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::targetAttr, &SVGAElement::m_target>();
    });
}

Ref<SVGAElement> SVGAElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGAElement(tagName, document));
}

String SVGAElement::title() const
{
    // If the xlink:title is set (non-empty string), use it.
    const AtomString& title = attributeWithoutSynchronization(XLinkNames::titleAttr);
    if (!title.isEmpty())
        return title;

    // Otherwise, use the title of this element.
    return SVGElement::title();
}

void SVGAElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::targetAttr) {
        m_target->setBaseValInternal(value);
        return;
    }

    SVGGraphicsElement::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
}

void SVGAElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (SVGURIReference::isKnownAttribute(attrName)) {
        bool wasLink = isLink();
        setIsLink(!href().isNull() && !shouldProhibitLinks(this));
        if (wasLink != isLink()) {
            InstanceInvalidationGuard guard(*this);
            invalidateStyleForSubtree();
        }
        return;
    }

    SVGGraphicsElement::svgAttributeChanged(attrName);
    SVGExternalResourcesRequired::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGAElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (parentNode() && parentNode()->isSVGElement() && downcast<SVGElement>(*parentNode()).isTextContent())
        return createRenderer<RenderSVGInline>(*this, WTFMove(style));

    return createRenderer<RenderSVGTransformableContainer>(*this, WTFMove(style));
}

void SVGAElement::defaultEventHandler(Event& event)
{
    if (isLink()) {
        if (focused() && isEnterKeyKeydownEvent(event)) {
            event.setDefaultHandled();
            dispatchSimulatedClick(&event);
            return;
        }

        if (MouseEvent::canTriggerActivationBehavior(event)) {
            String url = stripLeadingAndTrailingHTMLSpaces(href());

            if (url[0] == '#') {
                auto targetElement = makeRefPtr(treeScope().getElementById(url.substringSharingImpl(1)));
                if (is<SVGSMILElement>(targetElement)) {
                    downcast<SVGSMILElement>(*targetElement).beginByLinkActivation();
                    event.setDefaultHandled();
                    return;
                }
                // Only allow navigation to internal <view> anchors.
                if (targetElement && !targetElement->hasTagName(SVGNames::viewTag))
                    return;
            }

            String target = this->target();
            if (target.isEmpty() && attributeWithoutSynchronization(XLinkNames::showAttr) == "new")
                target = "_blank";
            event.setDefaultHandled();

            auto frame = makeRefPtr(document().frame());
            if (!frame)
                return;
            frame->loader().urlSelected(document().completeURL(url), target, &event, LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, document().shouldOpenExternalURLsPolicyToPropagate());
            return;
        }
    }

    SVGGraphicsElement::defaultEventHandler(event);
}

int SVGAElement::defaultTabIndex() const
{
    return 0;
}

bool SVGAElement::supportsFocus() const
{
    if (hasEditableStyle())
        return SVGGraphicsElement::supportsFocus();
    // If not a link we should still be able to focus the element if it has a tabIndex.
    return isLink() || SVGGraphicsElement::supportsFocus();
}

bool SVGAElement::isURLAttribute(const Attribute& attribute) const
{
    return SVGURIReference::isKnownAttribute(attribute.name()) || SVGGraphicsElement::isURLAttribute(attribute);
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

    if (parentElement() && parentElement()->isSVGElement())
        return parentElement()->childShouldCreateRenderer(child);

    return SVGElement::childShouldCreateRenderer(child);
}

bool SVGAElement::willRespondToMouseClickEvents()
{ 
    return isLink() || SVGGraphicsElement::willRespondToMouseClickEvents(); 
}

SharedStringHash SVGAElement::visitedLinkHash() const
{
    ASSERT(isLink());
    if (!m_storedVisitedLinkHash)
        m_storedVisitedLinkHash = computeVisitedLinkHash(document().baseURL(), getAttribute(SVGNames::hrefAttr, XLinkNames::hrefAttr));
    return *m_storedVisitedLinkHash;
}

} // namespace WebCore
