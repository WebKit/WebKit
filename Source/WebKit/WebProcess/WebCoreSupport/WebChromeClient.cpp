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
#include "APIInjectedBundleFormClient.h"
#include "APIInjectedBundlePageUIClient.h"
#include "APISecurityOrigin.h"
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
#include <WebCore/ApplicationCacheStorage.h>
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
#include "VideoFullscreenManager.h"
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
#endif

namespace WebKit {
using namespace WebCore;
using namespace HTMLNames;

WebChromeClient::WebChromeClient(WebPage& page)
    : m_page(page)
{
}

WebChromeClient::~WebChromeClient() = default;

void WebChromeClient::didInsertMenuElement(HTMLMenuElement& element)
{
    m_page.didInsertMenuElement(element);
}

void WebChromeClient::didRemoveMenuElement(HTMLMenuElement& element)
{
    m_page.didRemoveMenuElement(element);
}

void WebChromeClient::didInsertMenuItemElement(HTMLMenuItemElement& element)
{
    m_page.didInsertMenuItemElement(element);
}

void WebChromeClient::didRemoveMenuItemElement(HTMLMenuItemElement& element)
{
    m_page.didRemoveMenuItemElement(element);
}

void WebChromeClient::chromeDestroyed()
{
}

void WebChromeClient::setWindowRect(const FloatRect& windowFrame)
{
    m_page.sendSetWindowFrame(windowFrame);
}

FloatRect WebChromeClient::windowRect() const
{
#if PLATFORM(IOS_FAMILY)
    return FloatRect();
#else
#if PLATFORM(MAC)
    if (m_page.hasCachedWindowFrame())
        return m_page.windowFrameInUnflippedScreenCoordinates();
#endif

    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetWindowFrame(), m_page.identifier());
    auto [newWindowFrame] = sendResult.takeReplyOr(FloatRect { });
    return newWindowFrame;
#endif
}

FloatRect WebChromeClient::pageRect() const
{
    return FloatRect(FloatPoint(), m_page.size());
}

void WebChromeClient::focus()
{
    m_page.send(Messages::WebPageProxy::SetFocus(true));
}

void WebChromeClient::unfocus()
{
    m_page.send(Messages::WebPageProxy::SetFocus(false));
}

#if PLATFORM(COCOA)

void WebChromeClient::elementDidFocus(Element& element, const FocusOptions& options)
{
    m_page.elementDidFocus(element, options);
}

void WebChromeClient::elementDidRefocus(Element& element, const FocusOptions& options)
{
    m_page.elementDidRefocus(element, options);
}

void WebChromeClient::elementDidBlur(Element& element)
{
    m_page.elementDidBlur(element);
}

void WebChromeClient::focusedElementDidChangeInputMode(Element& element, InputMode mode)
{
    m_page.focusedElementDidChangeInputMode(element, mode);
}

void WebChromeClient::makeFirstResponder()
{
    m_page.send(Messages::WebPageProxy::MakeFirstResponder());
}

void WebChromeClient::assistiveTechnologyMakeFirstResponder()
{
    m_page.send(Messages::WebPageProxy::AssistiveTechnologyMakeFirstResponder());
}

#endif    

bool WebChromeClient::canTakeFocus(FocusDirection) const
{
    notImplemented();
    return true;
}

void WebChromeClient::takeFocus(FocusDirection direction)
{
    m_page.send(Messages::WebPageProxy::TakeFocus(direction));
}

void WebChromeClient::focusedElementChanged(Element* element)
{
    auto* inputElement = dynamicDowncast<HTMLInputElement>(element);
    if (!inputElement || !inputElement->isText())
        return;

    WebFrame* webFrame = WebFrame::fromCoreFrame(*element->document().frame());
    ASSERT(webFrame);
    m_page.injectedBundleFormClient().didFocusTextField(&m_page, *inputElement, webFrame);
}

void WebChromeClient::focusedFrameChanged(LocalFrame* frame)
{
    WebFrame* webFrame = frame ? WebFrame::fromCoreFrame(*frame) : nullptr;

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::FocusedFrameChanged(webFrame ? std::make_optional(webFrame->frameID()) : std::nullopt), m_page.identifier());
}

Page* WebChromeClient::createWindow(LocalFrame& frame, const WindowFeatures& windowFeatures, const NavigationAction& navigationAction)
{
#if ENABLE(FULLSCREEN_API)
    if (auto* document = frame.document())
        document->fullscreenManager().cancelFullscreen();
#endif

    auto& webProcess = WebProcess::singleton();

    auto& mouseEventData = navigationAction.mouseEventData();

    NavigationActionData navigationActionData {
        navigationAction.type(),
        modifiersForNavigationAction(navigationAction),
        mouseButton(navigationAction),
        syntheticClickType(navigationAction),
        webProcess.userGestureTokenIdentifier(navigationAction.userGestureToken()),
        navigationAction.userGestureToken() ? navigationAction.userGestureToken()->authorizationToken() : std::nullopt,
        m_page.canHandleRequest(navigationAction.resourceRequest()),
        navigationAction.shouldOpenExternalURLsPolicy(),
        navigationAction.downloadAttribute(),
        mouseEventData ? mouseEventData->locationInRootViewCoordinates : FloatPoint { },
        { }, /* redirectResponse */
        false, /* treatAsSameOriginNavigation */
        false, /* hasOpenedFrames */
        false, /* openedByDOMWithOpener */
        navigationAction.newFrameOpenerPolicy() == NewFrameOpenerPolicy::Allow, /* hasOpener */
        { }, /* requesterOrigin */
        std::nullopt, /* targetBackForwardItemIdentifier */
        std::nullopt, /* sourceBackForwardItemIdentifier */
        WebCore::LockHistory::No,
        WebCore::LockBackForwardList::No,
        { }, /* clientRedirectSourceForHistory */
        0, /* effectiveSandboxFlags */
        navigationAction.privateClickMeasurement(),
        { }, /* advancedPrivacyProtections */
        { }, /* originatorAdvancedPrivacyProtections */
#if PLATFORM(MAC) || HAVE(UIKIT_WITH_MOUSE_SUPPORT)
        std::nullopt, /* webHitTestResultData */
#endif
    };

    WebFrame* webFrame = WebFrame::fromCoreFrame(frame);

    auto sendResult = webProcess.parentProcessConnection()->sendSync(Messages::WebPageProxy::CreateNewPage(webFrame->info(), webFrame->page()->webPageProxyIdentifier(), navigationAction.resourceRequest(), windowFeatures, navigationActionData), m_page.identifier(), IPC::Timeout::infinity(), { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages, IPC::SendSyncOption::InformPlatformProcessWillSuspend });
    if (!sendResult.succeeded())
        return nullptr;

    auto [newPageID, parameters] = sendResult.takeReply();
    if (!newPageID)
        return nullptr;
    ASSERT(parameters);

    parameters->oldPageID = m_page.identifier();

    webProcess.createWebPage(*newPageID, WTFMove(*parameters));
    return webProcess.webPage(*newPageID)->corePage();
}

