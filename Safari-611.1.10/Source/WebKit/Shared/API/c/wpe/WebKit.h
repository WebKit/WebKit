/*
 * Copyright (C) 2015 Igalia S.L.
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

#ifndef WPE_WebKit_h
#define WPE_WebKit_h

#define BUILDING_WPE__

#include <WebKit/WKBase.h>
#include <WebKit/WKType.h>

// From Source/WebKit/Shared/API/c/
#include <WebKit/WKArray.h>
#include <WebKit/WKData.h>
#include <WebKit/WKDeclarationSpecifiers.h>
#include <WebKit/WKDictionary.h>
#include <WebKit/WKErrorRef.h>
#include <WebKit/WKGeometry.h>
#include <WebKit/WKMutableArray.h>
#include <WebKit/WKMutableDictionary.h>
#include <WebKit/WKNumber.h>
#include <WebKit/WKSecurityOriginRef.h>
#include <WebKit/WKString.h>
#include <WebKit/WKURL.h>
#include <WebKit/WKURLRequest.h>
#include <WebKit/WKURLResponse.h>
#include <WebKit/WKUserContentInjectedFrames.h>
#include <WebKit/WKUserScriptInjectionTime.h>

// From Source/WebKit/WebProcess/InjectedBundle/API/c/
#include <WebKit/WKBundle.h>
#include <WebKit/WKBundleBackForwardList.h>
#include <WebKit/WKBundleBackForwardListItem.h>
#include <WebKit/WKBundleFileHandleRef.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundleHitTestResult.h>
#include <WebKit/WKBundleInitialize.h>
#include <WebKit/WKBundleInspector.h>
#include <WebKit/WKBundleNavigationAction.h>
#include <WebKit/WKBundleNodeHandle.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundlePageBanner.h>
#include <WebKit/WKBundlePageContextMenuClient.h>
#include <WebKit/WKBundlePageEditorClient.h>
#include <WebKit/WKBundlePageFormClient.h>
#include <WebKit/WKBundlePageFullScreenClient.h>
#include <WebKit/WKBundlePageGroup.h>
#include <WebKit/WKBundlePageLoaderClient.h>
#include <WebKit/WKBundlePageOverlay.h>
#include <WebKit/WKBundlePagePolicyClient.h>
#include <WebKit/WKBundlePageResourceLoadClient.h>
#include <WebKit/WKBundlePageUIClient.h>
#include <WebKit/WKBundleRangeHandle.h>
#include <WebKit/WKBundleScriptWorld.h>

// From Source/WebKit/UIProcess/API/C
#include <WebKit/WKBackForwardListItemRef.h>
#include <WebKit/WKBackForwardListRef.h>
#include <WebKit/WKContext.h>
#include <WebKit/WKContextConfigurationRef.h>
#include <WebKit/WKCredential.h>
#include <WebKit/WKCredentialTypes.h>
#include <WebKit/WKFrame.h>
#include <WebKit/WKFrameInfoRef.h>
#include <WebKit/WKFramePolicyListener.h>
#include <WebKit/WKHitTestResult.h>
#include <WebKit/WKNavigationActionRef.h>
#include <WebKit/WKNavigationDataRef.h>
#include <WebKit/WKNavigationRef.h>
#include <WebKit/WKNavigationResponseRef.h>
#include <WebKit/WKPage.h>
#include <WebKit/WKPageConfigurationRef.h>
#include <WebKit/WKPageGroup.h>
#include <WebKit/WKPreferencesRef.h>
#include <WebKit/WKSessionStateRef.h>
#include <WebKit/WKUserContentControllerRef.h>
#include <WebKit/WKUserScriptRef.h>
#include <WebKit/WKView.h>
#include <WebKit/WKViewportAttributes.h>
#include <WebKit/WKWindowFeaturesRef.h>

#endif // WPE_WebKit_h
