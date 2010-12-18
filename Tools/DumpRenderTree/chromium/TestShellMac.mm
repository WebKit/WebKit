/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "TestShell.h"
#include "webkit/support/webkit_support.h"
#import <AppKit/AppKit.h>

// A class to be the target/selector of the "watchdog" thread that ensures
// pages timeout if they take too long and tells the test harness via stdout.
@interface WatchDogTarget : NSObject {
@private
    NSTimeInterval _timeout;
}
// |timeout| is in seconds
- (id)initWithTimeout:(NSTimeInterval)timeout;
// serves as the "run" method of a NSThread.
- (void)run:(id)sender;
@end

@implementation WatchDogTarget

- (id)initWithTimeout:(NSTimeInterval)timeout
{
    if ((self = [super init]))
        _timeout = timeout;
    return self;
}

- (void)run:(id)ignore
{
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    // check for debugger, just bail if so. We don't want the timeouts hitting
    // when we're trying to track down an issue.
    if (webkit_support::BeingDebugged())
        return;

    NSThread* currentThread = [NSThread currentThread];

    // Wait to be cancelled. If we are that means the test finished. If it hasn't,
    // then we need to tell the layout script we timed out and start again.
    NSDate* limitDate = [NSDate dateWithTimeIntervalSinceNow:_timeout];
    while ([(NSDate*)[NSDate date] compare:limitDate] == NSOrderedAscending &&
           ![currentThread isCancelled]) {
        // sleep for a small increment then check again
        NSDate* incrementDate = [NSDate dateWithTimeIntervalSinceNow:1.0];
        [NSThread sleepUntilDate:incrementDate];
    }
    if (![currentThread isCancelled]) {
        // Print a warning to be caught by the layout-test script.
        // Note: the layout test driver may or may not recognize
        // this as a timeout.
        puts("#TEST_TIMED_OUT\n");
        puts("#EOF\n");
        fflush(stdout);
        exit(0);
    }

    [pool release];
}

@end

void TestShell::waitTestFinished()
{
    ASSERT(!m_testIsPending);

    m_testIsPending = true;

    // Create a watchdog thread which just sets a timer and
    // kills the process if it times out.  This catches really
    // bad hangs where the shell isn't coming back to the
    // message loop.  If the watchdog is what catches a
    // timeout, it can't do anything except terminate the test
    // shell, which is unfortunate.
    // Windows multiplies by 2.5, but that causes us to run for far, far too
    // long. We use the passed value and let the scripts flag override
    // the value as needed.
    NSTimeInterval timeoutSeconds = layoutTestTimeoutForWatchDog() / 1000;
    WatchDogTarget* watchdog = [[[WatchDogTarget alloc]
                                    initWithTimeout:timeoutSeconds] autorelease];
    NSThread* thread = [[NSThread alloc] initWithTarget:watchdog
                                         selector:@selector(run:)
                                         object:nil];
    [thread start];

    // TestFinished() will post a quit message to break this loop when the page
    // finishes loading.
    while (m_testIsPending)
        webkit_support::RunMessageLoop();

    // Tell the watchdog that we're finished. No point waiting to re-join, it'll
    // die on its own.
    [thread cancel];
    [thread release];
}

void platformInit(int*, char***)
{
}

void openStartupDialog()
{
    // FIXME: This code doesn't work. Need NSApplication event loop?
    NSAlert* alert = [[[NSAlert alloc] init] autorelease];
    alert.messageText = @"Attach to me?";
    alert.informativeText = @"This would probably be a good time to attach your debugger.";
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

bool checkLayoutTestSystemDependencies()
{
    return true;
}

