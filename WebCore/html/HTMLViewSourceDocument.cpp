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
#include "HTMLTableElement.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableSectionElement.h"
#include "Text.h"
#include "HTMLNames.h"

namespace WebCore
{

using namespace HTMLNames;

HTMLViewSourceDocument::HTMLViewSourceDocument(DOMImplementation* implementation, Frame* frame)
    : HTMLDocument(implementation, frame)
    , m_current(0)
    , m_tbody(0)
    , m_td(0)
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
        Element* table = new HTMLTableElement(this);
        body->addChild(table);
        table->attach();
        m_tbody = new HTMLTableSectionElement(tbodyTag, this);
        table->addChild(m_tbody);
        m_tbody->attach();
        m_current = m_tbody;
    }
    
    if (token->tagName == textAtom)
        addText(token->text.get(), "");
    else if (token->tagName == commentAtom) {
        if (token->beginTag) {
            m_current = addSpanWithClassName("webkit-html-comment");
            addText(String("<!--") + token->text.get() + "-->", "webkit-html-comment");
        }
    } else {
        // Handle the tag.
        bool doctype = token->tagName.startsWith("!DOCTYPE", true);
        
        String classNameStr = doctype ? "webkit-html-doctype" : "webkit-html-tag";
        m_current = addSpanWithClassName(classNameStr);

        String text = "<";
        if (!token->beginTag)
            text += "/";
        text += token->tagName;
        Vector<UChar>* guide = token->m_sourceInfo.get();
        if (!guide || !guide->size())
            text += ">";

        addText(text, classNameStr);

        // Walk our guide string that tells us where attribute names/values should go.
        if (guide && guide->size()) {
            unsigned size = guide->size();
            unsigned begin = 0;
            unsigned currAttr = 0;
            for (unsigned i = 0; i < size; i++) {
                if (guide->at(i) == 'a' || guide->at(i) == 'x' || guide->at(i) == 'v') {
                    // Add in the string.
                    addText(String((UChar*)(guide->data()) + begin, i - begin), classNameStr);
                     
                    begin = i + 1;

                    if (token->attrs && currAttr < token->attrs->length()) {
                        if (guide->at(i) == 'a') {
                            Attribute* attr = token->attrs->attributeItem(currAttr);
                            String name = attr->name().toString();
                            String nameClassStr = doctype ? "webkit-html-doctype" : (name == "/" ? "webkit-html-tag" : "webkit-html-attribute-name");
                            addText(name, nameClassStr);
                            if (attr->value().isNull() || attr->value().isEmpty())
                                currAttr++;
                        } else {
                            Attribute* attr = token->attrs->attributeItem(currAttr);
                            String value = attr->value().domString();
                            if (doctype)
                                addText(value, "webkit-html-doctype");
                            else {
                                m_current = addSpanWithClassName("webkit-html-attribute-value");
                                addText(value, "webkit-html-attribute-value");
                                if (m_current != m_tbody)
                                    m_current = m_current->parent();
                            }
                            currAttr++;
                        }
                    }
                }
            }
            
            // Add in any string that might be left.
            if (begin < size)
                addText(String((UChar*)(guide->data()) + begin, size - begin), classNameStr);

            // Add in the end tag.
            addText(">", classNameStr);
        }
        
        m_current = m_td;
    }
}

Element* HTMLViewSourceDocument::addSpanWithClassName(const String& className)
{
    if (m_current == m_tbody)
        addLine(className);
    Element* span = new HTMLElement(spanTag, this);
    Attribute* a = new MappedAttribute(classAttr, className);
    NamedMappedAttrMap* attrs = new NamedMappedAttrMap(0);
    attrs->insertAttribute(a, true);   
    span->setAttributeMap(attrs);     
    m_current->addChild(span);
    span->attach();
    return span;
}

void HTMLViewSourceDocument::addLine(const String& className)
{
    // Create a table row.
    Element* trow = new HTMLTableRowElement(this);
    m_tbody->addChild(trow);
    trow->attach();
    
    // Create a cell that will hold the line number (it is generated in the stylesheet using counters).
    Element* td = new HTMLTableCellElement(tdTag, this);
    Attribute* classNameAttr = new MappedAttribute(classAttr, "webkit-line-number");
    NamedMappedAttrMap* attrs = new NamedMappedAttrMap(0);
    attrs->insertAttribute(classNameAttr, true);

    td->setAttributeMap(attrs);     
    trow->addChild(td);
    td->attach();

    // Create a second cell for the line contents
    td = new HTMLTableCellElement(tdTag, this);
    classNameAttr = new MappedAttribute(classAttr, "webkit-line-content");
    attrs = new NamedMappedAttrMap(0);
    attrs->insertAttribute(classNameAttr, true);
    td->setAttributeMap(attrs);     
    trow->addChild(td);
    td->attach();
    m_current = m_td = td;

    // Open up the needed spans.
    if (!className.isEmpty()) {
        if (className == "webkit-html-attribute-name" || className == "webkit-html-attribute-value")
            m_current = addSpanWithClassName("webkit-html-tag");
        m_current = addSpanWithClassName(className);
    }
}

void HTMLViewSourceDocument::addText(const String& text, const String& className)
{
    if (text.isEmpty())
        return;

    // Add in the content, splitting on newlines.
    Vector<String> lines = text.split('\n', true);
    unsigned size = lines.size();
    for (unsigned i = 0; i < size; i++) {
        String substring = lines[i];
        if (substring.isEmpty()) {
            if (i == size - 1)
                break;
            substring = " ";
        }
        if (m_current == m_tbody)
            addLine(className);
        Text* t = new Text(this, substring);
        m_current->addChild(t);
        t->attach();
        if (i < size - 1)
            m_current = m_tbody;
    }
    
    // Set current to m_tbody if the last character was a newline.
    if (text[text.length() - 1] == '\n')
        m_current = m_tbody;
}

}
