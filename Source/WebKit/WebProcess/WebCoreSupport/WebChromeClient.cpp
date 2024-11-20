/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "config.h"
#include "WebChromeClient.h"

#include "APIArray.h"
#include "APIDictionary.h"
#include "APIInjectedBundleFormClient.h"
#include "APIInjectedBundlePageUIClient.h"
#include "APINumber.h"
#include "APIObject.h"
#include "APISecurityOrigin.h"
#include "APIString.h"
#include "DrawingArea.h"
#include "FindController.h"
#include "FrameInfoData.h"
#include "HangDetectionDisabler.h"
#include "ImageBufferShareableBitmapBackend.h"
#include "InjectedBundleNodeHandle.h"
#include "MessageSenderInlines.h"
#include "NavigationActionData.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "PageBanner.h"
#include "PluginView.h"
#include "RemoteBarcodeDetectorProxy.h"
#include "RemoteFaceDetectorProxy.h"
#include "RemoteGPUProxy.h"
#include "RemoteImageBufferProxy.h"
#include "RemoteRenderingBackendProxy.h"
#include "RemoteTextDetectorProxy.h"
#include "SharedBufferReference.h"
#include "UserData.h"
#include "WebColorChooser.h"
#include "WebCoreArgumentCoders.h"
#include "WebDataListSuggestionPicker.h"
#include "WebDateTimeChooser.h"
#include "WebFrame.h"
#include "WebFullScreenManager.h"
#include "WebGPUDowncastConvertToBackingContext.h"
#include "WebHitTestResultData.h"
#include "WebImage.h"
#include "WebLocalFrameLoaderClient.h"
#include "WebOpenPanelResultListener.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPageProxyMessages.h"
#include "WebPopupMenu.h"
#include "WebProcess.h"
#include "WebProcessPoolMessages.h"
#include "WebProcessProxyMessages.h"
#include "WebSearchPopupMenu.h"
#include "WebWorkerClient.h"
#include <WebCore/AppHighlight.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/ColorChooser.h>
#include <WebCore/ContentRuleListResults.h>
#include <WebCore/CookieConsentDecisionResult.h>
#include <WebCore/DataListSuggestionPicker.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentStorageAccess.h>
#include <WebCore/ElementInlines.h>
#include <WebCore/FileChooser.h>
#include <WebCore/FileIconLoader.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FullscreenManager.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLParserIdioms.h>
#include <WebCore/HTMLPlugInImageElement.h>
#include <WebCore/Icon.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/Settings.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/TextRecognitionOptions.h>
#include <WebCore/ViewportConfiguration.h>
#include <WebCore/WindowFeatures.h>
#include <wtf/JSONValues.h>
#include <wtf/TZoneMallocInlines.h>

#if HAVE(WEBGPU_IMPLEMENTATION)
#import <WebCore/WebGPUCreateImpl.h>
#endif

#if HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
#import <WebCore/BarcodeDetectorImplementation.h>
#import <WebCore/FaceDetectorImplementation.h>
#import <WebCore/TextDetectorImplementation.h>
#endif

#if ENABLE(APPLE_PAY_AMS_UI)
#include <WebCore/ApplePayAMSUIRequest.h>
#endif

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
#include "PlaybackSessionManager.h"
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
#include "VideoPresentationManager.h"
#endif

#if ENABLE(ASYNC_SCROLLING)
#include "RemoteScrollingCoordinator.h"
#endif

#if ENABLE(WEB_AUTHN)
#include <WebCore/MockWebAuthenticationConfiguration.h>
#endif

#if ENABLE(WEBGL) && ENABLE(GPU_PROCESS)
#include "RemoteGraphicsContextGLProxy.h"
#endif

#if ENABLE(WEBGL)
#include <WebCore/GraphicsContextGL.h>
#endif

#if ENABLE(WEBXR) && !USE(OPENXR)
#include "PlatformXRSystemProxy.h"
#endif

#if PLATFORM(MAC)
#include "TiledCoreAnimationScrollingCoordinator.h"
#endif

#if PLATFORM(COCOA)
#include "WebIconUtilities.h"
#endif

#if PLATFORM(MAC)
#include "RemoteScrollbarsController.h"
#include <WebCore/ScrollbarsControllerMock.h>
#endif

namespace WebKit {
using namespace WebCore;
using namespace HTMLNames;

AXRelayProcessSuspendedNotification::AXRelayProcessSuspendedNotification(Ref<WebPage> page, AutomaticallySend automaticallySend)
    : m_page(page.get())
    , m_automaticallySend(automaticallySend)
{
    if (m_automaticallySend == AutomaticallySend::Yes)
        sendProcessSuspendMessage(true);
}

AXRelayProcessSuspendedNotification::~AXRelayProcessSuspendedNotification()
{
    if (m_automaticallySend == AutomaticallySend::Yes)
        sendProcessSuspendMessage(false);
}

#if !PLATFORM(COCOA)
void AXRelayProcessSuspendedNotification::sendProcessSuspendMessage(bool) { }
#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebChromeClient);

WebChromeClient::WebChromeClient(WebPage& page)
    : m_page(page)
{
}

WebChromeClient::~WebChromeClient() = default;

void WebChromeClient::chromeDestroyed()
{
}

Ref<WebPage> WebChromeClient::protectedPage() const
{
    return m_page.get();
}

void WebChromeClient::setWindowRect(const FloatRect& windowFrame)
{
    protectedPage()->sendSetWindowFrame(windowFrame);
}

FloatRect WebChromeClient::windowRect() const
{
#if PLATFORM(IOS_FAMILY)
    return FloatRect();
#else
    auto page = protectedPage();
#if PLATFORM(MAC)
    if (page->hasCachedWindowFrame())
        return page->windowFrameInUnflippedScreenCoordinates();
#endif

    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetWindowFrame(), page->identifier());
    auto [newWindowFrame] = sendResult.takeReplyOr(FloatRect { });
    return newWindowFrame;
#endif
}

FloatRect WebChromeClient::pageRect() const
{
    return FloatRect(FloatPoint(), page().size());
}

void WebChromeClient::focus()
{
    protectedPage()->send(Messages::WebPageProxy::SetFocus(true));
}

void WebChromeClient::unfocus()
{
    protectedPage()->send(Messages::WebPageProxy::SetFocus(false));
}

#if PLATFORM(COCOA)

void WebChromeClient::elementDidFocus(Element& element, const FocusOptions& options)
{
    protectedPage()->elementDidFocus(element, options);
}

void WebChromeClient::elementDidRefocus(Element& element, const FocusOptions& options)
{
    protectedPage()->elementDidRefocus(element, options);
}

void WebChromeClient::elementDidBlur(Element& element)
{
    protectedPage()->elementDidBlur(element);
}

void WebChromeClient::focusedElementDidChangeInputMode(Element& element, InputMode mode)
{
    protectedPage()->focusedElementDidChangeInputMode(element, mode);
}

void WebChromeClient::focusedSelectElementDidChangeOptions(const WebCore::HTMLSelectElement& element)
{
    protectedPage()->focusedSelectElementDidChangeOptions(element);
}

void WebChromeClient::makeFirstResponder()
{
    protectedPage()->send(Messages::WebPageProxy::MakeFirstResponder());
}

void WebChromeClient::assistiveTechnologyMakeFirstResponder()
{
    protectedPage()->send(Messages::WebPageProxy::AssistiveTechnologyMakeFirstResponder());
}

#endif    

bool WebChromeClient::canTakeFocus(FocusDirection) const
{
    notImplemented();
    return true;
}

void WebChromeClient::takeFocus(FocusDirection direction)
{
    protectedPage()->send(Messages::WebPageProxy::TakeFocus(direction));
}

void WebChromeClient::focusedElementChanged(Element* element)
{
    RefPtr inputElement = dynamicDowncast<HTMLInputElement>(element);
    if (!inputElement || !inputElement->isText())
        return;

    RefPtr frame = element->document().frame();
    RefPtr webFrame = WebFrame::fromCoreFrame(*frame);
    ASSERT(webFrame);
    auto page = protectedPage();
    page->injectedBundleFormClient().didFocusTextField(page.ptr(), *inputElement, webFrame.get());
}

void WebChromeClient::focusedFrameChanged(Frame* frame)
{
    auto webFrame = frame ? WebFrame::fromCoreFrame(*frame) : nullptr;

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::FocusedFrameChanged(webFrame ? std::make_optional(webFrame->frameID()) : std::nullopt), page().identifier());
}

