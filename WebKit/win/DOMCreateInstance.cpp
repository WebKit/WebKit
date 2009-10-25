/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This file is a temporary hack to implement the createInstance methods for
// the COM DOM bindings.

#include "config.h"
#include "DOMCreateInstance.h"

#include "GEN_DOMNode.h"
#include "GEN_DOMElement.h"
#include "GEN_DOMDocument.h"
#include "GEN_DOMAttr.h"
#include "GEN_DOMText.h"
#include "GEN_DOMCDATASection.h"
#include "GEN_DOMEntityReference.h"
#include "GEN_DOMEntity.h"
#include "GEN_DOMProcessingInstruction.h"
#include "GEN_DOMComment.h"
#include "GEN_DOMHTMLElement.h"
#include "GEN_DOMHTMLDocument.h"
#include "GEN_DOMHTMLCollection.h"
#include "GEN_DOMHTMLOptionsCollection.h"
#include "GEN_DOMDocumentType.h"
#include "GEN_DOMDocumentFragment.h"
#include "GEN_DOMNotation.h"
#include "GEN_DOMCSSCharsetRule.h"
#include "GEN_DOMCSSFontFaceRule.h"
#include "GEN_DOMCSSImportRule.h"
#include "GEN_DOMCSSMediaRule.h"
#include "GEN_DOMCSSPageRule.h"
#include "GEN_DOMCSSPrimitiveValue.h"
#include "GEN_DOMCSSRule.h"
#include "GEN_DOMCSSRuleList.h"
#include "GEN_DOMCSSStyleDeclaration.h"
#include "GEN_DOMCSSStyleRule.h"
#include "GEN_DOMCSSStyleSheet.h"
#include "GEN_DOMCSSValueList.h"
#include "GEN_DOMStyleSheet.h"
#include "GEN_DOMDOMImplementation.h"
#include "GEN_DOMNamedNodeMap.h"
#include "GEN_DOMNodeList.h"
#include "GEN_DOMCounter.h"
#include "GEN_DOMCSSRuleList.h"
#include "GEN_DOMCSSStyleDeclaration.h"
#include "GEN_DOMMediaList.h"
#include "GEN_DOMRect.h"
#include "GEN_DOMStyleSheet.h"
#include "GEN_DOMStyleSheetList.h"
#include "GEN_DOMEvent.h"
#include "GEN_DOMEvent.h"
#include <wtf/HashMap.h>

#pragma warning(push, 0)
#include <WebCore/Node.h>
#include <WebCore/Element.h>
#include <WebCore/Document.h>
#include <WebCore/Attr.h>
#include <WebCore/Text.h>
#include <WebCore/COMPtr.h>
#include <WebCore/CDATASection.h>
#include <WebCore/EntityReference.h>
#include <WebCore/Entity.h>
#include <WebCore/ProcessingInstruction.h>
#include <WebCore/Comment.h>
#include <WebCore/HTMLDocument.h>
#include <WebCore/HTMLElement.h>
#include <WebCore/HTMLCollection.h>
#include <WebCore/HTMLOptionsCollection.h>
#include <WebCore/DocumentType.h>
#include <WebCore/DocumentFragment.h>
#include <WebCore/Notation.h>
#include <WebCore/CSSCharsetRule.h>
#include <WebCore/CSSFontFaceRule.h>
#include <WebCore/CSSImportRule.h>
#include <WebCore/CSSMediaRule.h>
#include <WebCore/CSSPageRule.h>
#include <WebCore/CSSPrimitiveValue.h>
#include <WebCore/CSSRule.h>
#include <WebCore/CSSRuleList.h>
#include <WebCore/CSSStyleDeclaration.h>
#include <WebCore/CSSStyleRule.h>
#include <WebCore/CSSStyleSheet.h>
#include <WebCore/CSSValueList.h>
#include <WebCore/StyleSheet.h>
#include <WebCore/DOMImplementation.h>
#include <WebCore/NamedNodeMap.h>
#include <WebCore/NodeList.h>
#include <WebCore/Counter.h>
#include <WebCore/CSSRuleList.h>
#include <WebCore/CSSStyleDeclaration.h>
#include <WebCore/MediaList.h>
#include <WebCore/Rect.h>
#include <WebCore/StyleSheet.h>
#include <WebCore/StyleSheetList.h>
#include <WebCore/Event.h>
#include <WebCore/EventListener.h>
#pragma warning(pop)

typedef HashMap<void*, GEN_DOMObject*> DOMWrapperCache;

static DOMWrapperCache& domWrapperCache()
{
    static DOMWrapperCache cache;
    return cache;
}

GEN_DOMObject* getDOMWrapper(void* objectHandle)
{
    return domWrapperCache().get(objectHandle);
}

