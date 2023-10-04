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
#import "TestWebExtensionsDelegate.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WebExtensionUtilities.h"
#import <WebKit/_WKWebExtensionController.h>
#import <WebKit/_WKWebExtensionMatchPattern.h>
#import <WebKit/_WKWebExtensionPermission.h>

@implementation TestWebExtensionsDelegate

- (NSArray<id<_WKWebExtensionWindow>> *)webExtensionController:(_WKWebExtensionController *)controller openWindowsForExtensionContext:(_WKWebExtensionContext *)extensionContext
{
    if (_openWindows)
        return _openWindows(extensionContext);

    return @[ ];
}

- (id<_WKWebExtensionWindow>)webExtensionController:(_WKWebExtensionController *)controller focusedWindowForExtensionContext:(_WKWebExtensionContext *)extensionContext
{
    if (_focusedWindow)
        return _focusedWindow(extensionContext);

    return nil;
}

- (void)webExtensionController:(_WKWebExtensionController *)controller openNewWindowWithOptions:(_WKWebExtensionWindowCreationOptions *)options forExtensionContext:(_WKWebExtensionContext *)extensionContext completionHandler:(void (^)(id<_WKWebExtensionWindow> newWindow, NSError *error))completionHandler
{
    if (_openNewWindow)
        return _openNewWindow(options, extensionContext, completionHandler);

    completionHandler(nil, nil);
}

- (void)webExtensionController:(_WKWebExtensionController *)controller openNewTabWithOptions:(_WKWebExtensionTabCreationOptions *)options forExtensionContext:(_WKWebExtensionContext *)extensionContext completionHandler:(void (^)(id<_WKWebExtensionTab> newTab, NSError *error))completionHandler
{
    if (_openNewTab)
        return _openNewTab(options, extensionContext, completionHandler);

    auto *openWindows = [self webExtensionController:controller openWindowsForExtensionContext:extensionContext];
    if (!openWindows.count) {
        completionHandler(nil, nil);
        return;
    }

    TestWebExtensionWindow *window = openWindows.firstObject;
    auto newTab = adoptNS([[TestWebExtensionTab alloc] initWithWindow:window extensionController:controller]);

    window.tabs = [window.tabs arrayByAddingObject:newTab.get()];

    [controller didOpenTab:newTab.get()];

    completionHandler(newTab.get(), nil);
}

- (void)webExtensionController:(_WKWebExtensionController *)controller promptForPermissions:(NSSet<_WKWebExtensionPermission> *)permissions inTab:(id<_WKWebExtensionTab>)tab forExtensionContext:(_WKWebExtensionContext *)extensionContext completionHandler:(void (^)(NSSet<_WKWebExtensionPermission> *allowedPermissions))completionHandler
{
    if (_promptForPermissions)
        _promptForPermissions(tab, permissions, completionHandler);
    else
        completionHandler(permissions);
}

- (void)webExtensionController:(_WKWebExtensionController *)controller promptForPermissionMatchPatterns:(NSSet<_WKWebExtensionMatchPattern *> *)matchPatterns inTab:(id<_WKWebExtensionTab>)tab forExtensionContext:(_WKWebExtensionContext *)extensionContext completionHandler:(void (^)(NSSet<_WKWebExtensionMatchPattern *> *allowedMatchPatterns))completionHandler
{
    if (_promptForPermissionMatchPatterns)
        _promptForPermissionMatchPatterns(tab, matchPatterns, completionHandler);
    else
        completionHandler(matchPatterns);
}

- (void)webExtensionController:(_WKWebExtensionController *)controller sendMessage:(id)message toApplicationIdentifier:(NSString *)applicationIdentifier forExtensionContext:(_WKWebExtensionContext *)extensionContext replyHandler:(void (^)(id, NSError *))replyHandler
{
    if (_sendMessage)
        _sendMessage(message, applicationIdentifier, replyHandler);
    else
        replyHandler(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:0 userInfo:@{ NSDebugDescriptionErrorKey: @"runtime.sendNativeMessage() not implemneted" }]);
}

- (void)webExtensionController:(_WKWebExtensionController *)controller connectUsingMessagePort:(_WKWebExtensionMessagePort *)port forExtensionContext:(_WKWebExtensionContext *)extensionContext completionHandler:(void (^)(NSError *error))completionHandler
{
    if (_connectUsingMessagePort) {
        _connectUsingMessagePort(port);
        completionHandler(nil);
    } else
        completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:0 userInfo:@{ NSDebugDescriptionErrorKey: @"runtime.connectNative() not implemneted" }]);
}

- (void)webExtensionController:(_WKWebExtensionController *)controller presentActionPopup:(_WKWebExtensionAction *)action forExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_presentActionPopup) {
        _presentActionPopup(action);
        completionHandler(nil);
    } else
        completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:0 userInfo:@{ NSDebugDescriptionErrorKey: @"action.showPopup() not implemneted" }]);
}

@end

#endif // ENABLE(WK_WEB_EXTENSIONS)
