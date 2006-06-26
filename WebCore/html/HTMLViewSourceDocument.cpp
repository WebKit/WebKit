/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "HTMLViewSourceDocument.h"
#include "HTMLTokenizer.h"
#include "HTMLHtmlElement.h"
#include "HTMLBodyElement.h"
#include "HTMLPreElement.h"
#include "Text.h"
#include "HTMLNames.h"

namespace WebCore
{

using namespace HTMLNames;

HTMLViewSourceDocument::HTMLViewSourceDocument(DOMImplementation* implementation, FrameView* v)
    : HTMLDocument(implementation, v), m_current(0)
{
}

Tokenizer* HTMLViewSourceDocument::createTokenizer()
{
    return new HTMLTokenizer(this);
}

void HTMLViewSourceDocument::addViewSourceToken(Token* token)
{
    if (!m_current) {
        // Go ahead and create our <html> and <body>
        Element* html = new HTMLHtmlElement(this);
        addChild(html);
        html->attach();
        Element* body = new HTMLBodyElement(this);
        html->addChild(body);
        body->attach();
        Element* pre = new HTMLPreElement(preTag, this);
        body->addChild(pre);
        pre->attach();
        m_current = pre;
    }
    
    if (token->tagName == textAtom) {
        Text* t = new Text(this, token->text.get());
        m_current->addChild(t);
        t->attach();
    } else if (token->tagName == commentAtom) {
    
    } else {
        // Handle the tag.
        Element* span = addSpanWithClassName("webkit-html-tag");
        String text = "<";
        if (!token->beginTag)
            text += "/";
        text += token->tagName;
        if (!token->attrs)
            text += ">";
        Text* t = new Text(this, text);
        span->addChild(t);
        t->attach();
        
        // Now handle attributes.
        if (token->attrs) {
            unsigned length = token->attrs->length();
            for (unsigned i = 0; i < length; i++)
                addViewSourceAttribute(token->attrs->attributeItem(i));
        
            span = addSpanWithClassName("webkit-html-tag");
            t = new Text(this, ">");
            span->addChild(t);
            t->attach();
        }
        
    }
}

void HTMLViewSourceDocument::addViewSourceAttribute(Attribute* attr)
{
    // FIXME: Don't just hard-code the whitespace between attribute values.
    Text* t = new Text(this, " ");
    m_current->addChild(t);
    t->attach();
    
    // Attribute name.
    // FIXME: Losing whitespace between the name and the = sign and between the = sign and the value.
    Element* span = addSpanWithClassName("webkit-html-attribute-name");
    String name = attr->name().toString();
    if (!attr->value().isNull())
        name += "=";
    t = new Text(this, name);
    span->addChild(t);
    t->attach();
    
    // Attribute value.
    if (!attr->value().isNull()) {
        Element* span = addSpanWithClassName("webkit-html-attribute-value");
        String text = "\""; // FIXME: Don't assume the attr is quoted.
        text += attr->value();
        text += "\"";
        t = new Text(this, text);
        span->addChild(t);
        t->attach();
    }
}

Element* HTMLViewSourceDocument::addSpanWithClassName(const String& className)
{
    Element* span = new HTMLElement(spanTag, this);
    Attribute* a = new MappedAttribute(classAttr, className);
    NamedMappedAttrMap* attrs = new NamedMappedAttrMap(0);
    attrs->insertAttribute(a);   
    span->setAttributeMap(attrs);     
    m_current->addChild(span);
    span->attach();
    return span;
}

}
