/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import "PlatformUtilities.h"
#import <WebKit/WKWebProcessPlugIn.h>
#import <wtf/RetainPtr.h>

@interface WebProcessPlugIn : NSObject <WKWebProcessPlugIn>
@end

@implementation WebProcessPlugIn {
    RetainPtr<id <WKWebProcessPlugIn>> _testPlugIn;
}

- (NSArray *)additionalClassesForParameterCoder
{
    return @[@"MockContentFilterEnabler"];
}

- (void)webProcessPlugIn:(WKWebProcessPlugInController *)plugInController initializeWithObject:(id)initializationObject
{
    NSString *testPlugInClassName = [plugInController.parameters valueForKey:TestWebKitAPI::Util::TestPlugInClassNameParameter];
    ASSERT(testPlugInClassName);
    ASSERT([testPlugInClassName isKindOfClass:[NSString class]]);

    Class testPlugInClass = NSClassFromString(testPlugInClassName);
    ASSERT(testPlugInClass);
    ASSERT([testPlugInClass conformsToProtocol:@protocol(WKWebProcessPlugIn)]);

    ASSERT(!_testPlugIn);
    _testPlugIn = adoptNS([[testPlugInClass alloc] init]);

    if ([_testPlugIn respondsToSelector:@selector(webProcessPlugIn:initializeWithObject:)])
        [_testPlugIn webProcessPlugIn:plugInController initializeWithObject:initializationObject];
}

- (BOOL)respondsToSelector:(SEL)aSelector
{
    if ([_testPlugIn respondsToSelector:aSelector])
        return YES;
    return [super respondsToSelector:aSelector];
}

- (id)forwardingTargetForSelector:(SEL)aSelector
{
    return _testPlugIn.get();
}

@end
