/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(TEXT_EXTRACTION_FILTER)

#import <wtf/CompletionHandler.h>
#import <wtf/HashMap.h>
#import <wtf/Lock.h>
#import <wtf/Noncopyable.h>
#import <wtf/RetainPtr.h>
#import <wtf/ThreadSafeRefCounted.h>
#import <wtf/WorkQueue.h>

OBJC_CLASS NLTokenizer;
OBJC_CLASS NSString;
OBJC_CLASS MLModel;

namespace WebKit {

class TextExtractionFilter : public ThreadSafeRefCounted<TextExtractionFilter> {
    WTF_MAKE_TZONE_ALLOCATED(TextExtractionFilter);
    WTF_MAKE_NONCOPYABLE(TextExtractionFilter);
public:
    static TextExtractionFilter& singleton();
    static TextExtractionFilter* singletonIfCreated();

    void shouldFilter(const String&, CompletionHandler<void(bool)>&&);
    void prewarm();
    void resetCache();

private:
    TextExtractionFilter();

    static constexpr auto chunkSize = 120;

    void initializeModelIfNeeded();
    bool shouldFilter(const String&);
    Vector<RetainPtr<NSString>> segmentText(const String&);

    const Ref<WorkQueue> m_modelQueue;
    RetainPtr<MLModel> m_model;
    RetainPtr<NLTokenizer> m_tokenizer;
    bool m_failedInitialization { false };

    HashMap<unsigned /* String hash */, bool> m_cache WTF_GUARDED_BY_LOCK(m_cacheLock);
    Lock m_cacheLock;
};

} // namespace WebKit

#endif // ENABLE(TEXT_EXTRACTION_FILTER)
