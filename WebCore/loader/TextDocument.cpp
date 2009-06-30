/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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

class TextTokenizer : public Tokenizer {
public:
    TextTokenizer(Document*);
    virtual ~TextTokenizer();
    TextTokenizer(HTMLViewSourceDocument*);

    virtual void write(const SegmentedString&, bool appendData);
    virtual void finish();
    virtual bool isWaitingForScripts() const;
    
    inline void checkBuffer(int len = 10)
    {
        if ((m_dest - m_buffer) > m_size - len) {
            // Enlarge buffer
            int newSize = std::max(m_size * 2, m_size + len);
            int oldOffset = m_dest - m_buffer;
            m_buffer = static_cast<UChar*>(fastRealloc(m_buffer, newSize * sizeof(UChar)));
            m_dest = m_buffer + oldOffset;
            m_size = newSize;
        }
    }
        
private:
    Document* m_doc;
    Element* m_preElement;

    bool m_skipLF;
    
    int m_size;
    UChar* m_buffer;
    UChar* m_dest;
};

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

TextTokenizer::~TextTokenizer()
{
    // finish() should have been called to prevent any leaks
    ASSERT(!m_buffer);
}

void TextTokenizer::write(const SegmentedString& s, bool)
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
        RefPtr<Element> rootElement = m_doc->createElement(htmlTag, false);
        m_doc->appendChild(rootElement, ec);

        RefPtr<Element> body = m_doc->createElement(bodyTag, false);
        rootElement->appendChild(body, ec);

        RefPtr<Element> preElement = m_doc->createElement(preTag, false);
        preElement->setAttribute("style", "word-wrap: break-word; white-space: pre-wrap;", ec);

        body->appendChild(preElement, ec);
        
        m_preElement = preElement.get();
    } 
    
    String string = String(m_buffer, m_dest - m_buffer);
    if (inViewSourceMode()) {
        static_cast<HTMLViewSourceDocument*>(m_doc)->addViewSourceText(string);
        return;
    }

    unsigned charsLeft = string.length();
    while (charsLeft) {
        // split large text to nodes of manageable size
        RefPtr<Text> text = Text::createWithLengthLimit(m_doc, string, charsLeft);
        m_preElement->appendChild(text, ec);
    }
}

void TextTokenizer::finish()
{
    if (!m_preElement)
        write(SegmentedString(), true); // Create document structure for an empty text document.
    m_preElement = 0;
    fastFree(m_buffer);
    m_buffer = 0;
    m_dest = 0;

    m_doc->finishedParsing();
}

bool TextTokenizer::isWaitingForScripts() const
{
    // A text document is never waiting for scripts
    return false;
}

TextDocument::TextDocument(Frame* frame)
    : HTMLDocument(frame)
{
}

Tokenizer* TextDocument::createTokenizer()
{
    return new TextTokenizer(this);
}

Tokenizer* createTextTokenizer(HTMLViewSourceDocument* document)
{
    return new TextTokenizer(document);
}

}
