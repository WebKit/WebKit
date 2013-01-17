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

#ifndef BackgroundHTMLParser_h
#define BackgroundHTMLParser_h

#if ENABLE(THREADED_HTML_PARSER)

#include "CompactHTMLToken.h"
#include "HTMLInputStream.h"
#include "HTMLParserOptions.h"
#include "HTMLToken.h"
#include "HTMLTokenizer.h"

namespace WebCore {

typedef const void* ParserIdentifier;
class HTMLDocumentParser;

class BackgroundHTMLParser {
    WTF_MAKE_FAST_ALLOCATED;
public:
    void append(const String&);
    void continueParsing();
    void finish();

    static PassOwnPtr<BackgroundHTMLParser> create(const HTMLParserOptions& options, ParserIdentifier identifier)
    {
        return adoptPtr(new BackgroundHTMLParser(options, identifier));
    }

    static void createPartial(ParserIdentifier, HTMLParserOptions);
    static void stopPartial(ParserIdentifier);
    static void appendPartial(ParserIdentifier, const String& input);
    static void continuePartial(ParserIdentifier);
    static void finishPartial(ParserIdentifier);

private:
    explicit BackgroundHTMLParser(const HTMLParserOptions&, ParserIdentifier);

    void pumpTokenizer();

    void sendTokensToMainThread();

    HTMLInputStream m_input;
    HTMLToken m_token;
    bool m_isPausedWaitingForScripts;
    bool m_inForeignContent; // FIXME: We need a stack of foreign content markers.
    OwnPtr<HTMLTokenizer> m_tokenizer;
    HTMLParserOptions m_options;
    ParserIdentifier m_parserIdentifer;
    Vector<CompactHTMLToken> m_pendingTokens;
};

class ParserMap {
public:
    static ParserIdentifier identifierForParser(HTMLDocumentParser* parser)
    {
        return reinterpret_cast<ParserIdentifier>(parser);
    }

    typedef HashMap<ParserIdentifier, OwnPtr<BackgroundHTMLParser> > BackgroundParserMap;
    typedef HashMap<ParserIdentifier, HTMLDocumentParser*> MainThreadParserMap;

    BackgroundParserMap& backgroundParsers();
    MainThreadParserMap& mainThreadParsers();

private:
    BackgroundParserMap m_backgroundParsers;
    MainThreadParserMap m_mainThreadParsers;
};

ParserMap& parserMap();

}

#endif // ENABLE(THREADED_HTML_PARSER)

#endif