bool WebChromeClient::testProcessIncomingSyncMessagesWhenWaitingForSyncReply()
{
    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;

    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::TestProcessIncomingSyncMessagesWhenWaitingForSyncReply(m_page.webPageProxyIdentifier()), 0);
    auto [handled] = sendResult.takeReplyOr(false);
    return handled;
}

void WebChromeClient::show()
{
    m_page.show();
}

bool WebChromeClient::canRunModal() const
{
    return m_page.canRunModal();
}

void WebChromeClient::runModal()
{
    m_page.runModal();
}

void WebChromeClient::reportProcessCPUTime(Seconds cpuTime, ActivityStateForCPUSampling activityState)
{
    WebProcess::singleton().send(Messages::WebProcessPool::ReportWebContentCPUTime(cpuTime, static_cast<uint64_t>(activityState)), 0);
}

void WebChromeClient::setToolbarsVisible(bool toolbarsAreVisible)
{
    m_page.send(Messages::WebPageProxy::SetToolbarsAreVisible(toolbarsAreVisible));
}

bool WebChromeClient::toolbarsVisible() const
{
    API::InjectedBundle::PageUIClient::UIElementVisibility toolbarsVisibility = m_page.injectedBundleUIClient().toolbarsAreVisible(&m_page);
    if (toolbarsVisibility != API::InjectedBundle::PageUIClient::UIElementVisibility::Unknown)
        return toolbarsVisibility == API::InjectedBundle::PageUIClient::UIElementVisibility::Visible;
    
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetToolbarsAreVisible(), m_page.identifier());
    auto [toolbarsAreVisible] = sendResult.takeReplyOr(true);
    return toolbarsAreVisible;
}

void WebChromeClient::setStatusbarVisible(bool statusBarIsVisible)
{
    m_page.send(Messages::WebPageProxy::SetStatusBarIsVisible(statusBarIsVisible));
}

bool WebChromeClient::statusbarVisible() const
{
    API::InjectedBundle::PageUIClient::UIElementVisibility statusbarVisibility = m_page.injectedBundleUIClient().statusBarIsVisible(&m_page);
    if (statusbarVisibility != API::InjectedBundle::PageUIClient::UIElementVisibility::Unknown)
        return statusbarVisibility == API::InjectedBundle::PageUIClient::UIElementVisibility::Visible;

    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetStatusBarIsVisible(), m_page.identifier());
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
    m_page.send(Messages::WebPageProxy::SetMenuBarIsVisible(menuBarVisible));
}

bool WebChromeClient::menubarVisible() const
{
    API::InjectedBundle::PageUIClient::UIElementVisibility menubarVisibility = m_page.injectedBundleUIClient().menuBarIsVisible(&m_page);
    if (menubarVisibility != API::InjectedBundle::PageUIClient::UIElementVisibility::Unknown)
        return menubarVisibility == API::InjectedBundle::PageUIClient::UIElementVisibility::Visible;
    
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetMenuBarIsVisible(), m_page.identifier());
    auto [menuBarIsVisible] = sendResult.takeReplyOr(true);
    return menuBarIsVisible;
}

void WebChromeClient::setResizable(bool resizable)
{
    m_page.send(Messages::WebPageProxy::SetIsResizable(resizable));
}

void WebChromeClient::addMessageToConsole(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, unsigned columnNumber, const String& sourceID)
{
    // Notify the bundle client.
    m_page.injectedBundleUIClient().willAddMessageToConsole(&m_page, source, level, message, lineNumber, columnNumber, sourceID);
}

void WebChromeClient::addMessageWithArgumentsToConsole(MessageSource source, MessageLevel level, const String& message, std::span<const String> messageArguments, unsigned lineNumber, unsigned columnNumber, const String& sourceID)
{
    m_page.injectedBundleUIClient().willAddMessageWithArgumentsToConsole(&m_page, source, level, message, messageArguments, lineNumber, columnNumber, sourceID);
}

bool WebChromeClient::canRunBeforeUnloadConfirmPanel()
{
    return m_page.canRunBeforeUnloadConfirmPanel();
}

bool WebChromeClient::runBeforeUnloadConfirmPanel(const String& message, LocalFrame& frame)
{
    WebFrame* webFrame = WebFrame::fromCoreFrame(frame);

    HangDetectionDisabler hangDetectionDisabler;

    auto sendResult = m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::RunBeforeUnloadConfirmPanel(webFrame->frameID(), webFrame->info(), message), IPC::SendSyncOption::InformPlatformProcessWillSuspend);
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

    m_page.corePage()->setGroupName(String());

    auto& frame = m_page.mainWebFrame();
    if (auto* coreFrame = frame.coreLocalFrame())
        coreFrame->loader().stopForUserCancel();

    m_page.sendClose();
}

static bool shouldSuppressJavaScriptDialogs(LocalFrame& frame)
{
    if (frame.loader().opener() && frame.loader().stateMachine().isDisplayingInitialEmptyDocument() && frame.loader().provisionalDocumentLoader())
        return true;

    return false;
}

void WebChromeClient::runJavaScriptAlert(LocalFrame& frame, const String& alertText)
{
    if (shouldSuppressJavaScriptDialogs(frame))
        return;

    WebFrame* webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);

    // Notify the bundle client.
    m_page.injectedBundleUIClient().willRunJavaScriptAlert(&m_page, alertText, webFrame);
    m_page.prepareToRunModalJavaScriptDialog();

    HangDetectionDisabler hangDetectionDisabler;
    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;

    m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::RunJavaScriptAlert(webFrame->frameID(), webFrame->info(), alertText), { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages, IPC::SendSyncOption::InformPlatformProcessWillSuspend });
}

bool WebChromeClient::runJavaScriptConfirm(LocalFrame& frame, const String& message)
{
    if (shouldSuppressJavaScriptDialogs(frame))
        return false;

    WebFrame* webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);

    // Notify the bundle client.
    m_page.injectedBundleUIClient().willRunJavaScriptConfirm(&m_page, message, webFrame);
    m_page.prepareToRunModalJavaScriptDialog();

    HangDetectionDisabler hangDetectionDisabler;
    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;

    auto sendResult = m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::RunJavaScriptConfirm(webFrame->frameID(), webFrame->info(), message), { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages, IPC::SendSyncOption::InformPlatformProcessWillSuspend });
    auto [result] = sendResult.takeReplyOr(false);
    return result;
}

bool WebChromeClient::runJavaScriptPrompt(LocalFrame& frame, const String& message, const String& defaultValue, String& result)
{
    if (shouldSuppressJavaScriptDialogs(frame))
        return false;

    WebFrame* webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);

    // Notify the bundle client.
    m_page.injectedBundleUIClient().willRunJavaScriptPrompt(&m_page, message, defaultValue, webFrame);
    m_page.prepareToRunModalJavaScriptDialog();

    HangDetectionDisabler hangDetectionDisabler;
    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;

    auto sendResult = m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::RunJavaScriptPrompt(webFrame->frameID(), webFrame->info(), message, defaultValue), { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages, IPC::SendSyncOption::InformPlatformProcessWillSuspend });
    if (!sendResult.succeeded())
        return false;

    std::tie(result) = sendResult.takeReply();
    return !result.isNull();
}

