/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebInspectorProxy.h"

#include "APIProcessPoolConfiguration.h"
#include "APIURLRequest.h"
#include "WKArray.h"
#include "WKContextMenuItem.h"
#include "WKMutableArray.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebFrameProxy.h"
#include "WebInspectorMessages.h"
#include "WebInspectorProxyMessages.h"
#include "WebInspectorUIMessages.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/SchemeRegistry.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(INSPECTOR_SERVER)
#include "WebInspectorServer.h"
#endif

using namespace WebCore;

namespace WebKit {

const unsigned WebInspectorProxy::minimumWindowWidth = 500;
const unsigned WebInspectorProxy::minimumWindowHeight = 400;

const unsigned WebInspectorProxy::initialWindowWidth = 1000;
const unsigned WebInspectorProxy::initialWindowHeight = 650;

typedef HashMap<WebPageProxy*, unsigned> PageLevelMap;

static PageLevelMap& pageLevelMap()
{
    static NeverDestroyed<PageLevelMap> map;
    return map;
}

WebInspectorProxy::WebInspectorProxy(WebPageProxy* inspectedPage)
    : m_inspectedPage(inspectedPage)
#if PLATFORM(MAC) && WK_API_ENABLED
    , m_closeTimer(RunLoop::main(), this, &WebInspectorProxy::closeTimerFired)
#endif
{
    m_inspectedPage->process().addMessageReceiver(Messages::WebInspectorProxy::messageReceiverName(), m_inspectedPage->pageID(), *this);
}

WebInspectorProxy::~WebInspectorProxy()
{
}

unsigned WebInspectorProxy::inspectionLevel() const
{
    auto findResult = pageLevelMap().find(inspectedPage());
    if (findResult != pageLevelMap().end())
        return findResult->value + 1;

    return 1;
}

String WebInspectorProxy::inspectorPageGroupIdentifier() const
{
    return String::format("__WebInspectorPageGroupLevel%u__", inspectionLevel());
}

WebPreferences& WebInspectorProxy::inspectorPagePreferences() const
{
    ASSERT(m_inspectorPage);
    return m_inspectorPage->pageGroup().preferences();
}

void WebInspectorProxy::invalidate()
{
#if ENABLE(INSPECTOR_SERVER)
    if (m_remoteInspectionPageId)
        WebInspectorServer::singleton().unregisterPage(m_remoteInspectionPageId);
#endif

    // We can be called reentrantly through platformInvalidate(), in which case nothing needs to be done.
    if (!m_inspectedPage)
        return;

    m_inspectedPage->process().removeMessageReceiver(Messages::WebInspectorProxy::messageReceiverName(), m_inspectedPage->pageID());

    pageLevelMap().remove(m_inspectedPage);
    m_inspectedPage = nullptr;

    didClose();
    platformInvalidate();
}

// Public APIs
bool WebInspectorProxy::isFront()
{
    if (!m_inspectedPage)
        return false;

    return platformIsFront();
}

void WebInspectorProxy::connect()
{
    if (!m_inspectedPage)
        return;

    if (m_showMessageSent)
        return;

    m_showMessageSent = true;
    m_ignoreFirstBringToFront = true;

    eagerlyCreateInspectorPage();

    m_inspectedPage->process().send(Messages::WebInspector::Show(), m_inspectedPage->pageID());
}

void WebInspectorProxy::show()
{
    if (!m_inspectedPage)
        return;

    if (isConnected()) {
        bringToFront();
        return;
    }

    connect();

    // Don't ignore the first bringToFront so it opens the Inspector.
    m_ignoreFirstBringToFront = false;
}

void WebInspectorProxy::hide()
{
    if (!m_inspectedPage)
        return;

    m_isVisible = false;

    platformHide();
}

void WebInspectorProxy::close()
{
    if (!m_inspectedPage)
        return;

    m_inspectedPage->process().send(Messages::WebInspector::Close(), m_inspectedPage->pageID());

    didClose();
}

void WebInspectorProxy::didRelaunchInspectorPageProcess()
{
    if (inspectionLevel() == 1)
        m_inspectorPage->process().addMessageReceiver(Messages::WebInspectorProxy::messageReceiverName(), m_inspectedPage->pageID(), *this);
    m_inspectorPage->process().assumeReadAccessToBaseURL(WebInspectorProxy::inspectorBaseURL());

    // When didRelaunchInspectorPageProcess is called we can assume it is during a load request.
    // Any messages we would have sent to a terminated process need to be re-sent.

    m_inspectorPage->process().send(Messages::WebInspectorUI::EstablishConnection(m_connectionIdentifier, m_inspectedPage->pageID(), m_underTest, inspectionLevel()), m_inspectorPage->pageID());
}

void WebInspectorProxy::showConsole()
{
    if (!m_inspectedPage)
        return;

    eagerlyCreateInspectorPage();

    m_inspectedPage->process().send(Messages::WebInspector::ShowConsole(), m_inspectedPage->pageID());
}

void WebInspectorProxy::showResources()
{
    if (!m_inspectedPage)
        return;

    eagerlyCreateInspectorPage();

    m_inspectedPage->process().send(Messages::WebInspector::ShowResources(), m_inspectedPage->pageID());
}

void WebInspectorProxy::showMainResourceForFrame(WebFrameProxy* frame)
{
    if (!m_inspectedPage)
        return;

    eagerlyCreateInspectorPage();

    m_inspectedPage->process().send(Messages::WebInspector::ShowMainResourceForFrame(frame->frameID()), m_inspectedPage->pageID());
}

void WebInspectorProxy::attachBottom()
{
    attach(AttachmentSide::Bottom);
}

void WebInspectorProxy::attachRight()
{
    attach(AttachmentSide::Right);
}

void WebInspectorProxy::attach(AttachmentSide side)
{
    if (!m_inspectedPage || !canAttach())
        return;

    m_isAttached = true;
    m_attachmentSide = side;

    inspectorPagePreferences().setInspectorAttachmentSide(static_cast<uint32_t>(side));

    if (m_isVisible)
        inspectorPagePreferences().setInspectorStartsAttached(true);

    m_inspectedPage->process().send(Messages::WebInspector::SetAttached(true), m_inspectedPage->pageID());

    switch (m_attachmentSide) {
    case AttachmentSide::Bottom:
        m_inspectorPage->process().send(Messages::WebInspectorUI::AttachedBottom(), m_inspectorPage->pageID());
        break;

    case AttachmentSide::Right:
        m_inspectorPage->process().send(Messages::WebInspectorUI::AttachedRight(), m_inspectorPage->pageID());
        break;
    }

    platformAttach();
}

void WebInspectorProxy::detach()
{
    if (!m_inspectedPage)
        return;

    m_isAttached = false;

    if (m_isVisible)
        inspectorPagePreferences().setInspectorStartsAttached(false);

    m_inspectedPage->process().send(Messages::WebInspector::SetAttached(false), m_inspectedPage->pageID());
    m_inspectorPage->process().send(Messages::WebInspectorUI::Detached(), m_inspectorPage->pageID());

    platformDetach();
}

void WebInspectorProxy::setAttachedWindowHeight(unsigned height)
{
    inspectorPagePreferences().setInspectorAttachedHeight(height);
    platformSetAttachedWindowHeight(height);
}

void WebInspectorProxy::setAttachedWindowWidth(unsigned width)
{
    inspectorPagePreferences().setInspectorAttachedWidth(width);
    platformSetAttachedWindowWidth(width);
}

void WebInspectorProxy::startWindowDrag()
{
    platformStartWindowDrag();
}

void WebInspectorProxy::togglePageProfiling()
{
    if (!m_inspectedPage)
        return;

    if (m_isProfilingPage)
        m_inspectedPage->process().send(Messages::WebInspector::StopPageProfiling(), m_inspectedPage->pageID());
    else
        m_inspectedPage->process().send(Messages::WebInspector::StartPageProfiling(), m_inspectedPage->pageID());

    // FIXME: have the WebProcess notify us on state changes.
    m_isProfilingPage = !m_isProfilingPage;
}

static WebProcessPool* s_processPool;

WebProcessPool& WebInspectorProxy::inspectorProcessPool()
{
    // Having our own process pool removes us from the main process pool and
    // guarantees no process sharing for our user interface.
    if (!s_processPool) {
        auto configuration = API::ProcessPoolConfiguration::createWithLegacyOptions();
        s_processPool = &WebProcessPool::create(configuration.get()).leakRef();
    };

    return *s_processPool;
}

bool WebInspectorProxy::isInspectorProcessPool(WebProcessPool& processPool)
{
    if (!s_processPool)
        return false;

    return s_processPool == &processPool;
}

bool WebInspectorProxy::isInspectorPage(WebPageProxy& webPage)
{
    return pageLevelMap().contains(&webPage);
}

static bool isMainOrTestInspectorPage(WKURLRequestRef requestRef)
{
    URL requestURL(URL(), toImpl(requestRef)->resourceRequest().url());
    if (!WebCore::SchemeRegistry::shouldTreatURLSchemeAsLocal(requestURL.protocol()))
        return false;

    // Use URL so we can compare just the paths.
    URL mainPageURL(URL(), WebInspectorProxy::inspectorPageURL());
    ASSERT(WebCore::SchemeRegistry::shouldTreatURLSchemeAsLocal(mainPageURL.protocol()));
    if (decodeURLEscapeSequences(requestURL.path()) == decodeURLEscapeSequences(mainPageURL.path()))
        return true;

    // We might not have a Test URL in Production builds.
    String testPageURLString = WebInspectorProxy::inspectorTestPageURL();
    if (testPageURLString.isNull())
        return false;

    URL testPageURL(URL(), testPageURLString);
    ASSERT(WebCore::SchemeRegistry::shouldTreatURLSchemeAsLocal(testPageURL.protocol()));
    return decodeURLEscapeSequences(requestURL.path()) == decodeURLEscapeSequences(testPageURL.path());
}

static void processDidCrash(WKPageRef, const void* clientInfo)
{
    WebInspectorProxy* webInspectorProxy = static_cast<WebInspectorProxy*>(const_cast<void*>(clientInfo));
    ASSERT(webInspectorProxy);
    webInspectorProxy->close();
}

static void decidePolicyForNavigationAction(WKPageRef, WKFrameRef frameRef, WKFrameNavigationType, WKEventModifiers, WKEventMouseButton, WKFrameRef, WKURLRequestRef requestRef, WKFramePolicyListenerRef listenerRef, WKTypeRef, const void* clientInfo)
{
    // Allow non-main frames to navigate anywhere.
    if (!toImpl(frameRef)->isMainFrame()) {
        toImpl(listenerRef)->use();
        return;
    }

    const WebInspectorProxy* webInspectorProxy = static_cast<const WebInspectorProxy*>(clientInfo);
    ASSERT(webInspectorProxy);

    // Allow loading of the main inspector file.
    if (isMainOrTestInspectorPage(requestRef)) {
        toImpl(listenerRef)->use();
        return;
    }

    // Prevent everything else from loading in the inspector's page.
    toImpl(listenerRef)->ignore();

    // And instead load it in the inspected page.
    webInspectorProxy->inspectedPage()->loadRequest(toImpl(requestRef)->resourceRequest());
}

static void getContextMenuFromProposedMenu(WKPageRef pageRef, WKArrayRef proposedMenuRef, WKArrayRef* newMenuRef, WKHitTestResultRef, WKTypeRef, const void*)
{
    WKMutableArrayRef menuItems = WKMutableArrayCreate();

    size_t count = WKArrayGetSize(proposedMenuRef);
    for (size_t i = 0; i < count; ++i) {
        WKContextMenuItemRef contextMenuItem = static_cast<WKContextMenuItemRef>(WKArrayGetItemAtIndex(proposedMenuRef, i));
        switch (WKContextMenuItemGetTag(contextMenuItem)) {
        case kWKContextMenuItemTagOpenLinkInNewWindow:
        case kWKContextMenuItemTagOpenImageInNewWindow:
        case kWKContextMenuItemTagOpenFrameInNewWindow:
        case kWKContextMenuItemTagOpenMediaInNewWindow:
        case kWKContextMenuItemTagDownloadLinkToDisk:
        case kWKContextMenuItemTagDownloadImageToDisk:
            break;
        default:
            WKArrayAppendItem(menuItems, contextMenuItem);
            break;
        }
    }

    *newMenuRef = menuItems;
}

#if ENABLE(INSPECTOR_SERVER)
void WebInspectorProxy::enableRemoteInspection()
{
    if (!m_remoteInspectionPageId)
        m_remoteInspectionPageId = WebInspectorServer::singleton().registerPage(this);
}

void WebInspectorProxy::remoteFrontendConnected()
{
    m_inspectedPage->process().send(Messages::WebInspector::RemoteFrontendConnected(), m_inspectedPage->pageID());
}

void WebInspectorProxy::remoteFrontendDisconnected()
{
    m_inspectedPage->process().send(Messages::WebInspector::RemoteFrontendDisconnected(), m_inspectedPage->pageID());
}

void WebInspectorProxy::dispatchMessageFromRemoteFrontend(const String& message)
{
    m_inspectedPage->process().send(Messages::WebInspector::SendMessageToBackend(message), m_inspectedPage->pageID());
}
#endif

void WebInspectorProxy::eagerlyCreateInspectorPage()
{
    if (m_inspectorPage)
        return;

    m_inspectorPage = platformCreateInspectorPage();
    ASSERT(m_inspectorPage);
    if (!m_inspectorPage)
        return;

    pageLevelMap().set(m_inspectorPage, inspectionLevel());

    WKPagePolicyClientV1 policyClient = {
        { 1, this },
        nullptr, // decidePolicyForNavigationAction_deprecatedForUseWithV0
        nullptr, // decidePolicyForNewWindowAction
        nullptr, // decidePolicyForResponse_deprecatedForUseWithV0
        nullptr, // unableToImplementPolicy
        decidePolicyForNavigationAction,
        nullptr, // decidePolicyForResponse
    };

    WKPageLoaderClientV5 loaderClient = {
        { 5, this },
        nullptr, // didStartProvisionalLoadForFrame
        nullptr, // didReceiveServerRedirectForProvisionalLoadForFrame
        nullptr, // didFailProvisionalLoadWithErrorForFrame
        nullptr, // didCommitLoadForFrame
        nullptr, // didFinishDocumentLoadForFrame
        nullptr, // didFinishLoadForFrame
        nullptr, // didFailLoadWithErrorForFrame
        nullptr, // didSameDocumentNavigationForFrame
        nullptr, // didReceiveTitleForFrame
        nullptr, // didFirstLayoutForFrame
        nullptr, // didFirstVisuallyNonEmptyLayoutForFrame
        nullptr, // didRemoveFrameFromHierarchy
        nullptr, // didDisplayInsecureContentForFrame
        nullptr, // didRunInsecureContentForFrame
        nullptr, // canAuthenticateAgainstProtectionSpaceInFrame
        nullptr, // didReceiveAuthenticationChallengeInFrame
        nullptr, // didStartProgress
        nullptr, // didChangeProgress
        nullptr, // didFinishProgress
        nullptr, // didBecomeUnresponsive
        nullptr, // didBecomeResponsive
        processDidCrash,
        nullptr, // didChangeBackForwardList
        nullptr, // shouldGoToBackForwardListItem
        nullptr, // didFailToInitializePlugin_deprecatedForUseWithV0
        nullptr, // didDetectXSSForFrame
        nullptr, // didNewFirstVisuallyNonEmptyLayout_unavailable
        nullptr, // willGoToBackForwardListItem
        nullptr, // interactionOccurredWhileProcessUnresponsive
        nullptr, // pluginDidFail_deprecatedForUseWithV1
        nullptr, // didReceiveIntentForFrame_unavailable
        nullptr, // registerIntentServiceForFrame_unavailable
        nullptr, // didLayout
        nullptr, // pluginLoadPolicy_deprecatedForUseWithV2
        nullptr, // pluginDidFail
        nullptr, // pluginLoadPolicy
        nullptr, // webGLLoadPolicy
        nullptr, // resolveWebGLLoadPolicy
        nullptr, // shouldKeepCurrentBackForwardListItemInList
    };

    WKPageContextMenuClientV3 contextMenuClient = {
        { 3, this },
        0, // getContextMenuFromProposedMenu_deprecatedForUseWithV0
        0, // customContextMenuItemSelected
        0, // contextMenuDismissed
        getContextMenuFromProposedMenu,
        0, // showContextMenu
        0, // hideContextMenu
    };

    WKPageSetPagePolicyClient(toAPI(m_inspectorPage), &policyClient.base);
    WKPageSetPageLoaderClient(toAPI(m_inspectorPage), &loaderClient.base);
    WKPageSetPageContextMenuClient(toAPI(m_inspectorPage), &contextMenuClient.base);

    if (inspectionLevel() == 1)
        m_inspectorPage->process().addMessageReceiver(Messages::WebInspectorProxy::messageReceiverName(), m_inspectedPage->pageID(), *this);
    m_inspectorPage->process().assumeReadAccessToBaseURL(WebInspectorProxy::inspectorBaseURL());
}

// Called by WebInspectorProxy messages
void WebInspectorProxy::createInspectorPage(IPC::Attachment connectionIdentifier, bool canAttach, bool underTest)
{
    if (!m_inspectedPage)
        return;

    m_underTest = underTest;
    eagerlyCreateInspectorPage();

    ASSERT(m_inspectorPage);
    if (!m_inspectorPage)
        return;

    m_connectionIdentifier = WTFMove(connectionIdentifier);

    m_inspectorPage->process().send(Messages::WebInspectorUI::EstablishConnection(m_connectionIdentifier, m_inspectedPage->pageID(), m_underTest, inspectionLevel()), m_inspectorPage->pageID());

    if (!m_underTest) {
        m_canAttach = platformCanAttach(canAttach);
        m_isAttached = shouldOpenAttached();
        m_attachmentSide = static_cast<AttachmentSide>(inspectorPagePreferences().inspectorAttachmentSide());

        m_inspectedPage->process().send(Messages::WebInspector::SetAttached(m_isAttached), m_inspectedPage->pageID());

        if (m_isAttached) {
            switch (m_attachmentSide) {
            case AttachmentSide::Bottom:
                m_inspectorPage->process().send(Messages::WebInspectorUI::AttachedBottom(), m_inspectorPage->pageID());
                break;

            case AttachmentSide::Right:
                m_inspectorPage->process().send(Messages::WebInspectorUI::AttachedRight(), m_inspectorPage->pageID());
                break;
            }
        } else
            m_inspectorPage->process().send(Messages::WebInspectorUI::Detached(), m_inspectorPage->pageID());

        m_inspectorPage->process().send(Messages::WebInspectorUI::SetDockingUnavailable(!m_canAttach), m_inspectorPage->pageID());
    }

    m_inspectorPage->loadRequest(URL(URL(), m_underTest ? WebInspectorProxy::inspectorTestPageURL() : WebInspectorProxy::inspectorPageURL()));
}

void WebInspectorProxy::open()
{
    if (m_underTest)
        return;

    m_isVisible = true;

    platformOpen();
}

void WebInspectorProxy::didClose()
{
    if (!m_inspectorPage)
        return;

    if (inspectionLevel() == 1)
        m_inspectorPage->process().removeMessageReceiver(Messages::WebInspectorProxy::messageReceiverName(), m_inspectorPage->pageID());

    m_inspectorPage = nullptr;

    m_isVisible = false;
    m_isProfilingPage = false;
    m_showMessageSent = false;
    m_ignoreFirstBringToFront = false;

    if (m_isAttached)
        platformDetach();
    m_isAttached = false;
    m_canAttach = false;
    m_underTest = false;
    m_connectionIdentifier = IPC::Attachment();

    platformDidClose();
}

void WebInspectorProxy::bringToFront()
{
    // WebCore::InspectorFrontendClientLocal tells us to do this on load. We want to
    // ignore it once if we only wanted to connect. This allows the Inspector to later
    // request to be brought to the front when a breakpoint is hit or some other action.
    if (m_ignoreFirstBringToFront) {
        m_ignoreFirstBringToFront = false;
        return;
    }

    if (m_isVisible)
        platformBringToFront();
    else
        open();
}

void WebInspectorProxy::attachAvailabilityChanged(bool available)
{
    bool previousCanAttach = m_canAttach;

    m_canAttach = platformCanAttach(available);

    if (previousCanAttach == m_canAttach)
        return;

    if (m_inspectorPage && !m_underTest)
        m_inspectorPage->process().send(Messages::WebInspectorUI::SetDockingUnavailable(!m_canAttach), m_inspectorPage->pageID());

    platformAttachAvailabilityChanged(m_canAttach);
}

void WebInspectorProxy::inspectedURLChanged(const String& urlString)
{
    platformInspectedURLChanged(urlString);
}

void WebInspectorProxy::save(const String& filename, const String& content, bool base64Encoded, bool forceSaveAs)
{
    platformSave(filename, content, base64Encoded, forceSaveAs);
}

void WebInspectorProxy::append(const String& filename, const String& content)
{
    platformAppend(filename, content);
}

bool WebInspectorProxy::shouldOpenAttached()
{
    return inspectorPagePreferences().inspectorStartsAttached() && canAttach();
}

#if ENABLE(INSPECTOR_SERVER)
void WebInspectorProxy::sendMessageToRemoteFrontend(const String& message)
{
    ASSERT(m_remoteInspectionPageId);
    WebInspectorServer::singleton().sendMessageOverConnection(m_remoteInspectionPageId, message);
}
#endif

// Unsupported configurations can use the stubs provided here.

#if PLATFORM(IOS) || (PLATFORM(MAC) && !WK_API_ENABLED)

WebPageProxy* WebInspectorProxy::platformCreateInspectorPage()
{
    notImplemented();
    return nullptr;
}

void WebInspectorProxy::platformOpen()
{
    notImplemented();
}

void WebInspectorProxy::platformDidClose()
{
    notImplemented();
}

void WebInspectorProxy::platformInvalidate()
{
    notImplemented();
}

void WebInspectorProxy::platformBringToFront()
{
    notImplemented();
}

void WebInspectorProxy::platformHide()
{
    notImplemented();
}

bool WebInspectorProxy::platformIsFront()
{
    notImplemented();
    return false;
}

void WebInspectorProxy::platformInspectedURLChanged(const String&)
{
    notImplemented();
}

void WebInspectorProxy::platformSave(const String& suggestedURL, const String& content, bool base64Encoded, bool forceSaveDialog)
{
    notImplemented();
}

void WebInspectorProxy::platformAppend(const String& suggestedURL, const String& content)
{
    notImplemented();
}

unsigned WebInspectorProxy::platformInspectedWindowHeight()
{
    notImplemented();
    return 0;
}

unsigned WebInspectorProxy::platformInspectedWindowWidth()
{
    notImplemented();
    return 0;
}

void WebInspectorProxy::platformAttach()
{
    notImplemented();
}

void WebInspectorProxy::platformDetach()
{
    notImplemented();
}

void WebInspectorProxy::platformSetAttachedWindowHeight(unsigned)
{
    notImplemented();
}

void WebInspectorProxy::platformStartWindowDrag()
{
    notImplemented();
}

String WebInspectorProxy::inspectorPageURL()
{
    notImplemented();
    return String();
}

String WebInspectorProxy::inspectorTestPageURL()
{
    notImplemented();
    return String();
}

String WebInspectorProxy::inspectorBaseURL()
{
    notImplemented();
    return String();
}

void WebInspectorProxy::platformSetAttachedWindowWidth(unsigned)
{
    notImplemented();
}

void WebInspectorProxy::platformAttachAvailabilityChanged(bool)
{
    notImplemented();
}

#endif // PLATFORM(IOS) || (PLATFORM(MAC) && !WK_API_ENABLED)

} // namespace WebKit
