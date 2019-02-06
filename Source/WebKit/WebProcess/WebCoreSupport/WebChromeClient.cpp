/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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
#include "PageBanner.h"
#include "UserData.h"
#include "WebColorChooser.h"
#include "WebCoreArgumentCoders.h"
#include "WebDataListSuggestionPicker.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include "WebFullScreenManager.h"
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
#include <WebCore/ApplicationCacheStorage.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/ColorChooser.h>
#include <WebCore/DataListSuggestionPicker.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/FileChooser.h>
#include <WebCore/FileIconLoader.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLParserIdioms.h>
#include <WebCore/HTMLPlugInImageElement.h>
#include <WebCore/Icon.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/Settings.h>

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
#include "PlaybackSessionManager.h"
#include "VideoFullscreenManager.h"
#endif

#if ENABLE(ASYNC_SCROLLING)
#include "RemoteScrollingCoordinator.h"
#endif

#if PLATFORM(GTK)
#include "PrinterListGtk.h"
#endif

namespace WebKit {
using namespace WebCore;
using namespace HTMLNames;

static double area(WebFrame* frame)
{
    IntSize size = frame->visibleContentBoundsExcludingScrollbars().size();
    return static_cast<double>(size.height()) * size.width();
}

static WebFrame* findLargestFrameInFrameSet(WebPage& page)
{
    // Approximate what a user could consider a default target frame for application menu operations.

    WebFrame* mainFrame = page.mainWebFrame();
    if (!mainFrame || !mainFrame->isFrameSet())
        return nullptr;

    WebFrame* largestSoFar = nullptr;

    Ref<API::Array> frameChildren = mainFrame->childFrames();
    size_t count = frameChildren->size();
    for (size_t i = 0; i < count; ++i) {
        auto* childFrame = frameChildren->at<WebFrame>(i);
        if (!largestSoFar || area(childFrame) > area(largestSoFar))
            largestSoFar = childFrame;
    }

    return largestSoFar;
}

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

    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetWindowFrame(), Messages::WebPageProxy::GetWindowFrame::Reply(newWindowFrame), m_page.pageID()))
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
    m_page.send(Messages::WebPageProxy::TakeFocus(direction));
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

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebPageProxy::FocusedFrameChanged(webFrame ? webFrame->frameID() : 0), m_page.pageID());
}

Page* WebChromeClient::createWindow(Frame& frame, const FrameLoadRequest& request, const WindowFeatures& windowFeatures, const NavigationAction& navigationAction)
{
#if ENABLE(FULLSCREEN_API)
    if (frame.document() && frame.document()->webkitCurrentFullScreenElement())
        frame.document()->webkitCancelFullScreen();
#endif

    auto& webProcess = WebProcess::singleton();

    NavigationActionData navigationActionData;
    navigationActionData.navigationType = navigationAction.type();
    navigationActionData.modifiers = InjectedBundleNavigationAction::modifiersForNavigationAction(navigationAction);
    navigationActionData.mouseButton = InjectedBundleNavigationAction::mouseButtonForNavigationAction(navigationAction);
    navigationActionData.syntheticClickType = InjectedBundleNavigationAction::syntheticClickTypeForNavigationAction(navigationAction);
    navigationActionData.clickLocationInRootViewCoordinates = InjectedBundleNavigationAction::clickLocationInRootViewCoordinatesForNavigationAction(navigationAction);
    navigationActionData.userGestureTokenIdentifier = webProcess.userGestureTokenIdentifier(navigationAction.userGestureToken());
    navigationActionData.canHandleRequest = m_page.canHandleRequest(request.resourceRequest());
    navigationActionData.shouldOpenExternalURLsPolicy = navigationAction.shouldOpenExternalURLsPolicy();
    navigationActionData.downloadAttribute = navigationAction.downloadAttribute();

    WebFrame* webFrame = WebFrame::fromCoreFrame(frame);

    uint64_t newPageID = 0;
    WebPageCreationParameters parameters;
    if (!webProcess.parentProcessConnection()->sendSync(Messages::WebPageProxy::CreateNewPage(webFrame->info(), webFrame->page()->pageID(), request.resourceRequest(), windowFeatures, navigationActionData), Messages::WebPageProxy::CreateNewPage::Reply(newPageID, parameters), m_page.pageID()))
        return nullptr;

    if (!newPageID)
        return nullptr;

    webProcess.createWebPage(newPageID, WTFMove(parameters));
    return webProcess.webPage(newPageID)->corePage();
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
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetToolbarsAreVisible(), Messages::WebPageProxy::GetToolbarsAreVisible::Reply(toolbarsAreVisible), m_page.pageID()))
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
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetStatusBarIsVisible(), Messages::WebPageProxy::GetStatusBarIsVisible::Reply(statusBarIsVisible), m_page.pageID()))
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
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::GetMenuBarIsVisible(), Messages::WebPageProxy::GetMenuBarIsVisible::Reply(menuBarIsVisible), m_page.pageID()))
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

    if (!m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::RunBeforeUnloadConfirmPanel(webFrame->frameID(), SecurityOriginData::fromFrame(&frame), message), Messages::WebPageProxy::RunBeforeUnloadConfirmPanel::Reply(shouldClose)))
        return false;

    return shouldClose;
}