void WebChromeClient::setStatusbarText(const String& statusbarText)
{
    // Notify the bundle client.
    m_page.injectedBundleUIClient().willSetStatusbarText(&m_page, statusbarText);

    m_page.send(Messages::WebPageProxy::SetStatusText(statusbarText));
}

KeyboardUIMode WebChromeClient::keyboardUIMode()
{
    return m_page.keyboardUIMode();
}

bool WebChromeClient::hoverSupportedByPrimaryPointingDevice() const
{
    return m_page.hoverSupportedByPrimaryPointingDevice();
}

bool WebChromeClient::hoverSupportedByAnyAvailablePointingDevice() const
{
    return m_page.hoverSupportedByAnyAvailablePointingDevice();
}

std::optional<PointerCharacteristics> WebChromeClient::pointerCharacteristicsOfPrimaryPointingDevice() const
{
    return m_page.pointerCharacteristicsOfPrimaryPointingDevice();
}

OptionSet<PointerCharacteristics> WebChromeClient::pointerCharacteristicsOfAllAvailablePointingDevices() const
{
    return m_page.pointerCharacteristicsOfAllAvailablePointingDevices();
}

#if ENABLE(POINTER_LOCK)

bool WebChromeClient::requestPointerLock()
{
    m_page.send(Messages::WebPageProxy::RequestPointerLock());
    return true;
}

void WebChromeClient::requestPointerUnlock()
{
    m_page.send(Messages::WebPageProxy::RequestPointerUnlock());
}

#endif

void WebChromeClient::invalidateRootView(const IntRect&)
{
    // Do nothing here, there's no concept of invalidating the window in the web process.
}

void WebChromeClient::invalidateContentsAndRootView(const IntRect& rect)
{
    auto* localMainFrame = dynamicDowncast<LocalFrame>(m_page.corePage()->mainFrame());
    if (!localMainFrame)
        return;

    if (Document* document = localMainFrame->document()) {
        if (document->printing())
            return;
    }

    m_page.drawingArea()->setNeedsDisplayInRect(rect);
}

void WebChromeClient::invalidateContentsForSlowScroll(const IntRect& rect)
{
    auto* localMainFrame = dynamicDowncast<LocalFrame>(m_page.corePage()->mainFrame());
    if (!localMainFrame)
        return;

    if (Document* document = localMainFrame->document()) {
        if (document->printing())
            return;
    }

    m_page.pageDidScroll();
#if USE(COORDINATED_GRAPHICS)
    auto* frameView = m_page.localMainFrameView();
    if (frameView && frameView->delegatesScrolling()) {
        m_page.drawingArea()->scroll(rect, IntSize());
        return;
    }
#endif
    m_page.drawingArea()->setNeedsDisplayInRect(rect);
}

void WebChromeClient::scroll(const IntSize& scrollDelta, const IntRect& scrollRect, const IntRect& clipRect)
{
    m_page.pageDidScroll();
    m_page.drawingArea()->scroll(intersection(scrollRect, clipRect), scrollDelta);
}

IntPoint WebChromeClient::screenToRootView(const IntPoint& point) const
{
    return m_page.screenToRootView(point);
}

IntRect WebChromeClient::rootViewToScreen(const IntRect& rect) const
{
    return m_page.rootViewToScreen(rect);
}
    
IntPoint WebChromeClient::accessibilityScreenToRootView(const IntPoint& point) const
{
    return m_page.accessibilityScreenToRootView(point);
}

IntRect WebChromeClient::rootViewToAccessibilityScreen(const IntRect& rect) const
{
    return m_page.rootViewToAccessibilityScreen(rect);
}

void WebChromeClient::didFinishLoadingImageForElement(HTMLImageElement& element)
{
    m_page.didFinishLoadingImageForElement(element);
}

PlatformPageClient WebChromeClient::platformPageClient() const
{
    notImplemented();
    return 0;
}

void WebChromeClient::intrinsicContentsSizeChanged(const IntSize& size) const
{
    m_page.scheduleIntrinsicContentSizeUpdate(size);
}

void WebChromeClient::contentsSizeChanged(LocalFrame& frame, const IntSize& size) const
{
    auto* frameView = frame.view();

    if (&frame.page()->mainFrame() != &frame)
        return;

    m_page.send(Messages::WebPageProxy::DidChangeContentSize(size));

    m_page.drawingArea()->mainFrameContentSizeChanged(frame.frameID(), size);

    if (frameView && !frameView->delegatesScrollingToNativeView())  {
        bool hasHorizontalScrollbar = frameView->horizontalScrollbar();
        bool hasVerticalScrollbar = frameView->verticalScrollbar();

        if (hasHorizontalScrollbar != m_cachedMainFrameHasHorizontalScrollbar || hasVerticalScrollbar != m_cachedMainFrameHasVerticalScrollbar) {
            m_page.send(Messages::WebPageProxy::DidChangeScrollbarsForMainFrame(hasHorizontalScrollbar, hasVerticalScrollbar));

            m_cachedMainFrameHasHorizontalScrollbar = hasHorizontalScrollbar;
            m_cachedMainFrameHasVerticalScrollbar = hasVerticalScrollbar;
        }
    }
}

void WebChromeClient::scrollMainFrameToRevealRect(const IntRect& rect) const
{
    m_page.send(Messages::WebPageProxy::RequestScrollToRect(rect, rect.center()));
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
    m_page.injectedBundleUIClient().mouseDidMoveOverElement(&m_page, hitTestResult, wkModifiers, userData);

    // Notify the UIProcess.
    WebHitTestResultData webHitTestResultData(hitTestResult, toolTip);
    webHitTestResultData.elementBoundingBox = webHitTestResultData.elementBoundingBox.toRectWithExtentsClippedToNumericLimits();
    m_page.send(Messages::WebPageProxy::MouseDidMoveOverElement(webHitTestResultData, wkModifiers, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

static constexpr unsigned maxTitleLength = 1000; // Closest power of 10 above the W3C recommendation for Title length.

void WebChromeClient::print(LocalFrame& frame, const StringWithDirection& title)
{
    WebFrame* webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);

    WebCore::FloatSize pdfFirstPageSize;
#if ENABLE(PDFKIT_PLUGIN)
    if (auto* pluginView = WebPage::pluginViewForFrame(&frame))
        pdfFirstPageSize = pluginView->pdfDocumentSizeForPrinting();
#endif

    auto truncatedTitle = truncateFromEnd(title, maxTitleLength);

    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;
    m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::PrintFrame(webFrame->frameID(), truncatedTitle.string, pdfFirstPageSize), IPC::SendSyncOption::InformPlatformProcessWillSuspend);
}

void WebChromeClient::reachedMaxAppCacheSize(int64_t)
{
    notImplemented();
}

void WebChromeClient::reachedApplicationCacheOriginQuota(SecurityOrigin& origin, int64_t totalBytesNeeded)
{
    auto securityOrigin = API::SecurityOrigin::createFromString(origin.toString());
    if (m_page.injectedBundleUIClient().didReachApplicationCacheOriginQuota(&m_page, securityOrigin.ptr(), totalBytesNeeded))
        return;

    auto& cacheStorage = m_page.corePage()->applicationCacheStorage();
    int64_t currentQuota = 0;
    if (!cacheStorage.calculateQuotaForOrigin(origin, currentQuota))
        return;

    auto sendResult = m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::ReachedApplicationCacheOriginQuota(origin.data().databaseIdentifier(), currentQuota, totalBytesNeeded), IPC::SendSyncOption::InformPlatformProcessWillSuspend);
    auto [newQuota] = sendResult.takeReplyOr(0);

    cacheStorage.storeUpdatedQuotaForOrigin(&origin, newQuota);
}

