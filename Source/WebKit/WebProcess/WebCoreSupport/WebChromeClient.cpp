/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "ModelDowncastConvertToBackingContext.h"
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
#include <WebCore/AXObjectCache.h>
#include <WebCore/AppHighlight.h>
#include <WebCore/BarcodeDetectorInterface.h>
#include <WebCore/ColorChooser.h>
#include <WebCore/ColorChooserClient.h>
#include <WebCore/ContentRuleListMatchedRule.h>
#include <WebCore/ContentRuleListResults.h>
#include <WebCore/CookieConsentDecisionResult.h>
#include <WebCore/DataListSuggestionPicker.h>
#include <WebCore/DatabaseTracker.h>
#include <WebCore/DocumentFullscreen.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/DocumentStorageAccess.h>
#include <WebCore/DocumentView.h>
#include <WebCore/ElementInlines.h>
#include <WebCore/FaceDetectorInterface.h>
#include <WebCore/FileChooser.h>
#include <WebCore/FileIconLoader.h>
#include <WebCore/FocusController.h>
#include <WebCore/FocusControllerTypes.h>
#include <WebCore/FocusOptions.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLMediaElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLParserIdioms.h>
#include <WebCore/HTMLPlugInElement.h>
#include <WebCore/Icon.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/PointerLockController.h>
#include <WebCore/PopupMenuClient.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ScriptController.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/Settings.h>
#include <WebCore/SystemPreviewInfo.h>
#include <WebCore/TextDetectorInterface.h>
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

#if ENABLE(WEBXR)
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

#if ENABLE(DAMAGE_TRACKING)
#include <WebCore/Damage.h>
#endif

#if ENABLE(VIDEO)
#include <WebCore/HTMLMediaElement.h>
#endif

namespace WebKit {
using namespace WebCore;
using namespace HTMLNames;

AXRelayProcessSuspendedNotification::AXRelayProcessSuspendedNotification(WebPage& page, AutomaticallySend automaticallySend)
    : m_page(page)
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

void WebChromeClient::setWindowRect(const FloatRect& windowFrame)
{
    if (RefPtr page = m_page.get())
        page->sendSetWindowFrame(windowFrame);
}

FloatRect WebChromeClient::windowRect() const
{
#if PLATFORM(IOS_FAMILY)
    return FloatRect();
#else
    RefPtr page = m_page.get();
    if (!page)
        return { };
#if PLATFORM(MAC)
    if (page->hasCachedWindowFrame())
        return page->windowFrameInUnflippedScreenCoordinates();
#endif

    auto sendResult = WebProcess::singleton().protectedParentProcessConnection()->sendSync(Messages::WebPageProxy::GetWindowFrame(), page->identifier());
    auto [newWindowFrame] = sendResult.takeReplyOr(FloatRect { });
    return newWindowFrame;
#endif
}

FloatRect WebChromeClient::pageRect() const
{
    if (!m_page)
        return { };

    return FloatRect(FloatPoint(), m_page->size());
}

void WebChromeClient::focus()
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::SetFocus(true));
}

void WebChromeClient::unfocus()
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::SetFocus(false));
}

#if PLATFORM(COCOA)

void WebChromeClient::elementDidFocus(Element& element, const FocusOptions& options)
{
    if (RefPtr page = m_page.get())
        page->elementDidFocus(element, options);
}

void WebChromeClient::elementDidRefocus(Element& element, const FocusOptions& options)
{
    if (RefPtr page = m_page.get())
        page->elementDidRefocus(element, options);
}

void WebChromeClient::elementDidBlur(Element& element)
{
    if (RefPtr page = m_page.get())
        page->elementDidBlur(element);
}

void WebChromeClient::focusedElementDidChangeInputMode(Element& element, InputMode mode)
{
    if (RefPtr page = m_page.get())
        page->focusedElementDidChangeInputMode(element, mode);
}

void WebChromeClient::focusedSelectElementDidChangeOptions(const WebCore::HTMLSelectElement& element)
{
    if (RefPtr page = m_page.get())
        page->focusedSelectElementDidChangeOptions(element);
}

void WebChromeClient::makeFirstResponder()
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::MakeFirstResponder());
}

void WebChromeClient::assistiveTechnologyMakeFirstResponder()
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::AssistiveTechnologyMakeFirstResponder());
}

#endif    

bool WebChromeClient::canTakeFocus(FocusDirection) const
{
    notImplemented();
    return true;
}

void WebChromeClient::takeFocus(FocusDirection direction)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::TakeFocus(direction));
}

void WebChromeClient::focusedElementChanged(Element* element, LocalFrame* frame, FocusOptions options, BroadcastFocusedElement broadcast)
{
    RefPtr coreFrame =  element ? element->document().protectedFrame() : frame;
    RefPtr webFrame = coreFrame ? WebFrame::fromCoreFrame(*coreFrame) : nullptr;
    RefPtr page = m_page.get();
    if (page && broadcast == BroadcastFocusedElement::Yes)
        WebProcess::singleton().protectedParentProcessConnection()->send(Messages::WebPageProxy::FocusedElementChanged(webFrame ? std::make_optional(webFrame->frameID()) : std::nullopt, options), page->identifier());

    RefPtr inputElement = dynamicDowncast<HTMLInputElement>(element);
    if (!inputElement || !inputElement->isText())
        return;

    ASSERT(webFrame);
    if (page)
        page->injectedBundleFormClient().didFocusTextField(page.get(), *inputElement, webFrame.get());
}

void WebChromeClient::focusedFrameChanged(Frame* frame)
{
    if (!m_page)
        return;

    RefPtr webFrame = frame ? WebFrame::fromCoreFrame(*frame) : nullptr;
    WebProcess::singleton().protectedParentProcessConnection()->send(Messages::WebPageProxy::FocusedFrameChanged(webFrame ? std::make_optional(webFrame->frameID()) : std::nullopt), m_page->identifier());
}

RefPtr<Page> WebChromeClient::createWindow(LocalFrame& frame, const String& openedMainFrameName, const WindowFeatures& windowFeatures, const NavigationAction& navigationAction)
{
#if ENABLE(FULLSCREEN_API)
    if (RefPtr document = frame.document())
        document->protectedFullscreen()->fullyExitFullscreen();
#endif

    auto& webProcess = WebProcess::singleton();

    auto& mouseEventData = navigationAction.mouseEventData();

    RefPtr webFrame = WebFrame::fromCoreFrame(frame);

    RefPtr page = m_page.get();
    if (!page)
        return nullptr;

    auto& originalRequest = navigationAction.originalRequest();
    NavigationActionData navigationActionData {
        navigationAction.type(),
        modifiersForNavigationAction(navigationAction),
        mouseButton(navigationAction),
        syntheticClickType(navigationAction),
        webProcess.userGestureTokenIdentifier(navigationAction.requester()->pageID, navigationAction.userGestureToken()),
        navigationAction.userGestureToken() ? navigationAction.userGestureToken()->authorizationToken() : std::nullopt,
        page->canHandleRequest(navigationAction.originalRequest()),
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
        navigationAction.isInitialFrameSrcLoad(),
        navigationAction.isContentRuleListRedirect(),
        openedMainFrameName,
        std::nullopt, /* targetBackForwardItemIdentifier */
        std::nullopt, /* sourceBackForwardItemIdentifier */
        WebCore::LockHistory::No,
        WebCore::LockBackForwardList::No,
        { }, /* clientRedirectSourceForHistory */
        frame.effectiveSandboxFlags(),
        frame.document()->referrerPolicy(),
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
        originalRequest, /* originalRequest */
        originalRequest, /* request */
        originalRequest.url().isValid() ? String() : originalRequest.url().string(), /* invalidURLString */
        navigationAction.requester(), /* requester */
    };

    auto sendResult = webProcess.protectedParentProcessConnection()->sendSync(Messages::WebPageProxy::CreateNewPage(windowFeatures, navigationActionData), page->identifier(), IPC::Timeout::infinity(), { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages });
    if (!sendResult.succeeded())
        return nullptr;

    auto [newPageID, parameters] = sendResult.takeReply();
    if (!newPageID)
        return nullptr;
    ASSERT(parameters);

    parameters->oldPageID = page->identifier();

    webProcess.createWebPage(*newPageID, WTFMove(*parameters));
    return webProcess.webPage(*newPageID)->corePage();
}

bool WebChromeClient::testProcessIncomingSyncMessagesWhenWaitingForSyncReply()
{
    if (!m_page)
        return false;

    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;

    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::TestProcessIncomingSyncMessagesWhenWaitingForSyncReply(m_page->webPageProxyIdentifier()), 0);
    auto [handled] = sendResult.takeReplyOr(false);
    return handled;
}

void WebChromeClient::show()
{
    if (RefPtr page = m_page.get())
        page->show();
}

