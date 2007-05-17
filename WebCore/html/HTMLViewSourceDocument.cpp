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

HTMLViewSourceDocument::HTMLViewSourceDocument(DOMImplementation* implementation, Frame* frame)
    : HTMLDocument(implementation, frame)
    , m_current(0)
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
        Attribute* a = new MappedAttribute(styleAttr, "white-space:pre-wrap;margin:0;word-wrap:break-word");
        NamedMappedAttrMap* attrs = new NamedMappedAttrMap(0);
        attrs->insertAttribute(a);   
        pre->setAttributeMap(attrs);     
        body->addChild(pre);
        pre->attach();
        m_current = pre;
    }
    
    if (token->tagName == textAtom) {
        Text* t = new Text(this, token->text.get());
        m_current->addChild(t);
        t->attach();
    } else if (token->tagName == commentAtom) {
        if (token->beginTag) {
            Element* span = addSpanWithClassName("webkit-html-comment");
            Text* t = new Text(this, String("<!--") + token->text.get() + "-->"); // FIXME: If the comment was degenerate, would be good to show the original here.
            span->addChild(t);
            t->attach();
        }
    } else {
        // Handle the tag.
        bool doctype = token->tagName.startsWith("!DOCTYPE", true);
        
        Element* span = addSpanWithClassName(doctype ? "webkit-html-doctype" : "webkit-html-tag");
        String text = "<";
        if (!token->beginTag)
            text += "/";
        text += token->tagName;
        Vector<UChar>* guide = token->m_sourceInfo.get();
        if (!guide || !guide->size())
            text += ">";

        Text* t = new Text(this, text);
        span->addChild(t);
        t->attach();
            
        // Walk our guide string that tells us where attribute names/values should go.
        if (guide && guide->size()) {
            unsigned size = guide->size();
            unsigned begin = 0;
            unsigned currAttr = 0;
            m_current = span;
            for (unsigned i = 0; i < size; i++) {
                if (guide->at(i) == 'a' || guide->at(i) == 'x' || guide->at(i) == 'v') {
                    // Add in the string.
                    Text* t = new Text(this, String((UChar*)(guide->data()) + begin, i - begin));
                    span->addChild(t);
                    t->attach();
                    
                    begin = i + 1;

                    if (token->attrs && currAttr < token->attrs->length()) {
                        if (guide->at(i) == 'a') {
                            Attribute* attr = token->attrs->attributeItem(currAttr);
                            String name = attr->name().toString();
                            Element* attrSpan = doctype || name == "/" ? span : addSpanWithClassName("webkit-html-attribute-name");
                            t = new Text(this, name);
                            attrSpan->addChild(t);
                            t->attach();
                            if (attr->value().isNull() || attr->value().isEmpty())
                                currAttr++;
                        } else {
                            Attribute* attr = token->attrs->attributeItem(currAttr);
                            String value = attr->value().domString();
                            Element* attrSpan = doctype ? span : addSpanWithClassName("webkit-html-attribute-value");
                            t = new Text(this, value);
                            attrSpan->addChild(t);
                            t->attach();
                            currAttr++;
                        }
                    }
                }
            }
            
            // Add in any string that might be left.
            if (begin < size) {
                Text* t = new Text(this, String((UChar*)(guide->data()) + begin, size - begin));
                span->addChild(t);
                t->attach();
            }

            // Add in the end tag.
            Text* t = new Text(this, ">");
            span->addChild(t);
            t->attach();
            
            m_current = span->parentNode();
        }
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
