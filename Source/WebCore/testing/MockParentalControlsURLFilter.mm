/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "config.h"
#import "MockParentalControlsURLFilter.h"

#if HAVE(WEBCONTENTRESTRICTIONS)

#import <wtf/CompletionHandler.h>
#import <wtf/MainThread.h>
#import <wtf/URL.h>
#import <wtf/WorkQueue.h>

namespace WebCore {

Ref<MockParentalControlsURLFilter> MockParentalControlsURLFilter::create(Vector<URL>&& blockedURLs)
{
    return adoptRef(*new MockParentalControlsURLFilter(WTF::move(blockedURLs)));
}

MockParentalControlsURLFilter::MockParentalControlsURLFilter(Vector<URL>&& blockedURLs)
{
    for (auto& url : blockedURLs)
        m_blockedURLs.add(WTF::move(url));
}

MockParentalControlsURLFilter::~MockParentalControlsURLFilter() = default;

bool MockParentalControlsURLFilter::isEnabledImpl() const
{
    return true;
}

void MockParentalControlsURLFilter::isURLAllowedImpl(IsMainFrameLoad, const URL& mainDocumentURL, const URL& url, CompletionHandler<void(bool, NSData *)>&& completionHandler)
{
    ASSERT(isMainThread());

    bool isBlocked = m_blockedURLs.contains(url) || (!mainDocumentURL.isEmpty() && m_blockedURLs.contains(mainDocumentURL));

    workQueueSingleton().dispatch([completionHandler = WTF::move(completionHandler), isAllowed = !isBlocked]() mutable {
        completionHandler(isAllowed, nullptr);
    });
}

void MockParentalControlsURLFilter::allowURL(const URL&, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(true);
}

} // namespace WebCore

#endif // HAVE(WEBCONTENTRESTRICTIONS)