bool WebChromeClient::canRunModal() const
{
    RefPtr page = m_page.get();
    return page && page->canRunModal();
}

void WebChromeClient::runModal()
{
    if (RefPtr page = m_page.get())
        page->runModal();
}

void WebChromeClient::reportProcessCPUTime(Seconds cpuTime, ActivityStateForCPUSampling activityState)
{
    WebProcess::singleton().send(Messages::WebProcessPool::ReportWebContentCPUTime(cpuTime, static_cast<uint64_t>(activityState)), 0);
}

bool WebChromeClient::toolbarsVisible() const
{
    RefPtr page = m_page.get();
    if (!page)
        return false;
    return page->toolbarsAreVisible();
}

bool WebChromeClient::statusbarVisible() const
{
    RefPtr page = m_page.get();
    if (!page)
        return false;
    return page->statusBarIsVisible();
}

bool WebChromeClient::scrollbarsVisible() const
{
    notImplemented();
    return true;
}

bool WebChromeClient::menubarVisible() const
{
    RefPtr page = m_page.get();
    if (!page)
        return false;
    return page->menuBarIsVisible();
}

void WebChromeClient::setResizable(bool resizable)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::SetIsResizable(resizable));
}

void WebChromeClient::addMessageToConsole(MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, unsigned columnNumber, const String& sourceID)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

#if !PLATFORM(COCOA)
    page->injectedBundleUIClient().willAddMessageToConsole(page.get(), source, level, message, lineNumber, columnNumber, sourceID);
#endif

    if (!page->shouldSendConsoleLogsToUIProcessForTesting())
        return;
    page->send(Messages::WebPageProxy::AddMessageToConsoleForTesting(message.length() > String::MaxLength / 2 ? "Out of memory"_s : message), IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

bool WebChromeClient::canRunBeforeUnloadConfirmPanel()
{
    RefPtr page = m_page.get();
    return page && page->canRunBeforeUnloadConfirmPanel();
}

bool WebChromeClient::runBeforeUnloadConfirmPanel(String&& message, LocalFrame& frame)
{
    auto webFrame = WebFrame::fromCoreFrame(frame);

    HangDetectionDisabler hangDetectionDisabler;

    RefPtr page = m_page.get();
    if (!page)
        return false;

    auto relay = AXRelayProcessSuspendedNotification(*page);

    auto sendResult = page->sendSyncWithDelayedReply(Messages::WebPageProxy::RunBeforeUnloadConfirmPanel(webFrame->frameID(), webFrame->info(), WTFMove(message)));
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

    RefPtr page = m_page.get();
    if (!page)
        return;

    page->protectedCorePage()->setGroupName(String());

    Ref frame = page->mainWebFrame();
    if (RefPtr coreFrame = frame->coreLocalFrame())
        coreFrame->loader().stopForUserCancel();

    page->sendClose();
}

void WebChromeClient::rootFrameAdded(const WebCore::LocalFrame& frame)
{
    if (!m_page)
        return;

    if (RefPtr drawingArea = m_page->drawingArea())
        drawingArea->addRootFrame(frame.frameID());
}

void WebChromeClient::rootFrameRemoved(const WebCore::LocalFrame& frame)
{
    if (!m_page)
        return;

    if (RefPtr drawingArea = m_page->drawingArea())
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
    RefPtr page = m_page.get();
    if (!page)
        return;

    page->prepareToRunModalJavaScriptDialog();

    HangDetectionDisabler hangDetectionDisabler;
    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;

    auto relay = AXRelayProcessSuspendedNotification(*page);

    page->sendSyncWithDelayedReply(Messages::WebPageProxy::RunJavaScriptAlert(webFrame->frameID(), webFrame->info(), alertText), { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages });
}

bool WebChromeClient::runJavaScriptConfirm(LocalFrame& frame, const String& message)
{
    if (shouldSuppressJavaScriptDialogs(frame))
        return false;

    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);

    // Notify the bundle client.
    RefPtr page = m_page.get();
    if (!page)
        return false;

    page->prepareToRunModalJavaScriptDialog();

    HangDetectionDisabler hangDetectionDisabler;
    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;

    auto relay = AXRelayProcessSuspendedNotification(*page);

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
    RefPtr page = m_page.get();
    if (!page)
        return false;

    page->prepareToRunModalJavaScriptDialog();

    HangDetectionDisabler hangDetectionDisabler;
    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;

    auto relay = AXRelayProcessSuspendedNotification(*page);

    auto sendResult = page->sendSyncWithDelayedReply(Messages::WebPageProxy::RunJavaScriptPrompt(webFrame->frameID(), webFrame->info(), message, defaultValue), { IPC::SendSyncOption::MaintainOrderingWithAsyncMessages });
    if (!sendResult.succeeded())
        return false;

    std::tie(result) = sendResult.takeReply();
    return !result.isNull();
}

KeyboardUIMode WebChromeClient::keyboardUIMode()
{
    RefPtr page = m_page.get();
    return page ? page->keyboardUIMode() : KeyboardAccessDefault;
}

bool WebChromeClient::hasAccessoryMousePointingDevice() const
{
    RefPtr page = m_page.get();
    return page && page->hasAccessoryMousePointingDevice();
}

bool WebChromeClient::hoverSupportedByPrimaryPointingDevice() const
{
    RefPtr page = m_page.get();
    return page && page->hoverSupportedByPrimaryPointingDevice();
}

bool WebChromeClient::hoverSupportedByAnyAvailablePointingDevice() const
{
    RefPtr page = m_page.get();
    return page && page->hoverSupportedByAnyAvailablePointingDevice();
}

std::optional<PointerCharacteristics> WebChromeClient::pointerCharacteristicsOfPrimaryPointingDevice() const
{
    RefPtr page = m_page.get();
    return page ? page->pointerCharacteristicsOfPrimaryPointingDevice() : std::nullopt;
}

OptionSet<PointerCharacteristics> WebChromeClient::pointerCharacteristicsOfAllAvailablePointingDevices() const
{
    RefPtr page = m_page.get();
    if (!page)
        return { };
    return page->pointerCharacteristicsOfAllAvailablePointingDevices();
}

#if ENABLE(POINTER_LOCK)

void WebChromeClient::requestPointerLock(CompletionHandler<void(WebCore::PointerLockRequestResult)>&& completionHandler)
{
    RefPtr page = m_page.get();
    if (!page) {
        completionHandler(WebCore::PointerLockRequestResult::Failure);
        return;
    }

    page->sendWithAsyncReply(Messages::WebPageProxy::RequestPointerLock(), [completionHandler = WTFMove(completionHandler)](bool result) mutable {
        if (result)
            completionHandler(WebCore::PointerLockRequestResult::Success);
        else
            completionHandler(WebCore::PointerLockRequestResult::Failure);
    });
}

void WebChromeClient::requestPointerUnlock(CompletionHandler<void(bool)>&& completionHandler)
{
    if (RefPtr page = m_page.get())
        page->sendWithAsyncReply(Messages::WebPageProxy::RequestPointerUnlock(), WTFMove(completionHandler));
    else
        completionHandler(false);
}

#endif

void WebChromeClient::invalidateRootView(const IntRect&)
{
    // Do nothing here, there's no concept of invalidating the window in the web process.
}

void WebChromeClient::invalidateContentsAndRootView(const IntRect& rect)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    RefPtr corePage = page->corePage();
    if (!corePage)
        return;

    if (RefPtr document = corePage->localTopDocument()) {
        if (document->printing())
            return;
    }

    page->protectedDrawingArea()->setNeedsDisplayInRect(rect);
}

void WebChromeClient::invalidateContentsForSlowScroll(const IntRect& rect)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    RefPtr corePage = page->corePage();
    if (!corePage)
        return;

    if (RefPtr document = corePage->localTopDocument()) {
        if (document->printing())
            return;
    }

    page->pageDidScroll();
    page->protectedDrawingArea()->setNeedsDisplayInRect(rect);
}

void WebChromeClient::scroll(const IntSize& scrollDelta, const IntRect& scrollRect, const IntRect& clipRect)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    page->pageDidScroll();
    page->protectedDrawingArea()->scroll(intersection(scrollRect, clipRect), scrollDelta);
}

IntPoint WebChromeClient::screenToRootView(const IntPoint& point) const
{
    RefPtr page = m_page.get();
    return page ? page->screenToRootView(point) : IntPoint();
}

IntPoint WebChromeClient::rootViewToScreen(const IntPoint& point) const
{
    RefPtr page = m_page.get();
    return page ? page->rootViewToScreen(point) : IntPoint();
}

IntRect WebChromeClient::rootViewToScreen(const IntRect& rect) const
{
    RefPtr page = m_page.get();
    return page ? page->rootViewToScreen(rect) : IntRect();
}
    
IntPoint WebChromeClient::accessibilityScreenToRootView(const IntPoint& point) const
{
    RefPtr page = m_page.get();
    return page ? page->accessibilityScreenToRootView(point) : IntPoint();
}

