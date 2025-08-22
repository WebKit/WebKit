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
#import "_WKHitTestResultInternal.h"

#import "WKFrameInfoInternal.h"
#import "WebPageProxy.h"

#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)

#import <WebCore/WebCoreObjCExtras.h>

@implementation _WKHitTestResult

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKHitTestResult.class, self))
        return;

    _hitTestResult->~HitTestResult();

    [super dealloc];
}

static NSURL *URLFromString(const WTF::String& urlString)
{
    return urlString.isEmpty() ? nil : [NSURL URLWithString:urlString.createNSString().get()];
}

- (NSURL *)absoluteImageURL
{
    return URLFromString(_hitTestResult->absoluteImageURL());
}

- (NSURL *)absolutePDFURL
{
    return URLFromString(_hitTestResult->absolutePDFURL());
}

- (NSURL *)absoluteLinkURL
{
    return URLFromString(_hitTestResult->absoluteLinkURL());
}

- (BOOL)hasLocalDataForLinkURL
{
    return NO;
}

- (NSString *)linkLocalDataMIMEType
{
    return nil;
}

- (NSURL *)absoluteMediaURL
{
    return URLFromString(_hitTestResult->absoluteMediaURL());
}

- (NSString *)linkLabel
{
    return _hitTestResult->linkLabel().createNSString().autorelease();
}

- (NSString *)linkTitle
{
    return _hitTestResult->linkTitle().createNSString().autorelease();
}

- (NSString *)lookupText
{
    return _hitTestResult->lookupText().createNSString().autorelease();
}

- (NSString *)linkSuggestedFilename
{
    return _hitTestResult->linkSuggestedFilename().createNSString().autorelease();
}

- (NSString *)imageSuggestedFilename
{
    return _hitTestResult->imageSuggestedFilename().createNSString().autorelease();
}

- (NSString *)imageMIMEType
{
    return _hitTestResult->sourceImageMIMEType().createNSString().autorelease();
}

- (BOOL)isContentEditable
{
    return _hitTestResult->isContentEditable();
}

- (BOOL)isSelected
{
    return _hitTestResult->isSelected();
}

- (BOOL)isMediaDownloadable
{
    return _hitTestResult->isDownloadableMedia();
}

- (BOOL)isMediaFullscreen
{
    return _hitTestResult->mediaIsInFullscreen();
}

- (CGRect)elementBoundingBox
{
    return _hitTestResult->elementBoundingBox();
}

- (_WKHitTestResultElementType)elementType
{
    switch (_hitTestResult->elementType()) {
    case WebKit::WebHitTestResultData::ElementType::None:
        return _WKHitTestResultElementTypeNone;
    case WebKit::WebHitTestResultData::ElementType::Audio:
        return _WKHitTestResultElementTypeAudio;
    case WebKit::WebHitTestResultData::ElementType::Video:
        return _WKHitTestResultElementTypeVideo;
    }

    ASSERT_NOT_REACHED();
    return _WKHitTestResultElementTypeNone;
}

- (NSURLResponse *)linkLocalResourceResponse
{
    if (auto& response = _hitTestResult->linkLocalResourceResponse())
        return response->nsURLResponse();
    return nil;
}

- (WKFrameInfo *)frameInfo
{
    if (auto frameInfo = _hitTestResult->frameInfo())
        return wrapper(API::FrameInfo::create(WTFMove(*frameInfo))).autorelease();
    return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_hitTestResult;
}

@end

#endif
