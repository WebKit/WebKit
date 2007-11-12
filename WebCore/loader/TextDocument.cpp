/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
#include "HTMLViewSourceDocument.h"
#include "SegmentedString.h"
#include "Text.h"
#include "XMLTokenizer.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

TextTokenizer::TextTokenizer(Document* doc)
    : m_doc(doc)
    , m_preElement(0)
    , m_skipLF(false)
{    
    // Allocate buffer
    m_size = 254;
    m_buffer = static_cast<UChar*>(fastMalloc(sizeof(UChar) * m_size));
    m_dest = m_buffer;
}    

TextTokenizer::TextTokenizer(HTMLViewSourceDocument* doc)
    : Tokenizer(true)
    , m_doc(doc)
    , m_preElement(0)
    , m_skipLF(false)
{    
    // Allocate buffer
    m_size = 254;
    m_buffer = static_cast<UChar*>(fastMalloc(sizeof(UChar) * m_size));
    m_dest = m_buffer;
}    

bool TextTokenizer::write(const SegmentedString& s, bool appendData)
{
    ExceptionCode ec;

    m_dest = m_buffer;
    
    SegmentedString str = s;
    while (!str.isEmpty()) {
        UChar c = *str;
        
        if (c == '\r') {
            *m_dest++ = '\n';
            
            // possibly skip an LF in the case of an CRLF sequence
            m_skipLF = true;
        } else if (c == '\n') {
            if (!m_skipLF)
                *m_dest++ = c;
            else
                m_skipLF = false;
        } else {
            *m_dest++ = c;
            m_skipLF = false;
        }
        
        str.advance();
        
        // Maybe enlarge the buffer
        checkBuffer();
    }

    if (!m_preElement && !inViewSourceMode()) {
        RefPtr<Element> rootElement = m_doc->createElementNS(xhtmlNamespaceURI, "html", ec);
        m_doc->appendChild(rootElement, ec);

        RefPtr<Element> body = m_doc->createElementNS(xhtmlNamespaceURI, "body", ec);
        rootElement->appendChild(body, ec);

        RefPtr<Element> preElement = m_doc->createElementNS(xhtmlNamespaceURI, "pre", ec);
        preElement->setAttribute("style", "word-wrap: break-word; white-space: pre-wrap;", ec);

        body->appendChild(preElement, ec);
        
        m_preElement = preElement.get();
    } 
    
    String string = String(m_buffer, m_dest - m_buffer);
    if (inViewSourceMode()) {
        static_cast<HTMLViewSourceDocument*>(m_doc)->addViewSourceText(string);
        return false;
    }

    unsigned charsLeft = string.length();
    while (charsLeft) {
        // split large text to nodes of manageable size
        RefPtr<Text> text = Text::createWithLengthLimit(m_doc, string, charsLeft);
        m_preElement->appendChild(text, ec);
    }

    return false;
}

void TextTokenizer::finish()
{
    m_preElement = 0;
    fastFree(m_buffer);
        
    m_doc->finishedParsing();
}

bool TextTokenizer::isWaitingForScripts() const
{
    // A text document is never waiting for scripts
    return false;
}

TextDocument::TextDocument(DOMImplementation* implementation, Frame* frame)
    : HTMLDocument(implementation, frame)
{
}

Tokenizer* TextDocument::createTokenizer()
{
    return new TextTokenizer(this);
}

}
