/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#include "APISecurityOrigin.h"
#include "DrawingArea.h"
#include "FindController.h"
#include "FrameInfoData.h"
#include "HangDetectionDisabler.h"
#include "InjectedBundleNavigationAction.h"
#include "InjectedBundleNodeHandle.h"
#include "NavigationActionData.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "PageBanner.h"
#include "PluginView.h"
#include "RemoteGPUProxy.h"
#include "RemoteRenderingBackendProxy.h"
#include "SharedBufferCopy.h"
#include "UserData.h"
#include "WebColorChooser.h"
#include "WebCoreArgumentCoders.h"
#include "WebDataListSuggestionPicker.h"
#include "WebDateTimeChooser.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include "WebFullScreenManager.h"
#include "WebGPUDowncastConvertToBackingContext.h"
#include "WebHitTestResultData.h"
#include "WebImage.h"
#include "WebOpenPanelResultListener.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebPageProxyMessages.h"
#include "WebPopupMenu.h"
#include "WebProcess.h"
#include "WebProcessPoolMessages.h"
#include "WebProcessProxyMessages.h"
#include "WebSearchPopupMenu.h"
#include <WebCore/AppHighlight.h>
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/ColorChooser.h>
#include <WebCore/ContentRuleListResults.h>
#include <WebCore/DataListSuggestionPicker.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentStorageAccess.h>
#include <WebCore/ElementInlines.h>
#include <WebCore/FileChooser.h>
#include <WebCore/FileIconLoader.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameView.h>
#include <WebCore/FullscreenManager.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLParserIdioms.h>
#include <WebCore/HTMLPlugInImageElement.h>
#include <WebCore/Icon.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/Settings.h>
#include <WebCore/TextIndicator.h>

#if HAVE(WEBGPU_IMPLEMENTATION)
#import <pal/graphics/WebGPU/Impl/WebGPUImpl.h>
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

#if PLATFORM(GTK)
#include "PrinterListGtk.h"
#endif

#if ENABLE(WEB_AUTHN)
#include "WebAuthnConnectionToWebProcessMessages.h"
#include "WebAuthnProcessConnection.h"
#include <WebCore/MockWebAuthenticationConfiguration.h>
#endif

#if ENABLE(WEBGL) && ENABLE(GPU_PROCESS) && (PLATFORM(COCOA) || PLATFORM(WIN))
#include "RemoteGraphicsContextGLProxy.h"
#endif

#if ENABLE(WEBGL)
#include <WebCore/GraphicsContextGL.h>
#endif

#if PLATFORM(MAC)
#include "TiledCoreAnimationScrollingCoordinator.h"
#endif

namespace WebKit {
using namespace WebCore;
using namespace HTMLNames;

WebChromeClient::WebChromeClient(WebPage& page)
    : m_page(page)
{
}

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

inline WebChromeClient::~WebChromeClient()
{
}

void WebChromeClient::chromeDestroyed()
{
    delete this;
}

void WebChromeClient::setWindowRect(const FloatRect& windowFrame)
{
    m_page.sendSetWindowFrame(windowFrame);
}

FloatRect WebChromeClient::windowRect()
{
#if PLATFORM(IOS_FAMILY)
    return FloatRect();
#else
#if PLATFORM(MAC)
    if (m_page.hasCachedWindowFrame())
        return m_page.windowFrameInUnflippedScreenCoordinates();
#endif

    FloatRect newWindowFrame;

    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetWindowFrame(), Messages::WebPageProxy::GetWindowFrame::Reply(newWindowFrame), m_page.identifier()))
        return FloatRect();

    return newWindowFrame;
#endif
}

FloatRect WebChromeClient::pageRect()
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

void WebChromeClient::elementDidFocus(Element& element)
{
    m_page.elementDidFocus(element);
}

void WebChromeClient::elementDidRefocus(Element& element)
{
    m_page.elementDidRefocus(element);
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

bool WebChromeClient::canTakeFocus(FocusDirection)
{
    notImplemented();
    return true;
}

void WebChromeClient::takeFocus(FocusDirection direction)
{
    m_page.send(Messages::WebPageProxy::TakeFocus(static_cast<uint8_t>(direction)));
}

void WebChromeClient::focusedElementChanged(Element* element)
{
    if (!is<HTMLInputElement>(element))
        return;

    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*element);
    if (!inputElement.isText())
        return;

    WebFrame* webFrame = WebFrame::fromCoreFrame(*element->document().frame());
    ASSERT(webFrame);
    m_page.injectedBundleFormClient().didFocusTextField(&m_page, &inputElement, webFrame);
}