#if ENABLE(INPUT_TYPE_COLOR)

std::unique_ptr<ColorChooser> WebChromeClient::createColorChooser(ColorChooserClient& client, const Color& initialColor)
{
    return makeUnique<WebColorChooser>(&m_page, &client, initialColor);
}

#endif

#if ENABLE(DATALIST_ELEMENT)

std::unique_ptr<DataListSuggestionPicker> WebChromeClient::createDataListSuggestionPicker(DataListSuggestionsClient& client)
{
    return makeUnique<WebDataListSuggestionPicker>(m_page, client);
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

std::unique_ptr<DateTimeChooser> WebChromeClient::createDateTimeChooser(DateTimeChooserClient& client)
{
    return makeUnique<WebDateTimeChooser>(m_page, client);
}

#endif

void WebChromeClient::runOpenPanel(LocalFrame& frame, FileChooser& fileChooser)
{
    if (m_page.activeOpenPanelResultListener())
        return;

    m_page.setActiveOpenPanelResultListener(WebOpenPanelResultListener::create(m_page, fileChooser));

    auto* webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    m_page.send(Messages::WebPageProxy::RunOpenPanel(webFrame->frameID(), webFrame->info(), fileChooser.settings()));
}
    
void WebChromeClient::showShareSheet(ShareDataWithParsedURL& shareData, CompletionHandler<void(bool)>&& callback)
{
    m_page.showShareSheet(shareData, WTFMove(callback));
}

void WebChromeClient::showContactPicker(const WebCore::ContactsRequestData& requestData, WTF::CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&& callback)
{
    m_page.showContactPicker(requestData, WTFMove(callback));
}

void WebChromeClient::loadIconForFiles(const Vector<String>& filenames, FileIconLoader& loader)
{
    loader.iconLoaded(createIconForFiles(filenames));
}

void WebChromeClient::setCursor(const Cursor& cursor)
{
    m_page.send(Messages::WebPageProxy::SetCursor(cursor));
}

void WebChromeClient::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    m_page.send(Messages::WebPageProxy::SetCursorHiddenUntilMouseMoves(hiddenUntilMouseMoves));
}

#if !PLATFORM(COCOA)

RefPtr<Icon> WebChromeClient::createIconForFiles(const Vector<String>& filenames)
{
    return Icon::createIconForFiles(filenames);
}

#endif

void WebChromeClient::didAssociateFormControls(const Vector<RefPtr<Element>>& elements, WebCore::LocalFrame& frame)
{
    WebFrame* webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    return m_page.injectedBundleFormClient().didAssociateFormControls(&m_page, elements, webFrame);
}

bool WebChromeClient::shouldNotifyOnFormChanges()
{
    return m_page.injectedBundleFormClient().shouldNotifyOnFormChanges(&m_page);
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
    return WebPopupMenu::create(&m_page, &client);
}

RefPtr<SearchPopupMenu> WebChromeClient::createSearchPopupMenu(PopupMenuClient& client) const
{
    return WebSearchPopupMenu::create(&m_page, &client);
}

GraphicsLayerFactory* WebChromeClient::graphicsLayerFactory() const
{
    if (auto drawingArea = m_page.drawingArea())
        return drawingArea->graphicsLayerFactory();
    return nullptr;
}

WebCore::DisplayRefreshMonitorFactory* WebChromeClient::displayRefreshMonitorFactory() const
{
    return m_page.drawingArea();
}

#if ENABLE(GPU_PROCESS)
RefPtr<ImageBuffer> WebChromeClient::createImageBuffer(const FloatSize& size, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, bool avoidBackendSizeCheck) const
{
    if (!WebProcess::singleton().shouldUseRemoteRenderingFor(purpose)) {
        if (purpose != RenderingPurpose::ShareableSnapshot && purpose != RenderingPurpose::ShareableLocalSnapshot)
            return nullptr;

        return ImageBuffer::create<ImageBufferShareableBitmapBackend>(size, resolutionScale, colorSpace, PixelFormat::BGRA8, purpose, { });
    }

    return m_page.ensureRemoteRenderingBackendProxy().createImageBuffer(size, renderingMode, purpose, resolutionScale, colorSpace, pixelFormat, avoidBackendSizeCheck);
}

RefPtr<ImageBuffer> WebChromeClient::sinkIntoImageBuffer(std::unique_ptr<SerializedImageBuffer> imageBuffer)
{
    if (!is<RemoteSerializedImageBufferProxy>(imageBuffer))
        return SerializedImageBuffer::sinkIntoImageBuffer(WTFMove(imageBuffer));
    auto remote = std::unique_ptr<RemoteSerializedImageBufferProxy>(static_cast<RemoteSerializedImageBufferProxy*>(imageBuffer.release()));
    return RemoteSerializedImageBufferProxy::sinkIntoImageBuffer(WTFMove(remote), m_page.ensureRemoteRenderingBackendProxy());
}
#endif

std::unique_ptr<WebCore::WorkerClient> WebChromeClient::createWorkerClient(SerialFunctionDispatcher& dispatcher)
{
    return makeUnique<WebWorkerClient>(&m_page, dispatcher);
}

#if ENABLE(WEBGL)
RefPtr<GraphicsContextGL> WebChromeClient::createGraphicsContextGL(const GraphicsContextGLAttributes& attributes) const
{
#if ENABLE(GPU_PROCESS)
    if (WebProcess::singleton().shouldUseRemoteRenderingForWebGL())
        return RemoteGraphicsContextGLProxy::create(WebProcess::singleton().ensureGPUProcessConnection().connection(), attributes, m_page.ensureRemoteRenderingBackendProxy()
#if ENABLE(VIDEO)
            , WebProcess::singleton().ensureGPUProcessConnection().videoFrameObjectHeapProxy()
#endif
        );
#endif
    return WebCore::createWebProcessGraphicsContextGL(attributes);
}
#endif

