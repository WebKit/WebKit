/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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

#include "APIInspectorConfiguration.h"
#include "WKPage.h"
#include "WebEvent.h"
#include "WebHitTestResultData.h"
#include "WebPageProxy.h"
#include <WebCore/CookieConsentDecisionResult.h>
#include <WebCore/FloatRect.h>
#include <WebCore/ModalContainerTypes.h>
#include <WebCore/PermissionState.h>
#include <WebCore/ScreenOrientationType.h>
#include <wtf/CompletionHandler.h>

#if PLATFORM(COCOA)
#include <WebCore/PlatformViewController.h>
#endif

#if PLATFORM(IOS_FAMILY)
OBJC_CLASS NSArray;
OBJC_CLASS _WKActivatedElementInfo;
OBJC_CLASS UIViewController;
#endif

#if ENABLE(WEB_AUTHN)
#include "WebAuthenticationFlags.h"
#endif

#if ENABLE(WEBXR) && PLATFORM(COCOA)
#include <WebCore/PlatformXR.h>
#endif

namespace WebCore {
class RegistrableDomain;
class ResourceRequest;
struct FontAttributes;
struct SecurityOriginData;
struct WindowFeatures;
}

namespace WebKit {
enum class TapHandlingResult : uint8_t;
class NativeWebKeyboardEvent;
class NativeWebWheelEvent;
class UserMediaPermissionRequestProxy;
class WebColorPickerResultListenerProxy;
class WebFrameProxy;
class WebOpenPanelResultListenerProxy;
class WebPageProxy;
struct NavigationActionData;
}

namespace API {

class Data;
class Dictionary;
class Object;
class OpenPanelParameters;
class SecurityOrigin;
#if ENABLE(WEB_AUTHN)
class WebAuthenticationPanel;
#endif

class UIClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~UIClient() { }

    virtual void createNewPage(WebKit::WebPageProxy&, WebCore::WindowFeatures&&, Ref<API::NavigationAction>&&, CompletionHandler<void(RefPtr<WebKit::WebPageProxy>&&)>&& completionHandler) { completionHandler(nullptr); }
    virtual void showPage(WebKit::WebPageProxy*) { }
    virtual void fullscreenMayReturnToInline(WebKit::WebPageProxy*) { }
    virtual void didEnterFullscreen(WebKit::WebPageProxy*) { }
    virtual void didExitFullscreen(WebKit::WebPageProxy*) { }
    virtual void hasVideoInPictureInPictureDidChange(WebKit::WebPageProxy*, bool) { }
    virtual void close(WebKit::WebPageProxy*) { }

    virtual bool takeFocus(WebKit::WebPageProxy*, WKFocusDirection) { return false; }
    virtual void focus(WebKit::WebPageProxy*) { }
    virtual void unfocus(WebKit::WebPageProxy*) { }
    virtual bool focusFromServiceWorker(WebKit::WebPageProxy&) { return false; }

    virtual void runJavaScriptAlert(WebKit::WebPageProxy&, const WTF::String&, WebKit::WebFrameProxy*, WebKit::FrameInfoData&&, Function<void()>&& completionHandler) { completionHandler(); }
    virtual void runJavaScriptConfirm(WebKit::WebPageProxy&, const WTF::String&, WebKit::WebFrameProxy*, WebKit::FrameInfoData&&, Function<void(bool)>&& completionHandler) { completionHandler(false); }
    virtual void runJavaScriptPrompt(WebKit::WebPageProxy&, const WTF::String&, const WTF::String&, WebKit::WebFrameProxy*, WebKit::FrameInfoData&&, Function<void(const WTF::String&)>&& completionHandler) { completionHandler(WTF::String()); }

    virtual void setStatusText(WebKit::WebPageProxy*, const WTF::String&) { }
    virtual void mouseDidMoveOverElement(WebKit::WebPageProxy&, const WebKit::WebHitTestResultData&, OptionSet<WebKit::WebEventModifier>, Object*) { }

    virtual void didNotHandleKeyEvent(WebKit::WebPageProxy*, const WebKit::NativeWebKeyboardEvent&) { }
    virtual void didNotHandleWheelEvent(WebKit::WebPageProxy*, const WebKit::NativeWebWheelEvent&) { }

    virtual void toolbarsAreVisible(WebKit::WebPageProxy&, Function<void(bool)>&& completionHandler) { completionHandler(true); }
    virtual void setToolbarsAreVisible(WebKit::WebPageProxy&, bool) { }
    virtual void menuBarIsVisible(WebKit::WebPageProxy&, Function<void(bool)>&& completionHandler) { completionHandler(true); }
    virtual void setMenuBarIsVisible(WebKit::WebPageProxy&, bool) { }
    virtual void statusBarIsVisible(WebKit::WebPageProxy&, Function<void(bool)>&& completionHandler) { completionHandler(true); }
    virtual void setStatusBarIsVisible(WebKit::WebPageProxy&, bool) { }
    virtual void setIsResizable(WebKit::WebPageProxy&, bool) { }

