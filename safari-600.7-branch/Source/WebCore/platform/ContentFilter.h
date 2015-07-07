/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef ContentFilter_h
#define ContentFilter_h

#if USE(CONTENT_FILTERING)

#include <wtf/PassRefPtr.h>
#include <wtf/RetainPtr.h>

#if PLATFORM(IOS)
#include <functional>
#endif

#if PLATFORM(COCOA)
OBJC_CLASS NSData;
OBJC_CLASS NSKeyedArchiver;
OBJC_CLASS NSKeyedUnarchiver;
OBJC_CLASS WebFilterEvaluator;
#endif

#define HAVE_NE_FILTER_SOURCE TARGET_OS_EMBEDDED || (!TARGET_OS_IPHONE && __MAC_OS_X_VERSION_MIN_REQUIRED >= 10100)

#if HAVE(NE_FILTER_SOURCE)
#import <dispatch/dispatch.h>
OBJC_CLASS NEFilterSource;
OBJC_CLASS NSMutableData;
#endif

namespace WebCore {

class ResourceRequest;
class ResourceResponse;

class ContentFilter {
public:
    static bool canHandleResponse(const ResourceResponse&);

    explicit ContentFilter(const ResourceResponse&);
    ~ContentFilter();

    void addData(const char* data, int length);
    void finishedAddingData();
    bool needsMoreData() const;
    bool didBlockData() const;
    const char* getReplacementData(int& length) const;

#if PLATFORM(COCOA)
    ContentFilter();
    void encode(NSKeyedArchiver *) const;
    static bool decode(NSKeyedUnarchiver *, ContentFilter&);
#endif

#if PLATFORM(IOS)
    bool handleUnblockRequestAndDispatchIfSuccessful(const ResourceRequest&, std::function<void()>);
#endif

private:
#if PLATFORM(COCOA)
    RetainPtr<WebFilterEvaluator> m_platformContentFilter;
    RetainPtr<NSData> m_replacementData;
#endif

#if HAVE(NE_FILTER_SOURCE)
    long m_neFilterSourceStatus;
    RetainPtr<NEFilterSource> m_neFilterSource;
    dispatch_queue_t m_neFilterSourceQueue;
    RetainPtr<NSMutableData> m_originalData;
#endif
};

} // namespace WebCore

#endif // USE(CONTENT_FILTERING)

#endif // ContentFilter_h
