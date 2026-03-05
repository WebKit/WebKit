/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#import "WKWebExtensionActionInternal.h"

#import "CocoaHelpers.h"
#import "CocoaImage.h"
#import "WebExtensionAction.h"
#import "WebExtensionContext.h"
#import "WebExtensionTab.h"
#import <wtf/BlockPtr.h>
#import <wtf/CompletionHandler.h>

#if USE(APPKIT)
using CocoaMenuItem = NSMenuItem;
#else
using CocoaMenuItem = UIMenuElement;
#endif

@implementation WKWebExtensionAction

#if ENABLE(WK_WEB_EXTENSIONS)

WK_OBJECT_DEALLOC_IMPL_ON_MAIN_THREAD(WKWebExtensionAction, WebExtensionAction, _webExtensionAction);

- (BOOL)isEqual:(id)object
{
    if (self == object)
        return YES;

    auto *other = dynamic_objc_cast<WKWebExtensionAction>(object);
    if (!other)
        return NO;

    return *_webExtensionAction == *other->_webExtensionAction;
}

- (WKWebExtensionContext *)webExtensionContext
{
    if (RefPtr context = protect(*_webExtensionAction)->extensionContext())
        return context->wrapper();
    return nil;
}

- (id<WKWebExtensionTab>)associatedTab
{
    if (RefPtr tab = protect(*_webExtensionAction)->tab())
        return tab->delegate();
    return nil;
}

- (CocoaImage *)iconForSize:(CGSize)size
{
    return WebKit::toCocoaImage(protect(*_webExtensionAction)->icon(WebCore::FloatSize(size)));
}

- (NSString *)label
{
    return protect(*_webExtensionAction)->label().createNSString().autorelease();
}

- (NSString *)badgeText
{
    return protect(*_webExtensionAction)->badgeText().createNSString().autorelease();
}

- (BOOL)hasUnreadBadgeText
{
    return protect(*_webExtensionAction)->hasUnreadBadgeText();
}

- (void)setHasUnreadBadgeText:(BOOL)hasUnreadBadgeText
{
    return protect(*_webExtensionAction)->setHasUnreadBadgeText(hasUnreadBadgeText);
}

- (NSString *)inspectionName
{
    return protect(*_webExtensionAction)->popupWebViewInspectionName();
}

- (void)setInspectionName:(NSString *)name
{
    protect(*_webExtensionAction)->setPopupWebViewInspectionName(name);
}

- (BOOL)isEnabled
{
    return protect(*_webExtensionAction)->isEnabled();
}

- (NSArray<CocoaMenuItem *> *)menuItems
{
    return protect(*_webExtensionAction)->platformMenuItems();
}

- (BOOL)presentsPopup
{
    return protect(*_webExtensionAction)->presentsPopup();
}

#if PLATFORM(IOS_FAMILY)
- (UIViewController *)popupViewController
{
    return protect(*_webExtensionAction)->popupViewController();
}
#endif

#if PLATFORM(MAC)
- (NSPopover *)popupPopover
{
    return protect(*_webExtensionAction)->popupPopover();
}
#endif

- (WKWebView *)popupWebView
{
    return protect(*_webExtensionAction)->popupWebView();
}

- (void)closePopup
{
    protect(*_webExtensionAction)->closePopup();
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

- (WKWebExtensionContext *)webExtensionContext
{
    return nil;
}

- (id<WKWebExtensionTab>)associatedTab
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

- (BOOL)hasUnreadBadgeText
{
    return NO;
}

- (void)setHasUnreadBadgeText:(BOOL)hasUnreadBadgeText
{
}

- (NSString *)inspectionName
{
    return nil;
}

- (void)setInspectionName:(NSString *)name
{
}

- (BOOL)isEnabled
{
    return NO;
}

- (NSArray<CocoaMenuItem *> *)menuItems
{
    return nil;
}

- (BOOL)presentsPopup
{
    return NO;
}

#if PLATFORM(IOS_FAMILY)
- (UIViewController *)popupViewController
{
    return nil;
}
#endif

#if PLATFORM(MAC)
- (NSPopover *)popupPopover
{
    return nil;
}
#endif

- (WKWebView *)popupWebView
{
    return nil;
}

- (void)closePopup
{
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