void WebChromeClient::focusedFrameChanged(Frame* frame)
{
    WebFrame* webFrame = frame ? WebFrame::fromCoreFrame(*frame) : nullptr;

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::FocusedFrameChanged(webFrame ? std::make_optional(webFrame->frameID()) : std::nullopt), m_page.identifier());
}

Page* WebChromeClient::createWindow(Frame& frame, const WindowFeatures& windowFeatures, const NavigationAction& navigationAction)
{
#if ENABLE(FULLSCREEN_API)
    if (frame.document() && frame.document()->fullscreenManager().currentFullscreenElement())
        frame.document()->fullscreenManager().cancelFullscreen();
#endif

    auto& webProcess = WebProcess::singleton();

    NavigationActionData navigationActionData;
    navigationActionData.navigationType = navigationAction.type();
    navigationActionData.modifiers = InjectedBundleNavigationAction::modifiersForNavigationAction(navigationAction);
    navigationActionData.mouseButton = InjectedBundleNavigationAction::mouseButtonForNavigationAction(navigationAction);
    navigationActionData.syntheticClickType = InjectedBundleNavigationAction::syntheticClickTypeForNavigationAction(navigationAction);
    navigationActionData.clickLocationInRootViewCoordinates = InjectedBundleNavigationAction::clickLocationInRootViewCoordinatesForNavigationAction(navigationAction);
    navigationActionData.userGestureTokenIdentifier = webProcess.userGestureTokenIdentifier(navigationAction.userGestureToken());
    navigationActionData.canHandleRequest = m_page.canHandleRequest(navigationAction.resourceRequest());
    navigationActionData.shouldOpenExternalURLsPolicy = navigationAction.shouldOpenExternalURLsPolicy();
    navigationActionData.downloadAttribute = navigationAction.downloadAttribute();
    navigationActionData.privateClickMeasurement = navigationAction.privateClickMeasurement();

    WebFrame* webFrame = WebFrame::fromCoreFrame(frame);

    std::optional<PageIdentifier> newPageID;
    std::optional<WebPageCreationParameters> parameters;
    if (!webProcess.parentProcessConnection()->sendSync(Messages::WebPageProxy::CreateNewPage(webFrame->info(), webFrame->page()->webPageProxyIdentifier(), navigationAction.resourceRequest(), windowFeatures, navigationActionData), Messages::WebPageProxy::CreateNewPage::Reply(newPageID, parameters), m_page.identifier(), IPC::Timeout::infinity(), IPC::SendSyncOption::MaintainOrderingWithAsyncMessages))
        return nullptr;

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
    bool handled = false;
    if (!WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::TestProcessIncomingSyncMessagesWhenWaitingForSyncReply(m_page.webPageProxyIdentifier()), Messages::NetworkConnectionToWebProcess::TestProcessIncomingSyncMessagesWhenWaitingForSyncReply::Reply(handled), 0))
        return false;
    return handled;
}

void WebChromeClient::show()
{
    m_page.show();
}

bool WebChromeClient::canRunModal()
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

bool WebChromeClient::toolbarsVisible()
{
    API::InjectedBundle::PageUIClient::UIElementVisibility toolbarsVisibility = m_page.injectedBundleUIClient().toolbarsAreVisible(&m_page);
    if (toolbarsVisibility != API::InjectedBundle::PageUIClient::UIElementVisibility::Unknown)
        return toolbarsVisibility == API::InjectedBundle::PageUIClient::UIElementVisibility::Visible;
    
    bool toolbarsAreVisible = true;
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetToolbarsAreVisible(), Messages::WebPageProxy::GetToolbarsAreVisible::Reply(toolbarsAreVisible), m_page.identifier()))
        return true;

    return toolbarsAreVisible;
}

void WebChromeClient::setStatusbarVisible(bool statusBarIsVisible)
{
    m_page.send(Messages::WebPageProxy::SetStatusBarIsVisible(statusBarIsVisible));
}