IntRect WebChromeClient::rootViewToAccessibilityScreen(const IntRect& rect) const
{
    RefPtr page = m_page.get();
    return page ? page->rootViewToAccessibilityScreen(rect) : IntRect();
}

void WebChromeClient::mainFrameDidChange()
{
    if (RefPtr page = m_page.get())
        page->platformReinitializeAccessibilityToken();
}

void WebChromeClient::didFinishLoadingImageForElement(HTMLImageElement& element)
{
    if (RefPtr page = m_page.get())
        page->didFinishLoadingImageForElement(element);
}

#if ENABLE(MODEL_PROCESS)
void WebChromeClient::setHasModelElement(bool hasModelElement)
{
    if (RefPtr page = m_page.get())
        page->setHasModelElement(hasModelElement);
}
#endif

PlatformPageClient WebChromeClient::platformPageClient() const
{
    notImplemented();
    return 0;
}

void WebChromeClient::intrinsicContentsSizeChanged(const IntSize& size) const
{
    if (RefPtr page = m_page.get())
        page->scheduleIntrinsicContentSizeUpdate(size);
}

void WebChromeClient::contentsSizeChanged(LocalFrame& frame, const IntSize& size) const
{
    RefPtr frameView = frame.view();

    if (&frame.page()->mainFrame() != &frame)
        return;

    RefPtr page = m_page.get();
    if (!page)
        return;

    page->send(Messages::WebPageProxy::DidChangeContentSize(size));

    page->protectedDrawingArea()->mainFrameContentSizeChanged(frame.frameID(), size);

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
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::RequestScrollToRect(rect, rect.center()));
}

void WebChromeClient::scrollContainingScrollViewsToRevealRect(const IntRect&) const
{
    notImplemented();
}

bool WebChromeClient::shouldUnavailablePluginMessageBeButton(PluginUnavailabilityReason pluginUnavailabilityReason) const
{
    switch (pluginUnavailabilityReason) {
    case PluginUnavailabilityReason::PluginMissing:
        // FIXME: <rdar://problem/8794397> We should only return true when there is a
        // missingPluginButtonClicked callback defined on the Page UI client.
    case PluginUnavailabilityReason::InsecurePluginVersion:
        return true;


    case PluginUnavailabilityReason::PluginCrashed:
    case PluginUnavailabilityReason::PluginBlockedByContentSecurityPolicy:
    case PluginUnavailabilityReason::UnsupportedPlugin:
    case PluginUnavailabilityReason::PluginTooSmall:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}
    
void WebChromeClient::unavailablePluginButtonClicked(Element& element, PluginUnavailabilityReason pluginUnavailabilityReason) const
{
    UNUSED_PARAM(element);
    UNUSED_PARAM(pluginUnavailabilityReason);
}

void WebChromeClient::mouseDidMoveOverElement(const HitTestResult& hitTestResult, OptionSet<WebCore::PlatformEventModifier> modifiers, const String& toolTip, TextDirection)
{
    auto wkModifiers = modifiersFromPlatformEventModifiers(modifiers);

    RefPtr page = m_page.get();
    if (!page)
        return;

    // Notify the UIProcess.
    WebHitTestResultData webHitTestResultData(hitTestResult, toolTip);
    webHitTestResultData.elementBoundingBox = webHitTestResultData.elementBoundingBox.toRectWithExtentsClippedToNumericLimits();
    page->send(Messages::WebPageProxy::MouseDidMoveOverElement(webHitTestResultData, wkModifiers));
}

void WebChromeClient::print(LocalFrame& frame, const StringWithDirection& title)
{
    static constexpr unsigned maxTitleLength = 1000; // Closest power of 10 above the W3C recommendation for Title length.

    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);

    WebCore::FloatSize pdfFirstPageSize;
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = WebPage::pluginViewForFrame(&frame))
        pdfFirstPageSize = pluginView->pdfDocumentSizeForPrinting();
#endif

    auto truncatedTitle = truncateFromEnd(title, maxTitleLength);
    RefPtr page = m_page.get();
    if (!page)
        return;

    auto relay = AXRelayProcessSuspendedNotification(*page);

    IPC::UnboundedSynchronousIPCScope unboundedSynchronousIPCScope;
    page->sendSyncWithDelayedReply(Messages::WebPageProxy::PrintFrame(webFrame->frameID(), truncatedTitle.string, pdfFirstPageSize));
}

RefPtr<ColorChooser> WebChromeClient::createColorChooser(ColorChooserClient& client, const Color& initialColor)
{
    RefPtr page = m_page.get();
    return WebColorChooser::create(page.get(), &client, initialColor);
}

RefPtr<DataListSuggestionPicker> WebChromeClient::createDataListSuggestionPicker(DataListSuggestionsClient& client)
{
    RefPtr page = m_page.get();
    if (!page)
        return nullptr;

    return WebDataListSuggestionPicker::create(page.releaseNonNull(), client);
}

bool WebChromeClient::canShowDataListSuggestionLabels() const
{
#if PLATFORM(MAC)
    return true;
#else
    return false;
#endif
}

RefPtr<DateTimeChooser> WebChromeClient::createDateTimeChooser(DateTimeChooserClient& client)
{
    RefPtr page = m_page.get();
    if (!page)
        return nullptr;

    return WebDateTimeChooser::create(page.releaseNonNull(), client);
}

void WebChromeClient::runOpenPanel(LocalFrame& frame, FileChooser& fileChooser)
{
    RefPtr page = m_page.get();
    if (!page || page->activeOpenPanelResultListener())
        return;

    page->setActiveOpenPanelResultListener(WebOpenPanelResultListener::create(*page, fileChooser));

    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    page->send(Messages::WebPageProxy::RunOpenPanel(webFrame->frameID(), webFrame->info(), fileChooser.settings()));
}
    
void WebChromeClient::showShareSheet(ShareDataWithParsedURL&& shareData, CompletionHandler<void(bool)>&& callback)
{
    if (RefPtr page = m_page.get())
        page->showShareSheet(WTFMove(shareData), WTFMove(callback));
}

void WebChromeClient::showContactPicker(WebCore::ContactsRequestData&& requestData, WTF::CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&& callback)
{
    if (RefPtr page = m_page.get())
        page->showContactPicker(WTFMove(requestData), WTFMove(callback));
}

#if HAVE(DIGITAL_CREDENTIALS_UI)
void WebChromeClient::showDigitalCredentialsPicker(const WebCore::DigitalCredentialsRequestData& requestData, WTF::CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&& callback)
{
    if (RefPtr page = m_page.get())
        page->showDigitalCredentialsPicker(requestData, WTFMove(callback));
}

void WebChromeClient::dismissDigitalCredentialsPicker(WTF::CompletionHandler<void(bool)>&& completionHandler)
{
    if (RefPtr page = m_page.get())
        page->dismissDigitalCredentialsPicker(WTFMove(completionHandler));
}
#endif

void WebChromeClient::loadIconForFiles(const Vector<String>& filenames, FileIconLoader& loader)
{
    loader.iconLoaded(createIconForFiles(filenames));
}

void WebChromeClient::setCursor(const Cursor& cursor)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::SetCursor(cursor));
}

void WebChromeClient::setCursorHiddenUntilMouseMoves(bool hiddenUntilMouseMoves)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::SetCursorHiddenUntilMouseMoves(hiddenUntilMouseMoves));
}

#if !PLATFORM(COCOA)

RefPtr<Icon> WebChromeClient::createIconForFiles(const Vector<String>& filenames)
{
    return Icon::createIconForFiles(filenames);
}

#endif

void WebChromeClient::didAssociateFormControls(const Vector<Ref<Element>>& elements, WebCore::LocalFrame& frame)
{
    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    if (RefPtr page = m_page.get())
        page->injectedBundleFormClient().didAssociateFormControls(page.get(), elements, webFrame.get());
}

bool WebChromeClient::shouldNotifyOnFormChanges()
{
    RefPtr page = m_page.get();
    return page && page->injectedBundleFormClient().shouldNotifyOnFormChanges(page.get());
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
    RefPtr page = m_page.get();
    return WebPopupMenu::create(page.get(), &client);
}

RefPtr<SearchPopupMenu> WebChromeClient::createSearchPopupMenu(PopupMenuClient& client) const
{
    RefPtr page = m_page.get();
    return WebSearchPopupMenu::create(page.get(), &client);
}

GraphicsLayerFactory* WebChromeClient::graphicsLayerFactory() const
{
    RefPtr page = m_page.get();
    if (!page)
        return nullptr;
    if (RefPtr drawingArea = page->drawingArea())
        return drawingArea->graphicsLayerFactory();
    return nullptr;
}

WebCore::DisplayRefreshMonitorFactory* WebChromeClient::displayRefreshMonitorFactory() const
{
    return m_page ? m_page->drawingArea() : nullptr;
}

