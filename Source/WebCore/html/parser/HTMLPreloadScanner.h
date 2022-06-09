/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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

#pragma once

#include "CSSPreloadScanner.h"
#include "HTMLTokenizer.h"
#include "SegmentedString.h"

namespace WebCore {

class TokenPreloadScanner {
    WTF_MAKE_NONCOPYABLE(TokenPreloadScanner);
public:
    explicit TokenPreloadScanner(const URL& documentURL, float deviceScaleFactor = 1.0);

    void scan(const HTMLToken&, PreloadRequestStream&, Document&);

    void setPredictedBaseElementURL(const URL& url) { m_predictedBaseElementURL = url; }
    
    bool inPicture() { return !m_pictureSourceState.isEmpty(); }

private:
    enum class TagId {
        // These tags are scanned by the StartTagScanner.
        Img,
        Input,
        Link,
        Script,
        Meta,
        Source,

        // These tags are not scanned by the StartTagScanner.
        Unknown,
        Style,
        Base,
        Template,
        Picture
    };

    class StartTagScanner;

    static TagId tagIdFor(const HTMLToken::DataVector&);

    static String initiatorFor(TagId);

    void updatePredictedBaseURL(const HTMLToken&, bool shouldRestrictBaseURLSchemes);

    CSSPreloadScanner m_cssScanner;
    const URL m_documentURL;
    const float m_deviceScaleFactor { 1 };

    URL m_predictedBaseElementURL;
    bool m_inStyle { false };
    
    Vector<bool> m_pictureSourceState;

    unsigned m_templateCount { 0 };
};

class HTMLPreloadScanner {
    WTF_MAKE_FAST_ALLOCATED;
public:
    HTMLPreloadScanner(const HTMLParserOptions&, const URL& documentURL, float deviceScaleFactor = 1.0);

    void appendToEnd(const SegmentedString&);
    void scan(HTMLResourcePreloader&, Document&);

private:
    TokenPreloadScanner m_scanner;
    SegmentedString m_source;
    HTMLTokenizer m_tokenizer;
};

WEBCORE_EXPORT bool testPreloadScannerViewportSupport(Document*);

} // namespace WebCore
