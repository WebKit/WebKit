/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if USE(WEB_THREAD)

#import "PlatformUtilities.h"
#import <Foundation/Foundation.h>
#import <WebCore/WebCoreThread.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

TEST(WebKitLegacy, NestedRunLoopUnderRunLoopObserverDoubleUnlock)
{
    WebThreadEnable();

    WebThreadLock();
    
    __block BOOL spunInnerRunLoop = NO;
    auto observer = adoptCF(CFRunLoopObserverCreateWithHandler(NULL, kCFRunLoopBeforeWaiting, NO, 2, ^(CFRunLoopObserverRef observer, CFRunLoopActivity activity) {
        Util::spinRunLoop(1);
        spunInnerRunLoop = YES;
    }));

    CFRunLoopAddObserver(CFRunLoopGetMain(), observer.get(), kCFRunLoopCommonModes);

    // We must use a time-based instead of count-or-condition-based spin,
    // so that we hit the above "before waiting" observer.
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);
    
    EXPECT_TRUE(spunInnerRunLoop);
    
    // Spinning the runloop should have resulted in dropping the lock,
    // and *not underflowing* the lock count (which is also reported as "locked").
    EXPECT_FALSE(WebThreadIsLocked());
}

}

#endif
