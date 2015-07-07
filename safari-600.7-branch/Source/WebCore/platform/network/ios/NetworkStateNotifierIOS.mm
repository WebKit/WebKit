/*
 * Copyright (C) 2008, 2014 Apple Inc. All Rights Reserved.
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

#import "SoftLinking.h"
#import "WebCoreThreadRun.h"

#if defined(__has_include) && __has_include(<AppSupport/CPNetworkObserver.h>)
#import <AppSupport/CPNetworkObserver.h>
#else
extern NSString * const CPNetworkObserverReachable;
@interface CPNetworkObserver : NSObject
+ (CPNetworkObserver *)sharedNetworkObserver;
- (void)addNetworkReachableObserver:(id)observer selector:(SEL)selector;
- (void)removeNetworkReachableObserver:(id)observer;
- (BOOL)isNetworkReachable;
@end
#endif

SOFT_LINK_PRIVATE_FRAMEWORK(AppSupport);
SOFT_LINK_CLASS(AppSupport, CPNetworkObserver);
SOFT_LINK_POINTER(AppSupport, CPNetworkObserverReachable, NSString *);

@interface WebNetworkStateObserver : NSObject
@property (nonatomic) const WebCore::NetworkStateNotifier* notifier;
- (id)initWithNotifier:(const WebCore::NetworkStateNotifier*)notifier;
- (void)networkStateChanged:(NSNotification *)notification;
@end

@implementation WebNetworkStateObserver

- (id)initWithNotifier:(const WebCore::NetworkStateNotifier*)notifier
{
    ASSERT_ARG(notifier, notifier);
    if (!(self = [super init]))
        return nil;
    _notifier = notifier;
    [[getCPNetworkObserverClass() sharedNetworkObserver] addNetworkReachableObserver:self selector:@selector(networkStateChanged:)];
    return self;
}

- (void)dealloc
{
    [[getCPNetworkObserverClass() sharedNetworkObserver] removeNetworkReachableObserver:self];
    [super dealloc];
}

- (void)networkStateChanged:(NSNotification *)notification
{
    ASSERT_ARG(notification, notification);
    WebThreadRun(^{
        setOnLine(_notifier, [[[notification userInfo] objectForKey:getCPNetworkObserverReachable()] boolValue]);
    });
}

@end

namespace WebCore {

NetworkStateNotifier::NetworkStateNotifier()
    : m_isOnLine(false)
    , m_isOnLineInitialized(false)
{
}
    
NetworkStateNotifier::~NetworkStateNotifier()
{
    [m_observer setNotifier:nullptr];
}

void NetworkStateNotifier::registerObserverIfNecessary() const
{
    if (!m_observer)
        m_observer = adoptNS([[WebNetworkStateObserver alloc] initWithNotifier:this]);
}

bool NetworkStateNotifier::onLine() const
{
    registerObserverIfNecessary();
    if (!m_isOnLineInitialized) {
        m_isOnLine = [[getCPNetworkObserverClass() sharedNetworkObserver] isNetworkReachable];
        m_isOnLineInitialized = true;
    }
    return m_isOnLine;
}

void setOnLine(const NetworkStateNotifier* notifier, bool onLine)
{
    ASSERT_ARG(notifier, notifier);
    notifier->m_isOnLineInitialized = true;
    if (onLine == notifier->m_isOnLine)
        return;
    notifier->m_isOnLine = onLine;
    notifier->notifyNetworkStateChange();
}

} // namespace WebCore
