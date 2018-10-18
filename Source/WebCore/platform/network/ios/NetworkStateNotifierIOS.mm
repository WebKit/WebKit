/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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

#import "config.h"
#import "NetworkStateNotifier.h"

#if PLATFORM(IOS_FAMILY)

#import "DeprecatedGlobalSettings.h"
#import "WebCoreThreadRun.h"

#if USE(APPLE_INTERNAL_SDK)
#import <AppSupport/CPNetworkObserver.h>
#else
@interface CPNetworkObserver : NSObject
+ (CPNetworkObserver *)sharedNetworkObserver;
- (void)addNetworkReachableObserver:(id)observer selector:(SEL)selector;
- (BOOL)isNetworkReachable;
@end
#endif

@interface WebNetworkStateObserver : NSObject {
    void (^block)();
}
- (id)initWithBlock:(void (^)())block;
@end

@implementation WebNetworkStateObserver

- (id)initWithBlock:(void (^)())observerBlock
{
    if (!(self = [super init]))
        return nil;
    [[CPNetworkObserver sharedNetworkObserver] addNetworkReachableObserver:self selector:@selector(networkStateChanged:)];
    block = [observerBlock copy];
    return self;
}

- (void)dealloc
{
    [block release];
    [super dealloc];
}

- (void)networkStateChanged:(NSNotification *)unusedNotification
{
    UNUSED_PARAM(unusedNotification);
    block();
}

@end

namespace WebCore {

void NetworkStateNotifier::updateStateWithoutNotifying()
{
    m_isOnLine = [[CPNetworkObserver sharedNetworkObserver] isNetworkReachable];
}

void NetworkStateNotifier::startObserving()
{
    if (DeprecatedGlobalSettings::shouldOptOutOfNetworkStateObservation())
        return;
    m_observer = adoptNS([[WebNetworkStateObserver alloc] initWithBlock:^ {
        WebThreadRun(^ {
            NetworkStateNotifier::singleton().updateStateSoon();
        });
    }]);
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
