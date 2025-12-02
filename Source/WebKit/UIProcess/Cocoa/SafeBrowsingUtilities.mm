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

#if HAVE(SAFE_BROWSING)

#import "config.h"
#import "SafeBrowsingUtilities.h"

#import "SafeBrowsingSPI.h"
#import "WKError.h"
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK_FOR_SOURCE(WebKit, SafariSafeBrowsing);

SOFT_LINK_CLASS_FOR_SOURCE(WebKit, SafariSafeBrowsing, SSBLookupContext);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, SafariSafeBrowsing, SSBLookupResult);
SOFT_LINK_CLASS_FOR_SOURCE(WebKit, SafariSafeBrowsing, SSBServiceLookupResult);

namespace WebKit::SafeBrowsingUtilities {

bool canLookUp(const URL& url)
{
    return url.isValid() && [getSSBLookupContextClassSingleton() sharedLookupContext];
}

void lookUp(const URL& url, NavigationType navigationType, SSBLookupResult *cachedResult, CompletionHandler<void(SSBLookupResult *, NSError *)>&& completion)
{
    BlockPtr mainRunLoopCompletion = makeBlockPtr([completion = WTFMove(completion)](SSBLookupResult *result, NSError *error) mutable {
        RunLoop::mainSingleton().dispatch([completion = WTFMove(completion), result = retainPtr(result), error = retainPtr(error)] mutable {
            completion(result.get(), error.get());
        });
    });

    BOOL isMainFrame = navigationType == NavigationType::MainFrame;
    RetainPtr context = [getSSBLookupContextClassSingleton() sharedLookupContext];
    if ([context respondsToSelector:@selector(lookUpURL:isMainFrame:hasHighConfidenceOfSafety:cachedResult:completionHandler:)])
        [context lookUpURL:url.createNSURL().get() isMainFrame:isMainFrame hasHighConfidenceOfSafety:NO cachedResult:cachedResult completionHandler:mainRunLoopCompletion.get()];
    else if ([context respondsToSelector:@selector(lookUpURL:isMainFrame:hasHighConfidenceOfSafety:completionHandler:)])
        [context lookUpURL:url.createNSURL().get() isMainFrame:isMainFrame hasHighConfidenceOfSafety:NO completionHandler:mainRunLoopCompletion.get()];
    else
        [context lookUpURL:url.createNSURL().get() completionHandler:mainRunLoopCompletion.get()];
}

void listsForNamespace(const String& listNamespace, CompletionHandler<void(NamespacedLists *, NSError *)>&& completion)
{
    RetainPtr context = [getSSBLookupContextClassSingleton() sharedLookupContext];
    if (![context respondsToSelector:@selector(getListsForNamespace:completionHandler:)])
        return completion(nil, [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]);

    BlockPtr mainRunLoopCompletion = makeBlockPtr([completion = WTFMove(completion)](NamespacedLists *result, NSError *error) mutable {
        RunLoop::mainSingleton().dispatch([completion = WTFMove(completion), result = retainPtr(result), error = retainPtr(error)] mutable {
            completion(result.get(), error.get());
        });
    });
    [context getListsForNamespace:listNamespace.createNSString().get() completionHandler:mainRunLoopCompletion.get()];
}

} // namespace WebKit::SafeBrowsingUtilities

#endif // HAVE(SAFE_BROWSING)
