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
#import "WKNavigationActionInternal.h"

#import "NavigationActionData.h"
#import "WKFrameInfoInternal.h"
#import "WKNavigationInternal.h"
#import "WebEventFactory.h"
#import "_WKUserInitiatedActionInternal.h"
#import <WebCore/FloatPoint.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "WebIOSEventFactory.h"
#endif

@implementation WKNavigationAction

static WKNavigationType toWKNavigationType(WebCore::NavigationType navigationType)
{
    switch (navigationType) {
    case WebCore::NavigationType::LinkClicked:
        return WKNavigationTypeLinkActivated;
    case WebCore::NavigationType::FormSubmitted:
        return WKNavigationTypeFormSubmitted;
    case WebCore::NavigationType::BackForward:
        return WKNavigationTypeBackForward;
    case WebCore::NavigationType::Reload:
        return WKNavigationTypeReload;
    case WebCore::NavigationType::FormResubmitted:
        return WKNavigationTypeFormResubmitted;
    case WebCore::NavigationType::Other:
        return WKNavigationTypeOther;
    }

    ASSERT_NOT_REACHED();
    return WKNavigationTypeOther;
}

#if PLATFORM(IOS_FAMILY)
static WKSyntheticClickType toWKSyntheticClickType(WebKit::WebMouseEvent::SyntheticClickType syntheticClickType)
{
    switch (syntheticClickType) {
    case WebKit::WebMouseEvent::NoTap:
        return WKSyntheticClickTypeNoTap;
    case WebKit::WebMouseEvent::OneFingerTap:
        return WKSyntheticClickTypeOneFingerTap;
    case WebKit::WebMouseEvent::TwoFingerTap:
        return WKSyntheticClickTypeTwoFingerTap;
    }
    ASSERT_NOT_REACHED();
    return WKSyntheticClickTypeNoTap;
}
#endif

- (void)dealloc
{
    _navigationAction->~NavigationAction();

    [super dealloc];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; navigationType = %ld; syntheticClickType = %ld; position x = %.2f y = %.2f request = %@; sourceFrame = %@; targetFrame = %@>", NSStringFromClass(self.class), self,
        (long)self.navigationType,
#if PLATFORM(IOS_FAMILY)
        (long)self._syntheticClickType, self._clickLocationInRootViewCoordinates.x, self._clickLocationInRootViewCoordinates.y,
#else
        0L, 0.0, 0.0,
#endif
        self.request, self.sourceFrame, self.targetFrame];
}

- (WKFrameInfo *)sourceFrame
{
    return wrapper(_navigationAction->sourceFrame());
}

- (WKFrameInfo *)targetFrame
{
    return wrapper(_navigationAction->targetFrame());
}

- (WKNavigationType)navigationType
{
    return toWKNavigationType(_navigationAction->navigationType());
}

- (NSURLRequest *)request
{
    return _navigationAction->request().nsURLRequest(WebCore::HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
}

#if PLATFORM(IOS_FAMILY)
- (WKSyntheticClickType)_syntheticClickType
{
    return toWKSyntheticClickType(_navigationAction->syntheticClickType());
}

- (CGPoint)_clickLocationInRootViewCoordinates
{
    return _navigationAction->clickLocationInRootViewCoordinates();
}
#endif

#if PLATFORM(MAC)

- (NSEventModifierFlags)modifierFlags
{
    return WebKit::WebEventFactory::toNSEventModifierFlags(_navigationAction->modifiers());
}

- (NSInteger)buttonNumber
{
    return WebKit::WebEventFactory::toNSButtonNumber(_navigationAction->mouseButton());
}

#else

- (UIKeyModifierFlags)modifierFlags
{
    return WebIOSEventFactory::toUIKeyModifierFlags(_navigationAction->modifiers());
}

#endif

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_navigationAction;
}

@end

@implementation WKNavigationAction (WKPrivate)

- (NSURL *)_originalURL
{
    return _navigationAction->originalURL();
}

- (BOOL)_isUserInitiated
{
    return _navigationAction->isProcessingUserGesture();
}

- (BOOL)_canHandleRequest
{
    return _navigationAction->canHandleRequest();
}

- (BOOL)_shouldOpenExternalSchemes
{
    return _navigationAction->shouldOpenExternalSchemes();
}

- (BOOL)_shouldOpenAppLinks
{
    return _navigationAction->shouldOpenAppLinks();
}

- (BOOL)_shouldPerformDownload
{
    return _navigationAction->shouldPerformDownload();
}

- (BOOL)_shouldOpenExternalURLs
{
    return [self _shouldOpenExternalSchemes];
}

- (_WKUserInitiatedAction *)_userInitiatedAction
{
    return wrapper(_navigationAction->userInitiatedAction());
}

- (BOOL)_isRedirect
{
    return _navigationAction->isRedirect();
}

- (WKNavigation *)_mainFrameNavigation
{
    return wrapper(_navigationAction->mainFrameNavigation());
}

@end
