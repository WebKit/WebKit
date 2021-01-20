/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "WTFStringUtilities.h"
#import <JavaScriptCore/JavaScriptCore.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/URL.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/WTFString.h>

// JavaScriptCore's behavior at the time of writing is that destructors
// run asynchronously on the thread that allocated the VM, unless they run
// synchronously during an API call on some other thread.

static bool s_isRunningRunLoop;
static bool s_done;
static WTF::RunLoop* s_expectedRunLoop;

@interface TestObject : NSObject
- (void)dealloc;
@end

@implementation TestObject
- (void)dealloc
{
    if (s_isRunningRunLoop) {
        EXPECT_EQ(&RunLoop::current(), s_expectedRunLoop);
        s_done = true;
    }

    [super dealloc];
}
@end

namespace TestWebKitAPI {

static void triggerGC(JSContext *context)
{
    @autoreleasepool {
        for (size_t i = 0; i < 1000; ++i)
            [JSValue valueWithObject:[[TestObject new] autorelease] inContext:context];
        JSGarbageCollect([context JSGlobalContextRef]);
    }
}

static void cycleRunLoop()
{
    @autoreleasepool {
        s_isRunningRunLoop = true;
        RunLoop::current().cycle();
        s_isRunningRunLoop = false;
    }
}

TEST(JavaScriptCore, IncrementalSweeperMainThread)
{
    auto context = adoptNS([JSContext new]);
    s_expectedRunLoop = &RunLoop::current();

    while (!s_done) {
        triggerGC(context.get());
        cycleRunLoop();
    }
}

TEST(JavaScriptCore, IncrementalSweeperSecondaryThread)
{
    auto context = adoptNS([JSContext new]);
    s_expectedRunLoop = &RunLoop::current();

    while (!s_done) {
        dispatch_sync(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
            triggerGC(context.get());
            cycleRunLoop();
        });

        cycleRunLoop();
    }
}

} // namespace TestWebKitAPI
