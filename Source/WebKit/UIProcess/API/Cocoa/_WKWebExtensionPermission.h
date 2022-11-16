/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import <WebKit/WKFoundation.h>

#import <Foundation/Foundation.h>

/*! @abstract Constants for specifying permission in a @link WKWebExtensionContext @/link. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
typedef NSString * _WKWebExtensionPermission NS_TYPED_EXTENSIBLE_ENUM NS_SWIFT_NAME(_WKWebExtension.Permission);

/*! @abstract The `activeTab` permission requests that when the user interacts with the extension, the extension is granted extra permissions for the active tab only. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionActiveTab;

/*! @abstract The `alarms` permission requests access to the `browser.alarms` APIs. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionAlarms;

/*! @abstract The `clipboardWrite` permission requests access to write to the clipboard. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionClipboardWrite;

/*! @abstract The `contextMenus` permission requests access to the `browser.contextMenus` APIs. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionContextMenus;

/*! @abstract The `cookies` permission requests access to the `browser.cookies` APIs. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionCookies;

/*! @abstract The `declarativeNetRequest` permission requests access to the `browser.declarativeNetRequest` APIs. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionDeclarativeNetRequest;

/*! @abstract The `declarativeNetRequestFeedback` permission requests access to the `browser.declarativeNetRequest` APIs with extra information on matched rules. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionDeclarativeNetRequestFeedback;

/*! @abstract The `declarativeNetRequestWithHostAccess` permission requests access to the `browser.declarativeNetRequest` APIs with the ability to modify or redirect requests. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionDeclarativeNetRequestWithHostAccess;

/*! @abstract The `menus` permission requests access to the `browser.menus` APIs. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionMenus;

/*! @abstract The `nativeMessaging` permission requests access to send messages to the App Extension bundle. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionNativeMessaging;

/*! @abstract The `scripting` permission requests access to the `browser.scripting` APIs. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionScripting;

/*! @abstract The `storage` permission requests access to the `browser.storage` APIs. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionStorage;

/*! @abstract The `tabs` permission requests access extra information on the `browser.tabs` APIs. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionTabs;

/*! @abstract The `unlimitedStorage` permission requests access to an unlimited quota on the `browser.storage.local` APIs. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionUnlimitedStorage;

/*! @abstract The `webNavigation` permission requests access to the `browser.webNavigation` APIs. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionWebNavigation;

/*! @abstract The `webRequest` permission requests access to the `browser.webRequest` APIs. */
WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
WK_EXTERN _WKWebExtensionPermission const _WKWebExtensionPermissionWebRequest;
