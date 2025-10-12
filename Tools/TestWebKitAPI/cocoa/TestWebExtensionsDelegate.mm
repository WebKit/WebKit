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

#import "config.h"
#import "TestWebExtensionsDelegate.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WebExtensionUtilities.h"
#import <WebKit/WKWebExtensionController.h>
#import <WebKit/WKWebExtensionMatchPattern.h>
#import <WebKit/WKWebExtensionPermission.h>

@implementation TestWebExtensionsDelegate

- (NSArray<id<WKWebExtensionWindow>> *)webExtensionController:(WKWebExtensionController *)controller openWindowsForExtensionContext:(WKWebExtensionContext *)extensionContext
{
    if (_openWindows)
        return _openWindows(extensionContext);

    return @[ ];
}

- (id<WKWebExtensionWindow>)webExtensionController:(WKWebExtensionController *)controller focusedWindowForExtensionContext:(WKWebExtensionContext *)extensionContext
{
    if (_focusedWindow)
        return _focusedWindow(extensionContext);

    return nil;
}

#if PLATFORM(MAC)
- (void)webExtensionController:(WKWebExtensionController *)controller openNewWindowUsingConfiguration:(WKWebExtensionWindowConfiguration *)options forExtensionContext:(WKWebExtensionContext *)extensionContext completionHandler:(void (^)(id<WKWebExtensionWindow> newWindow, NSError *error))completionHandler
{
    if (_openNewWindow)
        return _openNewWindow(options, extensionContext, completionHandler);

    completionHandler(nil, nil);
}
#endif

- (void)webExtensionController:(WKWebExtensionController *)controller openNewTabUsingConfiguration:(WKWebExtensionTabConfiguration *)options forExtensionContext:(WKWebExtensionContext *)extensionContext completionHandler:(void (^)(id<WKWebExtensionTab> newTab, NSError *error))completionHandler
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

- (void)webExtensionController:(WKWebExtensionController *)controller openOptionsPageForExtensionContext:(WKWebExtensionContext *)extensionContext completionHandler:(void (^)(NSError *))completionHandler
{
    if (_openOptionsPage)
        _openOptionsPage(extensionContext, completionHandler);
    else
        completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:0 userInfo:@{ NSDebugDescriptionErrorKey: @"runtime.openOptionsPage() not implemented" }]);
}

- (void)webExtensionController:(WKWebExtensionController *)controller promptForPermissions:(NSSet<WKWebExtensionPermission> *)permissions inTab:(id<WKWebExtensionTab>)tab forExtensionContext:(WKWebExtensionContext *)extensionContext completionHandler:(void (^)(NSSet<WKWebExtensionPermission> *allowedPermissions, NSDate *expirationDate))completionHandler
{
    if (_promptForPermissions)
        _promptForPermissions(tab, permissions, completionHandler);
    else
        completionHandler(permissions, nil);
}

- (void)webExtensionController:(WKWebExtensionController *)controller promptForPermissionMatchPatterns:(NSSet<WKWebExtensionMatchPattern *> *)matchPatterns inTab:(id<WKWebExtensionTab>)tab forExtensionContext:(WKWebExtensionContext *)extensionContext completionHandler:(void (^)(NSSet<WKWebExtensionMatchPattern *> *allowedMatchPatterns, NSDate *expirationDate))completionHandler
{
    if (_promptForPermissionMatchPatterns)
        _promptForPermissionMatchPatterns(tab, matchPatterns, completionHandler);
    else
        completionHandler(matchPatterns, nil);
}

- (void)webExtensionController:(WKWebExtensionController *)controller promptForPermissionToAccessURLs:(NSSet<NSURL *> *)urls inTab:(id<WKWebExtensionTab>)tab forExtensionContext:(WKWebExtensionContext *)extensionContext completionHandler:(void (^)(NSSet<NSURL *> *allowedURLs, NSDate *expirationDate))completionHandler
{
    if (_promptForPermissionToAccessURLs)
        _promptForPermissionToAccessURLs(tab, urls, completionHandler);
    else
        completionHandler(urls, nil);
}

- (void)webExtensionController:(WKWebExtensionController *)controller sendMessage:(id)message toApplicationWithIdentifier:(NSString *)applicationIdentifier forExtensionContext:(WKWebExtensionContext *)extensionContext replyHandler:(void (^)(id, NSError *))replyHandler
{
    if (_sendMessage)
        _sendMessage(message, applicationIdentifier, replyHandler);
    else
        replyHandler(nil, [NSError errorWithDomain:NSCocoaErrorDomain code:0 userInfo:@{ NSDebugDescriptionErrorKey: @"runtime.sendNativeMessage() not implemented" }]);
}