RefPtr<Page> WebChromeClient::createWindow(LocalFrame& frame, const String& openedMainFrameName, const WindowFeatures& windowFeatures, const NavigationAction& navigationAction)
{
#if ENABLE(FULLSCREEN_API)
    if (RefPtr document = frame.document())
        document->fullscreenManager().cancelFullscreen();
#endif

    auto& webProcess = WebProcess::singleton();

    auto& mouseEventData = navigationAction.mouseEventData();

    RefPtr webFrame = WebFrame::fromCoreFrame(frame);

    NavigationActionData navigationActionData {
        navigationAction.type(),
        modifiersForNavigationAction(navigationAction),
        mouseButton(navigationAction),
        syntheticClickType(navigationAction),
        webProcess.userGestureTokenIdentifier(navigationAction.requester()->pageID, navigationAction.userGestureToken()),
        navigationAction.userGestureToken() ? navigationAction.userGestureToken()->authorizationToken() : std::nullopt,
        protectedPage()->canHandleRequest(navigationAction.originalRequest()),
        navigationAction.shouldOpenExternalURLsPolicy(),
        navigationAction.downloadAttribute(),
        mouseEventData ? mouseEventData->locationInRootViewCoordinates : FloatPoint { },
        { }, /* redirectResponse */
        navigationAction.isRequestFromClientOrUserInput(),
        false, /* treatAsSameOriginNavigation */
        false, /* hasOpenedFrames */
        false, /* openedByDOMWithOpener */
        navigationAction.newFrameOpenerPolicy() == NewFrameOpenerPolicy::Allow, /* hasOpener */
        frame.loader().isHTTPFallbackInProgress(),
        openedMainFrameName,
        { }, /* requesterOrigin */
        { }, /* requesterTopOrigin */
        std::nullopt, /* targetBackForwardItemIdentifier */
        std::nullopt, /* sourceBackForwardItemIdentifier */
        WebCore::LockHistory::No,
        WebCore::LockBackForwardList::No,
        { }, /* clientRedirectSourceForHistory */
        frame.effectiveSandboxFlags(),
        std::nullopt, /* ownerPermissionsPolicy */
        navigationAction.privateClickMeasurement(),
        { }, /* advancedPrivacyProtections */
        { }, /* originatorAdvancedPrivacyProtections */
#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
        std::nullopt, /* webHitTestResultData */
#endif
        webFrame->info(), /* originatingFrameInfoData */
        webFrame->page()->webPageProxyIdentifier(),
        webFrame->info(), /* frameInfo */
        std::nullopt, /* navigationID */
        navigationAction.originalRequest(), /* originalRequest */
        navigationAction.originalRequest() /* request */
    };

    auto sendResult = webProcess.parentProcessConnection()->sendSync(Messages::WebPageProxy::CreateNewPage(windowFeatures, navigationActionData), page().identifier(), IPC::Timeout::infinity(), { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages });
    if (!sendResult.succeeded())
        return nullptr;

    auto [newPageID, parameters] = sendResult.takeReply();
    if (!newPageID)
        return nullptr;
    ASSERT(parameters);

    parameters->oldPageID = page().identifier();

    webProcess.createWebPage(*newPageID, WTFMove(*parameters));
    return webProcess.webPage(*newPageID)->corePage();
}

bool WebChromeClient::testProcessIncomingSyncMessagesWhenWaitingForSyncReply()
{
    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;

    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::TestProcessIncomingSyncMessagesWhenWaitingForSyncReply(page().webPageProxyIdentifier()), 0);
    auto [handled] = sendResult.takeReplyOr(false);
    return handled;
}

void WebChromeClient::show()
{
    protectedPage()->show();
}

bool WebChromeClient::canRunModal() const
{
    return protectedPage()->canRunModal();
}

void WebChromeClient::runModal()
{
    protectedPage()->runModal();
}

void WebChromeClient::reportProcessCPUTime(Seconds cpuTime, ActivityStateForCPUSampling activityState)
{
    WebProcess::singleton().send(Messages::WebProcessPool::ReportWebContentCPUTime(cpuTime, static_cast<uint64_t>(activityState)), 0);
}

void WebChromeClient::setToolbarsVisible(bool toolbarsAreVisible)
{
    protectedPage()->send(Messages::WebPageProxy::SetToolbarsAreVisible(toolbarsAreVisible));
}

bool WebChromeClient::toolbarsVisible() const
{
    auto page = protectedPage();
    API::InjectedBundle::PageUIClient::UIElementVisibility toolbarsVisibility = page->injectedBundleUIClient().toolbarsAreVisible(page.ptr());
    if (toolbarsVisibility != API::InjectedBundle::PageUIClient::UIElementVisibility::Unknown)
        return toolbarsVisibility == API::InjectedBundle::PageUIClient::UIElementVisibility::Visible;
    
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetToolbarsAreVisible(), page->identifier());
    auto [toolbarsAreVisible] = sendResult.takeReplyOr(true);
    return toolbarsAreVisible;
}

void WebChromeClient::setStatusbarVisible(bool statusBarIsVisible)
{
    protectedPage()->send(Messages::WebPageProxy::SetStatusBarIsVisible(statusBarIsVisible));
}

bool WebChromeClient::statusbarVisible() const
{
    auto page = protectedPage();
    API::InjectedBundle::PageUIClient::UIElementVisibility statusbarVisibility = page->injectedBundleUIClient().statusBarIsVisible(page.ptr());
    if (statusbarVisibility != API::InjectedBundle::PageUIClient::UIElementVisibility::Unknown)
        return statusbarVisibility == API::InjectedBundle::PageUIClient::UIElementVisibility::Visible;

    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetStatusBarIsVisible(), page->identifier());
    auto [statusBarIsVisible] = sendResult.takeReplyOr(true);
    return statusBarIsVisible;
}

void WebChromeClient::setScrollbarsVisible(bool)
{
    notImplemented();
}

bool WebChromeClient::scrollbarsVisible() const
{
    notImplemented();
    return true;
}

void WebChromeClient::setMenubarVisible(bool menuBarVisible)
{
    protectedPage()->send(Messages::WebPageProxy::SetMenuBarIsVisible(menuBarVisible));
}

bool WebChromeClient::menubarVisible() const
{
    auto page = protectedPage();
    API::InjectedBundle::PageUIClient::UIElementVisibility menubarVisibility = page->injectedBundleUIClient().menuBarIsVisible(page.ptr());
    if (menubarVisibility != API::InjectedBundle::PageUIClient::UIElementVisibility::Unknown)
        return menubarVisibility == API::InjectedBundle::PageUIClient::UIElementVisibility::Visible;
    
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetMenuBarIsVisible(), page->identifier());
    auto [menuBarIsVisible] = sendResult.takeReplyOr(true);
    return menuBarIsVisible;
}

void WebChromeClient::setResizable(bool resizable)
{
    protectedPage()->send(Messages::WebPageProxy::SetIsResizable(resizable));
}

void WebChromeClient::addMessageToConsole(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, unsigned columnNumber, const String& sourceID)
{
    // Notify the bundle client.
    auto page = protectedPage();
    page->injectedBundleUIClient().willAddMessageToConsole(page.ptr(), source, level, message, lineNumber, columnNumber, sourceID);
}

void WebChromeClient::addMessageWithArgumentsToConsole(MessageSource source, MessageLevel level, const String& message, std::span<const String> messageArguments, unsigned lineNumber, unsigned columnNumber, const String& sourceID)
{
    auto page = protectedPage();
    page->injectedBundleUIClient().willAddMessageWithArgumentsToConsole(page.ptr(), source, level, message, messageArguments, lineNumber, columnNumber, sourceID);
}

bool WebChromeClient::canRunBeforeUnloadConfirmPanel()
{
    return protectedPage()->canRunBeforeUnloadConfirmPanel();
}

bool WebChromeClient::runBeforeUnloadConfirmPanel(const String& message, LocalFrame& frame)
{
    auto webFrame = WebFrame::fromCoreFrame(frame);

    HangDetectionDisabler hangDetectionDisabler;

    auto page = protectedPage();
    auto relay = AXRelayProcessSuspendedNotification(page);

    auto sendResult = page->sendSyncWithDelayedReply(Messages::WebPageProxy::RunBeforeUnloadConfirmPanel(webFrame->frameID(), webFrame->info(), message));
    auto [shouldClose] = sendResult.takeReplyOr(false);
    return shouldClose;
}

void WebChromeClient::closeWindow()
{
    // FIXME: This code assumes that the client will respond to a close page
    // message by actually closing the page. Safari does this, but there is
    // no guarantee that other applications will, which will leave this page
    // half detached. This approach is an inherent limitation making parts of
    // a close execute synchronously as part of window.close, but other parts
    // later on.

    auto page = protectedPage();
    page->corePage()->setGroupName(String());

    Ref frame = page->mainWebFrame();
    if (RefPtr coreFrame = frame->coreLocalFrame())
        coreFrame->loader().stopForUserCancel();

    page->sendClose();
}

void WebChromeClient::rootFrameAdded(const WebCore::LocalFrame& frame)
{
    if (auto* drawingArea = page().drawingArea())
        drawingArea->addRootFrame(frame.frameID());
}

void WebChromeClient::rootFrameRemoved(const WebCore::LocalFrame& frame)
{
    if (auto* drawingArea = page().drawingArea())
        drawingArea->removeRootFrame(frame.frameID());
}

static bool shouldSuppressJavaScriptDialogs(LocalFrame& frame)
{
    if (frame.opener() && frame.loader().stateMachine().isDisplayingInitialEmptyDocument() && frame.loader().provisionalDocumentLoader())
        return true;

    return false;
}