RefPtr<WebCore::WebGPU::GPU> WebChromeClient::createGPUForWebGPU() const
{
#if ENABLE(GPU_PROCESS)
    return RemoteGPUProxy::create(WebProcess::singleton().ensureGPUProcessConnection(), WebGPU::DowncastConvertToBackingContext::create(), WebGPUIdentifier::generate(), m_page.ensureRemoteRenderingBackendProxy().ensureBackendCreated());
#elif HAVE(WEBGPU_IMPLEMENTATION)
    return WebCore::WebGPU::create([](WebCore::WebGPU::WorkItem&& workItem) {
        callOnMainRunLoop(WTFMove(workItem));
    });
#else
    return nullptr;
#endif
}

RefPtr<WebCore::ShapeDetection::BarcodeDetector> WebChromeClient::createBarcodeDetector(const WebCore::ShapeDetection::BarcodeDetectorOptions& barcodeDetectorOptions) const
{
#if ENABLE(GPU_PROCESS)
    auto& remoteRenderingBackendProxy = m_page.ensureRemoteRenderingBackendProxy();
    return ShapeDetection::RemoteBarcodeDetectorProxy::create(remoteRenderingBackendProxy.streamConnection(), remoteRenderingBackendProxy.renderingBackendIdentifier(), ShapeDetectionIdentifier::generate(), barcodeDetectorOptions);
#elif HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    return WebCore::ShapeDetection::BarcodeDetectorImpl::create(barcodeDetectorOptions);
#else
    return nullptr;
#endif
}

void WebChromeClient::getBarcodeDetectorSupportedFormats(CompletionHandler<void(Vector<WebCore::ShapeDetection::BarcodeFormat>&&)>&& completionHandler) const
{
#if ENABLE(GPU_PROCESS)
    auto& remoteRenderingBackendProxy = m_page.ensureRemoteRenderingBackendProxy();
    ShapeDetection::RemoteBarcodeDetectorProxy::getSupportedFormats(remoteRenderingBackendProxy.streamConnection(), remoteRenderingBackendProxy.renderingBackendIdentifier(), WTFMove(completionHandler));
#elif HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    WebCore::ShapeDetection::BarcodeDetectorImpl::getSupportedFormats(WTFMove(completionHandler));
#else
    completionHandler({ });
#endif
}

RefPtr<WebCore::ShapeDetection::FaceDetector> WebChromeClient::createFaceDetector(const WebCore::ShapeDetection::FaceDetectorOptions& faceDetectorOptions) const
{
#if ENABLE(GPU_PROCESS)
    auto& remoteRenderingBackendProxy = m_page.ensureRemoteRenderingBackendProxy();
    return ShapeDetection::RemoteFaceDetectorProxy::create(remoteRenderingBackendProxy.streamConnection(), remoteRenderingBackendProxy.renderingBackendIdentifier(), ShapeDetectionIdentifier::generate(), faceDetectorOptions);
#elif HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    return WebCore::ShapeDetection::FaceDetectorImpl::create(faceDetectorOptions);
#else
    return nullptr;
#endif
}

RefPtr<WebCore::ShapeDetection::TextDetector> WebChromeClient::createTextDetector() const
{
#if ENABLE(GPU_PROCESS)
    auto& remoteRenderingBackendProxy = m_page.ensureRemoteRenderingBackendProxy();
    return ShapeDetection::RemoteTextDetectorProxy::create(remoteRenderingBackendProxy.streamConnection(), remoteRenderingBackendProxy.renderingBackendIdentifier(), ShapeDetectionIdentifier::generate());
#elif HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    return WebCore::ShapeDetection::TextDetectorImpl::create();
#else
    return nullptr;
#endif
}

void WebChromeClient::attachRootGraphicsLayer(LocalFrame& frame, GraphicsLayer* layer)
{
    if (layer)
        m_page.enterAcceleratedCompositingMode(frame, layer);
    else
        m_page.exitAcceleratedCompositingMode(frame);
}

void WebChromeClient::attachViewOverlayGraphicsLayer(GraphicsLayer* graphicsLayer)
{
    auto* drawingArea = m_page.drawingArea();
    if (!drawingArea)
        return;

    // FIXME: Support view overlays in iframe processes if needed.
    drawingArea->attachViewOverlayGraphicsLayer(m_page.mainWebFrame().frameID(), graphicsLayer);
}

void WebChromeClient::setNeedsOneShotDrawingSynchronization()
{
    notImplemented();
}

bool WebChromeClient::shouldTriggerRenderingUpdate(unsigned rescheduledRenderingUpdateCount) const
{
    return m_page.shouldTriggerRenderingUpdate(rescheduledRenderingUpdateCount);
}

void WebChromeClient::triggerRenderingUpdate()
{
    if (m_page.drawingArea())
        m_page.drawingArea()->triggerRenderingUpdate();
}

bool WebChromeClient::scheduleRenderingUpdate()
{
    if (m_page.drawingArea())
        return m_page.drawingArea()->scheduleRenderingUpdate();
    return false;
}

void WebChromeClient::renderingUpdateFramesPerSecondChanged()
{
    if (m_page.drawingArea())
        m_page.drawingArea()->renderingUpdateFramesPerSecondChanged();
}

unsigned WebChromeClient::remoteImagesCountForTesting() const
{
    return m_page.remoteImagesCountForTesting();
}

void WebChromeClient::contentRuleListNotification(const URL& url, const ContentRuleListResults& results)
{
#if ENABLE(CONTENT_EXTENSIONS)
    m_page.send(Messages::WebPageProxy::ContentRuleListNotification(url, results));
#endif
}

bool WebChromeClient::layerTreeStateIsFrozen() const
{
    if (m_page.drawingArea())
        return m_page.drawingArea()->layerTreeStateIsFrozen();

    return false;
}

#if ENABLE(ASYNC_SCROLLING)

RefPtr<WebCore::ScrollingCoordinator> WebChromeClient::createScrollingCoordinator(Page& page) const
{
    ASSERT_UNUSED(page, m_page.corePage() == &page);
#if PLATFORM(COCOA)
    switch (m_page.drawingArea()->type()) {
#if PLATFORM(MAC)
    case DrawingAreaType::TiledCoreAnimation:
        return TiledCoreAnimationScrollingCoordinator::create(&m_page);
#endif
    case DrawingAreaType::RemoteLayerTree:
        return RemoteScrollingCoordinator::create(&m_page);
    }
#endif
    return nullptr;
}

#endif

#if PLATFORM(MAC)
std::unique_ptr<ScrollbarsController> WebChromeClient::createScrollbarsController(Page& page, ScrollableArea& area) const
{
    ASSERT_UNUSED(page, m_page.corePage() == &page);
    
    if (area.mockScrollbarsControllerEnabled())
        return nullptr;
    
    switch (m_page.drawingArea()->type()) {
    case DrawingAreaType::RemoteLayerTree:
        return makeUnique<RemoteScrollbarsController>(area, page.scrollingCoordinator());
    default:
        return nullptr;
    }
    return nullptr;
}
#endif


