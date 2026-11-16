/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
#import "WKWebExtensionPermissionPrivate.h"

WKWebExtensionPermission const WKWebExtensionPermissionActiveTab = @"activeTab";
WKWebExtensionPermission const WKWebExtensionPermissionAlarms = @"alarms";
WKWebExtensionPermission const WKWebExtensionPermissionBookmarks = @"bookmarks";
WKWebExtensionPermission const WKWebExtensionPermissionClipboardWrite = @"clipboardWrite";
WKWebExtensionPermission const WKWebExtensionPermissionContextMenus = @"contextMenus";
WKWebExtensionPermission const WKWebExtensionPermissionCookies = @"cookies";
WKWebExtensionPermission const WKWebExtensionPermissionDeclarativeNetRequest = @"declarativeNetRequest";
WKWebExtensionPermission const WKWebExtensionPermissionDeclarativeNetRequestFeedback = @"declarativeNetRequestFeedback";
WKWebExtensionPermission const WKWebExtensionPermissionDeclarativeNetRequestWithHostAccess = @"declarativeNetRequestWithHostAccess";
WKWebExtensionPermission const WKWebExtensionPermissionMenus = @"menus";
WKWebExtensionPermission const WKWebExtensionPermissionNativeMessaging = @"nativeMessaging";
WKWebExtensionPermission const WKWebExtensionPermissionNotifications = @"notifications";
WKWebExtensionPermission const WKWebExtensionPermissionScripting = @"scripting";
WKWebExtensionPermission const WKWebExtensionPermissionSidePanel = @"sidePanel";
WKWebExtensionPermission const WKWebExtensionPermissionStorage = @"storage";
WKWebExtensionPermission const WKWebExtensionPermissionTabs = @"tabs";
WKWebExtensionPermission const WKWebExtensionPermissionUnlimitedStorage = @"unlimitedStorage";
WKWebExtensionPermission const WKWebExtensionPermissionWebNavigation = @"webNavigation";
WKWebExtensionPermission const WKWebExtensionPermissionWebRequest = @"webRequest";