- (void)webExtensionController:(WKWebExtensionController *)controller connectUsingMessagePort:(WKWebExtensionMessagePort *)port forExtensionContext:(WKWebExtensionContext *)extensionContext completionHandler:(void (^)(NSError *error))completionHandler
{
    if (_connectUsingMessagePort) {
        _connectUsingMessagePort(port);
        completionHandler(nil);
    } else
        completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:0 userInfo:@{ NSDebugDescriptionErrorKey: @"runtime.connectNative() not implemented" }]);
}

- (void)webExtensionController:(WKWebExtensionController *)controller didUpdateAction:(WKWebExtensionAction *)action forExtensionContext:(WKWebExtensionContext *)context
{
    if (_didUpdateAction)
        _didUpdateAction(action);
}

- (void)webExtensionController:(WKWebExtensionController *)controller presentPopupForAction:(WKWebExtensionAction *)action forExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_presentPopupForAction) {
        _presentPopupForAction(action);
        completionHandler(nil);
    } else
        completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:0 userInfo:@{ NSDebugDescriptionErrorKey: @"action.showPopup() not implemented" }]);
}

- (void)_webExtensionController:(WKWebExtensionController *)controller presentSidebar:(_WKWebExtensionSidebar *)sidebar forExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_presentSidebar) {
        _presentSidebar(sidebar);
        completionHandler(nil);
    } else
        completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:0 userInfo:@{ NSDebugDescriptionErrorKey: @"sidebarAction.open() / sidePanel.open() not implemented" }]);
}

- (void)_webExtensionController:(WKWebExtensionController *)controller closeSidebar:(_WKWebExtensionSidebar *)sidebar forExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_closeSidebar) {
        _closeSidebar(sidebar);
        completionHandler(nil);
    } else
        completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:0 userInfo:@{ NSDebugDescriptionErrorKey: @"sidebarAction.close() not implemented" }]);
}

- (void)_webExtensionController:(WKWebExtensionController *)controller didUpdateSidebar:(_WKWebExtensionSidebar *)sidebar forExtensionContext:(WKWebExtensionContext *)context
{
    if (_didUpdateSidebar)
        _didUpdateSidebar(sidebar);
}


- (void)_webExtensionController:(WKWebExtensionController *)controller createBookmarkWithParentIdentifier:(NSString *)parentId index:(NSNumber *)index url:(NSString *)url title:(NSString *)title forExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSObject<_WKWebExtensionBookmark> *, NSError *))completionHandler
{
    if (_createBookmarkWithParentIdentifier)
        _createBookmarkWithParentIdentifier(parentId, index, url, title, completionHandler);
    else if (completionHandler)
        completionHandler(nil, nil);
}

- (void)_webExtensionController:(WKWebExtensionController *)controller bookmarksForExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSArray<NSObject<_WKWebExtensionBookmark> *> *, NSError *))completionHandler
{
    if (_bookmarksForExtensionContext)
        _bookmarksForExtensionContext(completionHandler);
    else if (completionHandler)
        completionHandler(@[], nil);
}

- (void)_webExtensionController:(WKWebExtensionController *)controller removeBookmarkWithIdentifier:(NSString *)bookmarkId removeFolderWithChildren:(BOOL)removeFolderWithChildren forExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_removeBookmarkWithIdentifier)
        _removeBookmarkWithIdentifier(bookmarkId, removeFolderWithChildren, completionHandler);
    else if (completionHandler)
        completionHandler(nil);
}

- (void)_webExtensionController:(WKWebExtensionController *)controller updateBookmarkWithIdentifier:(NSString *)bookmarkId title:(NSString *)title url:(NSString *)url forExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSObject<_WKWebExtensionBookmark> *, NSError *))completionHandler
{
    if (_updateBookmarkWithIdentifier)
        _updateBookmarkWithIdentifier(bookmarkId, title, url, completionHandler);
    else if (completionHandler)
        completionHandler(nil, nil);
}

- (void)_webExtensionController:(WKWebExtensionController *)controller moveBookmarkWithIdentifier:(NSString *)bookmarkId toParent:(NSString *)parentId atIndex:(NSNumber *)index forExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSObject<_WKWebExtensionBookmark> *, NSError *))completionHandler
{
    if (_moveBookmarkWithIdentifier)
        _moveBookmarkWithIdentifier(bookmarkId, parentId, index, completionHandler);
    else if (completionHandler)
        completionHandler(nil, nil);
}
@end

#endif // ENABLE(WK_WEB_EXTENSIONS)
