/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#import "TestsController.h"
#import <wtf/RetainPtr.h>

#if !defined(BUILDING_TEST_IPC) && !defined(BUILDING_TEST_WTF) && !defined(BUILDING_TEST_WGSL)
#import <WebKit/WKProcessPoolPrivate.h>
#endif

extern "C" void _BeginEventReceiptOnThread(void);

int main(int argc, char** argv)
{
    bool passed = false;
    @autoreleasepool {
        [[NSUserDefaults standardUserDefaults] removePersistentDomainForName:@"TestWebKitAPI"];

        // Set a user default for TestWebKitAPI to bypass all linked-on-or-after checks in WebKit
        auto argumentDomain = adoptNS([[[NSUserDefaults standardUserDefaults] volatileDomainForName:NSArgumentDomain] mutableCopy]);
        if (!argumentDomain)
            argumentDomain = adoptNS([[NSMutableDictionary alloc] init]);

        // CAUTION: Defaults set here are not automatically propagated to the
        // Web Content process. Those listed below are propagated manually.
        NSDictionary *dict = @{
            // FIXME: We should switch these defaults to use overlay
            // scrollbars, since they are the default on the platform,
            // but a variety of tests will need changes.
            @"NSOverlayScrollersEnabled": @NO,
            @"AppleShowScrollBars": @"Always",
        };

        [argumentDomain addEntriesFromDictionary:dict];
        [[NSUserDefaults standardUserDefaults] setVolatileDomain:argumentDomain.get() forName:NSArgumentDomain];

#if !defined(BUILDING_TEST_IPC) && !defined(BUILDING_TEST_WTF) && !defined(BUILDING_TEST_WGSL)
        [WKProcessPool _setLinkedOnOrAfterEverythingForTesting];
#endif

        [NSApplication sharedApplication];
        _BeginEventReceiptOnThread(); // Makes window visibility notifications work (and possibly more).

        passed = TestWebKitAPI::TestsController::singleton().run(argc, argv);
    }

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