#if ENABLE(VIDEO_PRESENTATION_MODE)

void WebChromeClient::prepareForVideoFullscreen()
{
    m_page.videoFullscreenManager();
}

bool WebChromeClient::canEnterVideoFullscreen(HTMLMediaElementEnums::VideoFullscreenMode mode) const
{
    return m_page.videoFullscreenManager().canEnterVideoFullscreen(mode);
}

bool WebChromeClient::supportsVideoFullscreen(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    return m_page.videoFullscreenManager().supportsVideoFullscreen(mode);
}

bool WebChromeClient::supportsVideoFullscreenStandby()
{
    return m_page.videoFullscreenManager().supportsVideoFullscreenStandby();
}

void WebChromeClient::setMockVideoPresentationModeEnabled(bool enabled)
{
    m_page.send(Messages::WebPageProxy::SetMockVideoPresentationModeEnabled(enabled));
}

void WebChromeClient::enterVideoFullscreenForVideoElement(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode mode, bool standby)
{
#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
    ASSERT(standby || mode != HTMLMediaElementEnums::VideoFullscreenModeNone);
#else
    ASSERT(mode != HTMLMediaElementEnums::VideoFullscreenModeNone);
#endif
    m_page.videoFullscreenManager().enterVideoFullscreenForVideoElement(videoElement, mode, standby);
}

void WebChromeClient::exitVideoFullscreenForVideoElement(HTMLVideoElement& videoElement, CompletionHandler<void(bool)>&& completionHandler)
{
    m_page.videoFullscreenManager().exitVideoFullscreenForVideoElement(videoElement, WTFMove(completionHandler));
}

void WebChromeClient::setUpPlaybackControlsManager(HTMLMediaElement& mediaElement)
{
    m_page.playbackSessionManager().setUpPlaybackControlsManager(mediaElement);
}

void WebChromeClient::clearPlaybackControlsManager()
{
    m_page.playbackSessionManager().clearPlaybackControlsManager();
}

void WebChromeClient::playbackControlsMediaEngineChanged()
{
    m_page.playbackSessionManager().mediaEngineChanged();
}

#endif

#if ENABLE(MEDIA_USAGE)
void WebChromeClient::addMediaUsageManagerSession(MediaSessionIdentifier identifier, const String& bundleIdentifier, const URL& pageURL)
{
    m_page.addMediaUsageManagerSession(identifier, bundleIdentifier, pageURL);
}

void WebChromeClient::updateMediaUsageManagerSessionState(MediaSessionIdentifier identifier, const MediaUsageInfo& usage)
{
    m_page.updateMediaUsageManagerSessionState(identifier, usage);
}

void WebChromeClient::removeMediaUsageManagerSession(MediaSessionIdentifier identifier)
{
    m_page.removeMediaUsageManagerSession(identifier);
}
#endif // ENABLE(MEDIA_USAGE)

#if ENABLE(VIDEO_PRESENTATION_MODE)

void WebChromeClient::exitVideoFullscreenToModeWithoutAnimation(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode targetMode)
{
    m_page.videoFullscreenManager().exitVideoFullscreenToModeWithoutAnimation(videoElement, targetMode);
}

#endif

#if ENABLE(FULLSCREEN_API)

bool WebChromeClient::supportsFullScreenForElement(const Element&, bool withKeyboard)
{
    return m_page.fullScreenManager()->supportsFullScreen(withKeyboard);
}

void WebChromeClient::enterFullScreenForElement(Element& element)
{
    m_page.fullScreenManager()->enterFullScreenForElement(&element);
}

void WebChromeClient::exitFullScreenForElement(Element* element)
{
    m_page.fullScreenManager()->exitFullScreenForElement(element);
}

#endif

#if PLATFORM(IOS_FAMILY)

FloatSize WebChromeClient::screenSize() const
{
    return m_page.screenSize();
}

FloatSize WebChromeClient::availableScreenSize() const
{
    return m_page.availableScreenSize();
}

FloatSize WebChromeClient::overrideScreenSize() const
{
    return m_page.overrideScreenSize();
}

#endif

FloatSize WebChromeClient::screenSizeForFingerprintingProtections(const LocalFrame& frame, FloatSize defaultSize) const
{
    return m_page.screenSizeForFingerprintingProtections(frame, defaultSize);
}

void WebChromeClient::dispatchDisabledAdaptationsDidChange(const OptionSet<DisabledAdaptations>& disabledAdaptations) const
{
    m_page.disabledAdaptationsDidChange(disabledAdaptations);
}

void WebChromeClient::dispatchViewportPropertiesDidChange(const ViewportArguments& viewportArguments) const
{
    m_page.viewportPropertiesDidChange(viewportArguments);
}

void WebChromeClient::notifyScrollerThumbIsVisibleInRect(const IntRect& scrollerThumb)
{
    m_page.send(Messages::WebPageProxy::NotifyScrollerThumbIsVisibleInRect(scrollerThumb));
}

void WebChromeClient::recommendedScrollbarStyleDidChange(ScrollbarStyle newStyle)
{
    m_page.send(Messages::WebPageProxy::RecommendedScrollbarStyleDidChange(static_cast<int32_t>(newStyle)));
}

std::optional<ScrollbarOverlayStyle> WebChromeClient::preferredScrollbarOverlayStyle()
{
    return m_page.scrollbarOverlayStyle();
}

Color WebChromeClient::underlayColor() const
{
    return m_page.underlayColor();
}

void WebChromeClient::themeColorChanged() const
{
    m_page.themeColorChanged();
}

void WebChromeClient::pageExtendedBackgroundColorDidChange() const
{
    m_page.pageExtendedBackgroundColorDidChange();
}

void WebChromeClient::sampledPageTopColorChanged() const
{
    m_page.sampledPageTopColorChanged();
}

#if ENABLE(APP_HIGHLIGHTS)
WebCore::HighlightVisibility WebChromeClient::appHighlightsVisiblility() const
{
    return m_page.appHighlightsVisiblility();
}
#endif

void WebChromeClient::wheelEventHandlersChanged(bool hasHandlers)
{
    m_page.wheelEventHandlersChanged(hasHandlers);
}

String WebChromeClient::plugInStartLabelTitle(const String& mimeType) const
{
    return m_page.injectedBundleUIClient().plugInStartLabelTitle(mimeType);
}

String WebChromeClient::plugInStartLabelSubtitle(const String& mimeType) const
{
    return m_page.injectedBundleUIClient().plugInStartLabelSubtitle(mimeType);
}

String WebChromeClient::plugInExtraStyleSheet() const
{
    return m_page.injectedBundleUIClient().plugInExtraStyleSheet();
}

String WebChromeClient::plugInExtraScript() const
{
    return m_page.injectedBundleUIClient().plugInExtraScript();
}