void WebChromeClient::runJavaScriptAlert(LocalFrame& frame, const String& alertText)
{
    if (shouldSuppressJavaScriptDialogs(frame))
        return;

    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);

    // Notify the bundle client.
    auto page = protectedPage();
    page->injectedBundleUIClient().willRunJavaScriptAlert(page.ptr(), alertText, webFrame.get());
    page->prepareToRunModalJavaScriptDialog();

    HangDetectionDisabler hangDetectionDisabler;
    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;

    auto relay = AXRelayProcessSuspendedNotification(page);

    page->sendSyncWithDelayedReply(Messages::WebPageProxy::RunJavaScriptAlert(webFrame->frameID(), webFrame->info(), alertText), { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages });
}

bool WebChromeClient::runJavaScriptConfirm(LocalFrame& frame, const String& message)
{
    if (shouldSuppressJavaScriptDialogs(frame))
        return false;

    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);

    // Notify the bundle client.
    auto page = protectedPage();
    page->injectedBundleUIClient().willRunJavaScriptConfirm(page.ptr(), message, webFrame.get());
    page->prepareToRunModalJavaScriptDialog();

    HangDetectionDisabler hangDetectionDisabler;
    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;

    auto relay = AXRelayProcessSuspendedNotification(page);

    auto sendResult = page->sendSyncWithDelayedReply(Messages::WebPageProxy::RunJavaScriptConfirm(webFrame->frameID(), webFrame->info(), message), { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages });
    auto [result] = sendResult.takeReplyOr(false);
    return result;
}

bool WebChromeClient::runJavaScriptPrompt(LocalFrame& frame, const String& message, const String& defaultValue, String& result)
{
    if (shouldSuppressJavaScriptDialogs(frame))
        return false;

    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);

    // Notify the bundle client.
    auto page = protectedPage();
    page->injectedBundleUIClient().willRunJavaScriptPrompt(page.ptr(), message, defaultValue, webFrame.get());
    page->prepareToRunModalJavaScriptDialog();

    HangDetectionDisabler hangDetectionDisabler;
    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;

    auto relay = AXRelayProcessSuspendedNotification(page);

    auto sendResult = page->sendSyncWithDelayedReply(Messages::WebPageProxy::RunJavaScriptPrompt(webFrame->frameID(), webFrame->info(), message, defaultValue), { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages });
    if (!sendResult.succeeded())
        return false;

    std::tie(result) = sendResult.takeReply();
    return !result.isNull();
}

KeyboardUIMode WebChromeClient::keyboardUIMode()
{
    return protectedPage()->keyboardUIMode();
}

bool WebChromeClient::hoverSupportedByPrimaryPointingDevice() const
{
    return protectedPage()->hoverSupportedByPrimaryPointingDevice();
}

bool WebChromeClient::hoverSupportedByAnyAvailablePointingDevice() const
{
    return protectedPage()->hoverSupportedByAnyAvailablePointingDevice();
}

std::optional<PointerCharacteristics> WebChromeClient::pointerCharacteristicsOfPrimaryPointingDevice() const
{
    return protectedPage()->pointerCharacteristicsOfPrimaryPointingDevice();
}

OptionSet<PointerCharacteristics> WebChromeClient::pointerCharacteristicsOfAllAvailablePointingDevices() const
{
    return protectedPage()->pointerCharacteristicsOfAllAvailablePointingDevices();
}

#if ENABLE(POINTER_LOCK)

bool WebChromeClient::requestPointerLock()
{
    protectedPage()->send(Messages::WebPageProxy::RequestPointerLock());
    return true;
}

void WebChromeClient::requestPointerUnlock()
{
    protectedPage()->send(Messages::WebPageProxy::RequestPointerUnlock());
}

#endif

void WebChromeClient::invalidateRootView(const IntRect&)
{
    // Do nothing here, there's no concept of invalidating the window in the web process.
}

void WebChromeClient::invalidateContentsAndRootView(const IntRect& rect)
{
    auto page = protectedPage();
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(page->corePage()->mainFrame());
    if (!localMainFrame)
        return;

    if (RefPtr document = localMainFrame->document()) {
        if (document->printing())
            return;
    }

    page->drawingArea()->setNeedsDisplayInRect(rect);
}

void WebChromeClient::invalidateContentsForSlowScroll(const IntRect& rect)
{
    auto page = protectedPage();
    RefPtr localMainFrame = dynamicDowncast<LocalFrame>(page->corePage()->mainFrame());
    if (!localMainFrame)
        return;

    if (RefPtr document = localMainFrame->document()) {
        if (document->printing())
            return;
    }

    page->pageDidScroll();
    page->drawingArea()->setNeedsDisplayInRect(rect);
}

void WebChromeClient::scroll(const IntSize& scrollDelta, const IntRect& scrollRect, const IntRect& clipRect)
{
    auto page = protectedPage();
    page->pageDidScroll();
    page->drawingArea()->scroll(intersection(scrollRect, clipRect), scrollDelta);
}

IntPoint WebChromeClient::screenToRootView(const IntPoint& point) const
{
    return protectedPage()->screenToRootView(point);
}

IntRect WebChromeClient::rootViewToScreen(const IntRect& rect) const
{
    return protectedPage()->rootViewToScreen(rect);
}
    
IntPoint WebChromeClient::accessibilityScreenToRootView(const IntPoint& point) const
{
    return protectedPage()->accessibilityScreenToRootView(point);
}

IntRect WebChromeClient::rootViewToAccessibilityScreen(const IntRect& rect) const
{
    return protectedPage()->rootViewToAccessibilityScreen(rect);
}

void WebChromeClient::didFinishLoadingImageForElement(HTMLImageElement& element)
{
    protectedPage()->didFinishLoadingImageForElement(element);
}

PlatformPageClient WebChromeClient::platformPageClient() const
{
    notImplemented();
    return 0;
}

void WebChromeClient::intrinsicContentsSizeChanged(const IntSize& size) const
{
    protectedPage()->scheduleIntrinsicContentSizeUpdate(size);
}

void WebChromeClient::contentsSizeChanged(LocalFrame& frame, const IntSize& size) const
{
    RefPtr frameView = frame.view();

    if (&frame.page()->mainFrame() != &frame)
        return;

    auto page = protectedPage();
    page->send(Messages::WebPageProxy::DidChangeContentSize(size));

    page->drawingArea()->mainFrameContentSizeChanged(frame.frameID(), size);

    if (frameView && !frameView->delegatesScrollingToNativeView())  {
        bool hasHorizontalScrollbar = frameView->horizontalScrollbar();
        bool hasVerticalScrollbar = frameView->verticalScrollbar();

        if (hasHorizontalScrollbar != m_cachedMainFrameHasHorizontalScrollbar || hasVerticalScrollbar != m_cachedMainFrameHasVerticalScrollbar) {
            page->send(Messages::WebPageProxy::DidChangeScrollbarsForMainFrame(hasHorizontalScrollbar, hasVerticalScrollbar));

            m_cachedMainFrameHasHorizontalScrollbar = hasHorizontalScrollbar;
            m_cachedMainFrameHasVerticalScrollbar = hasVerticalScrollbar;
        }
    }
}

void WebChromeClient::scrollMainFrameToRevealRect(const IntRect& rect) const
{
    protectedPage()->send(Messages::WebPageProxy::RequestScrollToRect(rect, rect.center()));
}

void WebChromeClient::scrollContainingScrollViewsToRevealRect(const IntRect&) const
{
    notImplemented();
}

