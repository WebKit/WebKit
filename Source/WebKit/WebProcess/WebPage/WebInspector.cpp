/*
 * Copyright (C) 2010, 2014-2018 Apple Inc. All rights reserved.
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
#include "WebInspector.h"

#include "WebFrame.h"
#include "WebInspectorMessages.h"
#include "WebInspectorProxyMessages.h"
#include "WebInspectorUIMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Chrome.h>
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameView.h>
#include <WebCore/InspectorController.h>
#include <WebCore/InspectorFrontendClient.h>
#include <WebCore/InspectorPageAgent.h>
#include <WebCore/NavigationAction.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/ScriptController.h>
#include <WebCore/WindowFeatures.h>

static const float minimumAttachedHeight = 250;
static const float maximumAttachedHeightRatio = 0.75;
static const float minimumAttachedWidth = 500;

namespace WebKit {
using namespace WebCore;

Ref<WebInspector> WebInspector::create(WebPage* page)
{
    return adoptRef(*new WebInspector(page));
}

WebInspector::WebInspector(WebPage* page)
    : m_page(page)
{
}

WebInspector::~WebInspector()
{
    if (m_frontendConnection)
        m_frontendConnection->invalidate();
}

// Called from WebInspectorClient
void WebInspector::openFrontendConnection(bool underTest)
{
#if USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::SocketPair socketPair = IPC::Connection::createPlatformConnection();
    IPC::Connection::Identifier connectionIdentifier(socketPair.server);
    IPC::Attachment connectionClientPort(socketPair.client);
#elif OS(DARWIN)
    mach_port_t listeningPort = MACH_PORT_NULL;
    if (mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort) != KERN_SUCCESS)
        CRASH();

    if (mach_port_insert_right(mach_task_self(), listeningPort, listeningPort, MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS)
        CRASH();

    IPC::Connection::Identifier connectionIdentifier(listeningPort);
    IPC::Attachment connectionClientPort(listeningPort, MACH_MSG_TYPE_MOVE_SEND);
#elif PLATFORM(WIN)
    IPC::Connection::Identifier connectionIdentifier, connClient;
    IPC::Connection::createServerAndClientIdentifiers(connectionIdentifier, connClient);
    IPC::Attachment connectionClientPort(connClient);
#else
    notImplemented();
    return;
#endif

#if USE(UNIX_DOMAIN_SOCKETS) || OS(DARWIN) || PLATFORM(WIN)
    m_frontendConnection = IPC::Connection::createServerConnection(connectionIdentifier, *this);
    m_frontendConnection->open();

    m_page->setHasLocalInspectorFrontend(true);

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::CreateInspectorPage(connectionClientPort, canAttachWindow(), underTest), m_page->pageID());
#endif
}

void WebInspector::closeFrontendConnection()
{
    m_page->setHasLocalInspectorFrontend(false);

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::DidClose(), m_page->pageID());

    // If we tried to close the frontend before it was created, then no connection exists yet.
    if (m_frontendConnection) {
        m_frontendConnection->invalidate();
        m_frontendConnection = nullptr;
    }

    m_attached = false;
    m_previousCanAttach = false;
}

void WebInspector::bringToFront()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::BringToFront(), m_page->pageID());
}

// Called by WebInspector messages
void WebInspector::show()
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().show();
}

void WebInspector::close()
{
    if (!m_page->corePage())
        return;

    // Close could be called multiple times during teardown.
    if (!m_frontendConnection)
        return;

    m_page->corePage()->inspectorController().disconnectFrontend(this);
    closeFrontendConnection();
}

void WebInspector::openInNewTab(const String& urlString)
{
    UserGestureIndicator indicator { ProcessingUserGesture };

    Page* inspectedPage = m_page->corePage();
    if (!inspectedPage)
        return;

    Frame& inspectedMainFrame = inspectedPage->mainFrame();
    FrameLoadRequest frameLoadRequest { *inspectedMainFrame.document(), inspectedMainFrame.document()->securityOrigin(), { urlString }, "_blank"_s, LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, ShouldOpenExternalURLsPolicy::ShouldNotAllow, InitiatedByMainFrame::Unknown };

    NavigationAction action { *inspectedMainFrame.document(), frameLoadRequest.resourceRequest(), frameLoadRequest.initiatedByMainFrame(), NavigationType::LinkClicked };
    Page* newPage = inspectedPage->chrome().createWindow(inspectedMainFrame, frameLoadRequest, { }, action);
    if (!newPage)
        return;

    newPage->mainFrame().loader().load(WTFMove(frameLoadRequest));
}

void WebInspector::evaluateScriptForTest(const String& script)
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().evaluateForTestInFrontend(script);
}

void WebInspector::showConsole()
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().show();
    m_frontendConnection->send(Messages::WebInspectorUI::ShowConsole(), 0);
}

void WebInspector::showResources()
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().show();
    m_frontendConnection->send(Messages::WebInspectorUI::ShowResources(), 0);
}

void WebInspector::showTimelines()
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().show();
    m_frontendConnection->send(Messages::WebInspectorUI::ShowTimelines(), 0);
}

void WebInspector::showMainResourceForFrame(uint64_t frameIdentifier)
{
    WebFrame* frame = WebProcess::singleton().webFrame(frameIdentifier);
    if (!frame)
        return;

    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().show();

    String inspectorFrameIdentifier = m_page->corePage()->inspectorController().pageAgent()->frameId(frame->coreFrame());
    m_frontendConnection->send(Messages::WebInspectorUI::ShowMainResourceForFrame(inspectorFrameIdentifier), 0);
}

void WebInspector::startPageProfiling()
{
    if (!m_page->corePage())
        return;

    m_frontendConnection->send(Messages::WebInspectorUI::StartPageProfiling(), 0);
}

void WebInspector::stopPageProfiling()
{
    if (!m_page->corePage())
        return;

    m_frontendConnection->send(Messages::WebInspectorUI::StopPageProfiling(), 0);
}

void WebInspector::startElementSelection()
{
    if (!m_page->corePage())
        return;

    m_frontendConnection->send(Messages::WebInspectorUI::StartElementSelection(), 0);
}

void WebInspector::stopElementSelection()
{
    if (!m_page->corePage())
        return;

    m_frontendConnection->send(Messages::WebInspectorUI::StopElementSelection(), 0);
}

void WebInspector::elementSelectionChanged(bool active)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::ElementSelectionChanged(active), m_page->pageID());
}

bool WebInspector::canAttachWindow()
{
    if (!m_page->corePage())
        return false;

    // Don't allow attaching to another inspector -- two inspectors in one window is too much!
    if (m_page->isInspectorPage())
        return false;

    // If we are already attached, allow attaching again to allow switching sides.
    if (m_attached)
        return true;

    // Don't allow the attach if the window would be too small to accommodate the minimum inspector size.
    unsigned inspectedPageHeight = m_page->corePage()->mainFrame().view()->visibleHeight();
    unsigned inspectedPageWidth = m_page->corePage()->mainFrame().view()->visibleWidth();
    unsigned maximumAttachedHeight = inspectedPageHeight * maximumAttachedHeightRatio;
    return minimumAttachedHeight <= maximumAttachedHeight && minimumAttachedWidth <= inspectedPageWidth;
}

void WebInspector::updateDockingAvailability()
{
    if (m_attached)
        return;

    bool canAttachWindow = this->canAttachWindow();
    if (m_previousCanAttach == canAttachWindow)
        return;

    m_previousCanAttach = canAttachWindow;

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::AttachAvailabilityChanged(canAttachWindow), m_page->pageID());
}

void WebInspector::sendMessageToBackend(const String& message)
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().dispatchMessageFromFrontend(message);
}

void WebInspector::sendMessageToFrontend(const String& message)
{
    m_frontendConnection->send(Messages::WebInspectorUI::SendMessageToFrontend(message), 0);
}

} // namespace WebKit
