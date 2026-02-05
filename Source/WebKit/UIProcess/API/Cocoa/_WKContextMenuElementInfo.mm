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
#import "_WKContextMenuElementInfo.h"

#import "APIHitTestResult.h"
#import "APIString.h"
#import "_WKContextMenuElementInfoInternal.h"
#import "_WKHitTestResultInternal.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/RetainPtr.h>

#if !PLATFORM(IOS_FAMILY)

@implementation _WKContextMenuElementInfo

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKContextMenuElementInfo.class, self))
        return;
    _contextMenuElementInfoMac->API::ContextMenuElementInfoMac::~ContextMenuElementInfoMac();
    [super dealloc];
}

- (_WKHitTestResult *)hitTestResult
{
    auto& hitTestResultData = _contextMenuElementInfoMac->hitTestResultData();
    auto apiHitTestResult = API::HitTestResult::create(hitTestResultData, protect(_contextMenuElementInfoMac->page()).get());
    return retainPtr(wrapper(apiHitTestResult)).autorelease();
}

- (NSString *)qrCodePayloadString
{
    auto& qrCodePayloadString = _contextMenuElementInfoMac->qrCodePayloadString();
    return nsStringNilIfEmpty(qrCodePayloadString).autorelease();
}

- (BOOL)hasEntireImage
{
    return _contextMenuElementInfoMac->hasEntireImage();
}

- (BOOL)allowsFollowingLink
{
    return _contextMenuElementInfoMac->allowsFollowingLink();
}

- (BOOL)allowsFollowingImageURL
{
    return _contextMenuElementInfoMac->allowsFollowingImageURL();
}

// MARK: WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_contextMenuElementInfoMac;
}

@end

#endif // !PLATFORM(IOS_FAMILY)