bool WebChromeClient::shouldUnavailablePluginMessageBeButton(RenderEmbeddedObject::PluginUnavailabilityReason pluginUnavailabilityReason) const
{
    switch (pluginUnavailabilityReason) {
    case RenderEmbeddedObject::PluginMissing:
        // FIXME: <rdar://problem/8794397> We should only return true when there is a
        // missingPluginButtonClicked callback defined on the Page UI client.
    case RenderEmbeddedObject::InsecurePluginVersion:
        return true;


    case RenderEmbeddedObject::PluginCrashed:
    case RenderEmbeddedObject::PluginBlockedByContentSecurityPolicy:
    case RenderEmbeddedObject::UnsupportedPlugin:
    case RenderEmbeddedObject::PluginTooSmall:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}
    
void WebChromeClient::unavailablePluginButtonClicked(Element& element, RenderEmbeddedObject::PluginUnavailabilityReason pluginUnavailabilityReason) const
{
    UNUSED_PARAM(element);
    UNUSED_PARAM(pluginUnavailabilityReason);
}

void WebChromeClient::mouseDidMoveOverElement(const HitTestResult& hitTestResult, OptionSet<WebCore::PlatformEventModifier> modifiers, const String& toolTip, TextDirection)
{
    RefPtr<API::Object> userData;
    auto wkModifiers = modifiersFromPlatformEventModifiers(modifiers);

    // Notify the bundle client.
    auto page = protectedPage();
    page->injectedBundleUIClient().mouseDidMoveOverElement(page.ptr(), hitTestResult, wkModifiers, userData);

    // Notify the UIProcess.
    WebHitTestResultData webHitTestResultData(hitTestResult, toolTip);
    webHitTestResultData.elementBoundingBox = webHitTestResultData.elementBoundingBox.toRectWithExtentsClippedToNumericLimits();
    page->send(Messages::WebPageProxy::MouseDidMoveOverElement(webHitTestResultData, wkModifiers, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebChromeClient::print(LocalFrame& frame, const StringWithDirection& title)
{
    static constexpr unsigned maxTitleLength = 1000; // Closest power of 10 above the W3C recommendation for Title length.

    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);

    WebCore::FloatSize pdfFirstPageSize;
#if ENABLE(PDF_PLUGIN)
    if (auto* pluginView = WebPage::pluginViewForFrame(&frame))
        pdfFirstPageSize = pluginView->pdfDocumentSizeForPrinting();
#endif

    auto truncatedTitle = truncateFromEnd(title, maxTitleLength);
    auto page = protectedPage();
    auto relay = AXRelayProcessSuspendedNotification(page);

    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;
    page->sendSyncWithDelayedReply(Messages::WebPageProxy::PrintFrame(webFrame->frameID(), truncatedTitle.string, pdfFirstPageSize));
}

#if ENABLE(INPUT_TYPE_COLOR)

RefPtr<ColorChooser> WebChromeClient::createColorChooser(ColorChooserClient& client, const Color& initialColor)
{
    return WebColorChooser::create(protectedPage().ptr(), &client, initialColor);
}

#endif

#if ENABLE(DATALIST_ELEMENT)

RefPtr<DataListSuggestionPicker> WebChromeClient::createDataListSuggestionPicker(DataListSuggestionsClient& client)
{
    return WebDataListSuggestionPicker::create(protectedPage(), client);
}

bool WebChromeClient::canShowDataListSuggestionLabels() const
{
#if PLATFORM(MAC)
    return true;
#else
    return false;
#endif
}

#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)

RefPtr<DateTimeChooser> WebChromeClient::createDateTimeChooser(DateTimeChooserClient& client)
{
    return WebDateTimeChooser::create(protectedPage(), client);
}

#endif

void WebChromeClient::runOpenPanel(LocalFrame& frame, FileChooser& fileChooser)
{
    auto page = protectedPage();
    if (page->activeOpenPanelResultListener())
        return;

    page->setActiveOpenPanelResultListener(WebOpenPanelResultListener::create(page, fileChooser));

    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    page->send(Messages::WebPageProxy::RunOpenPanel(webFrame->frameID(), webFrame->info(), fileChooser.settings()));
}
    
void WebChromeClient::showShareSheet(ShareDataWithParsedURL& shareData, CompletionHandler<void(bool)>&& callback)
{
    protectedPage()->showShareSheet(shareData, WTFMove(callback));
}

void WebChromeClient::showContactPicker(const WebCore::ContactsRequestData& requestData, WTF::CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&& callback)
{
    protectedPage()->showContactPicker(requestData, WTFMove(callback));
}

void WebChromeClient::loadIconForFiles(const Vector<String>& filenames, FileIconLoader& loader)
{
    loader.iconLoaded(createIconForFiles(filenames));
}

void WebChromeClient::setCursor(const Cursor& cursor)
{
    protectedPage()->send(Messages::WebPageProxy::SetCursor(cursor));
}

void WebChromeClient::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    protectedPage()->send(Messages::WebPageProxy::SetCursorHiddenUntilMouseMoves(hiddenUntilMouseMoves));
}

#if !PLATFORM(COCOA)

RefPtr<Icon> WebChromeClient::createIconForFiles(const Vector<String>& filenames)
{
    return Icon::createIconForFiles(filenames);
}

#endif

void WebChromeClient::didAssociateFormControls(const Vector<RefPtr<Element>>& elements, WebCore::LocalFrame& frame)
{
    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    auto page = protectedPage();
    return page->injectedBundleFormClient().didAssociateFormControls(page.ptr(), elements, webFrame.get());
}

bool WebChromeClient::shouldNotifyOnFormChanges()
{
    auto page = protectedPage();
    return page->injectedBundleFormClient().shouldNotifyOnFormChanges(page.ptr());
}

bool WebChromeClient::selectItemWritingDirectionIsNatural()
{
    return false;
}

bool WebChromeClient::selectItemAlignmentFollowsMenuWritingDirection()
{
    return true;
}

RefPtr<PopupMenu> WebChromeClient::createPopupMenu(PopupMenuClient& client) const
{
    return WebPopupMenu::create(protectedPage().ptr(), &client);
}

RefPtr<SearchPopupMenu> WebChromeClient::createSearchPopupMenu(PopupMenuClient& client) const
{
    return WebSearchPopupMenu::create(protectedPage().ptr(), &client);
}

GraphicsLayerFactory* WebChromeClient::graphicsLayerFactory() const
{
    auto page = protectedPage();
    if (auto drawingArea = page->drawingArea())
        return drawingArea->graphicsLayerFactory();
    return nullptr;
}

WebCore::DisplayRefreshMonitorFactory* WebChromeClient::displayRefreshMonitorFactory() const
{
    return page().drawingArea();
}

#if ENABLE(GPU_PROCESS)
RefPtr<ImageBuffer> WebChromeClient::createImageBuffer(const FloatSize& size, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, ImageBufferPixelFormat pixelFormat, OptionSet<ImageBufferOptions> options) const
{
    if (!WebProcess::singleton().shouldUseRemoteRenderingFor(purpose)) {
        if (purpose != RenderingPurpose::ShareableSnapshot && purpose != RenderingPurpose::ShareableLocalSnapshot)
            return nullptr;

        return ImageBuffer::create<ImageBufferShareableBitmapBackend>(size, resolutionScale, colorSpace, ImageBufferPixelFormat::BGRA8, purpose, { });
    }

    return protectedPage()->ensureRemoteRenderingBackendProxy().createImageBuffer(size, purpose, resolutionScale, colorSpace, pixelFormat, options);
}

RefPtr<ImageBuffer> WebChromeClient::sinkIntoImageBuffer(std::unique_ptr<SerializedImageBuffer> imageBuffer)
{
    if (!is<RemoteSerializedImageBufferProxy>(imageBuffer))
        return SerializedImageBuffer::sinkIntoImageBuffer(WTFMove(imageBuffer));
    auto remote = std::unique_ptr<RemoteSerializedImageBufferProxy>(static_cast<RemoteSerializedImageBufferProxy*>(imageBuffer.release()));
    return RemoteSerializedImageBufferProxy::sinkIntoImageBuffer(WTFMove(remote), protectedPage()->ensureRemoteRenderingBackendProxy());
}
#endif

std::unique_ptr<WebCore::WorkerClient> WebChromeClient::createWorkerClient(SerialFunctionDispatcher& dispatcher)
{
    return WebWorkerClient::create(*protectedPage()->corePage(), dispatcher).moveToUniquePtr();
}

#if ENABLE(WEBGL)
RefPtr<GraphicsContextGL> WebChromeClient::createGraphicsContextGL(const GraphicsContextGLAttributes& attributes) const
{
#if PLATFORM(GTK)
    WebProcess::singleton().initializePlatformDisplayIfNeeded();
#endif
#if ENABLE(GPU_PROCESS)
    if (WebProcess::singleton().shouldUseRemoteRenderingForWebGL())
        return RemoteGraphicsContextGLProxy::create(attributes, protectedPage());
#endif
    return WebCore::createWebProcessGraphicsContextGL(attributes);
}
#endif

#if HAVE(WEBGPU_IMPLEMENTATION)
RefPtr<WebCore::WebGPU::GPU> WebChromeClient::createGPUForWebGPU() const
{
#if ENABLE(GPU_PROCESS)
    return RemoteGPUProxy::create(WebGPU::DowncastConvertToBackingContext::create(), protectedPage());
#else
    return WebCore::WebGPU::create([](WebCore::WebGPU::WorkItem&& workItem) {
        callOnMainRunLoop(WTFMove(workItem));
    }, nullptr);
#endif
}
#endif

RefPtr<WebCore::ShapeDetection::BarcodeDetector> WebChromeClient::createBarcodeDetector(const WebCore::ShapeDetection::BarcodeDetectorOptions& barcodeDetectorOptions) const
{
#if ENABLE(GPU_PROCESS)
    auto page = protectedPage();
    auto& remoteRenderingBackendProxy = page->ensureRemoteRenderingBackendProxy();
    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=275245): Does not work when GPUP crashes.
    RefPtr connection = remoteRenderingBackendProxy.connection();
    if (!connection)
        return nullptr;
    return ShapeDetection::RemoteBarcodeDetectorProxy::create(connection.releaseNonNull(), remoteRenderingBackendProxy.renderingBackendIdentifier(), ShapeDetectionIdentifier::generate(), barcodeDetectorOptions);
#elif HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    return WebCore::ShapeDetection::BarcodeDetectorImpl::create(barcodeDetectorOptions);
#else
    return nullptr;
#endif
}

void WebChromeClient::getBarcodeDetectorSupportedFormats(CompletionHandler<void(Vector<WebCore::ShapeDetection::BarcodeFormat>&&)>&& completionHandler) const
{
#if ENABLE(GPU_PROCESS)
    auto page = protectedPage();
    auto& remoteRenderingBackendProxy = page->ensureRemoteRenderingBackendProxy();
    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=275245): Does not work when GPUP crashes.
    RefPtr connection = remoteRenderingBackendProxy.connection();
    if (!connection) {
        completionHandler({ });
        return;
    }
    ShapeDetection::RemoteBarcodeDetectorProxy::getSupportedFormats(connection.releaseNonNull(), remoteRenderingBackendProxy.renderingBackendIdentifier(), WTFMove(completionHandler));
#elif HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    WebCore::ShapeDetection::BarcodeDetectorImpl::getSupportedFormats(WTFMove(completionHandler));
#else
    completionHandler({ });
#endif
}

