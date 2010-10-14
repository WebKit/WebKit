/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorCSSAgent.h"

#if ENABLE(INSPECTOR)

#include "CSSComputedStyleDeclaration.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "CSSPropertySourceData.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleRule.h"
#include "CSSStyleSelector.h"
#include "CSSStyleSheet.h"
#include "DOMWindow.h"
#include "HTMLHeadElement.h"
#include "InspectorDOMAgent.h"
#include "InspectorFrontend.h"
#include "InspectorValues.h"
#include "Node.h"
#include "StyleSheetList.h"

#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>


// cssProperty = {
//    name          : <string>,
//    value         : <string>,
//    priority      : <string>, // "" for non-parsedOk properties
//    implicit      : <boolean>,
//    parsedOk      : <boolean>, // whether property is understood by WebCore
//    status        : <string>, // "disabled" | "active" | "inactive" | "style"
//    shorthandName : <string>,
//    startOffset   : <number>, // Optional - property text start offset in enclosing style declaration. Absent for computed styles and such.
//    endOffset     : <number>, // Optional - property text end offset in enclosing style declaration. Absent for computed styles and such.
// }
//
// status:
// "disabled" == property disabled by user
// "active" == property participates in the computed style calculation
// "inactive" == property does no participate in the computed style calculation (i.e. overridden by a subsequent property with the same name)
// "style" == property is active and originates from the WebCore CSSStyleDeclaration rather than CSS source code (e.g. implicit longhand properties)
//
//
// cssStyle = {
//    styleId         : <number>, // Optional
//    styleSheetId    : <number>, // Parent: -1 for inline styles (<foo style="..">)
//    cssProperties   : [
//                          #cssProperty,
//                          ...
//                          #cssProperty
//                    ],
//    shorthandValues : {
//                          shorthandName1 : shorthandValue1,
//                          shorthandName2 : shorthandValue2
//                      },
//    cssText         : <string>, // declaration text
//    properties      : { } // ???
// }
//
// // TODO:
// // - convert disabledProperties to enabled flag.
// // - convert width, height to properties
//
// cssRule = {
//    ruleId       : <number>,
//    selectorText : <string>
//    sourceURL    : <string>
//    sourceLine   : <string>
//    styleSheetId : <number> // also found in style
//    origin       : <string> // "" || "user-agent" || "user" || "inspector"
//    style        : #cssStyle
// }
//
// // TODO:
// // - fix origin
// // - add sourceURL
// // - fix parentStyleSheetId
//
// cssStyleSheet = {
//    styleSheetId   : <number>
//    href           : <string>
//    title          : <string>
//    disabled       : <boolean>
//    documentNodeId : <number>
//    rules          : [
//                         #cssRule,
//                         ...
//                         #cssRule
//                     ]
// }