void WebChromeClient::enableSuddenTermination()
{
    m_page.send(Messages::WebProcessProxy::EnableSuddenTermination());
}

void WebChromeClient::disableSuddenTermination()
{
    m_page.send(Messages::WebProcessProxy::DisableSuddenTermination());
}

void WebChromeClient::didAddHeaderLayer(GraphicsLayer& headerParent)
{
#if HAVE(RUBBER_BANDING)
    if (PageBanner* banner = m_page.headerPageBanner())
        banner->didAddParentLayer(&headerParent);
#else
    UNUSED_PARAM(headerParent);
#endif
}

void WebChromeClient::didAddFooterLayer(GraphicsLayer& footerParent)
{
#if HAVE(RUBBER_BANDING)
    if (PageBanner* banner = m_page.footerPageBanner())
        banner->didAddParentLayer(&footerParent);
#else
    UNUSED_PARAM(footerParent);
#endif
}

bool WebChromeClient::shouldUseTiledBackingForFrameView(const LocalFrameView& frameView) const
{
    return m_page.drawingArea()->shouldUseTiledBackingForFrameView(frameView);
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
void WebChromeClient::isAnyAnimationAllowedToPlayDidChange(bool anyAnimationCanPlay)
{
    m_page.isAnyAnimationAllowedToPlayDidChange(anyAnimationCanPlay);
}
#endif

void WebChromeClient::isPlayingMediaDidChange(MediaProducerMediaStateFlags state)
{
    m_page.isPlayingMediaDidChange(state);
}

void WebChromeClient::handleAutoplayEvent(AutoplayEvent event, OptionSet<AutoplayEventFlags> flags)
{
    m_page.send(Messages::WebPageProxy::HandleAutoplayEvent(event, flags));
}

#if ENABLE(WEB_CRYPTO)

bool WebChromeClient::wrapCryptoKey(const Vector<uint8_t>& key, Vector<uint8_t>& wrappedKey) const
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::WrapCryptoKey(key), m_page.identifier());
    if (!sendResult.succeeded())
        return false;

    bool succeeded;
    std::tie(succeeded, wrappedKey) = sendResult.takeReply();
    return succeeded;
}

bool WebChromeClient::unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, Vector<uint8_t>& key) const
{
    auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::UnwrapCryptoKey(wrappedKey), m_page.identifier());
    if (!sendResult.succeeded())
        return false;

    bool succeeded;
    std::tie(succeeded, key) = sendResult.takeReply();
    return succeeded;
}

#endif

#if ENABLE(APP_HIGHLIGHTS)
void WebChromeClient::storeAppHighlight(WebCore::AppHighlight&& highlight) const
{
    highlight.isNewGroup = m_page.highlightIsNewGroup();
    highlight.requestOriginatedInApp = m_page.highlightRequestOriginatedInApp();
    m_page.send(Messages::WebPageProxy::StoreAppHighlight(highlight));
}
#endif

void WebChromeClient::setTextIndicator(const WebCore::TextIndicatorData& indicatorData) const
{
    m_page.setTextIndicator(indicatorData);
}

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)

void WebChromeClient::handleTelephoneNumberClick(const String& number, const IntPoint& point, const IntRect& rect)
{
    m_page.handleTelephoneNumberClick(number, point, rect);
}

#endif

#if ENABLE(DATA_DETECTION)

void WebChromeClient::handleClickForDataDetectionResult(const DataDetectorElementInfo& info, const IntPoint& clickLocation)
{
    m_page.handleClickForDataDetectionResult(info, clickLocation);
}

#endif

#if ENABLE(SERVICE_CONTROLS)

void WebChromeClient::handleSelectionServiceClick(FrameSelection& selection, const Vector<String>& telephoneNumbers, const IntPoint& point)
{
    m_page.handleSelectionServiceClick(selection, telephoneNumbers, point);
}

bool WebChromeClient::hasRelevantSelectionServices(bool isTextOnly) const
{
    return (isTextOnly && WebProcess::singleton().hasSelectionServices()) || WebProcess::singleton().hasRichContentServices();
}

void WebChromeClient::handleImageServiceClick(const IntPoint& point, Image& image, HTMLImageElement& element)
{
    m_page.handleImageServiceClick(point, image, element);
}

void WebChromeClient::handlePDFServiceClick(const IntPoint& point, HTMLAttachmentElement& element)
{
    m_page.handlePDFServiceClick(point, element);
}

#endif

bool WebChromeClient::shouldDispatchFakeMouseMoveEvents() const
{
    return m_page.shouldDispatchFakeMouseMoveEvents();
}

void WebChromeClient::handleAutoFillButtonClick(HTMLInputElement& inputElement)
{
    RefPtr<API::Object> userData;

    // Notify the bundle client.
    auto nodeHandle = InjectedBundleNodeHandle::getOrCreate(inputElement);
    m_page.injectedBundleUIClient().didClickAutoFillButton(m_page, nodeHandle.get(), userData);

    // Notify the UIProcess.
    m_page.send(Messages::WebPageProxy::HandleAutoFillButtonClick(UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebChromeClient::inputElementDidResignStrongPasswordAppearance(HTMLInputElement& inputElement)
{
    RefPtr<API::Object> userData;

    // Notify the bundle client.
    auto nodeHandle = InjectedBundleNodeHandle::getOrCreate(inputElement);
    m_page.injectedBundleUIClient().didResignInputElementStrongPasswordAppearance(m_page, nodeHandle.get(), userData);

    // Notify the UIProcess.
    m_page.send(Messages::WebPageProxy::DidResignInputElementStrongPasswordAppearance { UserData { WebProcess::singleton().transformObjectsToHandles(userData.get()).get() } });
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

void WebChromeClient::addPlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    m_page.send(Messages::WebPageProxy::AddPlaybackTargetPickerClient(contextId));
}

void WebChromeClient::removePlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    m_page.send(Messages::WebPageProxy::RemovePlaybackTargetPickerClient(contextId));
}

void WebChromeClient::showPlaybackTargetPicker(PlaybackTargetClientContextIdentifier contextId, const IntPoint& position, bool isVideo)
{
    auto* frameView = m_page.localMainFrameView();
    if (!frameView)
        return;

    FloatRect rect(frameView->contentsToRootView(frameView->windowToContents(position)), FloatSize());
    m_page.send(Messages::WebPageProxy::ShowPlaybackTargetPicker(contextId, rect, isVideo));
}

void WebChromeClient::playbackTargetPickerClientStateDidChange(PlaybackTargetClientContextIdentifier contextId, MediaProducerMediaStateFlags state)
{
    m_page.send(Messages::WebPageProxy::PlaybackTargetPickerClientStateDidChange(contextId, state));
}

void WebChromeClient::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    m_page.send(Messages::WebPageProxy::SetMockMediaPlaybackTargetPickerEnabled(enabled));
}

void WebChromeClient::setMockMediaPlaybackTargetPickerState(const String& name, MediaPlaybackTargetContext::MockState state)
{
    m_page.send(Messages::WebPageProxy::SetMockMediaPlaybackTargetPickerState(name, state));
}

void WebChromeClient::mockMediaPlaybackTargetPickerDismissPopup()
{
    m_page.send(Messages::WebPageProxy::MockMediaPlaybackTargetPickerDismissPopup());
}
#endif

void WebChromeClient::imageOrMediaDocumentSizeChanged(const IntSize& newSize)
{
    m_page.imageOrMediaDocumentSizeChanged(newSize);
}

void WebChromeClient::didInvalidateDocumentMarkerRects()
{
    m_page.findController().didInvalidateDocumentMarkerRects();
}

#if ENABLE(TRACKING_PREVENTION)
void WebChromeClient::hasStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, LocalFrame& frame, CompletionHandler<void(bool)>&& completionHandler)
{
    auto* webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    m_page.hasStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), *webFrame, WTFMove(completionHandler));
}

