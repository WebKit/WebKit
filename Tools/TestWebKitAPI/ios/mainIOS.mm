/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "UIKitMacHelperSPI.h"
#import <wtf/RetainPtr.h>

#if !defined(BUILDING_TEST_IPC) && !defined(BUILDING_TEST_WTF) && !defined(BUILDING_TEST_WGSL)
#import <WebKit/WKProcessPoolPrivate.h>
#endif

int main(int argc, char** argv)
{
    bool passed = false;
    @autoreleasepool {
#if PLATFORM(MACCATALYST)
        UINSApplicationInstantiate();
#endif

        [[NSUserDefaults standardUserDefaults] removePersistentDomainForName:@"TestWebKitAPI"];

        // Set up user defaults.
        auto argumentDomain = adoptNS([[[NSUserDefaults standardUserDefaults] volatileDomainForName:NSArgumentDomain] mutableCopy]);
        if (!argumentDomain)
            argumentDomain = adoptNS([[NSMutableDictionary alloc] init]);

        [[NSUserDefaults standardUserDefaults] setVolatileDomain:argumentDomain.get() forName:NSArgumentDomain];

#if !defined(BUILDING_TEST_IPC) && !defined(BUILDING_TEST_WTF) && !defined(BUILDING_TEST_WGSL)
        [WKProcessPool _setLinkedOnOrAfterEverythingForTesting];
#endif

        passed = TestWebKitAPI::TestsController::singleton().run(argc, argv);
    }

    // FIXME: Work-around for <rdar://problem/77922262>
    exit(passed ? EXIT_SUCCESS : EXIT_FAILURE);
}
