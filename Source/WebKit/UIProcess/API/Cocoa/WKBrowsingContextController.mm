/*
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
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
#import "WKBrowsingContextController.h"

#import "PageLoadStateObserver.h"
#import "WebProcessPool.h"

ALLOW_DEPRECATED_IMPLEMENTATIONS_BEGIN
@implementation WKBrowsingContextController
ALLOW_DEPRECATED_IMPLEMENTATIONS_END

#pragma mark Loading

+ (void)registerSchemeForCustomProtocol:(NSString *)scheme
{
    if ([NSThread isMainThread])
        WebKit::WebProcessPool::registerGlobalURLSchemeAsHavingCustomProtocolHandlers(scheme);
    else {
        // This cannot be RunLoop::mainSingleton().dispatch because it is called before the main runloop is initialized. See rdar://problem/73615999
        WorkQueue::mainSingleton().dispatch([scheme = retainPtr(scheme)] {
            WebKit::WebProcessPool::registerGlobalURLSchemeAsHavingCustomProtocolHandlers(scheme.get());
        });
    }
}

+ (void)unregisterSchemeForCustomProtocol:(NSString *)scheme
{
    if ([NSThread isMainThread])
        WebKit::WebProcessPool::unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(scheme);
    else {
        // This cannot be RunLoop::mainSingleton().dispatch because it is called before the main runloop is initialized. See rdar://problem/73615999
        WorkQueue::mainSingleton().dispatch([scheme = retainPtr(scheme)] {
            WebKit::WebProcessPool::unregisterGlobalURLSchemeAsHavingCustomProtocolHandlers(scheme.get());
        });
    }
}

@end
