/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef HTML5TreeBuilder_h
#define HTML5TreeBuilder_h

#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {
class Document;
class HTML5Lexer;
class HTML5Token;
class HTMLDocument;
class HTMLParser;
class Node;

class HTML5TreeBuilder : public Noncopyable {
public:
    HTML5TreeBuilder(HTML5Lexer*, HTMLDocument*, bool reportErrors);
    ~HTML5TreeBuilder();

    // The token really should be passed as a const& since it's never modified.
    PassRefPtr<Node> constructTreeFromToken(HTML5Token&);
    void finished();

private:
    PassRefPtr<Node> passTokenToLegacyParser(HTML5Token&);
    PassRefPtr<Node> processToken(HTML5Token&, UChar currentCharacter = 0);

    // We could grab m_document off the lexer if we wanted to save space.
    Document* m_document;
    bool m_reportErrors;
    // HTML5 spec requires that we be able to change the state of the lexer
    // from within parser actions.
    HTML5Lexer* m_lexer;

    // We're re-using logic from the old HTMLParser while this class is being written.
    OwnPtr<HTMLParser> m_legacyHTMLParser;
};

}

#endif