namespace WebCore {

// static
PassRefPtr<InspectorObject> InspectorCSSAgent::buildObjectForStyle(CSSStyleDeclaration* style, const String& fullStyleId, CSSStyleSourceData* sourceData)
{
    RefPtr<InspectorObject> result = InspectorObject::create();
    if (!fullStyleId.isEmpty())
        result->setString("id", fullStyleId);

    result->setString("width", style->getPropertyValue("width"));
    result->setString("height", style->getPropertyValue("height"));
    Vector<CSSPropertySourceData>* propertyData = 0;

    if (sourceData) {
        result->setNumber("startOffset", sourceData->styleBodyRange.start);
        result->setNumber("endOffset", sourceData->styleBodyRange.end);
        propertyData = &sourceData->propertyData;
    }
    populateObjectWithStyleProperties(style, result.get(), propertyData);

    return result.release();
}

// static
CSSStyleSheet* InspectorCSSAgent::parentStyleSheet(StyleBase* styleBase)
{
    if (!styleBase)
        return 0;

    StyleSheet* styleSheet = styleBase->stylesheet();
    if (styleSheet && styleSheet->isCSSStyleSheet())
        return static_cast<CSSStyleSheet*>(styleSheet);

    return 0;
}

// static
CSSStyleRule* InspectorCSSAgent::asCSSStyleRule(StyleBase* styleBase)
{
    if (!styleBase->isStyleRule())
        return 0;
    CSSRule* rule = static_cast<CSSRule*>(styleBase);
    if (rule->type() != CSSRule::STYLE_RULE)
        return 0;
    return static_cast<CSSStyleRule*>(rule);
}


InspectorCSSAgent::InspectorCSSAgent(InspectorDOMAgent* domAgent, InspectorFrontend* frontend)
    : m_domAgent(domAgent)
    , m_frontend(frontend)
    , m_lastStyleSheetId(1)
    , m_lastRuleId(1)
    , m_lastStyleId(1)
{
    m_domAgent->setDOMListener(this);
}

InspectorCSSAgent::~InspectorCSSAgent()
{
    reset();
}

void InspectorCSSAgent::reset()
{
    m_domAgent->setDOMListener(0);
}

void InspectorCSSAgent::getMatchedRulesForNode2(long nodeId, RefPtr<InspectorArray>* result)
{
    Element* element = elementForId(nodeId);
    if (!element)
        return;

    CSSStyleSelector* selector = element->ownerDocument()->styleSelector();
    RefPtr<CSSRuleList> matchedRules = selector->styleRulesForElement(element, false, true);
    *result = buildArrayForRuleList(matchedRules.get());
}

void InspectorCSSAgent::getMatchedPseudoRulesForNode2(long nodeId, RefPtr<InspectorArray>* result)
{
    Element* element = elementForId(nodeId);
    if (!element)
        return;

    CSSStyleSelector* selector = element->ownerDocument()->styleSelector();
    for (PseudoId pseudoId = FIRST_PUBLIC_PSEUDOID; pseudoId < AFTER_LAST_INTERNAL_PSEUDOID; pseudoId = static_cast<PseudoId>(pseudoId + 1)) {
        RefPtr<CSSRuleList> matchedRules = selector->pseudoStyleRulesForElement(element, pseudoId, false, true);
        if (matchedRules && matchedRules->length()) {
            RefPtr<InspectorObject> pseudoStyles = InspectorObject::create();
            pseudoStyles->setNumber("pseudoId", static_cast<int>(pseudoId));
            pseudoStyles->setArray("rules", buildArrayForRuleList(matchedRules.get()));
            (*result)->pushObject(pseudoStyles.release());
        }
    }
}

void InspectorCSSAgent::getAttributeStylesForNode2(long nodeId, RefPtr<InspectorValue>* result)
{
    Element* element = elementForId(nodeId);
    if (!element)
        return;

    *result = buildObjectForAttributeStyles(element);
}

void InspectorCSSAgent::getInlineStyleForNode2(long nodeId, RefPtr<InspectorValue>* style)
{
    Element* element = elementForId(nodeId);
    if (!element)
        return;

    InspectorStyleSheetForInlineStyle* styleSheet = asInspectorStyleSheet(element);
    if (!styleSheet)
        return;

    *style = styleSheet->buildObjectForStyle(element->style());
}

void InspectorCSSAgent::getComputedStyleForNode2(long nodeId, RefPtr<InspectorValue>* style)
{
    Element* element = elementForId(nodeId);
    if (!element)
        return;

    DOMWindow* defaultView = element->ownerDocument()->defaultView();
    if (!defaultView)
        return;

    *style = buildObjectForStyle(defaultView->getComputedStyle(element, "").get(), "");
}

void InspectorCSSAgent::getInheritedStylesForNode2(long nodeId, RefPtr<InspectorArray>* style)
{
    Element* element = elementForId(nodeId);
    if (!element) {
        *style = InspectorArray::create();
        return;
    }
    RefPtr<InspectorArray> inheritedStyles = InspectorArray::create();
    Element* parentElement = element->parentElement();
    while (parentElement) {
        RefPtr<InspectorObject> parentStyle = InspectorObject::create();
        if (parentElement->style() && parentElement->style()->length()) {
            InspectorStyleSheetForInlineStyle* styleSheet = asInspectorStyleSheet(element);
            if (styleSheet)
                parentStyle->setObject("inlineStyle", styleSheet->buildObjectForStyle(styleSheet->styleForId("0")));
        }

        CSSStyleSelector* parentSelector = parentElement->ownerDocument()->styleSelector();
        RefPtr<CSSRuleList> parentMatchedRules = parentSelector->styleRulesForElement(parentElement, false, true);
        parentStyle->setArray("matchedCSSRules", buildArrayForRuleList(parentMatchedRules.get()));
        inheritedStyles->pushObject(parentStyle.release());
        parentElement = parentElement->parentElement();
    }
    *style = inheritedStyles.release();
}

void InspectorCSSAgent::getAllStyles2(RefPtr<InspectorArray>* styles)
{
    const ListHashSet<RefPtr<Document> >& documents = m_domAgent->documents();
    for (ListHashSet<RefPtr<Document> >::const_iterator it = documents.begin(); it != documents.end(); ++it) {
        StyleSheetList* list = (*it)->styleSheets();
        for (unsigned i = 0; i < list->length(); ++i) {
            StyleSheet* styleSheet = list->item(i);
            if (styleSheet->isCSSStyleSheet()) {
                InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(static_cast<CSSStyleSheet*>(styleSheet));
                (*styles)->pushString(inspectorStyleSheet->id());
            }
        }
    }
}

void InspectorCSSAgent::getStyleSheet2(const String& styleSheetId, RefPtr<InspectorValue>* styleSheetObject)
{
    InspectorStyleSheet* inspectorStyleSheet = styleSheetForId(styleSheetId);
    if (!inspectorStyleSheet)
        return;

    *styleSheetObject = inspectorStyleSheet->buildObjectForStyleSheet();
}

void InspectorCSSAgent::setStyleSheetText2(const String& styleSheetId, const String& text)
{
    InspectorStyleSheet* inspectorStyleSheet = styleSheetForId(styleSheetId);
    if (!inspectorStyleSheet)
        return;

    inspectorStyleSheet->setText(text);
}

void InspectorCSSAgent::setStyleText2(const String& fullStyleId, const String& text, RefPtr<InspectorValue>* result)
{
    Vector<String> idParts;
    fullStyleId.split(':', idParts);
    ASSERT(idParts.size() == 2);

    InspectorStyleSheet* inspectorStyleSheet = styleSheetForId(idParts.at(0));
    if (!inspectorStyleSheet)
        return;

    if (!inspectorStyleSheet->setStyleText(idParts.at(1), text))
        return;

    *result = inspectorStyleSheet->buildObjectForStyle(inspectorStyleSheet->styleForId(idParts.at(1)));
}

void InspectorCSSAgent::toggleProperty2(const String&, long, bool)
{
    // FIXME(apavlov): implement
}

void InspectorCSSAgent::setRuleSelector2(const String& fullRuleId, const String& selector, RefPtr<InspectorValue>* result)
{
    Vector<String> idParts;
    fullRuleId.split(':', idParts);
    ASSERT(idParts.size() == 2);

    InspectorStyleSheet* inspectorStyleSheet = styleSheetForId(idParts.at(0));
    if (!inspectorStyleSheet)
        return;

    const String& ruleId = idParts.at(1);
    bool success = inspectorStyleSheet->setRuleSelector(ruleId, selector);
    if (!success)
        return;

    *result = inspectorStyleSheet->buildObjectForRule(inspectorStyleSheet->ruleForId(ruleId));
}

void InspectorCSSAgent::addRule2(const long contextNodeId, const String& selector, RefPtr<InspectorValue>* result)
{
    Node* node = m_domAgent->nodeForId(contextNodeId);
    if (!node)
        return;

    InspectorStyleSheet* inspectorStyleSheet = viaInspectorStyleSheet(node->document(), true);
    CSSStyleRule* newRule = inspectorStyleSheet->addRule(selector);
    if (newRule)
        *result = inspectorStyleSheet->buildObjectForRule(newRule);
}

void InspectorCSSAgent::getSupportedCSSProperties(RefPtr<InspectorArray>* cssProperties)
{
    RefPtr<InspectorArray> properties = InspectorArray::create();
    for (int i = 0; i < numCSSProperties; ++i)
        properties->pushString(propertyNameStrings[i]);

    *cssProperties = properties.release();
}

// static
Element* InspectorCSSAgent::inlineStyleElement(CSSStyleDeclaration* style)
{
    if (!style || !style->isMutableStyleDeclaration())
        return 0;
    Node* node = static_cast<CSSMutableStyleDeclaration*>(style)->node();
    if (!node || !node->isStyledElement() || static_cast<StyledElement*>(node)->getInlineStyleDecl() != style)
        return 0;
    return static_cast<Element*>(node);
}

// static
void InspectorCSSAgent::populateObjectWithStyleProperties(CSSStyleDeclaration* style, InspectorObject* result, Vector<CSSPropertySourceData>* propertyData)
{
    RefPtr<InspectorArray> properties = InspectorArray::create();
    RefPtr<InspectorObject> shorthandValues = InspectorObject::create();
    HashMap<String, RefPtr<InspectorObject> > propertyNameToPreviousActiveProperty;
    HashSet<String> foundShorthands;
    HashSet<String> sourcePropertyNames;
    if (propertyData) {
        for (Vector<CSSPropertySourceData>::const_iterator it = propertyData->begin(); it != propertyData->end(); ++it) {
            const CSSPropertySourceData& propertyEntry = *it;
            RefPtr<InspectorObject> property = InspectorObject::create();
            properties->pushObject(property);
            const String& name = propertyEntry.name;
            sourcePropertyNames.add(name);
            property->setString("name", name);
            property->setString("value", propertyEntry.value);
            property->setString("priority", propertyEntry.important ? "important" : "");
            property->setString("status", "active");
            property->setBoolean("parsedOk", propertyEntry.parsedOk);
            property->setNumber("startOffset", propertyEntry.range.start);
            property->setNumber("endOffset", propertyEntry.range.end);
            if (propertyEntry.parsedOk) {
                property->setBoolean("implicit", false);
                String shorthand = style->getPropertyShorthand(name);
                property->setString("shorthandName", shorthand);
                if (!shorthand.isEmpty() && !foundShorthands.contains(shorthand)) {
                    foundShorthands.add(shorthand);
                    shorthandValues->setString(shorthand, shorthandValue(style, shorthand));
                }
            } else {
                property->setBoolean("implicit", false);
                property->setString("shorthandName", "");
            }
            HashMap<String, RefPtr<InspectorObject> >::iterator activeIt = propertyNameToPreviousActiveProperty.find(name);
            if (activeIt != propertyNameToPreviousActiveProperty.end()) {
                activeIt->second->setString("status", "inactive");
                activeIt->second->setString("shorthandName", "");
            }
            propertyNameToPreviousActiveProperty.set(name, property);
        }
    }

    for (int i = 0, size = style->length(); i < size; ++i) {
        String name = style->item(i);
        if (sourcePropertyNames.contains(name))
            continue;

        sourcePropertyNames.add(name);
        RefPtr<InspectorObject> property = InspectorObject::create();
        properties->pushObject(property);
        property->setString("name", name);
        property->setString("value", style->getPropertyValue(name));
        property->setString("priority", style->getPropertyPriority("name"));
        property->setBoolean("implicit", style->isPropertyImplicit(name));
        property->setBoolean("parsedOk", true);
        property->setString("status", "style");
        String shorthand = style->getPropertyShorthand(name);
        property->setString("shorthandName", shorthand);
        if (!shorthand.isEmpty() && !foundShorthands.contains(shorthand)) {
            foundShorthands.add(shorthand);
            shorthandValues->setString(shorthand, shorthandValue(style, shorthand));
        }
    }

    result->setArray("properties", properties);
    result->setObject("shorthandValues", shorthandValues);
}

// static
String InspectorCSSAgent::shorthandValue(CSSStyleDeclaration* style, const String& shorthandProperty)
{
    String value = style->getPropertyValue(shorthandProperty);
    if (value.isEmpty()) {
        for (unsigned i = 0; i < style->length(); ++i) {
            String individualProperty = style->item(i);
            if (style->getPropertyShorthand(individualProperty) != shorthandProperty)
                continue;
            if (style->isPropertyImplicit(individualProperty))
                continue;
            String individualValue = style->getPropertyValue(individualProperty);
            if (individualValue == "initial")
                continue;
            if (value.length())
                value.append(" ");
            value.append(individualValue);
        }
    }
    return value;
}

// static
String InspectorCSSAgent::shorthandPriority(CSSStyleDeclaration* style, const String& shorthandProperty)
{
    String priority = style->getPropertyPriority(shorthandProperty);
    if (priority.isEmpty()) {
        for (unsigned i = 0; i < style->length(); ++i) {
            String individualProperty = style->item(i);
            if (style->getPropertyShorthand(individualProperty) != shorthandProperty)
                continue;
            priority = style->getPropertyPriority(individualProperty);
            break;
        }
    }
    return priority;
}


// static
Vector<String> InspectorCSSAgent::longhandProperties(CSSStyleDeclaration* style, const String& shorthandProperty)
{
    Vector<String> properties;
    HashSet<String> foundProperties;
    for (unsigned i = 0; i < style->length(); ++i) {
        String individualProperty = style->item(i);
        if (foundProperties.contains(individualProperty) || style->getPropertyShorthand(individualProperty) != shorthandProperty)
            continue;

        foundProperties.add(individualProperty);
        properties.append(individualProperty);
    }
    return properties;
}

InspectorStyleSheetForInlineStyle* InspectorCSSAgent::asInspectorStyleSheet(Element* element)
{
    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(element);
    if (it == m_nodeToInspectorStyleSheet.end()) {
        CSSStyleDeclaration* style = element->isStyledElement() ? element->style() : 0;
        if (!style)
            return 0;

        String newStyleSheetId = String::number(m_lastStyleSheetId++);
        RefPtr<InspectorStyleSheetForInlineStyle> inspectorStyleSheet = InspectorStyleSheetForInlineStyle::create(newStyleSheetId, element, "");
        m_idToInspectorStyleSheet.set(newStyleSheetId, inspectorStyleSheet);
        m_nodeToInspectorStyleSheet.set(element, inspectorStyleSheet);
        return inspectorStyleSheet.get();
    }

    return it->second.get();
}

Element* InspectorCSSAgent::elementForId(long nodeId)
{
    Node* node = m_domAgent->nodeForId(nodeId);
    return (!node || node->nodeType() != Node::ELEMENT_NODE) ? 0 : static_cast<Element*>(node);
}

InspectorStyleSheet* InspectorCSSAgent::bindStyleSheet(CSSStyleSheet* styleSheet)
{
    RefPtr<InspectorStyleSheet> inspectorStyleSheet = m_cssStyleSheetToInspectorStyleSheet.get(styleSheet);
    if (!inspectorStyleSheet) {
        String id = String::number(m_lastStyleSheetId++);
        inspectorStyleSheet = InspectorStyleSheet::create(id, styleSheet, detectOrigin(styleSheet, styleSheet->document()), m_domAgent->documentURLString(styleSheet->document()));
        m_idToInspectorStyleSheet.set(id, inspectorStyleSheet);
        m_cssStyleSheetToInspectorStyleSheet.set(styleSheet, inspectorStyleSheet);
    }
    return inspectorStyleSheet.get();
}

InspectorStyleSheet* InspectorCSSAgent::viaInspectorStyleSheet(Document* document, bool createIfAbsent)
{
    if (!document) {
        ASSERT(!createIfAbsent);
        return 0;
    }

    RefPtr<InspectorStyleSheet> inspectorStyleSheet = m_documentToInspectorStyleSheet.get(document);
    if (inspectorStyleSheet || !createIfAbsent)
        return inspectorStyleSheet.get();

    ExceptionCode ec = 0;
    RefPtr<Element> styleElement = document->createElement("style", ec);
    if (!ec)
        styleElement->setAttribute("type", "text/css", ec);
    if (!ec)
        document->head()->appendChild(styleElement, ec);
    if (ec)
        return 0;
    StyleSheetList* styleSheets = document->styleSheets();
    StyleSheet* styleSheet = styleSheets->item(styleSheets->length() - 1);
    if (!styleSheet->isCSSStyleSheet())
        return 0;
    CSSStyleSheet* cssStyleSheet = static_cast<CSSStyleSheet*>(styleSheet);
    String id = String::number(m_lastStyleSheetId++);
    inspectorStyleSheet = InspectorStyleSheet::create(id, cssStyleSheet, "inspector", m_domAgent->documentURLString(document));
    m_idToInspectorStyleSheet.set(id, inspectorStyleSheet);
    m_cssStyleSheetToInspectorStyleSheet.set(cssStyleSheet, inspectorStyleSheet);
    m_documentToInspectorStyleSheet.set(document, inspectorStyleSheet);
    return inspectorStyleSheet.get();
}

InspectorStyleSheet* InspectorCSSAgent::styleSheetForId(const String& styleSheetId)
{
    IdToInspectorStyleSheet::iterator it = m_idToInspectorStyleSheet.find(styleSheetId);
    return it == m_idToInspectorStyleSheet.end() ? 0 : it->second.get();
}

String InspectorCSSAgent::detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument)
{
    DEFINE_STATIC_LOCAL(String, userAgent, ("user-agent"));
    DEFINE_STATIC_LOCAL(String, user, ("user"));
    DEFINE_STATIC_LOCAL(String, inspector, ("inspector"));

    String origin("");
    if (pageStyleSheet && !pageStyleSheet->ownerNode() && pageStyleSheet->href().isEmpty())
        origin = userAgent;
    else if (pageStyleSheet && pageStyleSheet->ownerNode() && pageStyleSheet->ownerNode()->nodeName() == "#document")
        origin = user;
    else {
        InspectorStyleSheet* viaInspectorStyleSheetForOwner = viaInspectorStyleSheet(ownerDocument, false);
        if (viaInspectorStyleSheetForOwner && pageStyleSheet == viaInspectorStyleSheetForOwner->pageStyleSheet())
            origin = inspector;
    }
    return origin;
}

