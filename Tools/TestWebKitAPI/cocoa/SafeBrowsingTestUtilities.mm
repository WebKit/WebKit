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
#import "SafeBrowsingTestUtilities.h"

#import "TestNSBundleExtras.h"
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>

@implementation TestServiceLookupResult {
    RetainPtr<NSString> _provider;
    BOOL _isPhishing;
    BOOL _isMalware;
    BOOL _isUnwantedSoftware;
}

+ (instancetype)resultWithProvider:(NSString *)provider phishing:(BOOL)phishing malware:(BOOL)malware unwantedSoftware:(BOOL)unwantedSoftware
{
    RetainPtr result = adoptNS([TestServiceLookupResult new]);
    if (!result)
        return nil;

    result->_provider = adoptNS([provider copy]);
    result->_isPhishing = phishing;
    result->_isMalware = malware;
    result->_isUnwantedSoftware = unwantedSoftware;

    return result.autorelease();
}

- (NSString *)provider
{
    return _provider.get();
}

- (BOOL)isPhishing
{
    return _isPhishing;
}

- (BOOL)isMalware
{
    return _isMalware;
}

- (BOOL)isUnwantedSoftware
{
    return _isUnwantedSoftware;
}

- (NSString *)malwareDetailsBaseURLString
{
    return @"test://";
}

- (NSURL *)learnMoreURL
{
    return [NSURL URLWithString:@"test://"];
}

- (NSString *)reportAnErrorBaseURLString
{
    return @"test://";
}

- (NSString *)localizedProviderDisplayName
{
    return @"test display name";
}

@end

@implementation TestLookupResult {
    RetainPtr<NSArray> _results;
}

+ (instancetype)resultWithResults:(NSArray<TestServiceLookupResult *> *)results
{
    RetainPtr result = adoptNS([TestLookupResult new]);
    if (!result)
        return nil;

    result->_results = adoptNS([results copy]);
    return result.autorelease();
}

- (NSArray<TestServiceLookupResult *> *)serviceLookupResults
{
    return _results.get();
}

@end

@implementation TestLookupContext

+ (TestLookupContext *)sharedLookupContext
{
    static NeverDestroyed<RetainPtr<TestLookupContext>> context = adoptNS([TestLookupContext new]);
    return context.get().get();
}

- (void)lookUpURL:(NSURL *)URL completionHandler:(void (^)(TestLookupResult *, NSError *))completionHandler
{
    completionHandler([TestLookupResult resultWithResults:@[[TestServiceLookupResult resultWithProvider:@"SSBProviderApple" phishing:YES malware:NO unwantedSoftware:NO]]], nil);
}

@end

static Seconds globalDelayDuration;

@implementation DelayedLookupContext

+ (Seconds)delayDuration
{
    return globalDelayDuration;
}

+ (void)setDelayDuration:(Seconds)duration
{
    globalDelayDuration = duration;
}

+ (DelayedLookupContext *)sharedLookupContext
{
    static NeverDestroyed<RetainPtr<DelayedLookupContext>> context = adoptNS([DelayedLookupContext new]);
    return context.get().get();
}

- (void)lookUpURL:(NSURL *)url completionHandler:(void (^)(TestLookupResult *, NSError *))completionHandler
{
    RetainPtr resourceURL = [NSBundle.test_resourcesBundle URLForResource:@"simple2" withExtension:@"html"];
    BOOL phishing = ![url isEqual:resourceURL.get()] && ![url.path isEqual:@"/safe"];
    RunLoop::mainSingleton().dispatchAfter(globalDelayDuration, [completionHandler = makeBlockPtr(completionHandler), phishing] {
        completionHandler.get()([TestLookupResult resultWithResults:@[[TestServiceLookupResult resultWithProvider:@"SSBProviderApple" phishing:phishing malware:NO unwantedSoftware:NO]]], nil);
    });
}

@end