#if ENABLE(GPU_PROCESS)
RefPtr<ImageBuffer> WebChromeClient::createImageBuffer(const FloatSize& size, RenderingMode renderingMode, RenderingPurpose purpose, float resolutionScale, const DestinationColorSpace& colorSpace, ImageBufferFormat pixelFormat) const
{
    if (WebProcess::singleton().shouldUseRemoteRenderingFor(purpose)) {
        RefPtr page = m_page.get();
        if (!page)
            return nullptr;
        return page->ensureProtectedRemoteRenderingBackendProxy()->createImageBuffer(size, renderingMode, purpose, resolutionScale, colorSpace, pixelFormat);
    }

    if (purpose == RenderingPurpose::ShareableSnapshot || purpose == RenderingPurpose::ShareableLocalSnapshot)
        return ImageBuffer::create<ImageBufferShareableBitmapBackend>(size, resolutionScale, colorSpace, { PixelFormat::BGRA8 }, purpose, { });

    return nullptr;
}

RefPtr<ImageBuffer> WebChromeClient::sinkIntoImageBuffer(std::unique_ptr<SerializedImageBuffer> imageBuffer)
{
    if (!is<RemoteSerializedImageBufferProxy>(imageBuffer))
        return SerializedImageBuffer::sinkIntoImageBuffer(WTFMove(imageBuffer));

    RefPtr page = m_page.get();
    if (!page)
        return nullptr;

    auto remote = std::unique_ptr<RemoteSerializedImageBufferProxy>(static_cast<RemoteSerializedImageBufferProxy*>(imageBuffer.release()));
    return RemoteSerializedImageBufferProxy::sinkIntoImageBuffer(WTFMove(remote), page->ensureProtectedRemoteRenderingBackendProxy());
}
#endif

std::unique_ptr<WebCore::WorkerClient> WebChromeClient::createWorkerClient(SerialFunctionDispatcher& dispatcher)
{
    RefPtr page = m_page.get();
    if (!page)
        return nullptr;
    return WebWorkerClient::create(*page->protectedCorePage(), dispatcher).moveToUniquePtr();
}

#if ENABLE(WEBGL)
RefPtr<GraphicsContextGL> WebChromeClient::createGraphicsContextGL(const GraphicsContextGLAttributes& attributes) const
{
#if PLATFORM(GTK)
    WebProcess::singleton().initializePlatformDisplayIfNeeded();
#endif
#if ENABLE(GPU_PROCESS)
    if (WebProcess::singleton().shouldUseRemoteRenderingForWebGL()) {
        RefPtr page = m_page.get();
        if (!page)
            return nullptr;
        return RemoteGraphicsContextGLProxy::create(attributes, page.releaseNonNull());
    }
#endif
    return WebCore::createWebProcessGraphicsContextGL(attributes);
}
#endif

#if HAVE(WEBGPU_IMPLEMENTATION)
RefPtr<WebCore::WebGPU::GPU> WebChromeClient::createGPUForWebGPU() const
{
#if ENABLE(GPU_PROCESS)
    RefPtr page = m_page.get();
    if (!page)
        return nullptr;
    return RemoteGPUProxy::create(WebGPU::DowncastConvertToBackingContext::create(), DDModel::DowncastConvertToBackingContext::create(), page.releaseNonNull());
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
    RefPtr page = m_page.get();
    if (!page)
        return nullptr;
    Ref renderingBackend = page->ensureRemoteRenderingBackendProxy();
    return renderingBackend->createBarcodeDetector(barcodeDetectorOptions);
#elif HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    return WebCore::ShapeDetection::BarcodeDetectorImpl::create(barcodeDetectorOptions);
#else
    return nullptr;
#endif
}

void WebChromeClient::getBarcodeDetectorSupportedFormats(CompletionHandler<void(Vector<WebCore::ShapeDetection::BarcodeFormat>&&)>&& completionHandler) const
{
#if ENABLE(GPU_PROCESS)
    RefPtr page = m_page.get();
    if (!page) {
        completionHandler({ });
        return;
    }
    Ref renderingBackend = page->ensureRemoteRenderingBackendProxy();
    renderingBackend->supportedBarcodeDetectorBarcodeFormats(WTFMove(completionHandler));
#elif HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    WebCore::ShapeDetection::BarcodeDetectorImpl::getSupportedFormats(WTFMove(completionHandler));
#else
    completionHandler({ });
#endif
}

RefPtr<WebCore::ShapeDetection::FaceDetector> WebChromeClient::createFaceDetector(const WebCore::ShapeDetection::FaceDetectorOptions& faceDetectorOptions) const
{
#if ENABLE(GPU_PROCESS)
    RefPtr page = m_page.get();
    if (!page)
        return nullptr;
    Ref renderingBackend = page->ensureRemoteRenderingBackendProxy();
    return renderingBackend->createFaceDetector(faceDetectorOptions);
#elif HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    return WebCore::ShapeDetection::FaceDetectorImpl::create(faceDetectorOptions);
#else
    return nullptr;
#endif
}

RefPtr<WebCore::ShapeDetection::TextDetector> WebChromeClient::createTextDetector() const
{
#if ENABLE(GPU_PROCESS)
    RefPtr page = m_page.get();
    if (!page)
        return nullptr;
    Ref renderingBackend = page->ensureRemoteRenderingBackendProxy();
    return renderingBackend->createTextDetector();
#elif HAVE(SHAPE_DETECTION_API_IMPLEMENTATION)
    return WebCore::ShapeDetection::TextDetectorImpl::create();
#else
    return nullptr;
#endif
}

void WebChromeClient::attachRootGraphicsLayer(LocalFrame& frame, GraphicsLayer* layer)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (layer)
        page->enterAcceleratedCompositingMode(frame, layer);
    else
        page->exitAcceleratedCompositingMode(frame);
}

void WebChromeClient::attachViewOverlayGraphicsLayer(GraphicsLayer* graphicsLayer)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    RefPtr drawingArea = page->drawingArea();
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
    RefPtr page = m_page.get();
    return page && page->shouldTriggerRenderingUpdate(rescheduledRenderingUpdateCount);
}

void WebChromeClient::triggerRenderingUpdate()
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (RefPtr drawingArea = page->drawingArea())
        drawingArea->triggerRenderingUpdate();
}

bool WebChromeClient::scheduleRenderingUpdate()
{
    RefPtr page = m_page.get();
    if (!page)
        return false;
    if (RefPtr drawingArea = page->drawingArea())
        return drawingArea->scheduleRenderingUpdate();
    return false;
}

void WebChromeClient::renderingUpdateFramesPerSecondChanged()
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (RefPtr drawingArea = page->drawingArea())
        drawingArea->renderingUpdateFramesPerSecondChanged();
}

unsigned WebChromeClient::remoteImagesCountForTesting() const
{
    RefPtr page = m_page.get();
    return page ? page->remoteImagesCountForTesting() : 0;
}

void WebChromeClient::registerBlobPathForTesting(const String& path, CompletionHandler<void()>&& completionHandler)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::RegisterBlobPathForTesting(path), WTFMove(completionHandler));
}


void WebChromeClient::contentRuleListNotification(const URL& url, const ContentRuleListResults& results)
{
#if ENABLE(CONTENT_EXTENSIONS)
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::ContentRuleListNotification(url, results));
#endif
}

void WebChromeClient::contentRuleListMatchedRule(const WebCore::ContentRuleListMatchedRule& matchedRule)
{
#if ENABLE(CONTENT_EXTENSIONS)
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::ContentRuleListMatchedRule(matchedRule));
#endif
}

bool WebChromeClient::layerTreeStateIsFrozen() const
{
    RefPtr page = m_page.get();
    if (!page)
        return false;

    if (RefPtr drawingArea = page->drawingArea())
        return drawingArea->layerTreeStateIsFrozen();

    return false;
}

#if ENABLE(ASYNC_SCROLLING)

RefPtr<WebCore::ScrollingCoordinator> WebChromeClient::createScrollingCoordinator(Page& corePage) const
{
    RefPtr page = m_page.get();
    if (!page)
        return nullptr;

    ASSERT_UNUSED(corePage, page->corePage() == &corePage);
#if ENABLE(TILED_CA_DRAWING_AREA)
    switch (page->protectedDrawingArea()->type()) {
    case DrawingAreaType::TiledCoreAnimation:
        return TiledCoreAnimationScrollingCoordinator::create(page.get());
    case DrawingAreaType::RemoteLayerTree:
        return RemoteScrollingCoordinator::create(page.get());
    }
#elif PLATFORM(COCOA)
    return RemoteScrollingCoordinator::create(page.get());
#else
    return nullptr;
#endif
}

#endif