void WebChromeClient::requestStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, LocalFrame& frame, StorageAccessScope scope, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler)
{
    auto* webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    m_page.requestStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), *webFrame, scope, WTFMove(completionHandler));
}

bool WebChromeClient::hasPageLevelStorageAccess(const WebCore::RegistrableDomain& topLevelDomain, const WebCore::RegistrableDomain& resourceDomain) const
{
    return m_page.hasPageLevelStorageAccess(topLevelDomain, resourceDomain);
}
#endif

#if ENABLE(DEVICE_ORIENTATION)
void WebChromeClient::shouldAllowDeviceOrientationAndMotionAccess(LocalFrame& frame, bool mayPrompt, CompletionHandler<void(DeviceOrientationOrMotionPermissionState)>&& callback)
{
    auto* webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    m_page.shouldAllowDeviceOrientationAndMotionAccess(webFrame->frameID(), webFrame->info(), mayPrompt, WTFMove(callback));
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
    m_page.configureLoggingChannel(channelName, state, level);
}

bool WebChromeClient::userIsInteracting() const
{
    return m_page.userIsInteracting();
}

void WebChromeClient::setUserIsInteracting(bool userIsInteracting)
{
    m_page.setUserIsInteracting(userIsInteracting);
}

#if ENABLE(WEB_AUTHN)
void WebChromeClient::setMockWebAuthenticationConfiguration(const MockWebAuthenticationConfiguration& configuration)
{
    m_page.send(Messages::WebPageProxy::SetMockWebAuthenticationConfiguration(configuration));
}
#endif

void WebChromeClient::animationDidFinishForElement(const Element& element)
{
    m_page.animationDidFinishForElement(element);
}

#if PLATFORM(MAC)
void WebChromeClient::changeUniversalAccessZoomFocus(const WebCore::IntRect& viewRect, const WebCore::IntRect& selectionRect)
{
    m_page.send(Messages::WebPageProxy::ChangeUniversalAccessZoomFocus(viewRect, selectionRect));
}
#endif

#if ENABLE(IMAGE_ANALYSIS)

void WebChromeClient::requestTextRecognition(Element& element, TextRecognitionOptions&& options, CompletionHandler<void(RefPtr<Element>&&)>&& completion)
{
    m_page.requestTextRecognition(element, WTFMove(options), WTFMove(completion));
}

#endif

URL WebChromeClient::applyLinkDecorationFiltering(const URL& url, LinkDecorationFilteringTrigger trigger) const
{
    return m_page.applyLinkDecorationFiltering(url, trigger);
}

URL WebChromeClient::allowedQueryParametersForAdvancedPrivacyProtections(const URL& url) const
{
    return m_page.allowedQueryParametersForAdvancedPrivacyProtections(url);
}

#if ENABLE(TEXT_AUTOSIZING)

void WebChromeClient::textAutosizingUsesIdempotentModeChanged()
{
    m_page.textAutosizingUsesIdempotentModeChanged();
}

#endif

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

void WebChromeClient::showMediaControlsContextMenu(FloatRect&& targetFrame, Vector<MediaControlsContextMenuItem>&& items, CompletionHandler<void(MediaControlsContextMenuItem::ID)>&& completionHandler)
{
    m_page.showMediaControlsContextMenu(WTFMove(targetFrame), WTFMove(items), WTFMove(completionHandler));
}

#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if ENABLE(WEBXR) && !USE(OPENXR)
void WebChromeClient::enumerateImmersiveXRDevices(CompletionHandler<void(const PlatformXR::Instance::DeviceList&)>&& completionHandler)
{
    m_page.xrSystemProxy().enumerateImmersiveXRDevices(WTFMove(completionHandler));
}

void WebChromeClient::requestPermissionOnXRSessionFeatures(const SecurityOriginData& origin, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& consentRequired, const PlatformXR::Device::FeatureList& consentOptional, const PlatformXR::Device::FeatureList& requiredFeaturesRequested, const PlatformXR::Device::FeatureList& optionalFeaturesRequested,  CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler)
{
    m_page.xrSystemProxy().requestPermissionOnSessionFeatures(origin, mode, granted, consentRequired, consentOptional, requiredFeaturesRequested, optionalFeaturesRequested, WTFMove(completionHandler));
}
#endif

#if ENABLE(APPLE_PAY_AMS_UI)

void WebChromeClient::startApplePayAMSUISession(const URL& originatingURL, const ApplePayAMSUIRequest& request, CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    m_page.sendWithAsyncReply(Messages::WebPageProxy::StartApplePayAMSUISession(originatingURL, request), WTFMove(completionHandler));
}

void WebChromeClient::abortApplePayAMSUISession()
{
    m_page.send(Messages::WebPageProxy::AbortApplePayAMSUISession());
}

#endif // ENABLE(APPLE_PAY_AMS_UI)

#if USE(SYSTEM_PREVIEW)
void WebChromeClient::beginSystemPreview(const URL& url, const SystemPreviewInfo& systemPreviewInfo, CompletionHandler<void()>&& completionHandler)
{
    m_page.sendWithAsyncReply(Messages::WebPageProxy::BeginSystemPreview(WTFMove(url), WTFMove(systemPreviewInfo)), WTFMove(completionHandler));
}
#endif

void WebChromeClient::requestCookieConsent(CompletionHandler<void(CookieConsentDecisionResult)>&& completion)
{
    m_page.sendWithAsyncReply(Messages::WebPageProxy::RequestCookieConsent(), WTFMove(completion));
}

bool WebChromeClient::isUsingUISideCompositing() const
{
#if PLATFORM(COCOA)
    return m_page.drawingArea()->type() == DrawingAreaType::RemoteLayerTree;
#else
    return false;
#endif
}

bool WebChromeClient::isInStableState() const
{
#if PLATFORM(IOS_FAMILY)
    return m_page.isInStableState();
#else
    // FIXME (255877): Implement this client hook on macOS.
    return true;
#endif
}

} // namespace WebKit