bool WebChromeClient::statusbarVisible()
{
    API::InjectedBundle::PageUIClient::UIElementVisibility statusbarVisibility = m_page.injectedBundleUIClient().statusBarIsVisible(&m_page);
    if (statusbarVisibility != API::InjectedBundle::PageUIClient::UIElementVisibility::Unknown)
        return statusbarVisibility == API::InjectedBundle::PageUIClient::UIElementVisibility::Visible;

    bool statusBarIsVisible = true;
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetStatusBarIsVisible(), Messages::WebPageProxy::GetStatusBarIsVisible::Reply(statusBarIsVisible), m_page.identifier()))
        return true;

    return statusBarIsVisible;
}

void WebChromeClient::setScrollbarsVisible(bool)
{
    notImplemented();
}

bool WebChromeClient::scrollbarsVisible()
{
    notImplemented();
    return true;
}

void WebChromeClient::setMenubarVisible(bool menuBarVisible)
{
    m_page.send(Messages::WebPageProxy::SetMenuBarIsVisible(menuBarVisible));
}

bool WebChromeClient::menubarVisible()
{
    API::InjectedBundle::PageUIClient::UIElementVisibility menubarVisibility = m_page.injectedBundleUIClient().menuBarIsVisible(&m_page);
    if (menubarVisibility != API::InjectedBundle::PageUIClient::UIElementVisibility::Unknown)
        return menubarVisibility == API::InjectedBundle::PageUIClient::UIElementVisibility::Visible;
    
    bool menuBarIsVisible = true;
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetMenuBarIsVisible(), Messages::WebPageProxy::GetMenuBarIsVisible::Reply(menuBarIsVisible), m_page.identifier()))
        return true;

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

bool WebChromeClient::canRunBeforeUnloadConfirmPanel()
{
    return m_page.canRunBeforeUnloadConfirmPanel();
}

bool WebChromeClient::runBeforeUnloadConfirmPanel(const String& message, Frame& frame)
{
    WebFrame* webFrame = WebFrame::fromCoreFrame(frame);

    bool shouldClose = false;

    HangDetectionDisabler hangDetectionDisabler;

    if (!m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::RunBeforeUnloadConfirmPanel(webFrame->frameID(), webFrame->info(), message), Messages::WebPageProxy::RunBeforeUnloadConfirmPanel::Reply(shouldClose)))
        return false;

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
    if (auto* coreFrame = frame.coreFrame())
        coreFrame->loader().stopForUserCancel();

    m_page.sendClose();
}

static bool shouldSuppressJavaScriptDialogs(Frame& frame)
{
    if (frame.loader().opener() && frame.loader().stateMachine().isDisplayingInitialEmptyDocument() && frame.loader().provisionalDocumentLoader())
        return true;

    return false;
}

void WebChromeClient::runJavaScriptAlert(Frame& frame, const String& alertText)
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

    m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::RunJavaScriptAlert(webFrame->frameID(), webFrame->info(), alertText), Messages::WebPageProxy::RunJavaScriptAlert::Reply(), IPC::SendSyncOption::MaintainOrderingWithAsyncMessages);
}

bool WebChromeClient::runJavaScriptConfirm(Frame& frame, const String& message)
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

    bool result = false;
    if (!m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::RunJavaScriptConfirm(webFrame->frameID(), webFrame->info(), message), Messages::WebPageProxy::RunJavaScriptConfirm::Reply(result), IPC::SendSyncOption::MaintainOrderingWithAsyncMessages))
        return false;

    return result;
}

bool WebChromeClient::runJavaScriptPrompt(Frame& frame, const String& message, const String& defaultValue, String& result)
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

    if (!m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::RunJavaScriptPrompt(webFrame->frameID(), webFrame->info(), message, defaultValue), Messages::WebPageProxy::RunJavaScriptPrompt::Reply(result), IPC::SendSyncOption::MaintainOrderingWithAsyncMessages))
        return false;

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
    if (Document* document = m_page.corePage()->mainFrame().document()) {
        if (document->printing())
            return;
    }

    m_page.drawingArea()->setNeedsDisplayInRect(rect);
}

