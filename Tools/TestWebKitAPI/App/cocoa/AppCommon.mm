/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "AppCommon.h"

#import "Helpers/TestBundleLoader.h"
#import <Foundation/Foundation.h>
#import <WebKit/WebKitPrivate.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

namespace TestWebKitAPI {

static void registerTestClasses()
{
    NSURL *testBundleURL = [NSURL URLWithString:@"TestWebKitAPIBundle.xctest" relativeToURL:NSBundle.mainBundle.builtInPlugInsURL];
    if (!testBundleURL)
        return;

    Class principalClass = [NSBundle bundleWithURL:testBundleURL].principalClass;
    if ([principalClass respondsToSelector:@selector(registerTestClasses)])
        [principalClass registerTestClasses];
}

void initializeApp()
{
    RetainPtr standardDefaults = [NSUserDefaults standardUserDefaults];
    [standardDefaults removePersistentDomainForName:@"TestWebKitAPI"];
    [standardDefaults removeObjectForKey:@"WebCoreLogging"];
    [standardDefaults removeObjectForKey:@"WebKit2Logging"];

    // Set up user defaults.
    NSMutableDictionary *argumentDomain = [[NSUserDefaults.standardUserDefaults volatileDomainForName:NSArgumentDomain] mutableCopy];
    if (!argumentDomain)
        argumentDomain = [[NSMutableDictionary alloc] init];

    [NSUserDefaults.standardUserDefaults setVolatileDomain:argumentDomain forName:NSArgumentDomain];

    enableAllSDKAlignedBehaviors();

    registerTestClasses();
}

} // namespace TestWebKitAPI
