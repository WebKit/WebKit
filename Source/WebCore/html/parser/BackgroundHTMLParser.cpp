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
#include "HTMLParserIdioms.h"
#include "HTMLParserThread.h"
#include "HTMLTokenizer.h"
#include "MathMLNames.h"
#include "SVGNames.h"
#include "XSSAuditor.h"
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

static void checkThatPreloadsAreSafeToSendToAnotherThread(const PreloadRequestStream& preloads)
{
    for (size_t i = 0; i < preloads.size(); ++i)
        ASSERT(preloads[i]->isSafeToSendToAnotherThread());
}

#endif

static inline bool tokenExitsForeignContent(const CompactHTMLToken& token)
{
    // FIXME: This is copied from HTMLTreeBuilder::processTokenInForeignContent and changed to use threadSafeMatch.
    const String& tagName = token.data();
    return threadSafeMatch(tagName, bTag)
        || threadSafeMatch(tagName, bigTag)
        || threadSafeMatch(tagName, blockquoteTag)
        || threadSafeMatch(tagName, bodyTag)
        || threadSafeMatch(tagName, brTag)
        || threadSafeMatch(tagName, centerTag)
        || threadSafeMatch(tagName, codeTag)
        || threadSafeMatch(tagName, ddTag)
        || threadSafeMatch(tagName, divTag)
        || threadSafeMatch(tagName, dlTag)
        || threadSafeMatch(tagName, dtTag)
        || threadSafeMatch(tagName, emTag)
        || threadSafeMatch(tagName, embedTag)
        || threadSafeMatch(tagName, h1Tag)
        || threadSafeMatch(tagName, h2Tag)
        || threadSafeMatch(tagName, h3Tag)
        || threadSafeMatch(tagName, h4Tag)
        || threadSafeMatch(tagName, h5Tag)
        || threadSafeMatch(tagName, h6Tag)
        || threadSafeMatch(tagName, headTag)
        || threadSafeMatch(tagName, hrTag)
        || threadSafeMatch(tagName, iTag)
        || threadSafeMatch(tagName, imgTag)
        || threadSafeMatch(tagName, liTag)
        || threadSafeMatch(tagName, listingTag)
        || threadSafeMatch(tagName, menuTag)
        || threadSafeMatch(tagName, metaTag)
        || threadSafeMatch(tagName, nobrTag)
        || threadSafeMatch(tagName, olTag)
        || threadSafeMatch(tagName, pTag)
        || threadSafeMatch(tagName, preTag)
        || threadSafeMatch(tagName, rubyTag)
        || threadSafeMatch(tagName, sTag)
        || threadSafeMatch(tagName, smallTag)
        || threadSafeMatch(tagName, spanTag)
        || threadSafeMatch(tagName, strongTag)
        || threadSafeMatch(tagName, strikeTag)
        || threadSafeMatch(tagName, subTag)
        || threadSafeMatch(tagName, supTag)
        || threadSafeMatch(tagName, tableTag)
        || threadSafeMatch(tagName, ttTag)
        || threadSafeMatch(tagName, uTag)
        || threadSafeMatch(tagName, ulTag)
        || threadSafeMatch(tagName, varTag)
        || (threadSafeMatch(tagName, fontTag) && (token.getAttributeItem(colorAttr) || token.getAttributeItem(faceAttr) || token.getAttributeItem(sizeAttr)));
}

static const size_t pendingTokenLimit = 1000;

BackgroundHTMLParser::BackgroundHTMLParser(PassRefPtr<WeakReference<BackgroundHTMLParser> > reference, PassOwnPtr<Configuration> config)
    : m_weakFactory(reference, this)
    , m_token(adoptPtr(new HTMLToken))
    , m_tokenizer(HTMLTokenizer::create(config->options))
    , m_options(config->options)
    , m_parser(config->parser)
    , m_pendingTokens(adoptPtr(new CompactHTMLTokenStream))
    , m_xssAuditor(config->xssAuditor.release())
    , m_preloadScanner(config->preloadScanner.release())
{
    m_namespaceStack.append(HTML);
}

void BackgroundHTMLParser::append(const String& input)
{
    m_input.append(input);
    pumpTokenizer();
}

void BackgroundHTMLParser::resumeFrom(PassOwnPtr<Checkpoint> checkpoint)
{
    m_parser = checkpoint->parser;
    m_token = checkpoint->token.release();
    m_tokenizer = checkpoint->tokenizer.release();
    m_input.rewindTo(checkpoint->inputCheckpoint, checkpoint->unparsedInput);
    m_preloadScanner->rewindTo(checkpoint->preloadScannerCheckpoint);
    pumpTokenizer();
}

void BackgroundHTMLParser::finish()
{
    markEndOfFile();
    pumpTokenizer();
}

void BackgroundHTMLParser::stop()
{
    delete this;
}