void WebChromeClient::closeWindowSoon()
{
    // FIXME: This code assumes that the client will respond to a close page
    // message by actually closing the page. Safari does this, but there is
    // no guarantee that other applications will, which will leave this page
    // half detached. This approach is an inherent limitation making parts of
    // a close execute synchronously as part of window.close, but other parts
    // later on.

    m_page.corePage()->setGroupName(String());

    if (WebFrame* frame = m_page.mainWebFrame()) {
        if (Frame* coreFrame = frame->coreFrame())
            coreFrame->loader().stopForUserCancel();
    }

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

    HangDetectionDisabler hangDetectionDisabler;

    m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::RunJavaScriptAlert(webFrame->frameID(), SecurityOriginData::fromFrame(&frame), alertText), Messages::WebPageProxy::RunJavaScriptAlert::Reply());
}

bool WebChromeClient::runJavaScriptConfirm(Frame& frame, const String& message)
{
    if (shouldSuppressJavaScriptDialogs(frame))
        return false;

    WebFrame* webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);

    // Notify the bundle client.
    m_page.injectedBundleUIClient().willRunJavaScriptConfirm(&m_page, message, webFrame);

    HangDetectionDisabler hangDetectionDisabler;

    bool result = false;
    if (!m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::RunJavaScriptConfirm(webFrame->frameID(), SecurityOriginData::fromFrame(&frame), message), Messages::WebPageProxy::RunJavaScriptConfirm::Reply(result)))
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

    HangDetectionDisabler hangDetectionDisabler;

    if (!m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::RunJavaScriptPrompt(webFrame->frameID(), SecurityOriginData::fromFrame(&frame), message, defaultValue), Messages::WebPageProxy::RunJavaScriptPrompt::Reply(result)))
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
    
#if PLATFORM(IOS_FAMILY)
IntPoint WebChromeClient::accessibilityScreenToRootView(const IntPoint& point) const
{
    return m_page.accessibilityScreenToRootView(point);
}

IntRect WebChromeClient::rootViewToAccessibilityScreen(const IntRect& rect) const
{
    return m_page.rootViewToAccessibilityScreen(rect);
}
#endif

PlatformPageClient WebChromeClient::platformPageClient() const
{
    notImplemented();
    return 0;
}

void WebChromeClient::contentsSizeChanged(Frame& frame, const IntSize& size) const
{
    FrameView* frameView = frame.view();

    if (frameView && frameView->effectiveFrameFlattening() == FrameFlattening::Disabled) {
        WebFrame* largestFrame = findLargestFrameInFrameSet(m_page);
        if (largestFrame != m_cachedFrameSetLargestFrame.get()) {
            m_cachedFrameSetLargestFrame = largestFrame;
            m_page.send(Messages::WebPageProxy::FrameSetLargestFrameChanged(largestFrame ? largestFrame->frameID() : 0));
        }
    }

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

void WebChromeClient::scrollRectIntoView(const IntRect&) const
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
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}
    
void WebChromeClient::unavailablePluginButtonClicked(Element& element, RenderEmbeddedObject::PluginUnavailabilityReason pluginUnavailabilityReason) const
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    ASSERT(element.hasTagName(objectTag) || element.hasTagName(embedTag) || element.hasTagName(appletTag));
    ASSERT(pluginUnavailabilityReason == RenderEmbeddedObject::PluginMissing || pluginUnavailabilityReason == RenderEmbeddedObject::InsecurePluginVersion || pluginUnavailabilityReason);

    auto& pluginElement = downcast<HTMLPlugInImageElement>(element);

    String frameURLString = pluginElement.document().frame()->loader().documentLoader()->responseURL().string();
    String pageURLString = m_page.mainFrame()->loader().documentLoader()->responseURL().string();
    String pluginURLString = pluginElement.document().completeURL(pluginElement.url()).string();
    URL pluginspageAttributeURL = pluginElement.document().completeURL(stripLeadingAndTrailingHTMLSpaces(pluginElement.attributeWithoutSynchronization(pluginspageAttr)));
    if (!pluginspageAttributeURL.protocolIsInHTTPFamily())
        pluginspageAttributeURL = URL();
    m_page.send(Messages::WebPageProxy::UnavailablePluginButtonClicked(pluginUnavailabilityReason, pluginElement.serviceType(), pluginURLString, pluginspageAttributeURL.string(), frameURLString, pageURLString));
