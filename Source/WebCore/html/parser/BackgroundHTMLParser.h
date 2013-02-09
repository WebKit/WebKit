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

#include "BackgroundHTMLInputStream.h"
#include "CompactHTMLToken.h"
#include "HTMLParserOptions.h"
#include "HTMLSourceTracker.h"
#include "HTMLToken.h"
#include "HTMLTokenizer.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

typedef const void* ParserIdentifier;
class HTMLDocumentParser;
class XSSAuditor;

class BackgroundHTMLParser {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static void create(PassRefPtr<WeakReference<BackgroundHTMLParser> > reference, const HTMLParserOptions& options, const WeakPtr<HTMLDocumentParser>& parser, PassOwnPtr<XSSAuditor> xssAuditor)
    {
        new BackgroundHTMLParser(reference, options, parser, xssAuditor);
        // Caller must free by calling stop().
    }

    void append(const String&);
    void resumeFrom(const WeakPtr<HTMLDocumentParser>&, PassOwnPtr<HTMLToken>, PassOwnPtr<HTMLTokenizer>, HTMLInputCheckpoint);
    void finish();
    void stop();

    void forcePlaintextForTextDocument();

private:
    BackgroundHTMLParser(PassRefPtr<WeakReference<BackgroundHTMLParser> >, const HTMLParserOptions&, const WeakPtr<HTMLDocumentParser>&, PassOwnPtr<XSSAuditor>);

    void markEndOfFile();
    void pumpTokenizer();
    bool simulateTreeBuilder(const CompactHTMLToken&);

    void sendTokensToMainThread();

    bool m_inForeignContent; // FIXME: We need a stack of foreign content markers.
    WeakPtrFactory<BackgroundHTMLParser> m_weakFactory;
    BackgroundHTMLInputStream m_input;
    HTMLSourceTracker m_sourceTracker;
    OwnPtr<HTMLToken> m_token;
    OwnPtr<HTMLTokenizer> m_tokenizer;
    HTMLParserOptions m_options;
    WeakPtr<HTMLDocumentParser> m_parser;
    OwnPtr<CompactHTMLTokenStream> m_pendingTokens;
    OwnPtr<XSSAuditor> m_xssAuditor;
};

}

#endif // ENABLE(THREADED_HTML_PARSER)

#endif