void BackgroundHTMLParser::forcePlaintextForTextDocument()
{
    // This is only used by the TextDocumentParser (a subclass of HTMLDocumentParser)
    // to force us into the PLAINTEXT state w/o using a <plaintext> tag.
    // The TextDocumentParser uses a <pre> tag for historical/compatibility reasons.
    m_tokenizer->setState(HTMLTokenizer::PLAINTEXTState);
}

void BackgroundHTMLParser::markEndOfFile()
{
    ASSERT(!m_input.current().isClosed());
    m_input.append(String(&kEndOfFileMarker, 1));
    m_input.close();
}

bool BackgroundHTMLParser::simulateTreeBuilder(const CompactHTMLToken& token)
{
    if (token.type() == HTMLToken::StartTag) {
        const String& tagName = token.data();
        if (threadSafeMatch(tagName, SVGNames::svgTag))
            m_namespaceStack.append(SVG);
        if (threadSafeMatch(tagName, MathMLNames::mathTag))
            m_namespaceStack.append(MathML);
        if (inForeignContent() && tokenExitsForeignContent(token))
            m_namespaceStack.removeLast();
        // FIXME: Support tags that exit MathML.
        if (m_namespaceStack.last() == SVG && equalIgnoringCase(tagName, SVGNames::foreignObjectTag.localName()))
            m_namespaceStack.append(HTML);
        if (!inForeignContent()) {
            // FIXME: This is just a copy of Tokenizer::updateStateFor which uses threadSafeMatches.
            if (threadSafeMatch(tagName, textareaTag) || threadSafeMatch(tagName, titleTag))
                m_tokenizer->setState(HTMLTokenizer::RCDATAState);
            else if (threadSafeMatch(tagName, plaintextTag))
                m_tokenizer->setState(HTMLTokenizer::PLAINTEXTState);
            else if (threadSafeMatch(tagName, scriptTag))
                m_tokenizer->setState(HTMLTokenizer::ScriptDataState);
            else if (threadSafeMatch(tagName, styleTag)
                || threadSafeMatch(tagName, iframeTag)
                || threadSafeMatch(tagName, xmpTag)
                || (threadSafeMatch(tagName, noembedTag) && m_options.pluginsEnabled)
                || threadSafeMatch(tagName, noframesTag)
                || (threadSafeMatch(tagName, noscriptTag) && m_options.scriptEnabled))
                m_tokenizer->setState(HTMLTokenizer::RAWTEXTState);
        }
    }

    if (token.type() == HTMLToken::EndTag) {
        const String& tagName = token.data();
        // FIXME: Support tags that exit MathML.
        if ((m_namespaceStack.last() == SVG && threadSafeMatch(tagName, SVGNames::svgTag))
            || (m_namespaceStack.last() == MathML && threadSafeMatch(tagName, MathMLNames::mathTag))
            || (m_namespaceStack.contains(SVG) && m_namespaceStack.last() == HTML && equalIgnoringCase(tagName, SVGNames::foreignObjectTag.localName())))
            m_namespaceStack.removeLast();
        if (threadSafeMatch(tagName, scriptTag)) {
            if (!inForeignContent())
                m_tokenizer->setState(HTMLTokenizer::DataState);
            return false;
        }
    }

    // FIXME: Need to set setForceNullCharacterReplacement based on m_inForeignContent as well.
    m_tokenizer->setShouldAllowCDATA(inForeignContent());
    return true;
}

void BackgroundHTMLParser::pumpTokenizer()
{
    while (true) {
        m_sourceTracker.start(m_input.current(), m_tokenizer.get(), *m_token);
        if (!m_tokenizer->nextToken(m_input.current(), *m_token.get()))
            break;
        m_sourceTracker.end(m_input.current(), m_tokenizer.get(), *m_token);

        {
            OwnPtr<XSSInfo> xssInfo = m_xssAuditor->filterToken(FilterTokenRequest(*m_token, m_sourceTracker, m_tokenizer->shouldAllowCDATA()));
            CompactHTMLToken token(m_token.get(), TextPosition(m_input.current().currentLine(), m_input.current().currentColumn()));

            if (xssInfo)
                token.setXSSInfo(xssInfo.release());

            m_preloadScanner->scan(token, m_pendingPreloads);

            m_pendingTokens->append(token);
        }

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
    checkThatPreloadsAreSafeToSendToAnotherThread(m_pendingPreloads);
#endif

    OwnPtr<HTMLDocumentParser::ParsedChunk> chunk = adoptPtr(new HTMLDocumentParser::ParsedChunk);
    chunk->tokens = m_pendingTokens.release();
    chunk->preloads.swap(m_pendingPreloads);
    chunk->inputCheckpoint = m_input.createCheckpoint();
    chunk->preloadScannerCheckpoint = m_preloadScanner->createCheckpoint();
    callOnMainThread(bind(&HTMLDocumentParser::didReceiveParsedChunkFromBackgroundParser, m_parser, chunk.release()));

    m_pendingTokens = adoptPtr(new CompactHTMLTokenStream);
}

}

#endif // ENABLE(THREADED_HTML_PARSER)