void setDOMWrapper(void* objectHandle, GEN_DOMObject* wrapper)
{
    domWrapperCache().set(objectHandle, wrapper);
}

void removeDOMWrapper(void* objectHandle)
{
    domWrapperCache().remove(objectHandle);
}

#define COM_DOM_PREFIX(Type) GEN_DOM##Type
#define CREATE_ONLY_SELF(Type) \
    COM_DOM_PREFIX(Type)* COM_DOM_PREFIX(Type)::createInstance(WebCore::Type* impl) \
    { \
        if (!impl) \
            return 0; \
        if (GEN_DOMObject* cachedInstance = getDOMWrapper(impl)) { \
            cachedInstance->AddRef(); \
            return static_cast<COM_DOM_PREFIX(Type)*>(cachedInstance); \
        } \
        COMPtr<COM_DOM_PREFIX(Type)> comDOMObject = new COM_DOM_PREFIX(Type)(impl); \
        setDOMWrapper(impl, comDOMObject.get()); \
        return comDOMObject.releaseRef(); \
    } \

// Core

GEN_DOMNode* GEN_DOMNode::createInstance(WebCore::Node* node)
{
    if (!node)
        return 0;

    if (GEN_DOMObject* cachedInstance = getDOMWrapper(node)) {
        cachedInstance->AddRef();
        return static_cast<GEN_DOMNode*>(cachedInstance);
    }

    COMPtr<GEN_DOMNode> domNode;
    switch (node->nodeType()) {
        case WebCore::Node::ELEMENT_NODE:
            // FIXME: add support for creating subclasses of HTMLElement.
            // FIXME: add support for creating SVGElements and its subclasses.
            if (node->isHTMLElement())
                domNode = new GEN_DOMHTMLElement(static_cast<WebCore::HTMLElement*>(node));
            else
                domNode = new GEN_DOMElement(static_cast<WebCore::Element*>(node));
            break;
        case WebCore::Node::ATTRIBUTE_NODE:
            domNode = new GEN_DOMAttr(static_cast<WebCore::Attr*>(node));
            break;
        case WebCore::Node::TEXT_NODE:
            domNode = new GEN_DOMText(static_cast<WebCore::Text*>(node));
            break;
        case WebCore::Node::CDATA_SECTION_NODE:
            domNode = new GEN_DOMCDATASection(static_cast<WebCore::CDATASection*>(node));
            break;
        case WebCore::Node::ENTITY_REFERENCE_NODE:
            domNode = new GEN_DOMEntityReference(static_cast<WebCore::EntityReference*>(node));
            break;
        case WebCore::Node::ENTITY_NODE:
            domNode = new GEN_DOMEntity(static_cast<WebCore::Entity*>(node));
            break;
        case WebCore::Node::PROCESSING_INSTRUCTION_NODE:
            domNode = new GEN_DOMProcessingInstruction(static_cast<WebCore::ProcessingInstruction*>(node));
            break;
        case WebCore::Node::COMMENT_NODE:
            domNode = new GEN_DOMComment(static_cast<WebCore::Comment*>(node));
            break;
        case WebCore::Node::DOCUMENT_NODE:
            // FIXME: add support for SVGDocument.
            if (static_cast<WebCore::Document*>(node)->isHTMLDocument())
                domNode = new GEN_DOMHTMLDocument(static_cast<WebCore::HTMLDocument*>(node));
            else
                domNode = new GEN_DOMDocument(static_cast<WebCore::Document*>(node));
            break;
        case WebCore::Node::DOCUMENT_TYPE_NODE:
            domNode = new GEN_DOMDocumentType(static_cast<WebCore::DocumentType*>(node));
            break;
        case WebCore::Node::DOCUMENT_FRAGMENT_NODE:
            domNode = new GEN_DOMDocumentFragment(static_cast<WebCore::DocumentFragment*>(node));
            break;
        case WebCore::Node::NOTATION_NODE:
            domNode = new GEN_DOMNotation(static_cast<WebCore::Notation*>(node));
            break;
        default:
            domNode = new GEN_DOMNode(node);
            break;
    }

    setDOMWrapper(node, domNode.get());
    return domNode.releaseRef();
}

GEN_DOMImplementation* GEN_DOMImplementation::createInstance(WebCore::DOMImplementation* impl)
{
    if (!impl)
        return 0;

    if (GEN_DOMObject* cachedInstance = getDOMWrapper(impl)) {
        cachedInstance->AddRef();
        return static_cast<GEN_DOMImplementation*>(cachedInstance);
    }

    COMPtr<GEN_DOMImplementation> comDOMObject = new GEN_DOMImplementation(impl);
    setDOMWrapper(impl, comDOMObject.get());
    return comDOMObject.releaseRef();
}

