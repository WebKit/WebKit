/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>

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
#include "SVGScriptElement.h"

#include "BeforeLoadEvent.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "MappedAttribute.h"
#include "SVGNames.h"

namespace WebCore {

SVGScriptElement::SVGScriptElement(const QualifiedName& tagName, Document* doc, bool createdByParser)
    : SVGElement(tagName, doc)
    , SVGURIReference()
    , SVGExternalResourcesRequired()
    , m_data(this, this)
{
    m_data.setCreatedByParser(createdByParser);
}

SVGScriptElement::~SVGScriptElement()
{
}

String SVGScriptElement::scriptContent() const
{
    return m_data.scriptContent();
}

void SVGScriptElement::parseMappedAttribute(MappedAttribute* attr)
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
        handleSourceAttribute(m_data, href());
    else if (SVGExternalResourcesRequired::isKnownAttribute(attrName)) {
        // Handle dynamic updates of the 'externalResourcesRequired' attribute. Only possible case: changing from 'true' to 'false'
        // causes an immediate dispatch of the SVGLoad event. If the attribute value was 'false' before inserting the script element
        // in the document, the SVGLoad event has already been dispatched.
        if (!externalResourcesRequiredBaseValue() && !m_data.haveFiredLoadEvent() && !m_data.createdByParser()) {
            m_data.setHaveFiredLoadEvent(true);
            ASSERT(haveLoadedRequiredResources());

            sendSVGLoadEventIfPossible();
        }
    }
}

void SVGScriptElement::insertedIntoDocument()
{
    SVGElement::insertedIntoDocument();
    ScriptElement::insertedIntoDocument(m_data, sourceAttributeValue());

    if (m_data.createdByParser())
        return;

    // Eventually send SVGLoad event now for the dynamically inserted script element
    if (!externalResourcesRequiredBaseValue()) {
        m_data.setHaveFiredLoadEvent(true);
        sendSVGLoadEventIfPossible();
    }
}

void SVGScriptElement::removedFromDocument()
{
    SVGElement::removedFromDocument();
    ScriptElement::removedFromDocument(m_data);
}

void SVGScriptElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    ScriptElement::childrenChanged(m_data);
    SVGElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
}

bool SVGScriptElement::isURLAttribute(Attribute* attr) const
{
    return attr->name() == sourceAttributeValue();
}

void SVGScriptElement::finishParsingChildren()
{
    ScriptElement::finishParsingChildren(m_data, sourceAttributeValue());
    SVGElement::finishParsingChildren();

    // A SVGLoad event has been fired by SVGElement::finishParsingChildren.
    if (!externalResourcesRequiredBaseValue())
        m_data.setHaveFiredLoadEvent(true);
}

String SVGScriptElement::type() const
{
    return m_type;
}

void SVGScriptElement::setType(const String& type)
{
    m_type = type;
}

String SVGScriptElement::scriptCharset() const
{
    return m_data.scriptCharset();
}

void SVGScriptElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    SVGElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document()->completeURL(href()));
}

bool SVGScriptElement::haveLoadedRequiredResources()
{
    return !externalResourcesRequiredBaseValue() || m_data.haveFiredLoadEvent();
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

bool SVGScriptElement::dispatchBeforeLoadEvent(const String& sourceURL)
{
    RefPtr<SVGScriptElement> protector(this);
    RefPtr<BeforeLoadEvent> beforeLoadEvent = BeforeLoadEvent::create(sourceURL);
    dispatchEvent(beforeLoadEvent.get());
    return inDocument() && !beforeLoadEvent->defaultPrevented();
}

void SVGScriptElement::dispatchLoadEvent()
{
    bool externalResourcesRequired = externalResourcesRequiredBaseValue();

    if (m_data.createdByParser())
        ASSERT(externalResourcesRequired != m_data.haveFiredLoadEvent());
    else if (m_data.haveFiredLoadEvent()) {
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
        ASSERT(!m_data.haveFiredLoadEvent());

        // Dispatch SVGLoad event
        m_data.setHaveFiredLoadEvent(true);
        ASSERT(haveLoadedRequiredResources());

        sendSVGLoadEventIfPossible();
    }
}

void SVGScriptElement::dispatchErrorEvent()
{
    dispatchEvent(Event::create(eventNames().errorEvent, true, false));
}

}

#endif // ENABLE(SVG)
