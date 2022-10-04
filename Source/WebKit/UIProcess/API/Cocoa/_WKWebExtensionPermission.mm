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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "_WKWebExtensionPermission.h"

_WKWebExtensionPermission const _WKWebExtensionPermissionActiveTab = @"activeTab";
_WKWebExtensionPermission const _WKWebExtensionPermissionAlarms = @"alarms";
_WKWebExtensionPermission const _WKWebExtensionPermissionClipboardWrite = @"clipboardWrite";
_WKWebExtensionPermission const _WKWebExtensionPermissionContextMenus = @"contextMenus";
_WKWebExtensionPermission const _WKWebExtensionPermissionCookies = @"cookies";
_WKWebExtensionPermission const _WKWebExtensionPermissionDeclarativeNetRequest = @"declarativeNetRequest";
_WKWebExtensionPermission const _WKWebExtensionPermissionDeclarativeNetRequestFeedback = @"declarativeNetRequestFeedback";
_WKWebExtensionPermission const _WKWebExtensionPermissionDeclarativeNetRequestWithHostAccess = @"declarativeNetRequestWithHostAccess";
_WKWebExtensionPermission const _WKWebExtensionPermissionMenus = @"menus";
_WKWebExtensionPermission const _WKWebExtensionPermissionNativeMessaging = @"nativeMessaging";
_WKWebExtensionPermission const _WKWebExtensionPermissionScripting = @"scripting";
_WKWebExtensionPermission const _WKWebExtensionPermissionStorage = @"storage";
_WKWebExtensionPermission const _WKWebExtensionPermissionTabs = @"tabs";
_WKWebExtensionPermission const _WKWebExtensionPermissionUnlimitedStorage = @"unlimitedStorage";
_WKWebExtensionPermission const _WKWebExtensionPermissionWebNavigation = @"webNavigation";
_WKWebExtensionPermission const _WKWebExtensionPermissionWebRequest = @"webRequest";
