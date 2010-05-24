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

#include "config.h"
#include "HTML5TreeBuilder.h"

#include "Attribute.h"
#include "HTML5Lexer.h"
#include "HTML5Token.h"
#include "HTMLDocument.h"
#include "HTMLParser.h"
#include "HTMLTokenizer.h"
#include "NotImplemented.h"

namespace WebCore {

HTML5TreeBuilder::HTML5TreeBuilder(HTML5Lexer* lexer, HTMLDocument* document, bool reportErrors)
    : m_document(document)
    , m_reportErrors(reportErrors)
    , m_lexer(lexer)
    , m_legacyHTMLParser(new HTMLParser(document, reportErrors))
{
}

HTML5TreeBuilder::~HTML5TreeBuilder()
{
}

static void convertToOldStyle(HTML5Token& token, Token& oldStyleToken)
{
    switch (token.type()) {
    case HTML5Token::Uninitialized:
        ASSERT_NOT_REACHED();
        break;
    case HTML5Token::DOCTYPE:
    case HTML5Token::EndOfFile:
        ASSERT_NOT_REACHED();
        notImplemented();
        break;
    case HTML5Token::StartTag:
    case HTML5Token::EndTag: {
        oldStyleToken.beginTag = (token.type() == HTML5Token::StartTag);
        oldStyleToken.selfClosingTag = token.selfClosing();
        oldStyleToken.tagName = token.name();
        HTML5Token::AttributeList& attributes = token.attributes();
        for (HTML5Token::AttributeList::iterator iter = attributes.begin();
             iter != attributes.end(); ++iter) {
            if (!iter->m_name.isEmpty()) {
                String name = String(StringImpl::adopt(iter->m_name));
                String value = String(StringImpl::adopt(iter->m_value));
                RefPtr<Attribute> mappedAttribute = Attribute::createMapped(name, value);
                if (!oldStyleToken.attrs)
                    oldStyleToken.attrs = NamedNodeMap::create();
                oldStyleToken.attrs->insertAttribute(mappedAttribute.release(), false);
            }
        }
        break;
    }
    case HTML5Token::Comment:
        oldStyleToken.tagName = commentAtom;
        oldStyleToken.text = token.data().impl();
        break;
    case HTML5Token::Character:
        oldStyleToken.tagName = textAtom;
        oldStyleToken.text = token.characters().impl();
        break;
    }
}

PassRefPtr<Node> HTML5TreeBuilder::passTokenToLegacyParser(HTML5Token& token)
{
    if (token.type() == HTML5Token::StartTag && token.name() == "script") {
        // This work is supposed to be done by the parser, but
        // when using the old parser for we have to do this manually.
        m_lexer->setState(HTML5Lexer::ScriptDataState);
    }
    // For now, we translate into an old-style token for testing.
    Token oldStyleToken;
    convertToOldStyle(token, oldStyleToken);

    return m_legacyHTMLParser->parseToken(&oldStyleToken);
}

PassRefPtr<Node> HTML5TreeBuilder::constructTreeFromToken(HTML5Token& token)
{
    return passTokenToLegacyParser(token);
    // Our HTML5 parser implementation will go here in a separate patch.
}

void HTML5TreeBuilder::finished()
{
    m_legacyHTMLParser->finished();
}

}