RefPtr<WebCore::ShapeDetection::FaceDetector> WebChromeClient::createFaceDetector(const WebCore::ShapeDetection::FaceDetectorOptions& faceDetectorOptions) const
{
#if ENABLE(GPU_PROCESS)
    auto page = protectedPage();
    auto& remoteRenderingBackendProxy = page->ensureRemoteRenderingBackendProxy();
    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=275245): Does not work when GPUP crashes.
    RefPtr connection = remoteRenderingBackendProxy.connection();
    if (!connection)
        return nullptr;
    return ShapeDetection::RemoteFaceDetectorProxy::create(connection.releaseNonNull(), remoteRenderingBackendProxy.renderingBackendIdentifier(), ShapeDetectionIdentifier::generate(), faceDetectorOptions);
#elif HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    return WebCore::ShapeDetection::FaceDetectorImpl::create(faceDetectorOptions);
#else
    return nullptr;
#endif
}

RefPtr<WebCore::ShapeDetection::TextDetector> WebChromeClient::createTextDetector() const
{
#if ENABLE(GPU_PROCESS)
    auto page = protectedPage();
    auto& remoteRenderingBackendProxy = page->ensureRemoteRenderingBackendProxy();
    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=275245): Does not work when GPUP crashes.
    RefPtr connection = remoteRenderingBackendProxy.connection();
    if (!connection)
        return nullptr;
    return ShapeDetection::RemoteTextDetectorProxy::create(connection.releaseNonNull(), remoteRenderingBackendProxy.renderingBackendIdentifier(), ShapeDetectionIdentifier::generate());
#elif HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    return WebCore::ShapeDetection::TextDetectorImpl::create();
#else
    return nullptr;
#endif
}

void WebChromeClient::attachRootGraphicsLayer(LocalFrame& frame, GraphicsLayer* layer)
{
    auto page = protectedPage();
    if (layer)
        page->enterAcceleratedCompositingMode(frame, layer);
    else
        page->exitAcceleratedCompositingMode(frame);
}

void WebChromeClient::attachViewOverlayGraphicsLayer(GraphicsLayer* graphicsLayer)
{
    auto page = protectedPage();
    auto* drawingArea = page->drawingArea();
    if (!drawingArea)
        return;

    // FIXME: Support view overlays in iframe processes if needed. <rdar://116202544>
    if (page->mainWebFrame().coreLocalFrame())
        drawingArea->attachViewOverlayGraphicsLayer(page->mainWebFrame().frameID(), graphicsLayer);
}

void WebChromeClient::setNeedsOneShotDrawingSynchronization()
{
    notImplemented();
}

bool WebChromeClient::shouldTriggerRenderingUpdate(unsigned rescheduledRenderingUpdateCount) const
{
    return protectedPage()->shouldTriggerRenderingUpdate(rescheduledRenderingUpdateCount);
}

void WebChromeClient::triggerRenderingUpdate()
{
    auto page = protectedPage();
    if (auto* drawingArea = page->drawingArea())
        drawingArea->triggerRenderingUpdate();
}

bool WebChromeClient::scheduleRenderingUpdate()
{
    auto page = protectedPage();
    if (auto* drawingArea = page->drawingArea())
        return drawingArea->scheduleRenderingUpdate();
    return false;
}

void WebChromeClient::renderingUpdateFramesPerSecondChanged()
{
    auto page = protectedPage();
    if (auto* drawingArea = page->drawingArea())
        drawingArea->renderingUpdateFramesPerSecondChanged();
}

unsigned WebChromeClient::remoteImagesCountForTesting() const
{
    return protectedPage()->remoteImagesCountForTesting();
}

void WebChromeClient::registerBlobPathForTesting(const String& path, CompletionHandler<void()>&& completionHandler)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::RegisterBlobPathForTesting(path), WTFMove(completionHandler));
}


void WebChromeClient::contentRuleListNotification(const URL& url, const ContentRuleListResults& results)
{
#if ENABLE(CONTENT_EXTENSIONS)
    protectedPage()->send(Messages::WebPageProxy::ContentRuleListNotification(url, results));
#endif
}

bool WebChromeClient::layerTreeStateIsFrozen() const
{
    auto page = protectedPage();
    if (auto* drawingArea = page->drawingArea())
        return drawingArea->layerTreeStateIsFrozen();

    return false;
}

#if ENABLE(ASYNC_SCROLLING)

RefPtr<WebCore::ScrollingCoordinator> WebChromeClient::createScrollingCoordinator(Page& corePage) const
{
    auto page = protectedPage();
    ASSERT_UNUSED(corePage, page->corePage() == &corePage);
#if PLATFORM(COCOA)
    switch (page->drawingArea()->type()) {
#if PLATFORM(MAC)
    case DrawingAreaType::TiledCoreAnimation:
        return TiledCoreAnimationScrollingCoordinator::create(page.ptr());
#endif
    case DrawingAreaType::RemoteLayerTree:
        return RemoteScrollingCoordinator::create(page.ptr());
    }
#endif
    return nullptr;
}

#endif

#if PLATFORM(MAC)
void WebChromeClient::ensureScrollbarsController(Page& corePage, ScrollableArea& area, bool update) const
{
    auto page = protectedPage();
    ASSERT(page->corePage() == &corePage);
    auto* currentScrollbarsController = area.existingScrollbarsController();

    if (area.mockScrollbarsControllerEnabled() || (update && !currentScrollbarsController)) {
        ASSERT(!currentScrollbarsController || is<ScrollbarsControllerMock>(currentScrollbarsController));
        return;
    }
    
    switch (page->drawingArea()->type()) {
    case DrawingAreaType::RemoteLayerTree: {
        if (!area.usesCompositedScrolling() && (!currentScrollbarsController || is<RemoteScrollbarsController>(currentScrollbarsController)))
            area.setScrollbarsController(ScrollbarsController::create(area));
        else if (area.usesCompositedScrolling() && (!currentScrollbarsController || !is<RemoteScrollbarsController>(currentScrollbarsController)))
            area.setScrollbarsController(makeUnique<RemoteScrollbarsController>(area, corePage.scrollingCoordinator()));
        return;
    }
    default: {
        if (!currentScrollbarsController)
            area.setScrollbarsController(ScrollbarsController::create(area));
    }
    }
}

#endif


#if ENABLE(VIDEO_PRESENTATION_MODE)

void WebChromeClient::prepareForVideoFullscreen()
{
    protectedPage()->videoPresentationManager();
}

bool WebChromeClient::canEnterVideoFullscreen(HTMLMediaElementEnums::VideoFullscreenMode mode) const
{
    return protectedPage()->videoPresentationManager().canEnterVideoFullscreen(mode);
}

bool WebChromeClient::supportsVideoFullscreen(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    return protectedPage()->videoPresentationManager().supportsVideoFullscreen(mode);
}

bool WebChromeClient::supportsVideoFullscreenStandby()
{
    return protectedPage()->videoPresentationManager().supportsVideoFullscreenStandby();
}

void WebChromeClient::setMockVideoPresentationModeEnabled(bool enabled)
{
    protectedPage()->send(Messages::WebPageProxy::SetMockVideoPresentationModeEnabled(enabled));
}

void WebChromeClient::enterVideoFullscreenForVideoElement(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode mode, bool standby)
{
#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
    ASSERT(standby || mode != HTMLMediaElementEnums::VideoFullscreenModeNone);
#else
    ASSERT(mode != HTMLMediaElementEnums::VideoFullscreenModeNone);
#endif
    protectedPage()->videoPresentationManager().enterVideoFullscreenForVideoElement(videoElement, mode, standby);
}

void WebChromeClient::exitVideoFullscreenForVideoElement(HTMLVideoElement& videoElement, CompletionHandler<void(bool)>&& completionHandler)
{
    protectedPage()->videoPresentationManager().exitVideoFullscreenForVideoElement(videoElement, WTFMove(completionHandler));
}

void WebChromeClient::setUpPlaybackControlsManager(HTMLMediaElement& mediaElement)
{
    protectedPage()->playbackSessionManager().setUpPlaybackControlsManager(mediaElement);
}

