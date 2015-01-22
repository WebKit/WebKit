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

#import <Foundation/Foundation.h>

#if USE(APPLE_INTERNAL_SDK)

#import <AssertionServices/BKSProcessAssertion.h>

#else

enum {
    BKSProcessAssertionNone = 0,
    BKSProcessAssertionPreventTaskSuspend  = 1 << 0,
    BKSProcessAssertionPreventTaskThrottleDown = 1 << 1,
    BKSProcessAssertionAllowIdleSleep = 1 << 2,
    BKSProcessAssertionWantsForegroundResourcePriority = 1 << 3,
    BKSProcessAssertionAllowSuspendOnSleep = 1 << 4,
};
typedef uint32_t BKSProcessAssertionFlags;

enum {
    BKSProcessAssertionReasonExtension = 13,
};
typedef uint32_t BKSProcessAssertionReason;

typedef void(^BKSProcessAssertionAcquisitionHandler)(BOOL acquired);

@interface BKSProcessAssertion : NSObject
@end

@interface BKSProcessAssertion (Details)
@property (nonatomic, assign) BKSProcessAssertionFlags flags;
- (id)initWithPID:(pid_t)pid flags:(BKSProcessAssertionFlags)flags reason:(BKSProcessAssertionReason)reason name:(NSString *)name withHandler:(BKSProcessAssertionAcquisitionHandler)handler;
- (void)invalidate;
@end

#endif
