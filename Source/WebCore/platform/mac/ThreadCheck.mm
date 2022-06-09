/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ThreadCheck.h"

#if PLATFORM(MAC)

#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/StdLibExtras.h>
#import <wtf/text/StringHash.h>

namespace WebCore {

static bool didReadThreadViolationBehaviorFromUserDefaults = false;
static bool threadViolationBehaviorIsDefault = true;
static ThreadViolationBehavior threadViolationBehavior[MaximumThreadViolationRound] = { RaiseExceptionOnThreadViolation, RaiseExceptionOnThreadViolation, RaiseExceptionOnThreadViolation };

static void readThreadViolationBehaviorFromUserDefaults()
{
    didReadThreadViolationBehaviorFromUserDefaults = true;

    ThreadViolationBehavior newBehavior = LogOnFirstThreadViolation;
    NSString *threadCheckLevel = [[NSUserDefaults standardUserDefaults] stringForKey:@"WebCoreThreadCheck"];
    if (!threadCheckLevel)
        return;

    if ([threadCheckLevel isEqualToString:@"None"])
        newBehavior = NoThreadCheck;
    else if ([threadCheckLevel isEqualToString:@"Exception"])
        newBehavior = RaiseExceptionOnThreadViolation;
    else if ([threadCheckLevel isEqualToString:@"Log"])
        newBehavior = LogOnThreadViolation;
    else if ([threadCheckLevel isEqualToString:@"LogOnce"])
        newBehavior = LogOnFirstThreadViolation;
    else
        ASSERT_NOT_REACHED();

    threadViolationBehaviorIsDefault = false;

    for (unsigned i = 0; i < MaximumThreadViolationRound; ++i)
        threadViolationBehavior[i] = newBehavior;
}

void setDefaultThreadViolationBehavior(ThreadViolationBehavior behavior, ThreadViolationRound round)
{
    ASSERT(round < MaximumThreadViolationRound);
    if (round >= MaximumThreadViolationRound)
        return;
    if (!didReadThreadViolationBehaviorFromUserDefaults)
        readThreadViolationBehaviorFromUserDefaults();
    if (threadViolationBehaviorIsDefault)
        threadViolationBehavior[round] = behavior;
}

void reportThreadViolation(const char* function, ThreadViolationRound round)
{
    ASSERT(round < MaximumThreadViolationRound);
    if (round >= MaximumThreadViolationRound)
        return;
    if (!didReadThreadViolationBehaviorFromUserDefaults)
        readThreadViolationBehaviorFromUserDefaults();
    if (threadViolationBehavior[round] == NoThreadCheck)
        return;
    if (pthread_main_np())
        return;
    WebCoreReportThreadViolation(function, round);
}

} // namespace WebCore

// Split out the actual reporting of the thread violation to make it easier to set a breakpoint
void WebCoreReportThreadViolation(const char* function, WebCore::ThreadViolationRound round)
{
    using namespace WebCore;

    ASSERT(round < MaximumThreadViolationRound);
    if (round >= MaximumThreadViolationRound)
        return;

    static NeverDestroyed<HashSet<String>> loggedFunctions;
    switch (threadViolationBehavior[round]) {
        case NoThreadCheck:
            break;
        case LogOnFirstThreadViolation:
            if (loggedFunctions.get().add(function).isNewEntry) {
                NSLog(@"WebKit Threading Violation - %s called from secondary thread", function);
                NSLog(@"Additional threading violations for this function will not be logged.");
            }
            break;
        case LogOnThreadViolation:
            NSLog(@"WebKit Threading Violation - %s called from secondary thread", function);
            break;
        case RaiseExceptionOnThreadViolation:
            [NSException raise:@"WebKitThreadingException" format:@"%s was called from a secondary thread", function];
            break;
    }
}

#endif // PLATFORM(MAC)