void WebChromeClient::clearPlaybackControlsManager()
{
    protectedPage()->playbackSessionManager().clearPlaybackControlsManager();
}

void WebChromeClient::mediaEngineChanged(WebCore::HTMLMediaElement& mediaElement)
{
    protectedPage()->playbackSessionManager().mediaEngineChanged(mediaElement);
}

#endif

#if ENABLE(MEDIA_USAGE)
void WebChromeClient::addMediaUsageManagerSession(MediaSessionIdentifier identifier, const String& bundleIdentifier, const URL& pageURL)
{
    protectedPage()->addMediaUsageManagerSession(identifier, bundleIdentifier, pageURL);
}

void WebChromeClient::updateMediaUsageManagerSessionState(MediaSessionIdentifier identifier, const MediaUsageInfo& usage)
{
    protectedPage()->updateMediaUsageManagerSessionState(identifier, usage);
}

void WebChromeClient::removeMediaUsageManagerSession(MediaSessionIdentifier identifier)
{
    protectedPage()->removeMediaUsageManagerSession(identifier);
}
#endif // ENABLE(MEDIA_USAGE)

#if ENABLE(VIDEO_PRESENTATION_MODE)

void WebChromeClient::exitVideoFullscreenToModeWithoutAnimation(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode targetMode)
{
    protectedPage()->videoPresentationManager().exitVideoFullscreenToModeWithoutAnimation(videoElement, targetMode);
}

void WebChromeClient::setVideoFullscreenMode(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    protectedPage()->videoPresentationManager().setVideoFullscreenMode(videoElement, mode);
}

void WebChromeClient::clearVideoFullscreenMode(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    protectedPage()->videoPresentationManager().clearVideoFullscreenMode(videoElement, mode);
}

#endif

#if ENABLE(FULLSCREEN_API)

bool WebChromeClient::supportsFullScreenForElement(const Element& element, bool withKeyboard)
{
    return protectedPage()->fullScreenManager()->supportsFullScreenForElement(element, withKeyboard);
}

void WebChromeClient::enterFullScreenForElement(Element& element, HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    protectedPage()->fullScreenManager()->enterFullScreenForElement(&element, mode);
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (RefPtr videoElement = dynamicDowncast<HTMLVideoElement>(&element); videoElement && mode == HTMLMediaElementEnums::VideoFullscreenModeInWindow)
        setVideoFullscreenMode(*videoElement, mode);
#endif
}

#if ENABLE(QUICKLOOK_FULLSCREEN)
void WebChromeClient::updateImageSource(Element& element)
{
    protectedPage()->fullScreenManager()->updateImageSource(element);
}
#endif // ENABLE(QUICKLOOK_FULLSCREEN)

void WebChromeClient::exitFullScreenForElement(Element* element)
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    bool exitingInWindowFullscreen = false;
    if (element) {
        if (RefPtr videoElement = dynamicDowncast<HTMLVideoElement>(*element))
            exitingInWindowFullscreen = videoElement->fullscreenMode() == HTMLMediaElementEnums::VideoFullscreenModeInWindow;
    }
#endif
    protectedPage()->fullScreenManager()->exitFullScreenForElement(element);
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (exitingInWindowFullscreen)
        clearVideoFullscreenMode(*dynamicDowncast<HTMLVideoElement>(*element), HTMLMediaElementEnums::VideoFullscreenModeInWindow);
#endif
}

#endif

#if PLATFORM(IOS_FAMILY)

FloatSize WebChromeClient::screenSize() const
{
    return protectedPage()->screenSize();
}

FloatSize WebChromeClient::availableScreenSize() const
{
    return protectedPage()->availableScreenSize();
}

FloatSize WebChromeClient::overrideScreenSize() const
{
    return protectedPage()->overrideScreenSize();
}

FloatSize WebChromeClient::overrideAvailableScreenSize() const
{
    return protectedPage()->overrideAvailableScreenSize();
}

#endif

FloatSize WebChromeClient::screenSizeForFingerprintingProtections(const LocalFrame& frame, FloatSize defaultSize) const
{
    return protectedPage()->screenSizeForFingerprintingProtections(frame, defaultSize);
}

void WebChromeClient::dispatchDisabledAdaptationsDidChange(const OptionSet<DisabledAdaptations>& disabledAdaptations) const
{
    protectedPage()->disabledAdaptationsDidChange(disabledAdaptations);
}

void WebChromeClient::dispatchViewportPropertiesDidChange(const ViewportArguments& viewportArguments) const
{
    protectedPage()->viewportPropertiesDidChange(viewportArguments);
}

void WebChromeClient::notifyScrollerThumbIsVisibleInRect(const IntRect& scrollerThumb)
{
    protectedPage()->send(Messages::WebPageProxy::NotifyScrollerThumbIsVisibleInRect(scrollerThumb));
}

void WebChromeClient::recommendedScrollbarStyleDidChange(ScrollbarStyle newStyle)
{
    protectedPage()->send(Messages::WebPageProxy::RecommendedScrollbarStyleDidChange(static_cast<int32_t>(newStyle)));
}

std::optional<ScrollbarOverlayStyle> WebChromeClient::preferredScrollbarOverlayStyle()
{
    return protectedPage()->scrollbarOverlayStyle();
}

Color WebChromeClient::underlayColor() const
{
    return protectedPage()->underlayColor();
}

void WebChromeClient::themeColorChanged() const
{
    protectedPage()->themeColorChanged();
}

void WebChromeClient::pageExtendedBackgroundColorDidChange() const
{
    protectedPage()->pageExtendedBackgroundColorDidChange();
}

void WebChromeClient::sampledPageTopColorChanged() const
{
    protectedPage()->sampledPageTopColorChanged();
}

#if ENABLE(APP_HIGHLIGHTS)
WebCore::HighlightVisibility WebChromeClient::appHighlightsVisiblility() const
{
    return page().appHighlightsVisiblility();
}
#endif

void WebChromeClient::wheelEventHandlersChanged(bool hasHandlers)
{
    protectedPage()->wheelEventHandlersChanged(hasHandlers);
}

String WebChromeClient::plugInStartLabelTitle(const String& mimeType) const
{
    return protectedPage()->injectedBundleUIClient().plugInStartLabelTitle(mimeType);
}

String WebChromeClient::plugInStartLabelSubtitle(const String& mimeType) const
{
    return protectedPage()->injectedBundleUIClient().plugInStartLabelSubtitle(mimeType);
}

String WebChromeClient::plugInExtraStyleSheet() const
{
    return protectedPage()->injectedBundleUIClient().plugInExtraStyleSheet();
}

String WebChromeClient::plugInExtraScript() const
{
    return protectedPage()->injectedBundleUIClient().plugInExtraScript();
}

void WebChromeClient::enableSuddenTermination()
{
    protectedPage()->send(Messages::WebProcessProxy::EnableSuddenTermination());
}

void WebChromeClient::disableSuddenTermination()
{
    protectedPage()->send(Messages::WebProcessProxy::DisableSuddenTermination());
}

void WebChromeClient::didAddHeaderLayer(GraphicsLayer& headerParent)
{
#if HAVE(RUBBER_BANDING)
    auto page = protectedPage();
    if (auto* banner = page->headerPageBanner())
        banner->didAddParentLayer(&headerParent);
#else
    UNUSED_PARAM(headerParent);
#endif
}

void WebChromeClient::didAddFooterLayer(GraphicsLayer& footerParent)
{
#if HAVE(RUBBER_BANDING)
    auto page = protectedPage();
    if (auto* banner = page->footerPageBanner())
        banner->didAddParentLayer(&footerParent);
#else
    UNUSED_PARAM(footerParent);
#endif
}

bool WebChromeClient::shouldUseTiledBackingForFrameView(const LocalFrameView& frameView) const
{
    return protectedPage()->drawingArea()->shouldUseTiledBackingForFrameView(frameView);
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
void WebChromeClient::isAnyAnimationAllowedToPlayDidChange(bool anyAnimationCanPlay)
{
    protectedPage()->isAnyAnimationAllowedToPlayDidChange(anyAnimationCanPlay);
}
#endif

void WebChromeClient::isPlayingMediaDidChange(MediaProducerMediaStateFlags state)
{
    protectedPage()->isPlayingMediaDidChange(state);
}

void WebChromeClient::handleAutoplayEvent(AutoplayEvent event, OptionSet<AutoplayEventFlags> flags)
{
    protectedPage()->send(Messages::WebPageProxy::HandleAutoplayEvent(event, flags));
}

void WebChromeClient::setTextIndicator(const WebCore::TextIndicatorData& indicatorData) const
{
    protectedPage()->setTextIndicator(indicatorData);
}

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)

void WebChromeClient::handleTelephoneNumberClick(const String& number, const IntPoint& point, const IntRect& rect)
{
    protectedPage()->handleTelephoneNumberClick(number, point, rect);
}

#endif

#if ENABLE(DATA_DETECTION)

