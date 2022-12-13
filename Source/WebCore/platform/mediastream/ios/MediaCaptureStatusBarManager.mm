/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "MediaCaptureStatusBarManager.h"

#if ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)

#include "Logging.h"
#include "RuntimeApplicationChecks.h"
#include <pal/spi/ios/SBSStatusBarSPI.h>
#include <wtf/BlockPtr.h>

#include <pal/cocoa/AVFoundationSoftLink.h>

SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(SpringBoardServices)
SOFT_LINK_CLASS_OPTIONAL(SpringBoardServices, SBSStatusBarStyleOverridesAssertion)
SOFT_LINK_CLASS_OPTIONAL(SpringBoardServices, SBSStatusBarStyleOverridesCoordinator)

using namespace WebCore;

@interface WebCoreMediaCaptureStatusBarHandler : NSObject<SBSStatusBarStyleOverridesCoordinatorDelegate>
-(id)initWithManager:(MediaCaptureStatusBarManager*)manager;
-(void)validateIsStopped;
@end

@implementation WebCoreMediaCaptureStatusBarHandler {
    WeakPtr<MediaCaptureStatusBarManager> m_manager;
    RetainPtr<SBSStatusBarStyleOverridesAssertion> m_statusBarStyleOverride;
    RetainPtr<SBSStatusBarStyleOverridesCoordinator> m_coordinator;
}

- (id)initWithManager:(MediaCaptureStatusBarManager*)manager
{
    self = [self init];
    if (!self)
        return nil;

    m_manager = WeakPtr { *manager };
    m_statusBarStyleOverride = nil;
    m_coordinator = nil;
    return self;
}

- (void)validateIsStopped
{
    RELEASE_LOG_ERROR_IF(!!m_statusBarStyleOverride || !!m_coordinator, WebRTC, "WebCoreMediaCaptureStatusBarHandler is not correctly stopped");
    ASSERT(!m_statusBarStyleOverride);
    ASSERT(!m_coordinator);
}

- (void)start
{
    ASSERT(!m_statusBarStyleOverride);
    ASSERT(!m_coordinator);

    UIStatusBarStyleOverrides overrides = UIStatusBarStyleOverrideWebRTCAudioCapture;
    m_statusBarStyleOverride = [getSBSStatusBarStyleOverridesAssertionClass() assertionWithStatusBarStyleOverrides:overrides forPID:presentingApplicationPID() exclusive:YES showsWhenForeground:YES];
    m_coordinator = adoptNS([[getSBSStatusBarStyleOverridesCoordinatorClass() alloc] init]);
    m_coordinator.get().delegate = self;

    [m_coordinator setRegisteredStyleOverrides:overrides reply:^(NSError *error) {
        if (!error)
            return;
        RELEASE_LOG_ERROR(WebRTC, "WebCoreMediaCaptureStatusBarHandler _acquireStatusBarOverride failed, code = %ld, description is '%s'", [error code], [error localizedDescription].UTF8String);

        callOnMainThread([self, strongSelf = retainPtr(self)] {
            if (m_manager)
                m_manager->didError();
        });
    }];

    // FIXME: Set m_statusBarStyleOverride statusString
    [m_statusBarStyleOverride acquireWithHandler:^(BOOL acquired) {
        if (acquired)
            return;
        callOnMainThread([self, strongSelf = retainPtr(self)] {
            if (m_manager)
                m_manager->didError();
        });
    } invalidationHandler:^{
        callOnMainThread([self, strongSelf = retainPtr(self)] {
            if (m_manager)
                m_manager->didError();
        });
    }];
}

- (void)stop
{
    if (m_coordinator) {
        m_coordinator.get().delegate = nil;
        m_coordinator = nil;
    }

    if (m_statusBarStyleOverride) {
        [m_statusBarStyleOverride invalidate];
        m_statusBarStyleOverride = nil;
    }
    m_manager = nullptr;
}
// FIXME rdar://103273450 (Replace call to SBSStatusBarTapContext with non-deprecated API)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (BOOL)statusBarCoordinator:(SBSStatusBarStyleOverridesCoordinator *)coordinator receivedTapWithContext:(id<SBSStatusBarTapContext>)tapContext completionBlock:(void (^)(void))completion
ALLOW_DEPRECATED_DECLARATIONS_END
{
    callOnMainThread([self, strongSelf = retainPtr(self), completion = makeBlockPtr(completion)]() mutable {
        if (!m_manager)
            return;
        m_manager->didTap([completion = WTFMove(completion)] {
            completion.get()();
        });
    });

    return YES;
}

- (void)statusBarCoordinator:(SBSStatusBarStyleOverridesCoordinator *)coordinator invalidatedRegistrationWithError:(NSError *)error
{
    callOnMainThread([self, strongSelf = retainPtr(self)] {
        if (m_manager)
            m_manager->didError();
    });
}

@end

namespace WebCore {

bool MediaCaptureStatusBarManager::hasSupport()
{
    return SpringBoardServicesLibrary();
}

MediaCaptureStatusBarManager::~MediaCaptureStatusBarManager()
{
    if (m_handler)
        [m_handler validateIsStopped];
}

void MediaCaptureStatusBarManager::start()
{
    m_handler = adoptNS([[WebCoreMediaCaptureStatusBarHandler alloc] initWithManager:this]);
    [m_handler start];
}

void MediaCaptureStatusBarManager::stop()
{
    [m_handler stop];
}

}

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)