void WebChromeClient::invalidateContentsForSlowScroll(const IntRect& rect)
{
    if (Document* document = m_page.corePage()->mainFrame().document()) {
        if (document->printing())
            return;
    }

    m_page.pageDidScroll();
#if USE(COORDINATED_GRAPHICS)
    FrameView* frameView = m_page.mainFrame()->view();
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

void WebChromeClient::contentsSizeChanged(Frame& frame, const IntSize& size) const
{
    FrameView* frameView = frame.view();

    if (&frame.page()->mainFrame() != &frame)
        return;

    m_page.send(Messages::WebPageProxy::DidChangeContentSize(size));

    m_page.drawingArea()->mainFrameContentSizeChanged(size);

    if (frameView && !frameView->delegatesScrolling())  {
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

void WebChromeClient::mouseDidMoveOverElement(const HitTestResult& hitTestResult, unsigned modifierFlags, const String& toolTip, TextDirection)
{
    RefPtr<API::Object> userData;

    // Notify the bundle client.
    m_page.injectedBundleUIClient().mouseDidMoveOverElement(&m_page, hitTestResult, OptionSet<WebEvent::Modifier>::fromRaw(modifierFlags), userData);

    // Notify the UIProcess.
    WebHitTestResultData webHitTestResultData(hitTestResult, toolTip);
    m_page.send(Messages::WebPageProxy::MouseDidMoveOverElement(webHitTestResultData, modifierFlags, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

static constexpr unsigned maxTitleLength = 1000; // Closest power of 10 above the W3C recommendation for Title length.

void WebChromeClient::print(Frame& frame, const StringWithDirection& title)
{
    WebFrame* webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);

#if PLATFORM(GTK) && HAVE(GTK_UNIX_PRINTING)
    // When printing synchronously in GTK+ we need to make sure that we have a list of Printers before starting the print operation.
    // Getting the list of printers is done synchronously by GTK+, but using a nested main loop that might process IPC messages
    // comming from the UI process like EndPrinting. When the EndPriting message is received while the printer list is being populated,
    // the print operation is finished unexpectely and the web process crashes, see https://bugs.webkit.org/show_bug.cgi?id=126979.
    // The PrinterListGtk class gets the list of printers in the constructor so we just need to ensure there's an instance alive
    // during the synchronous print operation.
    RefPtr<PrinterListGtk> printerList = PrinterListGtk::getOrCreate();
    if (!printerList) {
        // PrinterListGtk::getOrCreate() returns nullptr when called while a printers enumeration is ongoing.
        // This can happen if a synchronous print is started by a JavaScript and another one is inmeditaley started
        // from a JavaScript event listener. The second print operation is handled by the nested main loop used by GTK+
        // to enumerate the printers, and we end up here trying to get a reference of an object that is being constructed.
        // It's very unlikely that the user wants to print twice in a row, and other browsers don't do two print operations
        // in this particular case either. So, the safest solution is to return early here and ignore the second print.
        // See https://bugs.webkit.org/show_bug.cgi?id=141035
        return;
    }
#endif

    WebCore::FloatSize pdfFirstPageSize;
#if PLATFORM(COCOA)
    if (auto* pluginView = WebPage::pluginViewForFrame(&frame)) {
        if (auto* plugin = pluginView->plugin())
            pdfFirstPageSize = plugin->pdfDocumentSizeForPrinting();
    }
#endif

    auto truncatedTitle = truncateFromEnd(title, maxTitleLength);
    m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::PrintFrame(webFrame->frameID(), truncatedTitle.string, pdfFirstPageSize), Messages::WebPageProxy::PrintFrame::Reply());
}

void WebChromeClient::exceededDatabaseQuota(Frame& frame, const String& databaseName, DatabaseDetails details)
{
    WebFrame* webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    
    auto& origin = frame.document()->securityOrigin();
    auto& originData = origin.data();
    auto& tracker = DatabaseTracker::singleton();
    auto currentQuota = tracker.quota(originData);
    auto currentOriginUsage = tracker.usage(originData);
    uint64_t newQuota = 0;
    auto securityOrigin = API::SecurityOrigin::create(SecurityOriginData::fromDatabaseIdentifier(originData.databaseIdentifier())->securityOrigin());
    newQuota = m_page.injectedBundleUIClient().didExceedDatabaseQuota(&m_page, securityOrigin.ptr(), databaseName, details.displayName(), currentQuota, currentOriginUsage, details.currentUsage(), details.expectedUsage());

    if (!newQuota)
        m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::ExceededDatabaseQuota(webFrame->frameID(), originData.databaseIdentifier(), databaseName, details.displayName(), currentQuota, currentOriginUsage, details.currentUsage(), details.expectedUsage()), Messages::WebPageProxy::ExceededDatabaseQuota::Reply(newQuota));

    tracker.setQuota(originData, newQuota);
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

    uint64_t newQuota = 0;
    m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::ReachedApplicationCacheOriginQuota(origin.data().databaseIdentifier(), currentQuota, totalBytesNeeded), Messages::WebPageProxy::ReachedApplicationCacheOriginQuota::Reply(newQuota));

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

void WebChromeClient::runOpenPanel(Frame& frame, FileChooser& fileChooser)
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

#if !PLATFORM(IOS_FAMILY)

RefPtr<Icon> WebChromeClient::createIconForFiles(const Vector<String>& filenames)
{
    return Icon::createIconForFiles(filenames);
}

#endif

void WebChromeClient::didAssociateFormControls(const Vector<RefPtr<Element>>& elements, WebCore::Frame& frame)
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
RefPtr<ImageBuffer> WebChromeClient::createImageBuffer(const FloatSize& size, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat) const
{
    if (!WebProcess::singleton().shouldUseRemoteRenderingFor(purpose))
        return nullptr;

    return m_page.ensureRemoteRenderingBackendProxy().createImageBuffer(size, renderingMode, resolutionScale, colorSpace, pixelFormat);
}
#endif

#if ENABLE(WEBGL)
RefPtr<GraphicsContextGL> WebChromeClient::createGraphicsContextGL(const GraphicsContextGLAttributes& attributes, PlatformDisplayID) const
{
#if ENABLE(GPU_PROCESS) && (PLATFORM(COCOA) || PLATFORM(WIN))
    if (WebProcess::singleton().shouldUseRemoteRenderingForWebGL())
        return RemoteGraphicsContextGLProxy::create(attributes, m_page.ensureRemoteRenderingBackendProxy().ensureBackendCreated());
#endif
    return WebCore::createWebProcessGraphicsContextGL(attributes);
}
#endif

RefPtr<PAL::WebGPU::GPU> WebChromeClient::createGPUForWebGPU() const
{
#if HAVE(WEBGPU_IMPLEMENTATION)
#if ENABLE(GPU_PROCESS)
    return RemoteGPUProxy::create(WebProcess::singleton().ensureGPUProcessConnection(), WebGPU::DowncastConvertToBackingContext::create(), WebGPUIdentifier::generate(), m_page.ensureRemoteRenderingBackendProxy().ensureBackendCreated());
#else
    return PAL::WebGPU::GPUImpl::create();
#endif
#else
    return nullptr;
#endif
}

void WebChromeClient::attachRootGraphicsLayer(Frame&, GraphicsLayer* layer)
{
    if (layer)
        m_page.enterAcceleratedCompositingMode(layer);
    else
        m_page.exitAcceleratedCompositingMode();
}

void WebChromeClient::attachViewOverlayGraphicsLayer(GraphicsLayer* graphicsLayer)
{
    if (auto drawingArea = m_page.drawingArea())
        drawingArea->attachViewOverlayGraphicsLayer(graphicsLayer);
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

RefPtr<ScrollingCoordinator> WebChromeClient::createScrollingCoordinator(Page& page) const
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

#if ENABLE(VIDEO_PRESENTATION_MODE)

void WebChromeClient::prepareForVideoFullscreen()
{
    m_page.videoFullscreenManager();
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

bool WebChromeClient::shouldUseTiledBackingForFrameView(const FrameView& frameView) const
{
    return m_page.drawingArea()->shouldUseTiledBackingForFrameView(frameView);
}

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
    bool succeeded;
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::WrapCryptoKey(key), Messages::WebPageProxy::WrapCryptoKey::Reply(succeeded, wrappedKey), m_page.identifier()))
        return false;
    return succeeded;
}

bool WebChromeClient::unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, Vector<uint8_t>& key) const
{
    bool succeeded;
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::UnwrapCryptoKey(wrappedKey), Messages::WebPageProxy::UnwrapCryptoKey::Reply(succeeded, key), m_page.identifier()))
        return false;
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
    m_page.send(Messages::WebPageProxy::SetTextIndicator(indicatorData, static_cast<uint64_t>(WebCore::TextIndicatorLifetime::Temporary)));
}

