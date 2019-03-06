/*
 * Copyright (C) 2015 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#if !defined(__WEBKIT2_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <webkit2/webkit2.h> can be included directly."
#endif

#ifndef WebKitAutocleanups_h
#define WebKitAutocleanups_h

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
#ifndef __GI_SCANNER__

G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitAuthenticationRequest, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitAutomationSession, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitBackForwardList, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitBackForwardListItem, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitColorChooserRequest, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitContextMenu, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitContextMenuItem, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitCookieManager, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitDownload, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitEditorState, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitFaviconDatabase, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitFileChooserRequest, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitFindController, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitFormSubmissionRequest, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitGeolocationPermissionRequest, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitHitTestResult, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitInstallMissingMediaPluginsPermissionRequest, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitNavigationPolicyDecision, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitNotification, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitNotificationPermissionRequest, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitOptionMenu, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitPermissionRequest, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitPlugin, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitPolicyDecision, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitPrintCustomWidget, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitPrintOperation, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitResponsePolicyDecision, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitSecurityManager, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitSettings, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitURIRequest, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitURIResponse, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitURISchemeRequest, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitUserContentFilterStore, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitUserContentManager, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitUserMediaPermissionRequest, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWebContext, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWebInspector, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWebResource, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWebView, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWebsiteDataManager, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWindowProperties, g_object_unref)

G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitApplicationInfo, webkit_application_info_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitCredential, webkit_credential_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitJavascriptResult, webkit_javascript_result_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitMimeInfo, webkit_mime_info_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitNavigationAction, webkit_navigation_action_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitNetworkProxySettings, webkit_network_proxy_settings_free)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitSecurityOrigin, webkit_security_origin_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitUserScript, webkit_user_script_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitUserStyleSheet, webkit_user_style_sheet_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitUserContentFilter, webkit_user_content_filter_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWebsiteData, webkit_website_data_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWebViewSessionState, webkit_web_view_session_state_unref)

#endif // __GI_SCANNER__
#endif // G_DEFINE_AUTOPTR_CLEANUP_FUNC

#endif // WebKitAutocleanups_h
