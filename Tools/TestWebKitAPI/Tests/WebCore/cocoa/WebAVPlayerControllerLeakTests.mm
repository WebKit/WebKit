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

#if PLATFORM(IOS_FAMILY) && HAVE(AVKIT)

#import <WebCore/WebAVPlayerController.h>
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

// Test that the normal WebAVPlayerControllerForwarder class properly releases its ivar.
TEST(WebAVPlayerControllerLeak, NormalForwarderClassReleasesIvar)
{
    __weak id weakController = nil;

    @autoreleasepool {
        // Get the normal WebAVPlayerControllerForwarder class (not the dynamic subclass)
        Class forwarderClass = objc_getClass("WebAVPlayerControllerForwarder");
        ASSERT_NE(forwarderClass, nullptr);

        // Create an instance of the normal forwarder class
        id forwarder = [[forwarderClass alloc] init];
        ASSERT_NE(forwarder, nil);

        // Get the _playerController via the getter
        id playerController = [forwarder performSelector:@selector(playerController)];
        ASSERT_NE(playerController, nil);

        // Store a weak reference to track if it gets deallocated
        weakController = playerController;
        EXPECT_NE(weakController, nil);

        // Release the forwarder - this should release the _playerController too
        [forwarder release];
    }

    // The weak reference should be nil because the _playerController was properly released
    EXPECT_EQ(weakController, nil);
}

// Test that the dynamically-created WebAVPlayerControllerForwarder_AVKitCompatible class
// properly releases its ivar.
TEST(WebAVPlayerControllerLeak, DynamicForwarderClassReleasesIvar)
{
    __weak id weakController = nil;

    @autoreleasepool {
        // Get the dynamically-created class
        Class dynamicClass = WebCore::webAVPlayerControllerClassSingleton();
        ASSERT_NE(dynamicClass, nullptr);

        // Verify it's the dynamic class, not the normal one
        EXPECT_STREQ(class_getName(dynamicClass), "WebAVPlayerControllerForwarder_AVKitCompatible");

        // Create an instance of the dynamic class
        id forwarder = [[dynamicClass alloc] init];
        ASSERT_NE(forwarder, nil);

        // Get the _playerController via the getter
        id playerController = [forwarder performSelector:@selector(playerController)];

        // The playerController should be set by -init
        ASSERT_NE(playerController, nil);

        // Store a weak reference to track if it gets deallocated
        weakController = playerController;
        EXPECT_NE(weakController, nil);

        // Release the forwarder - this should release the _playerController too
        [forwarder release];
    }

    // The weak reference should be nil because the _playerController was properly released.
    EXPECT_EQ(weakController, nil);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(IOS_FAMILY) && HAVE(AVKIT)