#else
    UNUSED_PARAM(element);
    UNUSED_PARAM(pluginUnavailabilityReason);
#endif // ENABLE(NETSCAPE_PLUGIN_API)
}

void WebChromeClient::mouseDidMoveOverElement(const HitTestResult& hitTestResult, unsigned modifierFlags)
{
    RefPtr<API::Object> userData;

    // Notify the bundle client.
    m_page.injectedBundleUIClient().mouseDidMoveOverElement(&m_page, hitTestResult, OptionSet<WebEvent::Modifier>::fromRaw(modifierFlags), userData);

    // Notify the UIProcess.
    WebHitTestResultData webHitTestResultData(hitTestResult);
    m_page.send(Messages::WebPageProxy::MouseDidMoveOverElement(webHitTestResultData, modifierFlags, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

void WebChromeClient::setToolTip(const String& toolTip, TextDirection)
{
    // Only send a tool tip to the WebProcess if it has changed since the last time this function was called.

    if (toolTip == m_cachedToolTip)
        return;
    m_cachedToolTip = toolTip;

    m_page.send(Messages::WebPageProxy::SetToolTip(m_cachedToolTip));
}

void WebChromeClient::print(Frame& frame)
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

    m_page.sendSyncWithDelayedReply(Messages::WebPageProxy::PrintFrame(webFrame->frameID()), Messages::WebPageProxy::PrintFrame::Reply());
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

#if ENABLE(DASHBOARD_SUPPORT)

void WebChromeClient::annotatedRegionsChanged()
{
    notImplemented();
}

#endif

bool WebChromeClient::shouldReplaceWithGeneratedFileForUpload(const String& path, String& generatedFilename)
{
    generatedFilename = m_page.injectedBundleUIClient().shouldGenerateFileForUpload(&m_page, path);
    return !generatedFilename.isNull();
}

String WebChromeClient::generateReplacementFile(const String& path)
{
    return m_page.injectedBundleUIClient().generateFileForUpload(&m_page, path);
}

#if ENABLE(INPUT_TYPE_COLOR)

std::unique_ptr<ColorChooser> WebChromeClient::createColorChooser(ColorChooserClient& client, const Color& initialColor)
{
    return std::make_unique<WebColorChooser>(&m_page, &client, initialColor);
}

#endif

#if ENABLE(DATALIST_ELEMENT)

std::unique_ptr<DataListSuggestionPicker> WebChromeClient::createDataListSuggestionPicker(DataListSuggestionsClient& client)
{
    return std::make_unique<WebDataListSuggestionPicker>(&m_page, &client);
}

#endif

void WebChromeClient::runOpenPanel(Frame& frame, FileChooser& fileChooser)
{
    if (m_page.activeOpenPanelResultListener())
        return;

    m_page.setActiveOpenPanelResultListener(WebOpenPanelResultListener::create(m_page, fileChooser));

    auto* webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    m_page.send(Messages::WebPageProxy::RunOpenPanel(webFrame->frameID(), SecurityOriginData::fromFrame(&frame), fileChooser.settings()));
}
    
void WebChromeClient::showShareSheet(ShareDataWithParsedURL& shareData, CompletionHandler<void(bool)>&& callback)
{
    m_page.showShareSheet(shareData, WTFMove(callback));
}

void WebChromeClient::loadIconForFiles(const Vector<String>& filenames, FileIconLoader& loader)
{
    loader.iconLoaded(createIconForFiles(filenames));
}

#if !PLATFORM(IOS_FAMILY)

void WebChromeClient::setCursor(const Cursor& cursor)
{
    m_page.send(Messages::WebPageProxy::SetCursor(cursor));
}

void WebChromeClient::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    m_page.send(Messages::WebPageProxy::SetCursorHiddenUntilMouseMoves(hiddenUntilMouseMoves));
}

RefPtr<Icon> WebChromeClient::createIconForFiles(const Vector<String>& filenames)
{
    return Icon::createIconForFiles(filenames);
}

#endif

void WebChromeClient::didAssociateFormControls(const Vector<RefPtr<Element>>& elements)
{
    return m_page.injectedBundleFormClient().didAssociateFormControls(&m_page, elements);
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

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)

RefPtr<DisplayRefreshMonitor> WebChromeClient::createDisplayRefreshMonitor(PlatformDisplayID displayID) const
{
    return m_page.drawingArea()->createDisplayRefreshMonitor(displayID);
}

#endif

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

void WebChromeClient::scheduleCompositingLayerFlush()
{
    if (m_page.drawingArea())
        m_page.drawingArea()->scheduleCompositingLayerFlush();
}

void WebChromeClient::contentRuleListNotification(const URL& url, const HashSet<std::pair<String, String>>& notificationPairs)
{
    Vector<String> identifiers;
    Vector<String> notifications;
    identifiers.reserveInitialCapacity(notificationPairs.size());
    notifications.reserveInitialCapacity(notificationPairs.size());
    for (auto& notification : notificationPairs) {
        identifiers.uncheckedAppend(notification.first);
        notifications.uncheckedAppend(notification.second);
    }

    m_page.send(Messages::WebPageProxy::ContentRuleListNotification(url, identifiers, notifications));
}

bool WebChromeClient::adjustLayerFlushThrottling(LayerFlushThrottleState::Flags flags)
{
    return m_page.drawingArea() && m_page.drawingArea()->adjustLayerFlushThrottling(flags);
}

bool WebChromeClient::layerTreeStateIsFrozen() const
{
    if (m_page.drawingArea())
        return m_page.drawingArea()->layerTreeStateIsFrozen();

    return false;
}

bool WebChromeClient::layerFlushThrottlingIsActive() const
{
    if (m_page.drawingArea())
        return m_page.drawingArea()->layerFlushThrottlingIsActive();

    return false;
}

#if ENABLE(ASYNC_SCROLLING)

RefPtr<ScrollingCoordinator> WebChromeClient::createScrollingCoordinator(Page& page) const
{
    ASSERT_UNUSED(page, m_page.corePage() == &page);
#if PLATFORM(COCOA)
    if (m_page.drawingArea()->type() != DrawingAreaTypeRemoteLayerTree)
        return nullptr;
    return RemoteScrollingCoordinator::create(&m_page);
#else
    return nullptr;
#endif
}

#endif

#if (PLATFORM(IOS_FAMILY) && HAVE(AVKIT)) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

bool WebChromeClient::supportsVideoFullscreen(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    return m_page.videoFullscreenManager().supportsVideoFullscreen(mode);
}

bool WebChromeClient::supportsVideoFullscreenStandby()
{
    return m_page.videoFullscreenManager().supportsVideoFullscreenStandby();
}

void WebChromeClient::setUpPlaybackControlsManager(HTMLMediaElement& mediaElement)
{
    m_page.playbackSessionManager().setUpPlaybackControlsManager(mediaElement);
}

void WebChromeClient::clearPlaybackControlsManager()
{
    m_page.playbackSessionManager().clearPlaybackControlsManager();
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

void WebChromeClient::exitVideoFullscreenForVideoElement(HTMLVideoElement& videoElement)
{
    m_page.videoFullscreenManager().exitVideoFullscreenForVideoElement(videoElement);
}

#endif

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

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

Optional<ScrollbarOverlayStyle> WebChromeClient::preferredScrollbarOverlayStyle()
{
    return m_page.scrollbarOverlayStyle();
}

Color WebChromeClient::underlayColor() const
{
    return m_page.underlayColor();
}

void WebChromeClient::pageExtendedBackgroundColorDidChange(Color backgroundColor) const
{
#if PLATFORM(MAC)
    m_page.send(Messages::WebPageProxy::PageExtendedBackgroundColorDidChange(backgroundColor));
#else
    UNUSED_PARAM(backgroundColor);
#endif
}

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
#if ENABLE(RUBBER_BANDING)
    if (PageBanner* banner = m_page.headerPageBanner())
        banner->didAddParentLayer(&headerParent);
#else
    UNUSED_PARAM(headerParent);
#endif
}

void WebChromeClient::didAddFooterLayer(GraphicsLayer& footerParent)
{
#if ENABLE(RUBBER_BANDING)
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

void WebChromeClient::isPlayingMediaDidChange(MediaProducer::MediaStateFlags state, uint64_t sourceElementID)
{
    m_page.send(Messages::WebPageProxy::IsPlayingMediaDidChange(state, sourceElementID));
}

void WebChromeClient::handleAutoplayEvent(AutoplayEvent event, OptionSet<AutoplayEventFlags> flags)
{
    m_page.send(Messages::WebPageProxy::HandleAutoplayEvent(event, flags));
}

#if ENABLE(MEDIA_SESSION)

void WebChromeClient::hasMediaSessionWithActiveMediaElementsDidChange(bool state)
{
    m_page.send(Messages::WebPageProxy::HasMediaSessionWithActiveMediaElementsDidChange(state));
}

void WebChromeClient::mediaSessionMetadataDidChange(const MediaSessionMetadata& metadata)
{
    m_page.send(Messages::WebPageProxy::MediaSessionMetadataDidChange(metadata));
}

void WebChromeClient::focusedContentMediaElementDidChange(uint64_t elementID)
{
    m_page.send(Messages::WebPageProxy::FocusedContentMediaElementDidChange(elementID));
}

#endif

#if ENABLE(WEB_CRYPTO)

bool WebChromeClient::wrapCryptoKey(const Vector<uint8_t>& key, Vector<uint8_t>& wrappedKey) const
{
    bool succeeded;
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::WrapCryptoKey(key), Messages::WebPageProxy::WrapCryptoKey::Reply(succeeded, wrappedKey), m_page.pageID()))
        return false;
    return succeeded;
}

bool WebChromeClient::unwrapCryptoKey(const Vector<uint8_t>& wrappedKey, Vector<uint8_t>& key) const
{
    bool succeeded;
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::UnwrapCryptoKey(wrappedKey), Messages::WebPageProxy::UnwrapCryptoKey::Reply(succeeded, key), m_page.pageID()))
        return false;
    return succeeded;
}

