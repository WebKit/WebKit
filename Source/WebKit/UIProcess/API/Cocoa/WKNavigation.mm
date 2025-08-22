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
#import "WKNavigationInternal.h"
#import "WKWebpagePreferencesInternal.h"

#import "APINavigation.h"
#import "FrameInfoData.h"
#import "WKFrameInfoInternal.h"
#import "WebFrameProxy.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/AlignedStorage.h>

@implementation WKNavigation {
    AlignedStorage<API::Navigation> _navigation;
}

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKNavigation.class, self))
        return;

    Ref { *_navigation }->~Navigation();

    [super dealloc];
}

- (NSURLRequest *)_request
{
    return _navigation->originalRequest().nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
}

- (BOOL)_isUserInitiated
{
    return _navigation->wasUserInitiated();
}

- (WKFrameInfo *)_initiatingFrame
{
    auto& frameInfo = _navigation->originatingFrameInfo();
    if (!frameInfo)
        return nil;
    RefPtr frame = WebKit::WebFrameProxy::webFrame(frameInfo->frameID);
    return wrapper(API::FrameInfo::create(WebKit::FrameInfoData { *frameInfo })).autorelease();
}

#if PLATFORM(IOS_FAMILY)

- (WKContentMode)effectiveContentMode
{
    return WebKit::contentMode(_navigation->effectiveContentMode());
}

#endif // PLATFORM(IOS_FAMILY)

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_navigation;
}

@end