    virtual void setWindowFrame(WebKit::WebPageProxy&, const WebCore::FloatRect&) { }
    virtual void windowFrame(WebKit::WebPageProxy&, Function<void(WebCore::FloatRect)>&& completionHandler) { completionHandler({ }); }

    virtual bool canRunBeforeUnloadConfirmPanel() const { return false; }
    virtual void runBeforeUnloadConfirmPanel(WebKit::WebPageProxy&, const WTF::String&, WebKit::WebFrameProxy*, WebKit::FrameInfoData&&, Function<void(bool)>&& completionHandler) { completionHandler(true); }

    virtual void pageDidScroll(WebKit::WebPageProxy*) { }

    virtual void exceededDatabaseQuota(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, SecurityOrigin*, const WTF::String&, const WTF::String&, unsigned long long currentQuota, unsigned long long, unsigned long long, unsigned long long, Function<void (unsigned long long)>&& completionHandler)
    {
        completionHandler(currentQuota);
    }

    virtual void reachedApplicationCacheOriginQuota(WebKit::WebPageProxy*, const WebCore::SecurityOrigin&, uint64_t currentQuota, uint64_t, Function<void (unsigned long long)>&& completionHandler)
    {
        completionHandler(currentQuota);
    }

    virtual bool lockScreenOrientation(WebKit::WebPageProxy&, WebCore::ScreenOrientationType) { return false; }
    virtual void unlockScreenOrientation(WebKit::WebPageProxy&) { }

    virtual bool needsFontAttributes() const { return false; }
    virtual void didChangeFontAttributes(const WebCore::FontAttributes&) { }

    virtual bool runOpenPanel(WebKit::WebPageProxy&, WebKit::WebFrameProxy*, WebKit::FrameInfoData&&, OpenPanelParameters*, WebKit::WebOpenPanelResultListenerProxy*) { return false; }
    virtual void decidePolicyForGeolocationPermissionRequest(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, const WebKit::FrameInfoData&, Function<void(bool)>&) { }
    virtual void decidePolicyForUserMediaPermissionRequest(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, SecurityOrigin&, SecurityOrigin&, WebKit::UserMediaPermissionRequestProxy& request) { request.doDefaultAction(); }
    virtual void checkUserMediaPermissionForOrigin(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, SecurityOrigin&, SecurityOrigin&, WebKit::UserMediaPermissionCheckProxy& request) { request.deny(); }
    virtual void decidePolicyForNotificationPermissionRequest(WebKit::WebPageProxy&, SecurityOrigin&, CompletionHandler<void(bool allowed)>&& completionHandler) { completionHandler(false); }
    virtual void requestStorageAccessConfirm(WebKit::WebPageProxy&, WebKit::WebFrameProxy*, const WebCore::RegistrableDomain& requestingDomain, const WebCore::RegistrableDomain& currentDomain, CompletionHandler<void(bool)>&& completionHandler) { completionHandler(true); }
    virtual void requestCookieConsent(CompletionHandler<void(WebCore::CookieConsentDecisionResult)>&& completionHandler) { completionHandler(WebCore::CookieConsentDecisionResult::NotSupported); }
    virtual void decidePolicyForModalContainer(OptionSet<WebCore::ModalContainerControlType>, CompletionHandler<void(WebCore::ModalContainerDecision)>&& completion) { completion(WebCore::ModalContainerDecision::Show); }

    // Printing.
    virtual float headerHeight(WebKit::WebPageProxy&, WebKit::WebFrameProxy&) { return 0; }
    virtual float footerHeight(WebKit::WebPageProxy&, WebKit::WebFrameProxy&) { return 0; }
    virtual void drawHeader(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, WebCore::FloatRect&&) { }
    virtual void drawFooter(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, WebCore::FloatRect&&) { }
    virtual void printFrame(WebKit::WebPageProxy&, WebKit::WebFrameProxy&, const WebCore::FloatSize& pdfFirstPageSize, CompletionHandler<void()>&& completionHandler) { completionHandler(); }

    virtual bool canRunModal() const { return false; }
    virtual void runModal(WebKit::WebPageProxy&) { }

    virtual void saveDataToFileInDownloadsFolder(WebKit::WebPageProxy*, const WTF::String&, const WTF::String&, const WTF::URL&, Data&) { }

    virtual void pinnedStateDidChange(WebKit::WebPageProxy&) { }

