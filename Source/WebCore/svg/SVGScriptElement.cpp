/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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
#include "SVGScriptElement.h"

#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "SVGAnimatedStaticPropertyTearOff.h"
#include "XLinkNames.h"

namespace WebCore {

// Animated property definitions
DEFINE_ANIMATED_STRING(SVGScriptElement, XLinkNames::hrefAttr, Href, href)
DEFINE_ANIMATED_BOOLEAN(SVGScriptElement, SVGNames::externalResourcesRequiredAttr, ExternalResourcesRequired, externalResourcesRequired)

BEGIN_REGISTER_ANIMATED_PROPERTIES(SVGScriptElement)
    REGISTER_LOCAL_ANIMATED_PROPERTY(href)
    REGISTER_LOCAL_ANIMATED_PROPERTY(externalResourcesRequired)
END_REGISTER_ANIMATED_PROPERTIES

inline SVGScriptElement::SVGScriptElement(const QualifiedName& tagName, Document& document, bool wasInsertedByParser, bool alreadyStarted)
    : SVGElement(tagName, document)
    , ScriptElement(*this, wasInsertedByParser, alreadyStarted)
    , m_svgLoadEventTimer(*this, &SVGElement::svgLoadEventTimerFired)
{
    ASSERT(hasTagName(SVGNames::scriptTag));
    registerAnimatedPropertiesForSVGScriptElement();
}

Ref<SVGScriptElement> SVGScriptElement::create(const QualifiedName& tagName, Document& document, bool insertedByParser)
{
    return adoptRef(*new SVGScriptElement(tagName, document, insertedByParser, false));
}

void SVGScriptElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGElement::parseAttribute(name, value);
    SVGURIReference::parseAttribute(name, value);
    SVGExternalResourcesRequired::parseAttribute(name, value);
}

void SVGScriptElement::svgAttributeChanged(const QualifiedName& attrName)
{
    InstanceInvalidationGuard guard(*this);

    if (SVGURIReference::isKnownAttribute(attrName)) {
        handleSourceAttribute(href());
        return;
    }

    SVGExternalResourcesRequired::handleAttributeChange(this, attrName);
    SVGElement::svgAttributeChanged(attrName);
}

Node::InsertionNotificationRequest SVGScriptElement::insertedInto(ContainerNode& rootParent)
{
    SVGElement::insertedInto(rootParent);
    if (rootParent.inDocument())
        SVGExternalResourcesRequired::insertedIntoDocument(this);
    return shouldCallFinishedInsertingSubtree(rootParent) ? InsertionShouldCallFinishedInsertingSubtree : InsertionDone;
}

void SVGScriptElement::finishedInsertingSubtree()
{
    ScriptElement::finishedInsertingSubtree();
}

void SVGScriptElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);
    ScriptElement::childrenChanged();
}

bool SVGScriptElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == sourceAttributeValue();
}

void SVGScriptElement::finishParsingChildren()
{
    SVGElement::finishParsingChildren();
    SVGExternalResourcesRequired::finishParsingChildren();
}

void SVGScriptElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    SVGElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(href()));
}

String SVGScriptElement::sourceAttributeValue() const
{
    return href();
}

String SVGScriptElement::charsetAttributeValue() const
{
    return String();
}

String SVGScriptElement::typeAttributeValue() const
{
    return getAttribute(SVGNames::typeAttr).string();
}

String SVGScriptElement::languageAttributeValue() const
{
    return String();
}

String SVGScriptElement::forAttributeValue() const
{
    return String();
}

String SVGScriptElement::eventAttributeValue() const
{
    return String();
}

bool SVGScriptElement::asyncAttributeValue() const
{
    return false;
}

bool SVGScriptElement::deferAttributeValue() const
{
    return false;
}

bool SVGScriptElement::hasSourceAttribute() const
{
    return hasAttribute(XLinkNames::hrefAttr);
}

Ref<Element> SVGScriptElement::cloneElementWithoutAttributesAndChildren(Document& targetDocument)
{
    return adoptRef(*new SVGScriptElement(tagQName(), targetDocument, false, alreadyStarted()));
}

#ifndef NDEBUG
bool SVGScriptElement::filterOutAnimatableAttribute(const QualifiedName& name) const
{
    return name == SVGNames::typeAttr;
}
#endif

}
