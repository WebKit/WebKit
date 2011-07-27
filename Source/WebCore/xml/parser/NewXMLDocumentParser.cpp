/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "NewXMLDocumentParser.h"

#include "SegmentedString.h"

namespace WebCore {

NewXMLDocumentParser::NewXMLDocumentParser(Document* document)
    : ScriptableDocumentParser(document)
    , m_tokenizer(XMLTokenizer::create())
{
}

TextPosition0 NewXMLDocumentParser::textPosition() const
{
    return TextPosition0(WTF::ZeroBasedNumber::fromZeroBasedInt(0),
                         WTF::ZeroBasedNumber::fromZeroBasedInt(0));
}

int NewXMLDocumentParser::lineNumber() const
{
    return 0;
}

void NewXMLDocumentParser::insert(const SegmentedString&)
{
    ASSERT_NOT_REACHED();
}

void NewXMLDocumentParser::append(const SegmentedString& string)
{
    SegmentedString input = string;
    while (!input.isEmpty()) {
        if (!m_tokenizer->nextToken(input, m_token))
            continue;

#ifndef NDEBUG
        m_token.print();
#endif

        if (m_token.type() == XMLTokenTypes::EndOfFile)
            break;

        m_token.clear();
        ASSERT(m_token.isUninitialized());
    }
}

void NewXMLDocumentParser::finish()
{
}

void NewXMLDocumentParser::detach()
{
    ScriptableDocumentParser::detach();
}

bool NewXMLDocumentParser::hasInsertionPoint()
{
    return false;
}

bool NewXMLDocumentParser::finishWasCalled()
{
    return false;
}

bool NewXMLDocumentParser::processingData() const
{
    return false;
}

void NewXMLDocumentParser::prepareToStopParsing()
{
}

void NewXMLDocumentParser::stopParsing()
{
}

bool NewXMLDocumentParser::isWaitingForScripts() const
{
    return false;
}

bool NewXMLDocumentParser::isExecutingScript() const
{
    return false;
}

void NewXMLDocumentParser::executeScriptsWaitingForStylesheets()
{
}

}