String WebChromeClient::signedPublicKeyAndChallengeString(unsigned keySizeIndex, const String& challengeString, const URL& url) const
{
    String result;
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::SignedPublicKeyAndChallengeString(keySizeIndex, challengeString, url), Messages::WebPageProxy::SignedPublicKeyAndChallengeString::Reply(result), m_page.identifier()))
        return emptyString();
    return result;
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
    FrameView* frameView = m_page.mainFrame()->view();
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

#if ENABLE(VIDEO) && USE(GSTREAMER)

void WebChromeClient::requestInstallMissingMediaPlugins(const String& details, const String& description, MediaPlayerRequestInstallMissingPluginsCallback& callback)
{
    m_page.requestInstallMissingMediaPlugins(details, description, callback);
}

#endif

void WebChromeClient::didInvalidateDocumentMarkerRects()
{
    m_page.findController().didInvalidateDocumentMarkerRects();
}

#if ENABLE(INTELLIGENT_TRACKING_PREVENTION)
void WebChromeClient::hasStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, Frame& frame, CompletionHandler<void(bool)>&& completionHandler)
{
    auto* webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    m_page.hasStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), *webFrame, WTFMove(completionHandler));
}

void WebChromeClient::requestStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, Frame& frame, StorageAccessScope scope, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler)
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
void WebChromeClient::shouldAllowDeviceOrientationAndMotionAccess(Frame& frame, bool mayPrompt, CompletionHandler<void(DeviceOrientationOrMotionPermissionState)>&& callback)
{
    auto* webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    m_page.shouldAllowDeviceOrientationAndMotionAccess(webFrame->frameID(), webFrame->info(), mayPrompt, WTFMove(callback));
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
    if (!RuntimeEnabledFeatures::sharedFeatures().webAuthenticationModernEnabled()) {
        m_page.send(Messages::WebPageProxy::SetMockWebAuthenticationConfiguration(configuration));
        return;
    }

    WebProcess::singleton().ensureWebAuthnProcessConnection().connection().send(Messages::WebAuthnConnectionToWebProcess::SetMockWebAuthenticationConfiguration(configuration), { });
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

void WebChromeClient::requestTextRecognition(Element& element, const String& identifier, CompletionHandler<void(RefPtr<Element>&&)>&& completion)
{
    m_page.requestTextRecognition(element, identifier, WTFMove(completion));
}

#endif

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

void WebChromeClient::requestPermissionOnXRSessionFeatures(const SecurityOriginData& origin, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& consentRequired, const PlatformXR::Device::FeatureList& consentOptional, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler)
{
    m_page.xrSystemProxy().requestPermissionOnSessionFeatures(origin, mode, granted, consentRequired, consentOptional, WTFMove(completionHandler));
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

void WebChromeClient::requestCookieConsent(CompletionHandler<void(CookieConsentDecisionResult)>&& completion)
{
    m_page.sendWithAsyncReply(Messages::WebPageProxy::RequestCookieConsent(), WTFMove(completion));
}

void WebChromeClient::classifyModalContainerControls(Vector<String>&& strings, CompletionHandler<void(Vector<ModalContainerControlType>&&)>&& completion)
{
    m_page.sendWithAsyncReply(Messages::WebPageProxy::ClassifyModalContainerControls(WTFMove(strings)), WTFMove(completion));
}

void WebChromeClient::decidePolicyForModalContainer(OptionSet<ModalContainerControlType> types, CompletionHandler<void(ModalContainerDecision)>&& completion)
{
    m_page.sendWithAsyncReply(Messages::WebPageProxy::DecidePolicyForModalContainer(types), WTFMove(completion));
}

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WebChromeClientAdditions.cpp>
#else
const AtomString& WebChromeClient::searchStringForModalContainerObserver() const
{
    return nullAtom();
}
#endif

} // namespace WebKit
