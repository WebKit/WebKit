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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "HTML5Tokenizer.h"

#include "HTML5Lexer.h"
#include "HTML5Token.h"
#include "HTML5TreeBuilder.h"
#include "Node.h"
#include "NotImplemented.h"

namespace WebCore {

HTML5Tokenizer::HTML5Tokenizer(HTMLDocument* document, bool reportErrors)
    : Tokenizer()
    , m_lexer(new HTML5Lexer)
    , m_treeBuilder(new HTML5TreeBuilder(m_lexer.get(), document, reportErrors))
{
    begin();
}

HTML5Tokenizer::~HTML5Tokenizer()
{
}

void HTML5Tokenizer::begin()
{
}

void HTML5Tokenizer::write(const SegmentedString& source, bool)
{
    m_source.append(source);

    HTML5Token token;
    while (m_lexer->nextToken(m_source, token)) {
        m_treeBuilder->constructTreeFromToken(token);
        token.clear();
    }
}

void HTML5Tokenizer::end()
{
    m_treeBuilder->finished();
}

void HTML5Tokenizer::finish()
{
    end();
}

bool HTML5Tokenizer::isWaitingForScripts() const
{
    notImplemented();
    return false;
}

}
