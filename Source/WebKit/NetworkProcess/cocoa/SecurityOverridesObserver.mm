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
#import "SecurityOverridesObserver.h"

#if HAVE(SECURITY_OVERRIDES)

#import "Logging.h"
#import "NetworkProcess.h"
#import "NetworkProcessProxyMessages.h"
#import <SecurityOverrides/SecurityOverrides.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/SoftLinking.h>
#import <wtf/TZoneMallocInlines.h>

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(SecurityOverrides)
SOFT_LINK_CLASS_OPTIONAL(SecurityOverrides, SecurityOverridesManager)

@interface WKSecurityOverridesManagerObserver : NSObject <SecurityOverridesManagerObserver>
- (instancetype)initWithObserver:(WebKit::SecurityOverridesObserver&)observer;
@end

@implementation WKSecurityOverridesManagerObserver {
    WeakPtr<WebKit::SecurityOverridesObserver> _observer;
}

- (instancetype)initWithObserver:(WebKit::SecurityOverridesObserver&)observer
{
    if (!(self = [super init]))
        return nil;
    _observer = observer;
    return self;
}

- (void)securityOverridesDidChange
{
    RunLoop::mainSingleton().dispatch([observer = _observer] {
        assertIsMainRunLoop();
        if (CheckedPtr checkedObserver = observer.get())
            checkedObserver->fetch();
    });
}

@end

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SecurityOverridesObserver);

SecurityOverridesObserver::SecurityOverridesObserver(NetworkProcess& networkProcess)
    : m_networkProcess(networkProcess)
    , m_queue(WorkQueue::create("com.apple.WebKit.SecurityOverridesObserver"_s))
{
    assertIsMainRunLoop();

    m_managerObserver = adoptNS([[WKSecurityOverridesManagerObserver alloc] initWithObserver:*this]);
    m_queue->dispatch([observer = m_managerObserver] {
        if (!SecurityOverridesLibrary()) {
            RELEASE_LOG_ERROR(Process, "SecurityOverridesObserver: SecurityOverrides is unavailable");
            return;
        }

        RetainPtr manager = [getSecurityOverridesManagerClassSingleton() shared];
        if ([manager respondsToSelector:@selector(addObserver:)])
            [manager addObserver:observer.get()];
        else
            RELEASE_LOG_ERROR(Process, "SecurityOverridesObserver: cannot add an observer");
    });

    fetch();
}

SecurityOverridesObserver::~SecurityOverridesObserver()
{
    if (!m_managerObserver)
        return;

    m_queue->dispatch([observer = WTF::move(m_managerObserver)] {
        RetainPtr manager = [getSecurityOverridesManagerClassSingleton() shared];
        if ([manager respondsToSelector:@selector(removeObserver:)])
            [manager removeObserver:observer.get()];
    });
}

void SecurityOverridesObserver::fetch()
{
    assertIsMainRunLoop();

    auto generation = ++m_generation;

#if defined(ENGINEERING_BUILD) && ENGINEERING_BUILD
    if (m_overridesForTesting) {
        RELEASE_LOG(Process, "SecurityOverridesObserver: answering with the overrides set for testing (#%" PRIu64 ")", generation);
        didFetch(generation, HashMap<String, String> { *m_overridesForTesting });
        return;
    }
#endif

    RELEASE_LOG(Process, "SecurityOverridesObserver: asking the store for overrides (#%" PRIu64 ")", generation);

    m_queue->dispatch([weakThis = WeakPtr { *this }, generation] mutable {
        if (!SecurityOverridesLibrary())
            return;

        RetainPtr manager = [getSecurityOverridesManagerClassSingleton() shared];
        if (![manager respondsToSelector:@selector(overridesForCategory:completion:)]) {
            RELEASE_LOG_ERROR(Process, "SecurityOverridesObserver: failed to ask for overrides");
            return;
        }

        [manager overridesForCategory:@"WebKit" completion:makeBlockPtr([weakThis = WTF::move(weakThis), generation](NSDictionary<NSString *, NSString *> *overrides, NSError *error) mutable {
            RunLoop::mainSingleton().dispatch([weakThis = WTF::move(weakThis), generation, overrides = RetainPtr { overrides }, error = RetainPtr { error }] {
                assertIsMainRunLoop();
                CheckedPtr checkedThis = weakThis;
                if (!checkedThis)
                    return;

                if (!overrides) {
                    RELEASE_LOG_ERROR(Process, "SecurityOverridesObserver: the store could not be asked (#%" PRIu64 "): %" PUBLIC_LOG_STRING, generation, error ? [error localizedDescription].UTF8String : "no error given");
                    return;
                }

                HashMap<String, String> result;
                for (id key in overrides.get()) {
                    id value = [overrides objectForKey:key];
                    if (![key isKindOfClass:NSString.class] || ![value isKindOfClass:NSString.class]) {
                        RELEASE_LOG_ERROR(Process, "SecurityOverridesObserver: ignoring an override that is not a string pair");
                        continue;
                    }
                    result.set(String { (NSString *)key }, String { (NSString *)value });
                }
                checkedThis->didFetch(generation, WTF::move(result));
            });
        }).get()];
    });
}

void SecurityOverridesObserver::setOverridesForTesting(std::optional<HashMap<String, String>>&& overrides)
{
#if defined(ENGINEERING_BUILD) && ENGINEERING_BUILD
    assertIsMainRunLoop();

    m_overridesForTesting = WTF::move(overrides);
    fetch();
#endif // ENGINEERING_BUILD
}

void SecurityOverridesObserver::didFetch(uint64_t generation, HashMap<String, String>&& overrides)
{
    assertIsMainRunLoop();

    if (generation != m_generation) {
        RELEASE_LOG(Process, "SecurityOverridesObserver: discarding a stale answer (#%" PRIu64 ", latest #%" PRIu64 ")", generation, m_generation);
        return;
    }

    RELEASE_LOG(Process, "SecurityOverridesObserver: forwarding %u override(s)", overrides.size());
    protect(m_networkProcess->parentProcessConnection())->send(Messages::NetworkProcessProxy::SetSecurityOverrides(WTF::move(overrides)), 0);
}

} // namespace WebKit

#endif // HAVE(SECURITY_OVERRIDES)
