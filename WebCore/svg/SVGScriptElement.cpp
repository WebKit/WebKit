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

#if ENABLE(SVG)
#include "SVGScriptElement.h"

#include "Attribute.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "SVGNames.h"

namespace WebCore {

inline SVGScriptElement::SVGScriptElement(const QualifiedName& tagName, Document* document, bool createdByParser, bool isEvaluated)
    : SVGElement(tagName, document)
    , ScriptElement(this, createdByParser, isEvaluated)
{
}

PassRefPtr<SVGScriptElement> SVGScriptElement::create(const QualifiedName& tagName, Document* document, bool createdByParser)
{
    return adoptRef(new SVGScriptElement(tagName, document, createdByParser, false));
}

void SVGScriptElement::parseMappedAttribute(Attribute* attr)
{
    const QualifiedName& attrName = attr->name();

    if (attrName == SVGNames::typeAttr)
        setType(attr->value());
    else {
        if (SVGURIReference::parseMappedAttribute(attr))
            return;
        if (SVGExternalResourcesRequired::parseMappedAttribute(attr))
            return;

        SVGElement::parseMappedAttribute(attr);
    }
}

void SVGScriptElement::svgAttributeChanged(const QualifiedName& attrName)
{
    SVGElement::svgAttributeChanged(attrName);

    if (SVGURIReference::isKnownAttribute(attrName))
        handleSourceAttribute(href());
    else if (SVGExternalResourcesRequired::isKnownAttribute(attrName)) {
        // Handle dynamic updates of the 'externalResourcesRequired' attribute. Only possible case: changing from 'true' to 'false'
        // causes an immediate dispatch of the SVGLoad event. If the attribute value was 'false' before inserting the script element
        // in the document, the SVGLoad event has already been dispatched.
        if (!externalResourcesRequiredBaseValue() && !haveFiredLoadEvent() && !createdByParser()) {
            setHaveFiredLoadEvent(true);
            ASSERT(haveLoadedRequiredResources());

            sendSVGLoadEventIfPossible();
        }
    }
}

void SVGScriptElement::synchronizeProperty(const QualifiedName& attrName)
{
    SVGElement::synchronizeProperty(attrName);

    if (attrName == anyQName()) {
        synchronizeExternalResourcesRequired();
        synchronizeHref();
        return;
    }

    if (SVGExternalResourcesRequired::isKnownAttribute(attrName))
        synchronizeExternalResourcesRequired();
    else if (SVGURIReference::isKnownAttribute(attrName))
        synchronizeHref();
}

void SVGScriptElement::insertedIntoDocument()
{
    SVGElement::insertedIntoDocument();
    ScriptElement::insertedIntoDocument(sourceAttributeValue());

    if (createdByParser())
        return;

    // Eventually send SVGLoad event now for the dynamically inserted script element
    if (!externalResourcesRequiredBaseValue()) {
        setHaveFiredLoadEvent(true);
        sendSVGLoadEventIfPossible();
    }
}

void SVGScriptElement::removedFromDocument()
{
    SVGElement::removedFromDocument();
    ScriptElement::removedFromDocument();
}

void SVGScriptElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    ScriptElement::childrenChanged();
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
}

bool SVGScriptElement::isURLAttribute(Attribute* attr) const
{
    return attr->name() == sourceAttributeValue();
}

void SVGScriptElement::finishParsingChildren()
{
    ScriptElement::finishParsingChildren(sourceAttributeValue());
    SVGElement::finishParsingChildren();

    // A SVGLoad event has been fired by SVGElement::finishParsingChildren.
    if (!externalResourcesRequiredBaseValue())
        setHaveFiredLoadEvent(true);
}

String SVGScriptElement::type() const
{
    return m_type;
}

void SVGScriptElement::setType(const String& type)
{
    m_type = type;
}

void SVGScriptElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    SVGElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document()->completeURL(href()));
}

bool SVGScriptElement::haveLoadedRequiredResources()
{
    return !externalResourcesRequiredBaseValue() || haveFiredLoadEvent();
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
    return type();
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

void SVGScriptElement::dispatchLoadEvent()
{
    bool externalResourcesRequired = externalResourcesRequiredBaseValue();

    if (createdByParser())
        ASSERT(externalResourcesRequired != haveFiredLoadEvent());
    else if (haveFiredLoadEvent()) {
        // If we've already fired an load event and externalResourcesRequired is set to 'true'
        // externalResourcesRequired has been modified while loading the <script>. Don't dispatch twice.
        if (externalResourcesRequired)
            return;
    }

    // HTML and SVG differ completly in the 'onload' event handling of <script> elements.
    // HTML fires the 'load' event after it sucessfully loaded a remote resource, otherwhise an error event.
    // SVG fires the SVGLoad event immediately after parsing the <script> element, if externalResourcesRequired
    // is set to 'false', otherwhise it dispatches the 'SVGLoad' event just after loading the remote resource.
    if (externalResourcesRequired) {
        ASSERT(!haveFiredLoadEvent());

        // Dispatch SVGLoad event
        setHaveFiredLoadEvent(true);
        ASSERT(haveLoadedRequiredResources());

        sendSVGLoadEventIfPossible();
    }
}

void SVGScriptElement::dispatchErrorEvent()
{
    dispatchEvent(Event::create(eventNames().errorEvent, true, false));
}

PassRefPtr<Element> SVGScriptElement::cloneElementWithoutAttributesAndChildren() const
{
    return adoptRef(new SVGScriptElement(tagQName(), document(), false, isEvaluated()));
}

}

#endif // ENABLE(SVG)