#endif

String WebChromeClient::signedPublicKeyAndChallengeString(unsigned keySizeIndex, const String& challengeString, const URL& url) const
{
    String result;
    if (!WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::SignedPublicKeyAndChallengeString(keySizeIndex, challengeString, url), Messages::WebPageProxy::SignedPublicKeyAndChallengeString::Reply(result), m_page.pageID()))
        return emptyString();
    return result;
}

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)

void WebChromeClient::handleTelephoneNumberClick(const String& number, const IntPoint& point)
{
    m_page.handleTelephoneNumberClick(number, point);
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

void WebChromeClient::addPlaybackTargetPickerClient(uint64_t contextId)
{
    m_page.send(Messages::WebPageProxy::AddPlaybackTargetPickerClient(contextId));
}

void WebChromeClient::removePlaybackTargetPickerClient(uint64_t contextId)
{
    m_page.send(Messages::WebPageProxy::RemovePlaybackTargetPickerClient(contextId));
}

void WebChromeClient::showPlaybackTargetPicker(uint64_t contextId, const IntPoint& position, bool isVideo)
{
    FrameView* frameView = m_page.mainFrame()->view();
    FloatRect rect(frameView->contentsToRootView(frameView->windowToContents(position)), FloatSize());
    m_page.send(Messages::WebPageProxy::ShowPlaybackTargetPicker(contextId, rect, isVideo));
}

void WebChromeClient::playbackTargetPickerClientStateDidChange(uint64_t contextId, MediaProducer::MediaStateFlags state)
{
    m_page.send(Messages::WebPageProxy::PlaybackTargetPickerClientStateDidChange(contextId, state));
}

void WebChromeClient::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    m_page.send(Messages::WebPageProxy::SetMockMediaPlaybackTargetPickerEnabled(enabled));
}

void WebChromeClient::setMockMediaPlaybackTargetPickerState(const String& name, MediaPlaybackTargetContext::State state)
{
    m_page.send(Messages::WebPageProxy::SetMockMediaPlaybackTargetPickerState(name, state));
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

#if ENABLE(RESOURCE_LOAD_STATISTICS)
void WebChromeClient::hasStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t, CompletionHandler<void(bool)>&& callback)
{
    m_page.hasStorageAccess(WTFMove(subFrameHost), WTFMove(topFrameHost), frameID, WTFMove(callback));
}

void WebChromeClient::requestStorageAccess(String&& subFrameHost, String&& topFrameHost, uint64_t frameID, uint64_t, CompletionHandler<void(bool)>&& callback)
{
    m_page.requestStorageAccess(WTFMove(subFrameHost), WTFMove(topFrameHost), frameID, WTFMove(callback));
}
#endif

} // namespace WebKit