void WebChromeClient::handleClickForDataDetectionResult(const DataDetectorElementInfo& info, const IntPoint& clickLocation)
{
    protectedPage()->handleClickForDataDetectionResult(info, clickLocation);
}

#endif

#if ENABLE(SERVICE_CONTROLS)

void WebChromeClient::handleSelectionServiceClick(FrameSelection& selection, const Vector<String>& telephoneNumbers, const IntPoint& point)
{
    protectedPage()->handleSelectionServiceClick(selection, telephoneNumbers, point);
}

bool WebChromeClient::hasRelevantSelectionServices(bool isTextOnly) const
{
    return (isTextOnly && WebProcess::singleton().hasSelectionServices()) || WebProcess::singleton().hasRichContentServices();
}

void WebChromeClient::handleImageServiceClick(const IntPoint& point, Image& image, HTMLImageElement& element)
{
    protectedPage()->handleImageServiceClick(point, image, element);
}

void WebChromeClient::handlePDFServiceClick(const IntPoint& point, HTMLAttachmentElement& element)
{
    protectedPage()->handlePDFServiceClick(point, element);
}

#endif

bool WebChromeClient::shouldDispatchFakeMouseMoveEvents() const
{
    return protectedPage()->shouldDispatchFakeMouseMoveEvents();
}

static RefPtr<API::Object> userDataFromJSONData(JSON::Value& value)
{
    switch (value.type()) {
    case JSON::Value::Type::Null:
        return API::String::createNull(); // FIXME: Encode nil properly.
    case JSON::Value::Type::Boolean:
        return API::Boolean::create(*value.asBoolean());
    case JSON::Value::Type::Double:
        return API::Double::create(*value.asDouble());
    case JSON::Value::Type::Integer:
        return API::Int64::create(*value.asInteger());
    case JSON::Value::Type::String:
        return API::String::create(value.asString());
    case JSON::Value::Type::Object: {
        auto result = API::Dictionary::create();
        for (auto [key, value] : *value.asObject())
            result->add(key, userDataFromJSONData(value));
        return result;
    }
    case JSON::Value::Type::Array: {
        auto array = value.asArray();
        Vector<RefPtr<API::Object>> result;
        for (auto& item : *value.asArray())
            result.append(userDataFromJSONData(item));
        return API::Array::create(WTFMove(result));
    }
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

void WebChromeClient::handleAutoFillButtonClick(HTMLInputElement& inputElement)
{
    RefPtr<API::Object> userData;

    // Notify the bundle client.
    auto nodeHandle = InjectedBundleNodeHandle::getOrCreate(inputElement);
    auto page = protectedPage();
    page->injectedBundleUIClient().didClickAutoFillButton(page, nodeHandle.get(), userData);

    if (!userData) {
        auto userInfo = inputElement.userInfo();
        if (!userInfo.isNull()) {
            if (auto data = JSON::Value::parseJSON(inputElement.userInfo()))
                userData = userDataFromJSONData(*data);
        }
    }

    // Notify the UIProcess.
    page->send(Messages::WebPageProxy::HandleAutoFillButtonClick(UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebChromeClient::inputElementDidResignStrongPasswordAppearance(HTMLInputElement& inputElement)
{
    RefPtr<API::Object> userData;

    // Notify the bundle client.
    auto nodeHandle = InjectedBundleNodeHandle::getOrCreate(inputElement);
    auto page = protectedPage();
    page->injectedBundleUIClient().didResignInputElementStrongPasswordAppearance(page, nodeHandle.get(), userData);

    // Notify the UIProcess.
    page->send(Messages::WebPageProxy::DidResignInputElementStrongPasswordAppearance { UserData { WebProcess::singleton().transformObjectsToHandles(userData.get()).get() } });
}

void WebChromeClient::performSwitchHapticFeedback()
{
    protectedPage()->send(Messages::WebPageProxy::PerformSwitchHapticFeedback());
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

void WebChromeClient::addPlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    protectedPage()->send(Messages::WebPageProxy::AddPlaybackTargetPickerClient(contextId));
}

void WebChromeClient::removePlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    protectedPage()->send(Messages::WebPageProxy::RemovePlaybackTargetPickerClient(contextId));
}

void WebChromeClient::showPlaybackTargetPicker(PlaybackTargetClientContextIdentifier contextId, const IntPoint& position, bool isVideo)
{
    auto page = protectedPage();
    RefPtr frameView = page->localMainFrameView();
    if (!frameView)
        return;

    FloatRect rect(frameView->contentsToRootView(frameView->windowToContents(position)), FloatSize());
    page->send(Messages::WebPageProxy::ShowPlaybackTargetPicker(contextId, rect, isVideo));
}

void WebChromeClient::playbackTargetPickerClientStateDidChange(PlaybackTargetClientContextIdentifier contextId, MediaProducerMediaStateFlags state)
{
    protectedPage()->send(Messages::WebPageProxy::PlaybackTargetPickerClientStateDidChange(contextId, state));
}

void WebChromeClient::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    protectedPage()->send(Messages::WebPageProxy::SetMockMediaPlaybackTargetPickerEnabled(enabled));
}

void WebChromeClient::setMockMediaPlaybackTargetPickerState(const String& name, MediaPlaybackTargetContext::MockState state)
{
    protectedPage()->send(Messages::WebPageProxy::SetMockMediaPlaybackTargetPickerState(name, state));
}

void WebChromeClient::mockMediaPlaybackTargetPickerDismissPopup()
{
    protectedPage()->send(Messages::WebPageProxy::MockMediaPlaybackTargetPickerDismissPopup());
}
#endif

void WebChromeClient::imageOrMediaDocumentSizeChanged(const IntSize& newSize)
{
    protectedPage()->imageOrMediaDocumentSizeChanged(newSize);
}

void WebChromeClient::didInvalidateDocumentMarkerRects()
{
    protectedPage()->findController().didInvalidateFindRects();
}

void WebChromeClient::hasStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, LocalFrame& frame, CompletionHandler<void(bool)>&& completionHandler)
{
    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    protectedPage()->hasStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), *webFrame, WTFMove(completionHandler));
}

void WebChromeClient::requestStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, LocalFrame& frame, StorageAccessScope scope, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler)
{
    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    protectedPage()->requestStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), *webFrame, scope, WTFMove(completionHandler));
}

void WebChromeClient::setLoginStatus(RegistrableDomain&& domain, IsLoggedIn loggedInStatus, CompletionHandler<void()>&& completionHandler)
{
    protectedPage()->setLoginStatus(WTFMove(domain), loggedInStatus, WTFMove(completionHandler));
}

void WebChromeClient::isLoggedIn(RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    protectedPage()->isLoggedIn(WTFMove(domain), WTFMove(completionHandler));
}

bool WebChromeClient::hasPageLevelStorageAccess(const WebCore::RegistrableDomain& topLevelDomain, const WebCore::RegistrableDomain& resourceDomain) const
{
    return protectedPage()->hasPageLevelStorageAccess(topLevelDomain, resourceDomain);
}

#if ENABLE(DEVICE_ORIENTATION)
void WebChromeClient::shouldAllowDeviceOrientationAndMotionAccess(LocalFrame& frame, bool mayPrompt, CompletionHandler<void(DeviceOrientationOrMotionPermissionState)>&& callback)
{
    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    protectedPage()->shouldAllowDeviceOrientationAndMotionAccess(webFrame->frameID(), webFrame->info(), mayPrompt, WTFMove(callback));
}
#endif

#if ENABLE(ORIENTATION_EVENTS) && !PLATFORM(IOS_FAMILY)
IntDegrees WebChromeClient::deviceOrientation() const
{
    notImplemented();
    return 0;
}
#endif

void WebChromeClient::configureLoggingChannel(const String& channelName, WTFLogChannelState state, WTFLogLevel level)
{
    protectedPage()->configureLoggingChannel(channelName, state, level);
}

bool WebChromeClient::userIsInteracting() const
{
    return protectedPage()->userIsInteracting();
}

void WebChromeClient::setUserIsInteracting(bool userIsInteracting)
{
    protectedPage()->setUserIsInteracting(userIsInteracting);
}

#if ENABLE(WEB_AUTHN)
void WebChromeClient::setMockWebAuthenticationConfiguration(const MockWebAuthenticationConfiguration& configuration)
{
    protectedPage()->send(Messages::WebPageProxy::SetMockWebAuthenticationConfiguration(configuration));
}
#endif

#if PLATFORM(PLAYSTATION)
void WebChromeClient::postAccessibilityNotification(WebCore::AccessibilityObject&, WebCore::AXObjectCache::AXNotification)
{
    notImplemented();
}

void WebChromeClient::postAccessibilityNodeTextChangeNotification(WebCore::AccessibilityObject*, WebCore::AXTextChange, unsigned, const String&)
{
    notImplemented();
}

void WebChromeClient::postAccessibilityFrameLoadingEventNotification(WebCore::AccessibilityObject*, WebCore::AXObjectCache::AXLoadingEvent)
{
    notImplemented();
}
#endif

