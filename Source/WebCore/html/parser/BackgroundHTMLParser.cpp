/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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

#if ENABLE(THREADED_HTML_PARSER)

#include "BackgroundHTMLParser.h"

#include "HTMLDocumentParser.h"
#include "HTMLNames.h"
#include "HTMLParserThread.h"
#include "HTMLTokenizer.h"
#include "MathMLNames.h"
#include "SVGNames.h"
#include <wtf/MainThread.h>
#include <wtf/Vector.h>
#include <wtf/text/TextPosition.h>

namespace WebCore {

using namespace HTMLNames;

#ifndef NDEBUG

static void checkThatTokensAreSafeToSendToAnotherThread(const CompactHTMLTokenStream* tokens)
{
    for (size_t i = 0; i < tokens->size(); ++i)
        ASSERT(tokens->at(i).isSafeToSendToAnotherThread());
}

#endif

// FIXME: Tune this constant based on a benchmark. The current value was choosen arbitrarily.
static const size_t pendingTokenLimit = 4000;

static bool threadSafeEqual(StringImpl* a, StringImpl* b)
{
    if (a->hash() != b->hash())
        return false;
    return StringHash::equal(a, b);
}

static bool threadSafeMatch(const String& localName, const QualifiedName& qName)
{
    return threadSafeEqual(localName.impl(), qName.localName().impl());
}

ParserMap& parserMap()
{
    // This initialization assumes that this will be initialize on the main thread before
    // any parser thread is started.
    static ParserMap* sParserMap = new ParserMap;
    return *sParserMap;
}

ParserMap::BackgroundParserMap& ParserMap::backgroundParsers()
{
    ASSERT(HTMLParserThread::shared()->threadId() == currentThread());
    return m_backgroundParsers;
}

BackgroundHTMLParser::BackgroundHTMLParser(const HTMLParserOptions& options, const WeakPtr<HTMLDocumentParser>& parser)
    : m_inForeignContent(false)
    , m_token(adoptPtr(new HTMLToken))
    , m_tokenizer(HTMLTokenizer::create(options))
    , m_options(options)
    , m_parser(parser)
    , m_pendingTokens(adoptPtr(new CompactHTMLTokenStream))
{
}

void BackgroundHTMLParser::append(const String& input)
{
    m_input.append(SegmentedString(input));
    pumpTokenizer();
}

void BackgroundHTMLParser::finish()
{
    markEndOfFile();
    pumpTokenizer();
}

void BackgroundHTMLParser::markEndOfFile()
{
    // FIXME: This should use InputStreamPreprocessor::endOfFileMarker
    // once InputStreamPreprocessor is split off into its own header.
    const LChar endOfFileMarker = 0;

    ASSERT(!m_input.isClosed());
    m_input.append(SegmentedString(String(&endOfFileMarker, 1)));
    m_input.close();
}

bool BackgroundHTMLParser::simulateTreeBuilder(const CompactHTMLToken& token)
{
    if (token.type() == HTMLTokenTypes::StartTag) {
        const String& tagName = token.data();
        if (threadSafeMatch(tagName, SVGNames::svgTag)
            || threadSafeMatch(tagName, MathMLNames::mathTag))
            m_inForeignContent = true;

        // FIXME: This is just a copy of Tokenizer::updateStateFor which uses threadSafeMatches.
        if (threadSafeMatch(tagName, textareaTag) || threadSafeMatch(tagName, titleTag))
            m_tokenizer->setState(HTMLTokenizerState::RCDATAState);
        else if (threadSafeMatch(tagName, plaintextTag))
            m_tokenizer->setState(HTMLTokenizerState::PLAINTEXTState);
        else if (threadSafeMatch(tagName, scriptTag))
            m_tokenizer->setState(HTMLTokenizerState::ScriptDataState);
        else if (threadSafeMatch(tagName, styleTag)
            || threadSafeMatch(tagName, iframeTag)
            || threadSafeMatch(tagName, xmpTag)
            || (threadSafeMatch(tagName, noembedTag) && m_options.pluginsEnabled)
            || threadSafeMatch(tagName, noframesTag)
            || (threadSafeMatch(tagName, noscriptTag) && m_options.scriptEnabled))
            m_tokenizer->setState(HTMLTokenizerState::RAWTEXTState);
    }

    if (token.type() == HTMLTokenTypes::EndTag) {
        const String& tagName = token.data();
        if (threadSafeMatch(tagName, SVGNames::svgTag) || threadSafeMatch(tagName, MathMLNames::mathTag))
            m_inForeignContent = false;
        if (threadSafeMatch(tagName, scriptTag))
            return false;
    }

    // FIXME: Need to set setForceNullCharacterReplacement based on m_inForeignContent as well.
    m_tokenizer->setShouldAllowCDATA(m_inForeignContent);
    return true;
}

void BackgroundHTMLParser::pumpTokenizer()
{
    while (m_tokenizer->nextToken(m_input, *m_token.get())) {
        m_pendingTokens->append(CompactHTMLToken(m_token.get(), TextPosition(m_input.currentLine(), m_input.currentColumn())));
        m_token->clear();

        if (!simulateTreeBuilder(m_pendingTokens->last()) || m_pendingTokens->size() >= pendingTokenLimit)
            sendTokensToMainThread();
    }

    sendTokensToMainThread();
}

void BackgroundHTMLParser::sendTokensToMainThread()
{
    if (m_pendingTokens->isEmpty())
        return;

#ifndef NDEBUG
    checkThatTokensAreSafeToSendToAnotherThread(m_pendingTokens.get());
#endif

    callOnMainThread(bind(&HTMLDocumentParser::didReceiveTokensFromBackgroundParser, m_parser, m_pendingTokens.release()));

    m_pendingTokens = adoptPtr(new CompactHTMLTokenStream);
}

void BackgroundHTMLParser::createPartial(ParserIdentifier identifier, const HTMLParserOptions& options, const WeakPtr<HTMLDocumentParser>& parser)
{
    ASSERT(!parserMap().backgroundParsers().get(identifier));
    parserMap().backgroundParsers().set(identifier, BackgroundHTMLParser::create(options, parser));
}

void BackgroundHTMLParser::stopPartial(ParserIdentifier identifier)
{
    parserMap().backgroundParsers().remove(identifier);
}

void BackgroundHTMLParser::appendPartial(ParserIdentifier identifier, const String& input)
{
    ASSERT(!input.impl() || input.impl()->hasOneRef() || input.isEmpty());
    if (BackgroundHTMLParser* parser = parserMap().backgroundParsers().get(identifier))
        parser->append(input);
}

void BackgroundHTMLParser::finishPartial(ParserIdentifier identifier)
{
    if (BackgroundHTMLParser* parser = parserMap().backgroundParsers().get(identifier))
        parser->finish();
}

}

#endif // ENABLE(THREADED_HTML_PARSER)
