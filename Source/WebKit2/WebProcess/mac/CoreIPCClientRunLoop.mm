/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import "CoreIPCClientRunLoop.h"

#import <WebCore/ResourceHandle.h>
#import <wtf/RetainPtr.h>

using namespace WebCore;

@interface WKFunctionAdapter : NSObject
{
@public
    WebKit::FunctionWithContext function;
    void* context;
}
- (void)perform;
@end

@implementation WKFunctionAdapter

- (void)perform
{
    function(context);
}

@end

namespace WebKit {

static CFArrayRef createCoreIPCRunLoopModesArray()
{
    // Ideally we'd like to list all modes here that might be used for run loops while we are handling networking.
    // We have to explicitly include the run loop mode used for synchronous loads in WebCore so we don't get deadlock
    // when those loads call security functions that are shimmed.
    const void* values[2] = { kCFRunLoopCommonModes, ResourceHandle::synchronousLoadRunLoopMode() };
    return CFArrayCreate(0, values, 2, &kCFTypeArrayCallBacks);
}

static NSArray *coreIPCRunLoopModesArray()
{
    static CFArrayRef modes = createCoreIPCRunLoopModesArray();
    return (NSArray *)modes;
}

void callOnCoreIPCClientRunLoopAndWait(FunctionWithContext function, void* context)
{
    // FIXME: It would fit better with WebKit2 coding style to use a WTF Function here.
    // To do that we'd need to make dispatch have an overload that takes an array of run loop modes or find some
    // other way to specify that we want to include the synchronous load run loop mode.
    RetainPtr<WKFunctionAdapter> adapter(AdoptNS, [[WKFunctionAdapter alloc] init]);
    adapter->function = function;
    adapter->context = context;
    [adapter.get() performSelectorOnMainThread:@selector(perform) withObject:nil waitUntilDone:YES modes:coreIPCRunLoopModesArray()];
}

}