#if PLATFORM(MAC)
void WebChromeClient::ensureScrollbarsController(Page& corePage, ScrollableArea& area, bool update) const
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    ASSERT(page->corePage() == &corePage);
    auto* currentScrollbarsController = area.existingScrollbarsController();

    if (area.mockScrollbarsControllerEnabled() || (update && !currentScrollbarsController)) {
        ASSERT(!currentScrollbarsController || is<ScrollbarsControllerMock>(currentScrollbarsController));
        return;
    }

#if ENABLE(TILED_CA_DRAWING_AREA)
    switch (page->protectedDrawingArea()->type()) {
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
#else
    if (!area.usesCompositedScrolling() && (!currentScrollbarsController || is<RemoteScrollbarsController>(currentScrollbarsController)))
        area.setScrollbarsController(ScrollbarsController::create(area));
    else if (area.usesCompositedScrolling() && (!currentScrollbarsController || !is<RemoteScrollbarsController>(currentScrollbarsController)))
        area.setScrollbarsController(makeUnique<RemoteScrollbarsController>(area, corePage.scrollingCoordinator()));
#endif
}

#endif


#if ENABLE(VIDEO_PRESENTATION_MODE)

void WebChromeClient::prepareForVideoFullscreen()
{
    if (RefPtr page = m_page.get())
        page->videoPresentationManager();
}

bool WebChromeClient::canEnterVideoFullscreen(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode mode) const
{
    RefPtr page = m_page.get();
    return page && page->protectedVideoPresentationManager()->canEnterVideoFullscreen(videoElement, mode);
}

bool WebChromeClient::supportsVideoFullscreen(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    RefPtr page = m_page.get();
    return page && page->protectedVideoPresentationManager()->supportsVideoFullscreen(mode);
}

bool WebChromeClient::supportsVideoFullscreenStandby()
{
    RefPtr page = m_page.get();
    return page && page->protectedVideoPresentationManager()->supportsVideoFullscreenStandby();
}

void WebChromeClient::setMockVideoPresentationModeEnabled(bool enabled)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::SetMockVideoPresentationModeEnabled(enabled));
}

void WebChromeClient::enterVideoFullscreenForVideoElement(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode mode, bool standby)
{
#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
    ASSERT(standby || mode != HTMLMediaElementEnums::VideoFullscreenModeNone);
#else
    ASSERT(mode != HTMLMediaElementEnums::VideoFullscreenModeNone);
#endif
    if (RefPtr page = m_page.get())
        page->protectedVideoPresentationManager()->enterVideoFullscreenForVideoElement(videoElement, mode, standby);
}

void WebChromeClient::setPlayerIdentifierForVideoElement(HTMLVideoElement& videoElement)
{
    if (RefPtr page = m_page.get())
        page->protectedVideoPresentationManager()->setPlayerIdentifierForVideoElement(videoElement);
}

void WebChromeClient::exitVideoFullscreenForVideoElement(HTMLVideoElement& videoElement, CompletionHandler<void(bool)>&& completionHandler)
{
    if (RefPtr page = m_page.get())
        page->protectedVideoPresentationManager()->exitVideoFullscreenForVideoElement(videoElement, WTFMove(completionHandler));
}

void WebChromeClient::setUpPlaybackControlsManager(HTMLMediaElement& mediaElement)
{
    if (RefPtr page = m_page.get())
        page->protectedPlaybackSessionManager()->setUpPlaybackControlsManager(mediaElement);
}

void WebChromeClient::clearPlaybackControlsManager()
{
    if (RefPtr page = m_page.get())
        page->protectedPlaybackSessionManager()->clearPlaybackControlsManager();
}

void WebChromeClient::mediaEngineChanged(WebCore::HTMLMediaElement& mediaElement)
{
    if (RefPtr page = m_page.get())
        page->protectedPlaybackSessionManager()->mediaEngineChanged(mediaElement);
}

#endif

#if ENABLE(MEDIA_USAGE)
void WebChromeClient::addMediaUsageManagerSession(MediaSessionIdentifier identifier, const String& bundleIdentifier, const URL& pageURL)
{
    if (RefPtr page = m_page.get())
        page->addMediaUsageManagerSession(identifier, bundleIdentifier, pageURL);
}

void WebChromeClient::updateMediaUsageManagerSessionState(MediaSessionIdentifier identifier, const MediaUsageInfo& usage)
{
    if (RefPtr page = m_page.get())
        page->updateMediaUsageManagerSessionState(identifier, usage);
}

void WebChromeClient::removeMediaUsageManagerSession(MediaSessionIdentifier identifier)
{
    if (RefPtr page = m_page.get())
        page->removeMediaUsageManagerSession(identifier);
}
#endif // ENABLE(MEDIA_USAGE)

#if ENABLE(VIDEO_PRESENTATION_MODE)

void WebChromeClient::exitVideoFullscreenToModeWithoutAnimation(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode targetMode)
{
    if (RefPtr page = m_page.get())
        page->protectedVideoPresentationManager()->exitVideoFullscreenToModeWithoutAnimation(videoElement, targetMode);
}

void WebChromeClient::setVideoFullscreenMode(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    if (RefPtr page = m_page.get())
        page->protectedVideoPresentationManager()->setVideoFullscreenMode(videoElement, mode);
}

void WebChromeClient::clearVideoFullscreenMode(HTMLVideoElement& videoElement, HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    if (RefPtr page = m_page.get())
        page->protectedVideoPresentationManager()->clearVideoFullscreenMode(videoElement, mode);
}

#endif

#if ENABLE(FULLSCREEN_API)

bool WebChromeClient::supportsFullScreenForElement(const Element& element, bool withKeyboard)
{
    RefPtr page = m_page.get();
    return page && page->protectedFullscreenManager()->supportsFullScreenForElement(element, withKeyboard);
}

void WebChromeClient::enterFullScreenForElement(Element& element, HTMLMediaElementEnums::VideoFullscreenMode mode, CompletionHandler<void(ExceptionOr<void>)>&& willEnterFullscreen, CompletionHandler<bool(bool)>&& didEnterFullscreen)
{
    RefPtr page = m_page.get();
    ASSERT(page);
    page->protectedFullscreenManager()->enterFullScreenForElement(element, mode, WTFMove(willEnterFullscreen), WTFMove(didEnterFullscreen));
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (RefPtr videoElement = dynamicDowncast<HTMLVideoElement>(element); videoElement && mode == HTMLMediaElementEnums::VideoFullscreenModeInWindow)
        setVideoFullscreenMode(*videoElement, mode);
#endif
}

#if ENABLE(QUICKLOOK_FULLSCREEN)
void WebChromeClient::updateImageSource(Element& element)
{
    if (RefPtr page = m_page.get())
        page->fullScreenManager().updateImageSource(element);
}
#endif // ENABLE(QUICKLOOK_FULLSCREEN)

void WebChromeClient::exitFullScreenForElement(Element* element, CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(VIDEO_PRESENTATION_MODE)
    bool exitingInWindowFullscreen = false;
    if (element) {
        if (RefPtr videoElement = dynamicDowncast<HTMLVideoElement>(*element))
            exitingInWindowFullscreen = videoElement->fullscreenMode() == HTMLMediaElementEnums::VideoFullscreenModeInWindow;
    }
#endif
    if (RefPtr page = m_page.get())
        page->protectedFullscreenManager()->exitFullScreenForElement(element, WTFMove(completionHandler));
#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (exitingInWindowFullscreen)
        clearVideoFullscreenMode(*dynamicDowncast<HTMLVideoElement>(*element), HTMLMediaElementEnums::VideoFullscreenModeInWindow);
#endif
}

#endif

#if PLATFORM(IOS_FAMILY)

FloatSize WebChromeClient::screenSize() const
{
    RefPtr page = m_page.get();
    return page ? page->screenSize() : FloatSize();
}

FloatSize WebChromeClient::availableScreenSize() const
{
    RefPtr page = m_page.get();
    return page ? page->availableScreenSize() : FloatSize();
}

FloatSize WebChromeClient::overrideScreenSize() const
{
    RefPtr page = m_page.get();
    return page ? page->overrideScreenSize() : FloatSize();
}

FloatSize WebChromeClient::overrideAvailableScreenSize() const
{
    RefPtr page = m_page.get();
    return page ? page->overrideAvailableScreenSize() : FloatSize();
}

#endif

FloatSize WebChromeClient::screenSizeForFingerprintingProtections(const LocalFrame& frame, FloatSize defaultSize) const
{
    RefPtr page = m_page.get();
    return page ? page->screenSizeForFingerprintingProtections(frame, defaultSize) : FloatSize();
}

void WebChromeClient::dispatchDisabledAdaptationsDidChange(const OptionSet<DisabledAdaptations>& disabledAdaptations) const
{
    if (RefPtr page = m_page.get())
        page->disabledAdaptationsDidChange(disabledAdaptations);
}