CREATE_ONLY_SELF(NamedNodeMap)
CREATE_ONLY_SELF(NodeList)

// Events

// FIXME: Add the subclasses for Event when they get generated.
CREATE_ONLY_SELF(Event)


// CSS

GEN_DOMCSSRule* GEN_DOMCSSRule::createInstance(WebCore::CSSRule* rule)
{
    if (!rule)
        return 0;

    if (GEN_DOMObject* cachedInstance = getDOMWrapper(rule)) {
        cachedInstance->AddRef();
        return static_cast<GEN_DOMCSSRule*>(cachedInstance);
    }

    COMPtr<GEN_DOMCSSRule> domRule;
    switch (rule->type()) {
        case WebCore::CSSRule::STYLE_RULE:
            domRule = new GEN_DOMCSSStyleRule(static_cast<WebCore::CSSStyleRule*>(rule));
            break;
        case WebCore::CSSRule::CHARSET_RULE:
            domRule = new GEN_DOMCSSCharsetRule(static_cast<WebCore::CSSCharsetRule*>(rule));
            break;
        case WebCore::CSSRule::IMPORT_RULE:
            domRule = new GEN_DOMCSSImportRule(static_cast<WebCore::CSSImportRule*>(rule));
            break;
        case WebCore::CSSRule::MEDIA_RULE:
            domRule = new GEN_DOMCSSMediaRule(static_cast<WebCore::CSSMediaRule*>(rule));
            break;
        case WebCore::CSSRule::FONT_FACE_RULE:
            domRule = new GEN_DOMCSSFontFaceRule(static_cast<WebCore::CSSFontFaceRule*>(rule));
            break;
        case WebCore::CSSRule::PAGE_RULE:
            domRule = new GEN_DOMCSSPageRule(static_cast<WebCore::CSSPageRule*>(rule));
            break;
        case WebCore::CSSRule::UNKNOWN_RULE:
        default:
            domRule = new GEN_DOMCSSRule(rule);
            break;
    }

    setDOMWrapper(rule, domRule.get());
    return domRule.releaseRef();
}

GEN_DOMStyleSheet* GEN_DOMStyleSheet::createInstance(WebCore::StyleSheet* styleSheet)
{
    if (!styleSheet)
        return 0;

    if (GEN_DOMObject* cachedInstance = getDOMWrapper(styleSheet)) {
        cachedInstance->AddRef();
        return static_cast<GEN_DOMStyleSheet*>(cachedInstance);
    }

    COMPtr<GEN_DOMStyleSheet> domStyleSheet;
    if (styleSheet->isCSSStyleSheet())
        domStyleSheet = new GEN_DOMCSSStyleSheet(static_cast<WebCore::CSSStyleSheet*>(styleSheet));
    else
        domStyleSheet = new GEN_DOMStyleSheet(styleSheet);

    setDOMWrapper(styleSheet, domStyleSheet.get());
    return domStyleSheet.releaseRef();
}


GEN_DOMCSSValue* GEN_DOMCSSValue::createInstance(WebCore::CSSValue* value)
{
    if (!value)
        return 0;

    if (GEN_DOMObject* cachedInstance = getDOMWrapper(value)) {
        cachedInstance->AddRef();
        return static_cast<GEN_DOMCSSValue*>(cachedInstance);
    }

    COMPtr<GEN_DOMCSSValue> domValue;
    switch (value->cssValueType()) {
        case WebCore::CSSValue::CSS_PRIMITIVE_VALUE:
            domValue = new GEN_DOMCSSPrimitiveValue(static_cast<WebCore::CSSPrimitiveValue*>(value));
            break;
        case WebCore::CSSValue::CSS_VALUE_LIST:
            domValue = new GEN_DOMCSSValueList(static_cast<WebCore::CSSValueList*>(value));
            break;
        case WebCore::CSSValue::CSS_INHERIT:
            domValue = new GEN_DOMCSSValue(value);
            break;
        case WebCore::CSSValue::CSS_CUSTOM:
            // FIXME: add support for SVGPaint and SVGColor
            domValue = new GEN_DOMCSSValue(value);
            break;
    }

    setDOMWrapper(value, domValue.get());
    return domValue.releaseRef();
}

CREATE_ONLY_SELF(Counter)
CREATE_ONLY_SELF(CSSRuleList)
CREATE_ONLY_SELF(CSSStyleDeclaration)
CREATE_ONLY_SELF(MediaList)
CREATE_ONLY_SELF(Rect)
CREATE_ONLY_SELF(StyleSheetList)


// HTML

CREATE_ONLY_SELF(HTMLCollection)
CREATE_ONLY_SELF(HTMLOptionsCollection)