PassRefPtr<InspectorArray> InspectorCSSAgent::buildArrayForRuleList(CSSRuleList* ruleList)
{
    RefPtr<InspectorArray> result = InspectorArray::create();
    if (!ruleList)
        return result.release();

    for (unsigned i = 0, size = ruleList->length(); i < size; ++i) {
        CSSStyleRule* rule = asCSSStyleRule(ruleList->item(i));
        if (!rule)
            continue;

        InspectorStyleSheet* styleSheet = bindStyleSheet(parentStyleSheet(rule));
        if (styleSheet)
            result->pushObject(styleSheet->buildObjectForRule(rule));
    }
    return result.release();
}

PassRefPtr<InspectorObject> InspectorCSSAgent::buildObjectForAttributeStyles(Element* element)
{
    RefPtr<InspectorObject> styleAttributes = InspectorObject::create();
    NamedNodeMap* attributes = element->attributes();
    for (unsigned i = 0; attributes && i < attributes->length(); ++i) {
        Attribute* attribute = attributes->attributeItem(i);
        if (attribute->style()) {
            String attributeName = attribute->localName();
            styleAttributes->setObject(attributeName.utf8().data(), buildObjectForStyle(attribute->style(), ""));
        }
    }

    return styleAttributes;
}

void InspectorCSSAgent::didRemoveDocument(Document* document)
{
    m_documentToInspectorStyleSheet.remove(document);
}

void InspectorCSSAgent::didRemoveDOMNode(Node* node)
{
    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(node);
    if (it == m_nodeToInspectorStyleSheet.end())
        return;

    m_idToInspectorStyleSheet.remove(it->second->id());
    m_nodeToInspectorStyleSheet.remove(node);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