void WebChromeClient::dispatchViewportPropertiesDidChange(const ViewportArguments& viewportArguments) const
{
    if (RefPtr page = m_page.get())
        page->viewportPropertiesDidChange(viewportArguments);
}

void WebChromeClient::notifyScrollerThumbIsVisibleInRect(const IntRect& scrollerThumb)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::NotifyScrollerThumbIsVisibleInRect(scrollerThumb));
}

void WebChromeClient::recommendedScrollbarStyleDidChange(ScrollbarStyle newStyle)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::RecommendedScrollbarStyleDidChange(static_cast<int32_t>(newStyle)));
}

std::optional<ScrollbarOverlayStyle> WebChromeClient::preferredScrollbarOverlayStyle()
{
    RefPtr page = m_page.get();
    return page ? page->scrollbarOverlayStyle() : std::nullopt;
}

Color WebChromeClient::underlayColor() const
{
    RefPtr page = m_page.get();
    return page ? page->underlayColor() : Color();
}

void WebChromeClient::themeColorChanged() const
{
    if (RefPtr page = m_page.get())
        page->themeColorChanged();
}

void WebChromeClient::pageExtendedBackgroundColorDidChange() const
{
    if (RefPtr page = m_page.get())
        page->pageExtendedBackgroundColorDidChange();
}

void WebChromeClient::sampledPageTopColorChanged() const
{
    if (RefPtr page = m_page.get())
        page->sampledPageTopColorChanged();
}

#if ENABLE(WEB_PAGE_SPATIAL_BACKDROP)
void WebChromeClient::spatialBackdropSourceChanged() const
{
    if (RefPtr page = m_page.get())
        page->spatialBackdropSourceChanged();
}
#endif

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
void WebChromeClient::canEnterImmersiveElement(const Element& element, CompletionHandler<void(bool)>&& completion) const
{
    if (RefPtr page = m_page.get())
        page->canEnterImmersiveElement(element, WTFMove(completion));
    else
        completion(false);
}
#endif

#if ENABLE(APP_HIGHLIGHTS)
WebCore::HighlightVisibility WebChromeClient::appHighlightsVisiblility() const
{
    if (!m_page)
        return { };

    return m_page->appHighlightsVisiblility();
}
#endif

void WebChromeClient::wheelEventHandlersChanged(bool hasHandlers)
{
    if (RefPtr page = m_page.get())
        page->wheelEventHandlersChanged(hasHandlers);
}

void WebChromeClient::enableSuddenTermination()
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebProcessProxy::EnableSuddenTermination());
}

void WebChromeClient::disableSuddenTermination()
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebProcessProxy::DisableSuddenTermination());
}

void WebChromeClient::didAddHeaderLayer(GraphicsLayer& headerParent)
{
#if HAVE(RUBBER_BANDING)
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (RefPtr banner = page->headerPageBanner())
        banner->didAddParentLayer(&headerParent);
#else
    UNUSED_PARAM(headerParent);
#endif
}

void WebChromeClient::didAddFooterLayer(GraphicsLayer& footerParent)
{
#if HAVE(RUBBER_BANDING)
    RefPtr page = m_page.get();
    if (!page)
        return;

    if (RefPtr banner = page->footerPageBanner())
        banner->didAddParentLayer(&footerParent);
#else
    UNUSED_PARAM(footerParent);
#endif
}

bool WebChromeClient::shouldUseTiledBackingForFrameView(const LocalFrameView& frameView) const
{
    RefPtr page = m_page.get();
    return page && page->protectedDrawingArea()->shouldUseTiledBackingForFrameView(frameView);
}

void WebChromeClient::frameViewLayoutOrVisualViewportChanged(const LocalFrameView& frameView)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    page->frameViewLayoutOrVisualViewportChanged(frameView);
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
void WebChromeClient::isAnyAnimationAllowedToPlayDidChange(bool anyAnimationCanPlay)
{
    if (RefPtr page = m_page.get())
        page->isAnyAnimationAllowedToPlayDidChange(anyAnimationCanPlay);
}
#endif

void WebChromeClient::resolveAccessibilityHitTestForTesting(FrameIdentifier frameID, const IntPoint& point, CompletionHandler<void(String)>&& callback)
{
    if (RefPtr page = m_page.get())
        page->sendWithAsyncReply(Messages::WebPageProxy::ResolveAccessibilityHitTestForTesting(frameID, point), WTFMove(callback));
    else
        callback({ });
}

void WebChromeClient::isPlayingMediaDidChange(MediaProducerMediaStateFlags state)
{
    if (RefPtr page = m_page.get())
        page->isPlayingMediaDidChange(state);
}

void WebChromeClient::handleAutoplayEvent(AutoplayEvent event, OptionSet<AutoplayEventFlags> flags)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::HandleAutoplayEvent(event, flags));
}

void WebChromeClient::setTextIndicator(RefPtr<WebCore::TextIndicator>&& textIndicator) const
{
    if (RefPtr page = m_page.get())
        page->setTextIndicator(WTFMove(textIndicator));
}

void WebChromeClient::updateTextIndicator(RefPtr<WebCore::TextIndicator>&& textIndicator) const
{
    if (RefPtr page = m_page.get())
        page->updateTextIndicator(WTFMove(textIndicator));
}

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)

void WebChromeClient::handleTelephoneNumberClick(const String& number, const IntPoint& point, const IntRect& rect)
{
    if (RefPtr page = m_page.get())
        page->handleTelephoneNumberClick(number, point, rect);
}

#endif

#if ENABLE(DATA_DETECTION)

void WebChromeClient::handleClickForDataDetectionResult(const DataDetectorElementInfo& info, const IntPoint& clickLocation)
{
    if (RefPtr page = m_page.get())
        page->handleClickForDataDetectionResult(info, clickLocation);
}

#endif

#if ENABLE(SERVICE_CONTROLS)

void WebChromeClient::handleSelectionServiceClick(WebCore::FrameIdentifier frameID, FrameSelection& selection, const Vector<String>& telephoneNumbers, const IntPoint& point)
{
    if (RefPtr page = m_page.get())
        page->handleSelectionServiceClick(frameID, selection, telephoneNumbers, point);
}

bool WebChromeClient::hasRelevantSelectionServices(bool isTextOnly) const
{
    return (isTextOnly && WebProcess::singleton().hasSelectionServices()) || WebProcess::singleton().hasRichContentServices();
}

void WebChromeClient::handleImageServiceClick(WebCore::FrameIdentifier frameID, const IntPoint& point, Image& image, HTMLImageElement& element)
{
    if (RefPtr page = m_page.get())
        page->handleImageServiceClick(frameID, point, image, element);
}

void WebChromeClient::handlePDFServiceClick(WebCore::FrameIdentifier frameID, const IntPoint& point, HTMLAttachmentElement& element)
{
    if (RefPtr page = m_page.get())
        page->handlePDFServiceClick(frameID, point, element);
}

#endif

bool WebChromeClient::shouldDispatchFakeMouseMoveEvents() const
{
    RefPtr page = m_page.get();
    return page && page->shouldDispatchFakeMouseMoveEvents();
}

RefPtr<API::Object> userDataFromJSONData(JSON::Value& value)
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
        for (auto [key, value] : *value.asObject().unsafeGet())
            result->add(key, userDataFromJSONData(value));
        return result;
    }
    case JSON::Value::Type::Array: {
        auto array = value.asArray();
        Vector<RefPtr<API::Object>> result;
        for (auto& item : *value.asArray().unsafeGet())
            result.append(userDataFromJSONData(item));
        return API::Array::create(WTFMove(result));
    }
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

void WebChromeClient::handleAutoFillButtonClick(HTMLInputElement& inputElement)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    RefPtr<API::Object> userData;

    // Notify the bundle client.
    auto nodeHandle = InjectedBundleNodeHandle::getOrCreate(inputElement);
    page->injectedBundleUIClient().didClickAutoFillButton(*page, nodeHandle.get(), userData);

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
    RefPtr page = m_page.get();
    if (!page)
        return;
    page->send(Messages::WebPageProxy::DidResignInputElementStrongPasswordAppearance { UserData { } });
}

void WebChromeClient::performSwitchHapticFeedback()
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::PerformSwitchHapticFeedback());
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

void WebChromeClient::addPlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::AddPlaybackTargetPickerClient(contextId));
}

void WebChromeClient::removePlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier contextId)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::RemovePlaybackTargetPickerClient(contextId));
}

void WebChromeClient::showPlaybackTargetPicker(PlaybackTargetClientContextIdentifier contextId, const IntPoint& position, bool isVideo)
{
    RefPtr page = m_page.get();
    if (!page)
        return;

    RefPtr frameView = page->localMainFrameView();
    if (!frameView)
        return;

    FloatRect rect(frameView->contentsToRootView(frameView->windowToContents(position)), FloatSize());
    page->send(Messages::WebPageProxy::ShowPlaybackTargetPicker(contextId, rect, isVideo));
}

