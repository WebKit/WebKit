/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef HTMLViewSourceDocument_h
#define HTMLViewSourceDocument_h

#include "HTMLDocument.h"

namespace WebCore {

class DoctypeToken;
struct Token;

class HTMLViewSourceDocument : public HTMLDocument {
public:
    static PassRefPtr<HTMLViewSourceDocument> create(Frame* frame, const String& mimeType)
    {
        return new HTMLViewSourceDocument(frame, mimeType);
    }
    
    virtual Tokenizer* createTokenizer();
    
    void addViewSourceToken(Token*); // Used by the HTML tokenizer.
    void addViewSourceText(const String&); // Used by the plaintext tokenizer.
    void addViewSourceDoctypeToken(DoctypeToken*);

private:
    HTMLViewSourceDocument(Frame*, const String& mimeType);

    void createContainingTable();
    Element* addSpanWithClassName(const String&);
    void addLine(const String& className);
    void addText(const String& text, const String& className);
    Element* addLink(const String& url, bool isAnchor);

    String m_type;
    Element* m_current;
    Element* m_tbody;
    Element* m_td;
};

}

#endif // HTMLViewSourceDocument_h
