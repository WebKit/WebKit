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

#ifndef HTML5Tokenizer_h
#define HTML5Tokenizer_h

#include "CachedResourceClient.h"
#include "HTML5Token.h"
#include "SegmentedString.h"
#include "Tokenizer.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class HTMLDocument;
class HTML5TreeBuilder;
class HTML5Lexer;

// FIXME: This is the wrong layer to hook in the new HTML 5 Lexer,
// however HTMLTokenizer is too large and too fragile of a class to hack into.
// Eventually we should split all of the HTML lexer logic out from HTMLTokenizer
// and then share non-lexer-specific tokenizer logic between HTML5 and the
// legacy WebKit HTML lexer.

// FIXME: This class is far from complete.
class HTML5Tokenizer :  public Tokenizer, public CachedResourceClient {
public:
    HTML5Tokenizer(HTMLDocument*, bool reportErrors);
    virtual ~HTML5Tokenizer();

    virtual void begin();
    virtual void write(const SegmentedString&, bool appendData);
    virtual void end();
    virtual void finish();
    virtual bool isWaitingForScripts() const;

private:
    SegmentedString m_source;

    // We hold m_token here because it might be partially complete.
    HTML5Token m_token;

    OwnPtr<HTML5Lexer> m_lexer;
    OwnPtr<HTML5TreeBuilder> m_treeBuilder;
};

}

#endif