void WebChromeClient::playbackTargetPickerClientStateDidChange(PlaybackTargetClientContextIdentifier contextId, MediaProducerMediaStateFlags state)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::PlaybackTargetPickerClientStateDidChange(contextId, state));
}

void WebChromeClient::setMockMediaPlaybackTargetPickerEnabled(bool enabled)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::SetMockMediaPlaybackTargetPickerEnabled(enabled));
}

void WebChromeClient::setMockMediaPlaybackTargetPickerState(const String& name, MediaPlaybackTargetMockState state)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::SetMockMediaPlaybackTargetPickerState(name, state));
}

void WebChromeClient::mockMediaPlaybackTargetPickerDismissPopup()
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::MockMediaPlaybackTargetPickerDismissPopup());
}
#endif

void WebChromeClient::imageOrMediaDocumentSizeChanged(const IntSize& newSize)
{
    if (RefPtr page = m_page.get())
        page->imageOrMediaDocumentSizeChanged(newSize);
}

void WebChromeClient::didInvalidateDocumentMarkerRects()
{
    if (RefPtr page = m_page.get())
        page->findController().didInvalidateFindRects();
}

void WebChromeClient::hasStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, LocalFrame& frame, CompletionHandler<void(bool)>&& completionHandler)
{
    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    if (RefPtr page = m_page.get())
        page->hasStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), *webFrame, WTFMove(completionHandler));
    else
        completionHandler(false);
}

void WebChromeClient::requestStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, LocalFrame& frame, StorageAccessScope scope, HasOrShouldIgnoreUserGesture hasOrShouldIgnoreUserGesture, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler)
{
    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    if (RefPtr page = m_page.get())
        page->requestStorageAccess(WTFMove(subFrameDomain), WTFMove(topFrameDomain), *webFrame, scope, hasOrShouldIgnoreUserGesture, WTFMove(completionHandler));
    else
        completionHandler({ });
}

void WebChromeClient::setLoginStatus(RegistrableDomain&& domain, IsLoggedIn loggedInStatus, CompletionHandler<void()>&& completionHandler)
{
    if (RefPtr page = m_page.get())
        page->setLoginStatus(WTFMove(domain), loggedInStatus, WTFMove(completionHandler));
    else
        completionHandler();
}

void WebChromeClient::isLoggedIn(RegistrableDomain&& domain, CompletionHandler<void(bool)>&& completionHandler)
{
    if (RefPtr page = m_page.get())
        page->isLoggedIn(WTFMove(domain), WTFMove(completionHandler));
    else
        completionHandler(false);
}

bool WebChromeClient::hasPageLevelStorageAccess(const WebCore::RegistrableDomain& topLevelDomain, const WebCore::RegistrableDomain& resourceDomain) const
{
    RefPtr page = m_page.get();
    return page && page->hasPageLevelStorageAccess(topLevelDomain, resourceDomain);
}

#if ENABLE(DEVICE_ORIENTATION)
void WebChromeClient::shouldAllowDeviceOrientationAndMotionAccess(LocalFrame& frame, bool mayPrompt, CompletionHandler<void(DeviceOrientationOrMotionPermissionState)>&& callback)
{
    auto webFrame = WebFrame::fromCoreFrame(frame);
    ASSERT(webFrame);
    if (RefPtr page = m_page.get())
        page->shouldAllowDeviceOrientationAndMotionAccess(webFrame->frameID(), webFrame->info(), mayPrompt, WTFMove(callback));
    else
        callback(DeviceOrientationOrMotionPermissionState::Denied);
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
    if (RefPtr page = m_page.get())
        page->configureLoggingChannel(channelName, state, level);
}

bool WebChromeClient::userIsInteracting() const
{
    RefPtr page = m_page.get();
    return page && page->userIsInteracting();
}

void WebChromeClient::setUserIsInteracting(bool userIsInteracting)
{
    if (RefPtr page = m_page.get())
        page->setUserIsInteracting(userIsInteracting);
}

#if ENABLE(WEB_AUTHN)
void WebChromeClient::setMockWebAuthenticationConfiguration(const MockWebAuthenticationConfiguration& configuration)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::SetMockWebAuthenticationConfiguration(configuration));
}
#endif

#if PLATFORM(PLAYSTATION)
void WebChromeClient::postAccessibilityNotification(WebCore::AccessibilityObject&, WebCore::AXNotification)
{
    notImplemented();
}

void WebChromeClient::postAccessibilityNodeTextChangeNotification(WebCore::AccessibilityObject*, WebCore::AXTextChange, unsigned, const String&)
{
    notImplemented();
}

void WebChromeClient::postAccessibilityFrameLoadingEventNotification(WebCore::AccessibilityObject*, WebCore::AXLoadingEvent)
{
    notImplemented();
}
#endif

void WebChromeClient::animationDidFinishForElement(const Element& element)
{
    if (RefPtr page = m_page.get())
        page->animationDidFinishForElement(element);
}

#if PLATFORM(MAC)
void WebChromeClient::changeUniversalAccessZoomFocus(const WebCore::IntRect& viewRect, const WebCore::IntRect& selectionRect)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::ChangeUniversalAccessZoomFocus(viewRect, selectionRect));
}
#endif

#if ENABLE(IMAGE_ANALYSIS)

void WebChromeClient::requestTextRecognition(Element& element, TextRecognitionOptions&& options, CompletionHandler<void(RefPtr<Element>&&)>&& completion)
{
    if (RefPtr page = m_page.get())
        page->requestTextRecognition(element, WTFMove(options), WTFMove(completion));
    else
        completion(nullptr);
}

#endif

std::pair<URL, DidFilterLinkDecoration> WebChromeClient::applyLinkDecorationFilteringWithResult(const URL& url, LinkDecorationFilteringTrigger trigger) const
{
    RefPtr page = m_page.get();
    if (!page)
        return { };
    return page->applyLinkDecorationFilteringWithResult(url, trigger);
}

URL WebChromeClient::allowedQueryParametersForAdvancedPrivacyProtections(const URL& url) const
{
    RefPtr page = m_page.get();
    return page ? page->allowedQueryParametersForAdvancedPrivacyProtections(url) : URL();
}

void WebChromeClient::didAddOrRemoveViewportConstrainedObjects()
{
    if (RefPtr page = m_page.get())
        page->didAddOrRemoveViewportConstrainedObjects();
}

#if ENABLE(TEXT_AUTOSIZING)

void WebChromeClient::textAutosizingUsesIdempotentModeChanged()
{
    if (RefPtr page = m_page.get())
        page->textAutosizingUsesIdempotentModeChanged();
}

#endif

bool WebChromeClient::needsScrollGeometryUpdates() const
{
    if (RefPtr page = m_page.get())
        return page->needsScrollGeometryUpdates();

    return false;
}

#if ENABLE(META_VIEWPORT)

double WebChromeClient::baseViewportLayoutSizeScaleFactor() const
{
    RefPtr page = m_page.get();
    return page ? page->baseViewportLayoutSizeScaleFactor() : 0;
}

#endif

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

void WebChromeClient::showMediaControlsContextMenu(FloatRect&& targetFrame, Vector<MediaControlsContextMenuItem>&& items, HTMLMediaElement& element, CompletionHandler<void(MediaControlsContextMenuItem::ID)>&& completionHandler)
{
    if (RefPtr page = m_page.get())
        page->showMediaControlsContextMenu(WTFMove(targetFrame), WTFMove(items), element.identifier(), WTFMove(completionHandler));
    else
        completionHandler({ });
}

#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if ENABLE(WEBXR)
void WebChromeClient::enumerateImmersiveXRDevices(CompletionHandler<void(const PlatformXR::DeviceList&)>&& completionHandler)
{
    if (RefPtr page = m_page.get())
        page->xrSystemProxy().enumerateImmersiveXRDevices(WTFMove(completionHandler));
    else
        completionHandler({ });
}

void WebChromeClient::requestPermissionOnXRSessionFeatures(const SecurityOriginData& origin, PlatformXR::SessionMode mode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& consentRequired, const PlatformXR::Device::FeatureList& consentOptional, const PlatformXR::Device::FeatureList& requiredFeaturesRequested, const PlatformXR::Device::FeatureList& optionalFeaturesRequested,  CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler)
{
    if (RefPtr page = m_page.get())
        page->xrSystemProxy().requestPermissionOnSessionFeatures(origin, mode, granted, consentRequired, consentOptional, requiredFeaturesRequested, optionalFeaturesRequested, WTFMove(completionHandler));
    else
        completionHandler(std::nullopt);
}
#endif

#if ENABLE(APPLE_PAY_AMS_UI)

