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
#include "HTMLParserThread.h"
#include "HTMLTokenizer.h"
#include <wtf/MainThread.h>
#include <wtf/Vector.h>

namespace WebCore {

#ifndef NDEBUG

static void checkThatTokensAreSafeToSendToAnotherThread(const Vector<CompactHTMLToken>& tokens)
{
    for (size_t i = 0; i < tokens.size(); ++i)
        ASSERT(tokens[i].isSafeToSendToAnotherThread());
}

#endif

typedef const void* ParserIdentifier;
class HTMLDocumentParser;

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

ParserMap::MainThreadParserMap& ParserMap::mainThreadParsers()
{
    ASSERT(isMainThread());
    return m_mainThreadParsers;
}

BackgroundHTMLParser::BackgroundHTMLParser(const HTMLParserOptions& options, ParserIdentifier identifier)
    : m_isPausedWaitingForScripts(false)
    , m_inForeignContent(false)
    , m_tokenizer(HTMLTokenizer::create(options))
    , m_options(options)
    , m_parserIdentifer(identifier)
{
}

void BackgroundHTMLParser::append(const String& input)
{
    m_input.appendToEnd(input);
    pumpTokenizer();
}

void BackgroundHTMLParser::continueParsing()
{
    m_isPausedWaitingForScripts = false;
    pumpTokenizer();
}

void BackgroundHTMLParser::finish()
{
    ASSERT(!m_input.haveSeenEndOfFile());
    m_input.markEndOfFile();
    pumpTokenizer();
}

void BackgroundHTMLParser::pumpTokenizer()
{
    if (m_isPausedWaitingForScripts)
        return;

    // It's unclear whether we want to use AtomicStrings on the background
    // thread. We will likely eventually use libdispatch to schedule parsing
    // in a separate sequenced queue for each HTMLDocumentParser instance.
    // Once we do that, the code below will be unsafe because libdispatch
    // might schedule us on many different threads.
    DEFINE_STATIC_LOCAL(AtomicString, iframeTag, ("iframe", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, mathTag, ("math", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, noembedTag, ("noembed", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, noframesTag, ("noframes", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, noscriptTag, ("noscript", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, plaintextTag, ("plaintext", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, scriptTag, ("script", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, styleTag, ("style", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, svgTag, ("svg", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, textareaTag, ("textarea", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, titleTag, ("title", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(AtomicString, xmpTag, ("xmp", AtomicString::ConstructFromLiteral));

    while (m_tokenizer->nextToken(m_input.current(), m_token)) {
        m_pendingTokens.append(CompactHTMLToken(m_token));

        if (m_token.type() == HTMLTokenTypes::StartTag) {
            AtomicString tagName(m_token.name().data(), m_token.name().size());
            if (tagName == svgTag || tagName == mathTag)
                m_inForeignContent = true;

            // FIXME: This is just a copy of Tokenizer::updateStateFor which doesn't use HTMLNames.
            if (tagName == textareaTag || tagName == titleTag)
                m_tokenizer->setState(HTMLTokenizerState::RCDATAState);
            else if (tagName == plaintextTag)
                m_tokenizer->setState(HTMLTokenizerState::PLAINTEXTState);
            else if (tagName == scriptTag)
                m_tokenizer->setState(HTMLTokenizerState::ScriptDataState);
            else if (tagName == styleTag
                || tagName == iframeTag
                || tagName == xmpTag
                || (tagName == noembedTag && m_options.pluginsEnabled)
                || tagName == noframesTag
                || (tagName == noscriptTag && m_options.scriptEnabled))
                m_tokenizer->setState(HTMLTokenizerState::RAWTEXTState);
        }
        if (m_token.type() == HTMLTokenTypes::EndTag) {
            AtomicString tagName(m_token.name().data(), m_token.name().size());
            if (tagName == svgTag || tagName == mathTag)
                m_inForeignContent = false;
            if (tagName == scriptTag) {
                m_isPausedWaitingForScripts = true;
                m_token.clear();
                break;
            }
        }
        // FIXME: Need to set setForceNullCharacterReplacement based on m_inForeignContent as well.
        m_tokenizer->setShouldAllowCDATA(m_inForeignContent);
        m_token.clear();
    }

#ifndef NDEBUG
    checkThatTokensAreSafeToSendToAnotherThread(m_pendingTokens);
#endif

    sendTokensToMainThread();
}

class TokenDelivery {
    WTF_MAKE_NONCOPYABLE(TokenDelivery);
public:
    TokenDelivery()
        : identifier(0)
        , isPausedWaitingForScripts(false)
    {
    }

    ParserIdentifier identifier;
    Vector<CompactHTMLToken> tokens;
    // FIXME: This bool will be replaced by a CheckPoint object once
    // we implement speculative parsing. Then the main thread will decide
    // to either accept the speculative tokens we've already given it
    // (or ask for them, depending on who ends up owning them), or send
    // us a "reset to checkpoint message".
    bool isPausedWaitingForScripts;

    static void execute(void* context)
    {
        TokenDelivery* delivery = static_cast<TokenDelivery*>(context);
        HTMLDocumentParser* parser = parserMap().mainThreadParsers().get(delivery->identifier);
        if (parser)
            parser->didReceiveTokensFromBackgroundParser(delivery->tokens, delivery->isPausedWaitingForScripts);
        // FIXME: Ideally we wouldn't need to call delete manually. Instead
        // we would like an API where the message queue owns the tasks and
        // takes care of deleting them.
        delete delivery;
    }
};

void BackgroundHTMLParser::sendTokensToMainThread()
{
    TokenDelivery* delivery = new TokenDelivery;
    delivery->identifier = m_parserIdentifer;
    delivery->tokens.swap(m_pendingTokens);
    delivery->isPausedWaitingForScripts = m_isPausedWaitingForScripts;
    callOnMainThread(TokenDelivery::execute, delivery);
}

void BackgroundHTMLParser::createPartial(ParserIdentifier identifier, HTMLParserOptions options)
{
    ASSERT(!parserMap().backgroundParsers().get(identifier));
    parserMap().backgroundParsers().set(identifier, BackgroundHTMLParser::create(options, identifier));
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

void BackgroundHTMLParser::continuePartial(ParserIdentifier identifier)
{
    if (BackgroundHTMLParser* parser = parserMap().backgroundParsers().get(identifier))
        parser->continueParsing();
}

void BackgroundHTMLParser::finishPartial(ParserIdentifier identifier)
{
    if (BackgroundHTMLParser* parser = parserMap().backgroundParsers().get(identifier))
        parser->finish();
}

}

#endif // ENABLE(THREADED_HTML_PARSER)
