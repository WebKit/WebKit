/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#include "APINavigation.h"
#include "APIProcessPoolConfiguration.h"
#include "WebAutomationSession.h"
#include "WebFrameProxy.h"
#include "WebInspectorInterruptDispatcherMessages.h"
#include "WebInspectorMessages.h"
#include "WebInspectorProxyMessages.h"
#include "WebInspectorUIMessages.h"
#include "WebPageGroup.h"
#include "WebPageInspectorController.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/TextEncoding.h>
#include <wtf/SetForScope.h>

#if PLATFORM(GTK)
#include "WebInspectorProxyClient.h"
#endif

namespace WebKit {
using namespace WebCore;

const unsigned WebInspectorProxy::minimumWindowWidth = 500;
const unsigned WebInspectorProxy::minimumWindowHeight = 400;

const unsigned WebInspectorProxy::initialWindowWidth = 1000;
const unsigned WebInspectorProxy::initialWindowHeight = 650;

WebInspectorProxy::WebInspectorProxy(WebPageProxy* inspectedPage)
    : m_inspectedPage(inspectedPage)
#if PLATFORM(MAC) && WK_API_ENABLED
    , m_closeFrontendAfterInactivityTimer(RunLoop::main(), this, &WebInspectorProxy::closeFrontendAfterInactivityTimerFired)
#endif
{
    m_inspectedPage->process().addMessageReceiver(Messages::WebInspectorProxy::messageReceiverName(), m_inspectedPage->pageID(), *this);
}

WebInspectorProxy::~WebInspectorProxy()
{
}

unsigned WebInspectorProxy::inspectionLevel() const
{
    return inspectorLevelForPage(inspectedPage());
}

WebPreferences& WebInspectorProxy::inspectorPagePreferences() const
{
    ASSERT(m_inspectorPage);
    return m_inspectorPage->pageGroup().preferences();
}

void WebInspectorProxy::invalidate()
{
    if (m_inspectedPage)
        m_inspectedPage->process().removeMessageReceiver(Messages::WebInspectorProxy::messageReceiverName(), m_inspectedPage->pageID());

    closeFrontendPageAndWindow();
    platformInvalidate();

    m_inspectedPage = nullptr;
}

void WebInspectorProxy::sendMessageToFrontend(const String& message)
{
    if (!m_inspectorPage)
        return;

    m_inspectorPage->process().send(Messages::WebInspectorUI::SendMessageToFrontend(message), m_inspectorPage->pageID());
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

    createFrontendPage();

    m_inspectedPage->process().send(Messages::WebInspectorInterruptDispatcher::NotifyNeedDebuggerBreak(), 0);
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

    closeFrontendPageAndWindow();
}

void WebInspectorProxy::closeForCrash()
{
    close();

    platformDidCloseForCrash();
}

void WebInspectorProxy::reset()
{
    if (m_inspectedPage) {
        m_inspectedPage->process().removeMessageReceiver(Messages::WebInspectorProxy::messageReceiverName(), m_inspectedPage->pageID());
        m_inspectedPage = nullptr;
    }
}

void WebInspectorProxy::updateForNewPageProcess(WebPageProxy* inspectedPage)
{
    ASSERT(!m_inspectedPage);
    ASSERT(inspectedPage);

    m_inspectedPage = inspectedPage;
    m_inspectedPage->process().addMessageReceiver(Messages::WebInspectorProxy::messageReceiverName(), m_inspectedPage->pageID(), *this);

    if (m_inspectorPage)
        m_inspectorPage->process().send(Messages::WebInspectorUI::UpdateConnection(), m_inspectorPage->pageID());
}

void WebInspectorProxy::setFrontendConnection(IPC::Attachment connectionIdentifier)
{
    if (!m_inspectedPage)
        return;

    m_inspectedPage->process().send(Messages::WebInspector::SetFrontendConnection(connectionIdentifier), m_inspectedPage->pageID());
}

void WebInspectorProxy::showConsole()
{
    if (!m_inspectedPage)
        return;

    createFrontendPage();

    m_inspectedPage->process().send(Messages::WebInspector::ShowConsole(), m_inspectedPage->pageID());
}

void WebInspectorProxy::showResources()
{
    if (!m_inspectedPage)
        return;

    createFrontendPage();

    m_inspectedPage->process().send(Messages::WebInspector::ShowResources(), m_inspectedPage->pageID());
}

void WebInspectorProxy::showTimelines()
{
    if (!m_inspectedPage)
        return;

    createFrontendPage();

    m_inspectedPage->process().send(Messages::WebInspector::ShowTimelines(), m_inspectedPage->pageID());
}

void WebInspectorProxy::showMainResourceForFrame(WebFrameProxy* frame)
{
    if (!m_inspectedPage)
        return;

    createFrontendPage();

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

void WebInspectorProxy::attachLeft()
{
    attach(AttachmentSide::Left);
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

    case AttachmentSide::Left:
        m_inspectorPage->process().send(Messages::WebInspectorUI::AttachedLeft(), m_inspectorPage->pageID());
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

void WebInspectorProxy::toggleElementSelection()
{
    if (!m_inspectedPage)
        return;

    if (m_elementSelectionActive) {
        m_ignoreElementSelectionChange = true;
        m_inspectedPage->process().send(Messages::WebInspector::StopElementSelection(), m_inspectedPage->pageID());
    } else {
        connect();
        m_inspectedPage->process().send(Messages::WebInspector::StartElementSelection(), m_inspectedPage->pageID());
    }
}

bool WebInspectorProxy::isMainOrTestInspectorPage(const URL& url)
{
    // Use URL so we can compare the paths and protocols.
    URL mainPageURL(URL(), WebInspectorProxy::inspectorPageURL());
    if (url.protocol() == mainPageURL.protocol() && decodeURLEscapeSequences(url.path()) == decodeURLEscapeSequences(mainPageURL.path()))
        return true;

    // We might not have a Test URL in Production builds.
    String testPageURLString = WebInspectorProxy::inspectorTestPageURL();
    if (testPageURLString.isNull())
        return false;

    URL testPageURL(URL(), testPageURLString);
    return url.protocol() == testPageURL.protocol() && decodeURLEscapeSequences(url.path()) == decodeURLEscapeSequences(testPageURL.path());
}

void WebInspectorProxy::createFrontendPage()
{
    if (m_inspectorPage)
        return;

    m_inspectorPage = platformCreateFrontendPage();
    ASSERT(m_inspectorPage);
    if (!m_inspectorPage)
        return;

    trackInspectorPage(m_inspectorPage);

    m_inspectorPage->process().addMessageReceiver(Messages::WebInspectorProxy::messageReceiverName(), m_inspectedPage->pageID(), *this);
    m_inspectorPage->process().assumeReadAccessToBaseURL(WebInspectorProxy::inspectorBaseURL());
}

void WebInspectorProxy::openLocalInspectorFrontend(bool canAttach, bool underTest)
{
    if (!m_inspectedPage)
        return;

    if (m_inspectedPage->inspectorController().hasLocalFrontend()) {
        show();
        return;
    }

    m_underTest = underTest;
    createFrontendPage();

    ASSERT(m_inspectorPage);
    if (!m_inspectorPage)
        return;

    m_inspectorPage->process().send(Messages::WebInspectorUI::EstablishConnection(m_inspectedPage->pageID(), m_underTest, inspectionLevel()), m_inspectorPage->pageID());

    m_inspectedPage->inspectorController().connectFrontend(*this);

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

            case AttachmentSide::Left:
                m_inspectorPage->process().send(Messages::WebInspectorUI::AttachedLeft(), m_inspectorPage->pageID());
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

    if (!m_inspectorPage)
        return;

    SetForScope<bool> isOpening(m_isOpening, true);
    m_isVisible = true;
    m_inspectorPage->process().send(Messages::WebInspectorUI::SetIsVisible(m_isVisible), m_inspectorPage->pageID());

    if (m_isAttached)
        platformAttach();
    else
        platformCreateFrontendWindow();

    platformBringToFront();
}

void WebInspectorProxy::didClose()
{
    closeFrontendPageAndWindow();
}

void WebInspectorProxy::closeFrontendPageAndWindow()
{
    if (!m_inspectorPage)
        return;

    m_isVisible = false;
    m_isProfilingPage = false;
    m_showMessageSent = false;
    m_ignoreFirstBringToFront = false;

    untrackInspectorPage(m_inspectorPage);

    m_inspectorPage->process().send(Messages::WebInspectorUI::SetIsVisible(m_isVisible), m_inspectorPage->pageID());
    m_inspectorPage->process().removeMessageReceiver(Messages::WebInspectorProxy::messageReceiverName(), m_inspectedPage->pageID());

    m_inspectedPage->inspectorController().disconnectFrontend(*this);

    if (m_isAttached)
        platformDetach();

    // Null out m_inspectorPage after platformDetach(), so the views can be cleaned up correctly.
    m_inspectorPage = nullptr;

    m_isAttached = false;
    m_canAttach = false;
    m_underTest = false;

    platformCloseFrontendPageAndWindow();
}

void WebInspectorProxy::sendMessageToBackend(const String& message)
{
    if (!m_inspectedPage)
        return;

    m_inspectedPage->inspectorController().dispatchMessageFromFrontend(message);
}

void WebInspectorProxy::frontendLoaded()
{
    if (!m_inspectedPage)
        return;

    if (auto* automationSession = m_inspectedPage->process().processPool().automationSession())
        automationSession->inspectorFrontendLoaded(*m_inspectedPage);
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

void WebInspectorProxy::elementSelectionChanged(bool active)
{
    m_elementSelectionActive = active;

    if (m_ignoreElementSelectionChange) {
        m_ignoreElementSelectionChange = false;
        if (!m_isVisible)
            close();
        return;
    }

    if (active)
        platformBringInspectedPageToFront();
    else if (isConnected())
        bringToFront();
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

// Unsupported configurations can use the stubs provided here.

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && !WK_API_ENABLED)

WebPageProxy* WebInspectorProxy::platformCreateFrontendPage()
{
    notImplemented();
    return nullptr;
}

void WebInspectorProxy::platformCreateFrontendWindow()
{
    notImplemented();
}

void WebInspectorProxy::platformCloseFrontendPageAndWindow()
{
    notImplemented();
}

void WebInspectorProxy::platformDidCloseForCrash()
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

void WebInspectorProxy::platformBringInspectedPageToFront()
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

#endif // PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && !WK_API_ENABLED)

} // namespace WebKit
