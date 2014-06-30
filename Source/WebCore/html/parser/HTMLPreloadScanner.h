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

#ifndef HTMLPreloadScanner_h
#define HTMLPreloadScanner_h

#include "CSSPreloadScanner.h"
#include "HTMLToken.h"
#include "SegmentedString.h"
#include <wtf/Vector.h>

namespace WebCore {

typedef size_t TokenPreloadScannerCheckpoint;

class HTMLParserOptions;
class HTMLTokenizer;
class SegmentedString;
class Frame;

class TokenPreloadScanner {
    WTF_MAKE_NONCOPYABLE(TokenPreloadScanner); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit TokenPreloadScanner(const URL& documentURL, float deviceScaleFactor = 1.0);
    ~TokenPreloadScanner();

    void scan(const HTMLToken&, PreloadRequestStream& requests
#if ENABLE_PICTURE_SIZES
        , RenderView*, Frame*
#endif
        );

    void setPredictedBaseElementURL(const URL& url) { m_predictedBaseElementURL = url; }

    // A TokenPreloadScannerCheckpoint is valid until the next call to rewindTo,
    // at which point all outstanding checkpoints are invalidated.
    TokenPreloadScannerCheckpoint createCheckpoint();
    void rewindTo(TokenPreloadScannerCheckpoint);

    bool isSafeToSendToAnotherThread()
    {
        return m_documentURL.isSafeToSendToAnotherThread()
            && m_predictedBaseElementURL.isSafeToSendToAnotherThread();
    }

private:
    enum class TagId {
        // These tags are scanned by the StartTagScanner.
        Img,
        Input,
        Link,
        Script,

        // These tags are not scanned by the StartTagScanner.
        Unknown,
        Style,
        Base,
        Template,
    };

    class StartTagScanner;

    static TagId tagIdFor(const HTMLToken::DataVector&);

    static String initiatorFor(TagId);

    template<typename Token>
    void updatePredictedBaseURL(const Token&);

    struct Checkpoint {
        Checkpoint(const URL& predictedBaseElementURL, bool inStyle
#if ENABLE(TEMPLATE_ELEMENT)
            , size_t templateCount
#endif
            )
            : predictedBaseElementURL(predictedBaseElementURL)
            , inStyle(inStyle)
#if ENABLE(TEMPLATE_ELEMENT)
            , templateCount(templateCount)
#endif
        {
        }

        URL predictedBaseElementURL;
        bool inStyle;
#if ENABLE(TEMPLATE_ELEMENT)
        size_t templateCount;
#endif
    };

    CSSPreloadScanner m_cssScanner;
    const URL m_documentURL;
    URL m_predictedBaseElementURL;
    bool m_inStyle;
    float m_deviceScaleFactor;

#if ENABLE(TEMPLATE_ELEMENT)
    size_t m_templateCount;
#endif

    Vector<Checkpoint> m_checkpoints;
};

class HTMLPreloadScanner {
    WTF_MAKE_NONCOPYABLE(HTMLPreloadScanner); WTF_MAKE_FAST_ALLOCATED;
public:
    HTMLPreloadScanner(const HTMLParserOptions&, const URL& documentURL, float deviceScaleFactor = 1.0);
    ~HTMLPreloadScanner();

    void appendToEnd(const SegmentedString&);
    void scan(HTMLResourcePreloader*, const URL& documentBaseElementURL
#if ENABLE_PICTURE_SIZES
        , RenderView*, Frame*
#endif
        );

private:
    TokenPreloadScanner m_scanner;
    SegmentedString m_source;
    HTMLToken m_token;
    std::unique_ptr<HTMLTokenizer> m_tokenizer;
};

}

#endif
