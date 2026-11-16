/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import <wtf/Logger.h>

#import <Foundation/Foundation.h>
#import <wtf/HexNumber.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

@interface TestWTFLogger : NSObject
@end

@implementation TestWTFLogger

- (NSString *)description
{
    return @"TestWTFLogger description";
}

@end

@interface TestWTFLoggerProxy : NSProxy
- (instancetype)initWithTarget:(id)target;
@end

@implementation TestWTFLoggerProxy {
    RetainPtr<id> _target;
}

- (instancetype)initWithTarget:(id)target
{
    _target = target;
    return self;
}

- (void)forwardInvocation:(NSInvocation *)invocation
{
    [invocation invokeWithTarget:_target.get()];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector
{
    return [_target methodSignatureForSelector:aSelector];
}

@end

OBJC_ROOT_CLASS @interface TestWTFLoggerRootClass
@end

TEST(WTF_Logger, NSError)
{
    @autoreleasepool {
        NSError *error = [NSError errorWithDomain:@"TestWTFDomain" code:1 userInfo:@{ NSLocalizedDescriptionKey : @"TestWTF Error" }];
        EXPECT_EQ(WTF::LogArgument<NSError *>::toString(error), String(error.localizedDescription));
    }
}

TEST(WTF_Logger, NSObject)
{
    RetainPtr logger = adoptNS([[TestWTFLogger alloc] init]);
    EXPECT_EQ(WTF::LogArgument<TestWTFLogger *>::toString(logger.get()), String([logger description]));
}

TEST(WTF_Logger, NSProxy)
{
    RetainPtr logger = adoptNS([[TestWTFLogger alloc] init]);
    RetainPtr loggerProxy = adoptNS([[TestWTFLoggerProxy alloc] initWithTarget:logger.get()]);
    EXPECT_EQ(WTF::LogArgument<TestWTFLoggerProxy *>::toString(loggerProxy.get()), String([loggerProxy description]));
}

TEST(WTF_Logger, NSString)
{
    @autoreleasepool {
        NSString *string = @"TestWTF String";
        EXPECT_EQ(WTF::LogArgument<NSString *>::toString(string), String(string));
    }
}

TEST(WTF_Logger, ObjCIDType)
{
    RetainPtr logger = adoptNS([[TestWTFLogger alloc] init]);
    EXPECT_EQ(WTF::LogArgument<id>::toString(logger.get()), String([logger description]));
}

TEST(WTF_Logger, ObjCRootClass)
{
    TestWTFLoggerRootClass *rootObject = nil;
    EXPECT_EQ(WTF::LogArgument<id>::toString(rootObject), WTF::LogArgument<const void*>::toString(rootObject));
}
