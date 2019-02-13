/*
 * Copyright (C) 2011, 2017 Igalia S.L.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
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

#pragma once

#include "APIPageConfiguration.h"
#include "InstallMissingMediaPluginsPermissionRequest.h"
#include "WebContextMenuItemData.h"
#include "WebEvent.h"
#include "WebHitTestResultData.h"
#include "WebImage.h"
#include "WebKitWebView.h"
#include "WebPageProxy.h"
#include <WebCore/IntRect.h>
#include <WebCore/LinkIcon.h>
#include <wtf/CompletionHandler.h>
#include <wtf/text/CString.h>

void webkitWebViewCreatePage(WebKitWebView*, Ref<API::PageConfiguration>&&);
WebKit::WebPageProxy& webkitWebViewGetPage(WebKitWebView*);
void webkitWebViewLoadChanged(WebKitWebView*, WebKitLoadEvent);
void webkitWebViewLoadFailed(WebKitWebView*, WebKitLoadEvent, const char* failingURI, GError*);
void webkitWebViewLoadFailedWithTLSErrors(WebKitWebView*, const char* failingURI, GError*, GTlsCertificateFlags, GTlsCertificate*);
#if PLATFORM(GTK)
void webkitWebViewGetLoadDecisionForIcon(WebKitWebView*, const WebCore::LinkIcon&, Function<void(bool)>&&);
void webkitWebViewSetIcon(WebKitWebView*, const WebCore::LinkIcon&, API::Data&);
#endif
WebKit::WebPageProxy* webkitWebViewCreateNewPage(WebKitWebView*, const WebCore::WindowFeatures&, WebKitNavigationAction*);
void webkitWebViewReadyToShowPage(WebKitWebView*);
void webkitWebViewRunAsModal(WebKitWebView*);
void webkitWebViewClosePage(WebKitWebView*);
void webkitWebViewRunJavaScriptAlert(WebKitWebView*, const CString& message, Function<void()>&& completionHandler);
void webkitWebViewRunJavaScriptConfirm(WebKitWebView*, const CString& message, Function<void(bool)>&& completionHandler);
void webkitWebViewRunJavaScriptPrompt(WebKitWebView*, const CString& message, const CString& defaultText, Function<void(const String&)>&& completionHandler);
void webkitWebViewRunJavaScriptBeforeUnloadConfirm(WebKitWebView*, const CString& message, Function<void(bool)>&& completionHandler);
bool webkitWebViewIsShowingScriptDialog(WebKitWebView*);
bool webkitWebViewIsScriptDialogRunning(WebKitWebView*, WebKitScriptDialog*);
String webkitWebViewGetCurrentScriptDialogMessage(WebKitWebView*);
void webkitWebViewSetCurrentScriptDialogUserInput(WebKitWebView*, const String&);
void webkitWebViewAcceptCurrentScriptDialog(WebKitWebView*);
void webkitWebViewDismissCurrentScriptDialog(WebKitWebView*);
Optional<WebKitScriptDialogType> webkitWebViewGetCurrentScriptDialogType(WebKitWebView*);
void webkitWebViewMakePermissionRequest(WebKitWebView*, WebKitPermissionRequest*);
void webkitWebViewMakePolicyDecision(WebKitWebView*, WebKitPolicyDecisionType, WebKitPolicyDecision*);
void webkitWebViewMouseTargetChanged(WebKitWebView*, const WebKit::WebHitTestResultData&, OptionSet<WebKit::WebEvent::Modifier>);
void webkitWebViewHandleDownloadRequest(WebKitWebView*, WebKit::DownloadProxy*);
void webkitWebViewPrintFrame(WebKitWebView*, WebKit::WebFrameProxy*);
void webkitWebViewResourceLoadStarted(WebKitWebView*, WebKit::WebFrameProxy*, uint64_t resourceIdentifier, WebKitURIRequest*);
void webkitWebViewRunFileChooserRequest(WebKitWebView*, WebKitFileChooserRequest*);
WebKitWebResource* webkitWebViewGetLoadingWebResource(WebKitWebView*, uint64_t resourceIdentifier);
#if PLATFORM(GTK)
void webKitWebViewDidReceiveSnapshot(WebKitWebView*, uint64_t callbackID, WebKit::WebImage*);
#endif
void webkitWebViewRemoveLoadingWebResource(WebKitWebView*, uint64_t resourceIdentifier);
void webkitWebViewMaximizeWindow(WebKitWebView*, CompletionHandler<void()>&&);
void webkitWebViewMinimizeWindow(WebKitWebView*, CompletionHandler<void()>&&);
void webkitWebViewRestoreWindow(WebKitWebView*, CompletionHandler<void()>&&);
void webkitWebViewEnterFullScreen(WebKitWebView*);
void webkitWebViewExitFullScreen(WebKitWebView*);
void webkitWebViewPopulateContextMenu(WebKitWebView*, const Vector<WebKit::WebContextMenuItemData>& proposedMenu, const WebKit::WebHitTestResultData&, GVariant*);
void webkitWebViewSubmitFormRequest(WebKitWebView*, WebKitFormSubmissionRequest*);
void webkitWebViewHandleAuthenticationChallenge(WebKitWebView*, WebKit::AuthenticationChallengeProxy*);
void webkitWebViewInsecureContentDetected(WebKitWebView*, WebKitInsecureContentEvent);
bool webkitWebViewEmitShowNotification(WebKitWebView*, WebKitNotification*);
void webkitWebViewWebProcessTerminated(WebKitWebView*, WebKitWebProcessTerminationReason);
void webkitWebViewIsPlayingAudioChanged(WebKitWebView*);
void webkitWebViewSelectionDidChange(WebKitWebView*);
void webkitWebViewRequestInstallMissingMediaPlugins(WebKitWebView*, WebKit::InstallMissingMediaPluginsPermissionRequest&);
WebKitWebsiteDataManager* webkitWebViewGetWebsiteDataManager(WebKitWebView*);

#if PLATFORM(GTK)
bool webkitWebViewEmitRunColorChooser(WebKitWebView*, WebKitColorChooserRequest*);
bool webkitWebViewShowOptionMenu(WebKitWebView*, const WebCore::IntRect&, WebKitOptionMenu*, const GdkEvent*);
#endif

gboolean webkitWebViewAuthenticate(WebKitWebView*, WebKitAuthenticationRequest*);
gboolean webkitWebViewScriptDialog(WebKitWebView*, WebKitScriptDialog*);
gboolean webkitWebViewRunFileChooser(WebKitWebView*, WebKitFileChooserRequest*);