void WebChromeClient::animationDidFinishForElement(const Element& element)
{
    protectedPage()->animationDidFinishForElement(element);
}

#if PLATFORM(MAC)
void WebChromeClient::changeUniversalAccessZoomFocus(const WebCore::IntRect& viewRect, const WebCore::IntRect& selectionRect)
{
    protectedPage()->send(Messages::WebPageProxy::ChangeUniversalAccessZoomFocus(viewRect, selectionRect));
}
#endif

#if ENABLE(IMAGE_ANALYSIS)

void WebChromeClient::requestTextRecognition(Element& element, TextRecognitionOptions&& options, CompletionHandler<void(RefPtr<Element>&&)>&& completion)
{
    protectedPage()->requestTextRecognition(element, WTFMove(options), WTFMove(completion));
}

#endif

std::pair<URL, DidFilterLinkDecoration> WebChromeClient::applyLinkDecorationFilteringWithResult(const URL& url, LinkDecorationFilteringTrigger trigger) const
{
    return protectedPage()->applyLinkDecorationFilteringWithResult(url, trigger);
}

URL WebChromeClient::allowedQueryParametersForAdvancedPrivacyProtections(const URL& url) const
{
    return protectedPage()->allowedQueryParametersForAdvancedPrivacyProtections(url);
}

void WebChromeClient::didAddOrRemoveViewportConstrainedObjects()
{
    protectedPage()->didAddOrRemoveViewportConstrainedObjects();
}

#if ENABLE(TEXT_AUTOSIZING)

void WebChromeClient::textAutosizingUsesIdempotentModeChanged()
{
    protectedPage()->textAutosizingUsesIdempotentModeChanged();
}

#endif

#if ENABLE(META_VIEWPORT)

double WebChromeClient::baseViewportLayoutSizeScaleFactor() const
{
    return protectedPage()->baseViewportLayoutSizeScaleFactor();
}

#endif

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

void WebChromeClient::showMediaControlsContextMenu(FloatRect&& targetFrame, Vector<MediaControlsContextMenuItem>&& items, CompletionHandler<void(MediaControlsContextMenuItem::ID)>&& completionHandler)
{
    protectedPage()->showMediaControlsContextMenu(WTFMove(targetFrame), WTFMove(items), WTFMove(completionHandler));
}

#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if ENABLE(WEBXR) && !USE(OPENXR)
void WebChromeClient::enumerateImmersiveXRDevices(CompletionHandler<void(const PlatformXR::Instance::DeviceList&)>&& completionHandler)
{
    protectedPage()->xrSystemProxy().enumerateImmersiveXRDevices(WTFMove(completionHandler));
}

void WebChromeClient::requestPermissionOnXRSessionFeatures(const SecurityOriginData& origin, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& consentRequired, const PlatformXR::Device::FeatureList& consentOptional, const PlatformXR::Device::FeatureList& requiredFeaturesRequested, const PlatformXR::Device::FeatureList& optionalFeaturesRequested,  CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler)
{
    protectedPage()->xrSystemProxy().requestPermissionOnSessionFeatures(origin, mode, granted, consentRequired, consentOptional, requiredFeaturesRequested, optionalFeaturesRequested, WTFMove(completionHandler));
}
#endif

#if ENABLE(APPLE_PAY_AMS_UI)

void WebChromeClient::startApplePayAMSUISession(const URL& originatingURL, const ApplePayAMSUIRequest& request, CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    protectedPage()->sendWithAsyncReply(Messages::WebPageProxy::StartApplePayAMSUISession(originatingURL, request), WTFMove(completionHandler));
}

void WebChromeClient::abortApplePayAMSUISession()
{
    protectedPage()->send(Messages::WebPageProxy::AbortApplePayAMSUISession());
}

#endif // ENABLE(APPLE_PAY_AMS_UI)

#if USE(SYSTEM_PREVIEW)
void WebChromeClient::beginSystemPreview(const URL& url, const SecurityOriginData& topOrigin, const SystemPreviewInfo& systemPreviewInfo, CompletionHandler<void()>&& completionHandler)
{
    protectedPage()->sendWithAsyncReply(Messages::WebPageProxy::BeginSystemPreview(WTFMove(url), topOrigin, WTFMove(systemPreviewInfo)), WTFMove(completionHandler));
}
#endif

void WebChromeClient::requestCookieConsent(CompletionHandler<void(CookieConsentDecisionResult)>&& completion)
{
    protectedPage()->sendWithAsyncReply(Messages::WebPageProxy::RequestCookieConsent(), WTFMove(completion));
}

bool WebChromeClient::isUsingUISideCompositing() const
{
#if PLATFORM(COCOA)
    return protectedPage()->drawingArea()->type() == DrawingAreaType::RemoteLayerTree;
#else
    return false;
#endif
}

bool WebChromeClient::isInStableState() const
{
#if PLATFORM(IOS_FAMILY)
    return protectedPage()->isInStableState();
#else
    // FIXME (255877): Implement this client hook on macOS.
    return true;
#endif
}

void WebChromeClient::didAdjustVisibilityWithSelectors(Vector<String>&& selectors)
{
    return protectedPage()->didAdjustVisibilityWithSelectors(WTFMove(selectors));
}

#if ENABLE(GAMEPAD)
void WebChromeClient::gamepadsRecentlyAccessed()
{
    protectedPage()->gamepadsRecentlyAccessed();
}
#endif

#if ENABLE(WRITING_TOOLS)

void WebChromeClient::proofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(const WebCore::WritingTools::TextSuggestion::ID& replacementID, WebCore::IntRect selectionBoundsInRootView)
{
    protectedPage()->proofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(replacementID, selectionBoundsInRootView);
}

void WebChromeClient::proofreadingSessionUpdateStateForSuggestionWithID(WritingTools::TextSuggestion::State state, const WritingTools::TextSuggestion::ID& replacementID)
{
    protectedPage()->proofreadingSessionUpdateStateForSuggestionWithID(state, replacementID);
}

void WebChromeClient::removeTextAnimationForAnimationID(const WTF::UUID& animationID)
{
    protectedPage()->removeTextAnimationForAnimationID(animationID);
}

void WebChromeClient::removeInitialTextAnimationForActiveWritingToolsSession()
{
    protectedPage()->removeInitialTextAnimationForActiveWritingToolsSession();
}

void WebChromeClient::addInitialTextAnimationForActiveWritingToolsSession()
{
    protectedPage()->addInitialTextAnimationForActiveWritingToolsSession();
}

void WebChromeClient::addSourceTextAnimationForActiveWritingToolsSession(const WTF::UUID& sourceAnimationUUID, const WTF::UUID& destinationAnimationUUID, bool finished, const CharacterRange& range, const String& string, CompletionHandler<void(WebCore::TextAnimationRunMode)>&& completionHandler)
{
    protectedPage()->addSourceTextAnimationForActiveWritingToolsSession(sourceAnimationUUID, destinationAnimationUUID, finished, range, string, WTFMove(completionHandler));
}

void WebChromeClient::addDestinationTextAnimationForActiveWritingToolsSession(const WTF::UUID& sourceAnimationUUID, const WTF::UUID& destinationAnimationUUID, const std::optional<CharacterRange>& range, const String& string)
{
    protectedPage()->addDestinationTextAnimationForActiveWritingToolsSession(sourceAnimationUUID, destinationAnimationUUID, range, string);
}

void WebChromeClient::saveSnapshotOfTextPlaceholderForAnimation(const WebCore::SimpleRange& placeholderRange)
{
    protectedPage()->saveSnapshotOfTextPlaceholderForAnimation(placeholderRange);
}

void WebChromeClient::clearAnimationsForActiveWritingToolsSession()
{
    protectedPage()->clearAnimationsForActiveWritingToolsSession();
}

#endif

void WebChromeClient::setIsInRedo(bool isInRedo)
{
    protectedPage()->setIsInRedo(isInRedo);
}

void WebChromeClient::hasActiveNowPlayingSessionChanged(bool hasActiveNowPlayingSession)
{
    protectedPage()->hasActiveNowPlayingSessionChanged(hasActiveNowPlayingSession);
}

#if ENABLE(GPU_PROCESS)
void WebChromeClient::getImageBufferResourceLimitsForTesting(CompletionHandler<void(std::optional<ImageBufferResourceLimits>)>&& callback) const
{
    protectedPage()->ensureRemoteRenderingBackendProxy().getImageBufferResourceLimitsForTesting(WTFMove(callback));
}
#endif

bool WebChromeClient::requiresScriptTelemetryForURL(const URL& url, const SecurityOrigin& topOrigin) const
{
    return WebProcess::singleton().requiresScriptTelemetryForURL(url, topOrigin);
}

void WebChromeClient::callAfterPendingSyntheticClick(CompletionHandler<void(SyntheticClickResult)>&& completion)
{
    protectedPage()->callAfterPendingSyntheticClick(WTFMove(completion));
}

void WebChromeClient::didSwallowClickEvent(const PlatformMouseEvent& event, Node& node)
{
    protectedPage()->didSwallowClickEvent(event, node);
}

} // namespace WebKit
