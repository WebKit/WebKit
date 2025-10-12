/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "ScreenTimeWebsiteDataSupport.h"

#if ENABLE(SCREEN_TIME)

#import "WebsiteDataStoreConfiguration.h"
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/HashSet.h>
#import <wtf/URL.h>
#import <wtf/URLHash.h>
#import <wtf/UUID.h>
#import <wtf/WallTime.h>

#import <pal/cocoa/ScreenTimeSoftLink.h>

namespace WebKit::ScreenTimeWebsiteDataSupport {

void getScreenTimeURLs(std::optional<WTF::UUID> identifier, CompletionHandler<void(HashSet<URL>&&)>&& completionHandler)
{
    RetainPtr<NSString> profileIdentifier;
    if (identifier)
        profileIdentifier = [identifier->createNSUUID() UUIDString];

    RetainPtr webHistory = adoptNS([PAL::allocSTWebHistoryInstance() initWithProfileIdentifier:profileIdentifier.get()]);

    [webHistory fetchAllHistoryWithCompletionHandler:makeBlockPtr([completionHandler = WTFMove(completionHandler)](NSSet<NSURL *> *urls, NSError *error) mutable {
        ensureOnMainRunLoop([completionHandler = WTFMove(completionHandler), urls = retainPtr(urls), error = retainPtr(error)] mutable {
            if (error) {
                completionHandler({ });
                return;
            }

            HashSet<URL> result;
            for (NSURL *site in urls.get()) {
                URL url { site };
                if (url.isValid())
                    result.add(WTFMove(url));
            }

            completionHandler(WTFMove(result));
        });
    }).get()];
}

void removeScreenTimeData(const HashSet<URL>& websitesToRemove, const WebsiteDataStoreConfiguration& configuration)
{
    RetainPtr<NSString> profileIdentifier;
    if (configuration.identifier())
        profileIdentifier = [configuration.identifier()->createNSUUID() UUIDString];

    RetainPtr webHistory = adoptNS([PAL::allocSTWebHistoryInstance() initWithProfileIdentifier:profileIdentifier.get()]);

    RetainPtr<NSMutableSet<NSString *>> websitesToRemoveDomains = [NSMutableSet set];
    for (auto& url : websitesToRemove)
        if (RetainPtr host = url.host().createNSString())
            [websitesToRemoveDomains addObject:host.get()];

    [webHistory fetchAllHistoryWithCompletionHandler:^(NSSet<NSURL *> *urls, NSError *error) {
        if (error)
            return;

        for (NSURL *url in urls) {
            for (NSString *domainString in websitesToRemoveDomains.get()) {
                if ([[url host] hasSuffix:domainString])
                    [webHistory deleteHistoryForURL:url];
            }
        }
    }];
}

void removeScreenTimeDataWithInterval(WallTime modifiedSince, const WebsiteDataStoreConfiguration& configuration)
{
    RetainPtr<NSString> profileIdentifier;
    if (configuration.identifier())
        profileIdentifier = [configuration.identifier()->createNSUUID() UUIDString];

    RetainPtr webHistory = adoptNS([PAL::allocSTWebHistoryInstance() initWithProfileIdentifier:profileIdentifier.get()]);

    if (!modifiedSince.isNaN()) {
        NSTimeInterval timeInterval = modifiedSince.secondsSinceEpoch().seconds();
        RetainPtr date = [NSDate dateWithTimeIntervalSince1970:timeInterval];
        RetainPtr dateInterval = adoptNS([[NSDateInterval alloc] initWithStartDate:date.get() endDate:NSDate.now]);
        [webHistory deleteHistoryDuringInterval:dateInterval.get()];
    }
}

} // namespace WebKit::ScreenTimeWebsiteDataSupport

#endif
