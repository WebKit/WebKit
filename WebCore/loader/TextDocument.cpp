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

#include "TextDocument.h"

#include "Element.h"
#include "HTMLNames.h"
#include "SegmentedString.h"
#include "Text.h"
#include "xml_tokenizer.h"

namespace WebCore {

using namespace HTMLNames;
    
class TextTokenizer : public Tokenizer {
public:
    TextTokenizer(Document* doc) : m_doc(doc), m_preElement(0) { }

    virtual bool write(const SegmentedString&, bool appendData);
    virtual void finish();
    virtual bool isWaitingForScripts() const;
    
private:
    Document* m_doc;
    Element* m_preElement;
};

bool TextTokenizer::write(const SegmentedString& s, bool appendData)
{
    ExceptionCode ec;
    
    if (!m_preElement) {
        RefPtr<Element> rootElement = m_doc->createElementNS(xhtmlNamespaceURI, "html", ec);
        m_doc->appendChild(rootElement, ec);

        RefPtr<Element> body = m_doc->createElementNS(xhtmlNamespaceURI, "body", ec);
        rootElement->appendChild(body, ec);

        RefPtr<Element> preElement = m_doc->createElementNS(xhtmlNamespaceURI, "pre", ec);
        body->appendChild(preElement, ec);
        
        m_preElement = preElement.get();
    } 
    
    RefPtr<Text> text = m_doc->createTextNode(s.toString());
    m_preElement->appendChild(text, ec);

    return false;
}

void TextTokenizer::finish()
{
    m_preElement = 0;
    
    m_doc->finishedParsing();
}

bool TextTokenizer::isWaitingForScripts() const
{
    // A text document is never waiting for scripts
    return false;
}

TextDocument::TextDocument(DOMImplementation* implementation, FrameView* v)
    : HTMLDocument(implementation, v)
{
}

Tokenizer* TextDocument::createTokenizer()
{
    return new TextTokenizer(this);
}

}