    virtual void isPlayingMediaDidChange(WebKit::WebPageProxy&) { }
    virtual void mediaCaptureStateDidChange(WebCore::MediaProducerMediaStateFlags) { }
    virtual void handleAutoplayEvent(WebKit::WebPageProxy&, WebCore::AutoplayEvent, OptionSet<WebCore::AutoplayEventFlags>) { }

#if PLATFORM(IOS_FAMILY)
#if HAVE(APP_LINKS)
    virtual bool shouldIncludeAppLinkActionsForElement(_WKActivatedElementInfo *) { return true; }
#endif
    virtual RetainPtr<NSArray> actionsForElement(_WKActivatedElementInfo *, RetainPtr<NSArray> defaultActions) { return defaultActions; }
    virtual void didNotHandleTapAsClick(const WebCore::IntPoint&) { }
    virtual void statusBarWasTapped() { }
#endif
#if PLATFORM(COCOA)
    virtual PlatformViewController *presentingViewController() { return nullptr; }
    virtual NSDictionary *dataDetectionContext() { return nullptr; }
#endif

#if ENABLE(POINTER_LOCK)
    virtual void requestPointerLock(WebKit::WebPageProxy*) { }
    virtual void didLosePointerLock(WebKit::WebPageProxy*) { }
#endif

#if ENABLE(DEVICE_ORIENTATION)
    virtual void shouldAllowDeviceOrientationAndMotionAccess(WebKit::WebPageProxy&, WebKit::WebFrameProxy& webFrameProxy, WebKit::FrameInfoData&&, CompletionHandler<void(bool)>&& completionHandler) { completionHandler(false); }
#endif

    virtual void didClickAutoFillButton(WebKit::WebPageProxy&, Object*) { }

    virtual void didResignInputElementStrongPasswordAppearance(WebKit::WebPageProxy&, Object*) { }

    virtual void imageOrMediaDocumentSizeChanged(const WebCore::IntSize&) { }

    virtual void didShowSafeBrowsingWarning() { }

    virtual void confirmPDFOpening(WebKit::WebPageProxy&, const WTF::URL&, WebKit::FrameInfoData&&, CompletionHandler<void(bool)>&& completionHandler) { completionHandler(true); }

#if ENABLE(WEB_AUTHN)
    virtual void runWebAuthenticationPanel(WebKit::WebPageProxy&, WebAuthenticationPanel&, WebKit::WebFrameProxy&, WebKit::FrameInfoData&&, CompletionHandler<void(WebKit::WebAuthenticationPanelResult)>&& completionHandler) { completionHandler(WebKit::WebAuthenticationPanelResult::Unavailable); }

    virtual void requestWebAuthenticationNoGesture(API::SecurityOrigin& origin, CompletionHandler<void(bool)>&& completionHandler)
    {
        completionHandler(true);
    }
#endif

    virtual void didAttachLocalInspector(WebKit::WebPageProxy&, WebKit::WebInspectorUIProxy&) { }
    virtual void willCloseLocalInspector(WebKit::WebPageProxy&, WebKit::WebInspectorUIProxy&) { }
    virtual Ref<API::InspectorConfiguration> configurationForLocalInspector(WebKit::WebPageProxy&, WebKit::WebInspectorUIProxy&)
    {
        return API::InspectorConfiguration::create();
    }
    virtual void didEnableInspectorBrowserDomain(WebKit::WebPageProxy&) { }
    virtual void didDisableInspectorBrowserDomain(WebKit::WebPageProxy&) { }

    virtual void decidePolicyForSpeechRecognitionPermissionRequest(WebKit::WebPageProxy& page, API::SecurityOrigin& origin, CompletionHandler<void(bool)>&& completionHandler) { page.requestSpeechRecognitionPermissionByDefaultAction(origin.securityOrigin(), WTFMove(completionHandler)); }

    virtual void decidePolicyForMediaKeySystemPermissionRequest(WebKit::WebPageProxy& page, API::SecurityOrigin& origin, const WTF::String& keySystem, CompletionHandler<void(bool)>&& completionHandler) { page.requestMediaKeySystemPermissionByDefaultAction(origin.securityOrigin(), WTFMove(completionHandler)); }

    virtual void queryPermission(const WTF::String& permissionName, API::SecurityOrigin& origin, CompletionHandler<void(std::optional<WebCore::PermissionState>)>&& completionHandler) { completionHandler({ }); }

#if ENABLE(WEBXR) && PLATFORM(COCOA)
    virtual void requestPermissionOnXRSessionFeatures(WebKit::WebPageProxy&, const WebCore::SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& /* consentRequired */, const PlatformXR::Device::FeatureList& /* consentOptional */, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler) { completionHandler(granted); }    
    virtual void startXRSession(WebKit::WebPageProxy&, CompletionHandler<void(RetainPtr<id>)>&& completionHandler) { completionHandler(nil); }
    virtual void endXRSession(WebKit::WebPageProxy&) { }
#endif
};

} // namespace API
