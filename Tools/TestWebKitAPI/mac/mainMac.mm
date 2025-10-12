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
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

static void forceSiteIsolationForTesting()
{
    // Use these indirect lookup techniques because some users of mainMac.mm don't link WebKit
    // e.g. TestWTF, TestIPC, etc
    Class theClass = NSClassFromString(@"WKPreferences");
    SEL sel = NSSelectorFromString(@"_forceSiteIsolationAlwaysOnForTesting");
    if ([theClass respondsToSelector:sel])
        [theClass performSelector:sel];
}

static void handleArguments(int argc, char** argv, NSMutableDictionary *argumentDefaults)
{
    // FIXME: We should switch these defaults to use overlay scrollbars, since they are the
    // default on the platform, but a variety of tests will need changes.
    argumentDefaults[@"NSOverlayScrollersEnabled"] = @NO;
    argumentDefaults[@"AppleShowScrollBars"] = @"Always";

    // FIXME: Remove once the root cause is fixed in rdar://159372811
    argumentDefaults[@"NSEventConcurrentProcessingEnabled"] = @NO;

    for (int i = 1; i < argc; ++i) {
        // These defaults are not propagated manually but are only consulted in the UI process.
        if (strcmp(argv[i], "--remote-layer-tree") == 0)
            argumentDefaults[@"WebKit2UseRemoteLayerTreeDrawingArea"] = @YES;
        else if (strcmp(argv[i], "--no-remote-layer-tree") == 0)
            argumentDefaults[@"WebKit2UseRemoteLayerTreeDrawingArea"] = @NO;
        else if (strcmp(argv[i], "--use-gpu-process") == 0)
            argumentDefaults[@"WebKit2GPUProcessForDOMRendering"] = @YES;
        else if (strcmp(argv[i], "--no-use-gpu-process") == 0)
            argumentDefaults[@"WebKit2GPUProcessForDOMRendering"] = @NO;
        else if (strcmp(argv[i], "--site-isolation") == 0)
            forceSiteIsolationForTesting();
    }
}

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

        auto argumentDefaults = adoptNS([[NSMutableDictionary alloc] init]);
        handleArguments(argc, argv, argumentDefaults.get());

        [argumentDomain addEntriesFromDictionary:argumentDefaults.get()];
        [[NSUserDefaults standardUserDefaults] setVolatileDomain:argumentDomain.get() forName:NSArgumentDomain];

        enableAllSDKAlignedBehaviors();

        [NSApplication sharedApplication];

        passed = TestWebKitAPI::TestsController::singleton().run(argc, argv);
    }

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
