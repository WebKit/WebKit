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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "_WKWebExtensionActionInternal.h"

#import "CocoaImage.h"
#import "WebExtensionAction.h"
#import "WebExtensionContext.h"
#import "WebExtensionTab.h"
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>

NSNotificationName const _WKWebExtensionActionPropertiesDidChangeNotification = @"_WKWebExtensionActionPropertiesDidChange";
NSNotificationName const _WKWebExtensionActionPopupWebViewContentSizeDidChangeNotification = @"_WKWebExtensionActionPopupWebViewContentSizeDidChange";
NSNotificationName const _WKWebExtensionActionPopupWebViewDidCloseNotification = @"_WKWebExtensionActionPopupWebViewDidClose";

@implementation _WKWebExtensionAction

#if ENABLE(WK_WEB_EXTENSIONS)

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKWebExtensionAction.class, self))
        return;

    _webExtensionAction->~WebExtensionAction();
}

- (BOOL)isEqual:(id)object
{
    if (self == object)
        return YES;

    auto *other = dynamic_objc_cast<_WKWebExtensionAction>(object);
    if (!other)
        return NO;

    return *_webExtensionAction == *other->_webExtensionAction;
}

- (_WKWebExtensionContext *)webExtensionContext
{
    if (auto *context = _webExtensionAction->extensionContext())
        return context->wrapper();
    return nil;
}

- (id<_WKWebExtensionTab>)associatedTab
{
    if (auto *tab = _webExtensionAction->tab())
        return tab->delegate();
    return nil;
}

- (CocoaImage *)iconForSize:(CGSize)size
{
    return _webExtensionAction->icon(size);
}

- (NSString *)label
{
    return _webExtensionAction->label();
}

- (NSString *)badgeText
{
    return _webExtensionAction->badgeText();
}

- (BOOL)isEnabled
{
    return _webExtensionAction->isEnabled();
}

- (BOOL)presentsPopup
{
    return _webExtensionAction->presentsPopup();
}

- (WKWebView *)popupWebView
{
    return _webExtensionAction->popupWebView();
}

- (void)closePopupWebView
{
    _webExtensionAction->closePopupWebView();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_webExtensionAction;
}

- (WebKit::WebExtensionAction&)_webExtensionAction
{
    return *_webExtensionAction;
}

#else // ENABLE(WK_WEB_EXTENSIONS)

- (_WKWebExtensionContext *)webExtensionContext
{
    return nil;
}

- (id<_WKWebExtensionTab>)associatedTab
{
    return nil;
}

- (CocoaImage *)iconForSize:(CGSize)size
{
    return nil;
}

- (NSString *)label
{
    return nil;
}

- (NSString *)badgeText
{
    return nil;
}

- (BOOL)isEnabled
{
    return NO;
}

- (BOOL)presentsPopup
{
    return NO;
}

- (WKWebView *)popupWebView
{
    return nil;
}

- (void)closePopupWebView
{
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
