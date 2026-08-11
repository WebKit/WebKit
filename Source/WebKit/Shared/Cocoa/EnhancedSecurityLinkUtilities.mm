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
#import "EnhancedSecurityLinkUtilities.h"

#if HAVE(ENHANCED_SECURITY_LINKS)

#import <LinkSecurity/LinkSecurity.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/MainThread.h>
#import <wtf/SoftLinking.h>
#import <wtf/URL.h>

SOFT_LINK_FRAMEWORK_OPTIONAL(LinkSecurity)
SOFT_LINK_CLASS_OPTIONAL(LinkSecurity, LSLinkSecurityManager)

namespace WebKit {

bool hasURLsRequiringEnhancedSecurityCheck()
{
    if (!LinkSecurityLibrary())
        return false;

    LSLinkSecurityManager* manager = [getLSLinkSecurityManagerClassSingleton() sharedManager];
    return manager.hasFlaggedURLs;
}

void isEnhancedSecurityEnabledForURL(const WTF::URL& url, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!LinkSecurityLibrary())
        return completionHandler(false);

    RetainPtr<NSURL> testURL = url.createNSURL();
    if (url.isEmpty() || !testURL)
        return completionHandler(false);

    LSLinkSecurityManager* manager = [getLSLinkSecurityManagerClassSingleton() sharedManager];

    [manager checkIsFlaggedURL:testURL.get() completion:makeBlockPtr([completionHandler = WTF::move(completionHandler)](BOOL result) mutable {
        ensureOnMainThread([completionHandler = WTF::move(completionHandler), result]() mutable {
            completionHandler(result);
        });
    }).get()];
}

} // namespace WebKit

#else

namespace WebKit {

bool hasURLsRequiringEnhancedSecurityCheck()
{
    return false;
}

void isEnhancedSecurityEnabledForURL(const WTF::URL&, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(false);
}

} // namespace WebKit

#endif // HAVE(ENHANCED_SECURITY_LINKS)