void WebChromeClient::startApplePayAMSUISession(const URL& originatingURL, const ApplePayAMSUIRequest& request, CompletionHandler<void(std::optional<bool>&&)>&& completionHandler)
{
    if (RefPtr page = m_page.get())
        page->sendWithAsyncReply(Messages::WebPageProxy::StartApplePayAMSUISession(originatingURL, request), WTFMove(completionHandler));
    else
        completionHandler(false);
}

void WebChromeClient::abortApplePayAMSUISession()
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::AbortApplePayAMSUISession());
}

#endif // ENABLE(APPLE_PAY_AMS_UI)

#if USE(SYSTEM_PREVIEW)
void WebChromeClient::beginSystemPreview(const URL& url, const SecurityOriginData& topOrigin, const SystemPreviewInfo& systemPreviewInfo, CompletionHandler<void()>&& completionHandler)
{
    if (RefPtr page = m_page.get())
        page->sendWithAsyncReply(Messages::WebPageProxy::BeginSystemPreview(url, topOrigin, systemPreviewInfo), WTFMove(completionHandler));
    else
        completionHandler();
}
#endif

void WebChromeClient::requestCookieConsent(CompletionHandler<void(CookieConsentDecisionResult)>&& completion)
{
    if (RefPtr page = m_page.get())
        page->sendWithAsyncReply(Messages::WebPageProxy::RequestCookieConsent(), WTFMove(completion));
    else
        completion(CookieConsentDecisionResult::NotSupported);
}

bool WebChromeClient::isUsingUISideCompositing() const
{
#if ENABLE(TILED_CA_DRAWING_AREA)
    RefPtr page = m_page.get();
    return page && page->protectedDrawingArea()->type() == DrawingAreaType::RemoteLayerTree;
#elif PLATFORM(COCOA)
    return true;
#else
    return false;
#endif
}

bool WebChromeClient::isInStableState() const
{
#if PLATFORM(IOS_FAMILY)
    RefPtr page = m_page.get();
    return page && page->isInStableState();
#else
    // FIXME (255877): Implement this client hook on macOS.
    return true;
#endif
}

void WebChromeClient::didAdjustVisibilityWithSelectors(Vector<String>&& selectors)
{
    if (RefPtr page = m_page.get())
        page->didAdjustVisibilityWithSelectors(WTFMove(selectors));
}

#if ENABLE(GAMEPAD)
void WebChromeClient::gamepadsRecentlyAccessed()
{
    if (RefPtr page = m_page.get())
        page->gamepadsRecentlyAccessed();
}
#endif

#if ENABLE(WRITING_TOOLS)

void WebChromeClient::proofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(const WebCore::WritingTools::TextSuggestion::ID& replacementID, WebCore::IntRect selectionBoundsInRootView)
{
    if (RefPtr page = m_page.get())
        page->proofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(replacementID, selectionBoundsInRootView);
}

void WebChromeClient::proofreadingSessionUpdateStateForSuggestionWithID(WritingTools::TextSuggestion::State state, const WritingTools::TextSuggestion::ID& replacementID)
{
    if (RefPtr page = m_page.get())
        page->proofreadingSessionUpdateStateForSuggestionWithID(state, replacementID);
}

void WebChromeClient::removeTextAnimationForAnimationID(const WTF::UUID& animationID)
{
    if (RefPtr page = m_page.get())
        page->removeTextAnimationForAnimationID(animationID);
}

void WebChromeClient::removeInitialTextAnimationForActiveWritingToolsSession()
{
    if (RefPtr page = m_page.get())
        page->removeInitialTextAnimationForActiveWritingToolsSession();
}

void WebChromeClient::addInitialTextAnimationForActiveWritingToolsSession()
{
    if (RefPtr page = m_page.get())
        page->addInitialTextAnimationForActiveWritingToolsSession();
}

void WebChromeClient::addSourceTextAnimationForActiveWritingToolsSession(const WTF::UUID& sourceAnimationUUID, const WTF::UUID& destinationAnimationUUID, bool finished, const CharacterRange& range, const String& string, CompletionHandler<void(WebCore::TextAnimationRunMode)>&& completionHandler)
{
    if (RefPtr page = m_page.get())
        page->addSourceTextAnimationForActiveWritingToolsSession(sourceAnimationUUID, destinationAnimationUUID, finished, range, string, WTFMove(completionHandler));
}

void WebChromeClient::addDestinationTextAnimationForActiveWritingToolsSession(const WTF::UUID& sourceAnimationUUID, const WTF::UUID& destinationAnimationUUID, const std::optional<CharacterRange>& range, const String& string)
{
    if (RefPtr page = m_page.get())
        page->addDestinationTextAnimationForActiveWritingToolsSession(sourceAnimationUUID, destinationAnimationUUID, range, string);
}

void WebChromeClient::saveSnapshotOfTextPlaceholderForAnimation(const WebCore::SimpleRange& placeholderRange)
{
    if (RefPtr page = m_page.get())
        page->saveSnapshotOfTextPlaceholderForAnimation(placeholderRange);
}

void WebChromeClient::clearAnimationsForActiveWritingToolsSession()
{
    if (RefPtr page = m_page.get())
        page->clearAnimationsForActiveWritingToolsSession();
}

#endif

void WebChromeClient::setIsInRedo(bool isInRedo)
{
    if (RefPtr page = m_page.get())
        page->setIsInRedo(isInRedo);
}

void WebChromeClient::hasActiveNowPlayingSessionChanged(bool hasActiveNowPlayingSession)
{
    if (RefPtr page = m_page.get())
        page->hasActiveNowPlayingSessionChanged(hasActiveNowPlayingSession);
}

#if ENABLE(GPU_PROCESS)
void WebChromeClient::getImageBufferResourceLimitsForTesting(CompletionHandler<void(std::optional<ImageBufferResourceLimits>)>&& callback) const
{
    if (RefPtr page = m_page.get())
        page->ensureProtectedRemoteRenderingBackendProxy()->getImageBufferResourceLimitsForTesting(WTFMove(callback));
    else
        callback(std::nullopt);
}
#endif

bool WebChromeClient::requiresScriptTrackingPrivacyProtections(const URL& url, const SecurityOrigin& topOrigin) const
{
    return WebProcess::singleton().requiresScriptTrackingPrivacyProtections(url, topOrigin);
}

bool WebChromeClient::shouldAllowScriptAccess(const URL& url, const SecurityOrigin& topOrigin, ScriptTrackingPrivacyCategory category) const
{
    return WebProcess::singleton().shouldAllowScriptAccess(url, topOrigin, category);
}

void WebChromeClient::callAfterPendingSyntheticClick(CompletionHandler<void(SyntheticClickResult)>&& completion)
{
    if (RefPtr page = m_page.get())
        page->callAfterPendingSyntheticClick(WTFMove(completion));
    else
        completion(SyntheticClickResult::Failed);
}

void WebChromeClient::didDispatchClickEvent(const PlatformMouseEvent& event, Node& node)
{
    if (RefPtr page = m_page.get())
        page->didDispatchClickEvent(event, node);
}

void WebChromeClient::didProgrammaticallyClearTextFormControl(const HTMLTextFormControlElement& element)
{
    if (RefPtr page = m_page.get())
        page->didProgrammaticallyClearTextFormControl(element);
}

#if ENABLE(DAMAGE_TRACKING)
void WebChromeClient::resetDamageHistoryForTesting()
{
    if (!m_page)
        return;

    if (RefPtr drawingArea = m_page->drawingArea())
        drawingArea->resetDamageHistoryForTesting();
}

void WebChromeClient::foreachRegionInDamageHistoryForTesting(Function<void(const Region&)>&& callback) const
{
    if (!m_page)
        return;

    if (const RefPtr drawingArea = m_page->drawingArea())
        drawingArea->foreachRegionInDamageHistoryForTesting(WTFMove(callback));
}
#endif

void WebChromeClient::setNeedsFixedContainerEdgesUpdate()
{
    m_page->setNeedsFixedContainerEdgesUpdate();
}

bool WebChromeClient::usePluginRendererScrollableArea(LocalFrame& frame) const
{
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = WebPage::pluginViewForFrame(&frame))
        return !pluginView->pluginDelegatesScrollingToMainFrame();
#endif
    return true;
}

#if ENABLE(VIDEO)
void WebChromeClient::showCaptionDisplaySettings(HTMLMediaElement& element, const ResolvedCaptionDisplaySettingsOptions& options, CompletionHandler<void(ExceptionOr<void>)>&& completionHandler)
{
    RefPtr page = m_page.get();
    if (!page) {
        completionHandler(Exception { ExceptionCode::NotSupportedError, "Caption Display Settings are not supported."_s });
        return;
    }

    page->sendWithAsyncReply(Messages::WebPageProxy::ShowCaptionDisplaySettings(element.identifier(), options), [completionHandler = WTFMove(completionHandler)] (auto&& expected) mutable {
        if (expected)
            completionHandler({ });
        else
            completionHandler(expected.error().toException());
    });
}
#endif

} // namespace WebKit
