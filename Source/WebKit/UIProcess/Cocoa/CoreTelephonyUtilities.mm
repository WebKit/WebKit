/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#import "CoreTelephonyUtilities.h"

#if HAVE(CORE_TELEPHONY)

#import "DefaultWebBrowserChecks.h"
#import "Logging.h"
#import <WebCore/RegistrableDomain.h>
#import <wtf/RetainPtr.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/spi/cocoa/SecuritySPI.h>

#import <pal/cocoa/CoreTelephonySoftLink.h>

namespace WebKit {

#if HAVE(ESIM_AUTOFILL_SYSTEM_SUPPORT)

bool shouldAllowAutoFillForCellularIdentifiers(const URL& topURL)
{
    static bool hasLogged = false;
    if (isFullWebBrowserOrRunningTest()) {
        if (!std::exchange(hasLogged, true))
            RELEASE_LOG(Telephony, "Skipped cellular AutoFill status check (app is a web browser)");
        return false;
    }

    static bool hasPublicCellularPlanEntitlement = [] {
        auto task = adoptCF(SecTaskCreateFromSelf(kCFAllocatorDefault));
        if (!task)
            return false;

        auto entitlementValue = adoptCF(SecTaskCopyValueForEntitlement(task.get(), CFSTR("com.apple.CommCenter.fine-grained"), nullptr));
        auto entitlementValueAsArray = (__bridge NSArray *)dynamic_cf_cast<CFArrayRef>(entitlementValue.get());
        for (id value in entitlementValueAsArray) {
            if (auto string = dynamic_objc_cast<NSString>(value); [string isEqualToString:@"public-cellular-plan"])
                return true;
        }
        return false;
    }();

    if (!hasPublicCellularPlanEntitlement) {
        if (!std::exchange(hasLogged, true))
            RELEASE_LOG(Telephony, "Skipped cellular AutoFill status check (app does not have cellular plan entitlement)");
        return false;
    }

    auto host = topURL.host().toString();
    if (host.isEmpty()) {
        if (!std::exchange(hasLogged, true))
            RELEASE_LOG(Telephony, "Skipped cellular AutoFill status check (no registrable domain)");
        return false;
    }

#if HAVE(DELAY_INIT_LINKING)
    static NeverDestroyed cachedClient = adoptNS([[CoreTelephonyClient alloc] initWithQueue:dispatch_get_main_queue()]);
#else
    static NeverDestroyed cachedClient = adoptNS([PAL::allocCoreTelephonyClientInstance() initWithQueue:dispatch_get_main_queue()]);
#endif
    auto client = cachedClient->get();

    static NeverDestroyed<String> lastQueriedHost;
    static bool lastQueriedHostResult = false;
    if (lastQueriedHost.get() == host)
        return lastQueriedHostResult;

    NSError *error = nil;
    BOOL result = [client isAutofilleSIMIdAllowedForDomain:host error:&error];
    if (error && !std::exchange(hasLogged, true)) {
        RELEASE_LOG_ERROR(Telephony, "Failed to query cellular AutoFill status: %{public}@", error);
        return false;
    }

    if (!std::exchange(hasLogged, true))
        RELEASE_LOG(Telephony, "Is cellular AutoFill allowed for current host? %{public}@", result ? @"YES" : @"NO");

    lastQueriedHost.get() = WTFMove(host);
    lastQueriedHostResult = !!result;
    return lastQueriedHostResult;
}

#endif // HAVE(ESIM_AUTOFILL_SYSTEM_SUPPORT)

} // namespace WebKit

#endif // HAVE(CORE_TELEPHONY)
