/*
 * Copyright (C) 2014, 2016 Apple Inc. All rights reserved.
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
#include "WebInspectorUI.h"

#include "WebInspectorMessages.h"
#include "WebInspectorProxyMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Chrome.h>
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/InspectorController.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/RuntimeEnabledFeatures.h>

using namespace WebCore;

namespace WebKit {

Ref<WebInspectorUI> WebInspectorUI::create(WebPage& page)
{
    return adoptRef(*new WebInspectorUI(page));
}

WebInspectorUI::WebInspectorUI(WebPage& page)
    : m_page(page)
    , m_frontendAPIDispatcher(page)
{
    RuntimeEnabledFeatures::sharedFeatures().setInspectorAdditionsEnabled(true);
    RuntimeEnabledFeatures::sharedFeatures().setImageBitmapOffscreenCanvasEnabled(true);
}

void WebInspectorUI::establishConnection(IPC::Attachment encodedConnectionIdentifier, uint64_t inspectedPageIdentifier, bool underTest, unsigned inspectionLevel)
{
#if USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::Identifier connectionIdentifier(encodedConnectionIdentifier.releaseFileDescriptor());
#elif OS(DARWIN)
    IPC::Connection::Identifier connectionIdentifier(encodedConnectionIdentifier.port());
#else
    notImplemented();
    return;
#endif

    if (IPC::Connection::identifierIsNull(connectionIdentifier))
        return;

    m_inspectedPageIdentifier = inspectedPageIdentifier;
    m_frontendAPIDispatcher.reset();
    m_underTest = underTest;
    m_inspectionLevel = inspectionLevel;

    m_frontendController = &m_page.corePage()->inspectorController();
    m_frontendController->setInspectorFrontendClient(this);

    m_backendConnection = IPC::Connection::createClientConnection(connectionIdentifier, *this);
    m_backendConnection->open();
}

void WebInspectorUI::windowObjectCleared()
{
    if (m_frontendHost)
        m_frontendHost->disconnectClient();

    m_frontendHost = InspectorFrontendHost::create(this, m_page.corePage());
    m_frontendHost->addSelfToGlobalObjectInWorld(mainThreadNormalWorld());
}

void WebInspectorUI::frontendLoaded()
{
    m_frontendAPIDispatcher.frontendLoaded();

    // Tell the new frontend about the current dock state. If the window object
    // cleared due to a reload, the dock state won't be resent from UIProcess.
    setDockingUnavailable(m_dockingUnavailable);
    setDockSide(m_dockSide);
    setIsVisible(m_isVisible);

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::FrontendLoaded(), m_inspectedPageIdentifier);

    bringToFront();
}

void WebInspectorUI::startWindowDrag()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::StartWindowDrag(), m_inspectedPageIdentifier);
}

void WebInspectorUI::moveWindowBy(float x, float y)
{
    FloatRect frameRect = m_page.corePage()->chrome().windowRect();
    frameRect.move(x, y);
    m_page.corePage()->chrome().setWindowRect(frameRect);
}

void WebInspectorUI::bringToFront()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::BringToFront(), m_inspectedPageIdentifier);
}

void WebInspectorUI::closeWindow()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::DidClose(), m_inspectedPageIdentifier);

    if (m_backendConnection)
        m_backendConnection->invalidate();
    m_backendConnection = nullptr;

    if (m_frontendController)
        m_frontendController->setInspectorFrontendClient(nullptr);
    m_frontendController = nullptr;

    if (m_frontendHost)
        m_frontendHost->disconnectClient();

    m_inspectedPageIdentifier = 0;
    m_underTest = false;
}

WebCore::UserInterfaceLayoutDirection WebInspectorUI::userInterfaceLayoutDirection() const
{
    return m_page.corePage()->userInterfaceLayoutDirection();
}

void WebInspectorUI::requestSetDockSide(DockSide side)
{
    auto& webProcess = WebProcess::singleton();
    switch (side) {
    case DockSide::Undocked:
        webProcess.parentProcessConnection()->send(Messages::WebInspectorProxy::Detach(), m_inspectedPageIdentifier);
        break;
    case DockSide::Right:
        webProcess.parentProcessConnection()->send(Messages::WebInspectorProxy::AttachRight(), m_inspectedPageIdentifier);
        break;
    case DockSide::Left:
        webProcess.parentProcessConnection()->send(Messages::WebInspectorProxy::AttachLeft(), m_inspectedPageIdentifier);
        break;
    case DockSide::Bottom:
        webProcess.parentProcessConnection()->send(Messages::WebInspectorProxy::AttachBottom(), m_inspectedPageIdentifier);
        break;
    }
}

void WebInspectorUI::setDockSide(DockSide side)
{
    const char* sideString;

    switch (side) {
    case DockSide::Undocked:
        sideString = "undocked";
        break;

    case DockSide::Right:
        sideString = "right";
        break;

    case DockSide::Left:
        sideString = "left";
        break;

    case DockSide::Bottom:
        sideString = "bottom";
        break;
    }

    m_dockSide = side;

    m_frontendAPIDispatcher.dispatchCommand(ASCIILiteral("setDockSide"), String(ASCIILiteral(sideString)));
}

void WebInspectorUI::setDockingUnavailable(bool unavailable)
{
    m_dockingUnavailable = unavailable;

    m_frontendAPIDispatcher.dispatchCommand(ASCIILiteral("setDockingUnavailable"), unavailable);
}

void WebInspectorUI::setIsVisible(bool visible)
{
    m_isVisible = visible;

    m_frontendAPIDispatcher.dispatchCommand(ASCIILiteral("setIsVisible"), visible);
}

void WebInspectorUI::changeAttachedWindowHeight(unsigned height)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::SetAttachedWindowHeight(height), m_inspectedPageIdentifier);
}

void WebInspectorUI::changeAttachedWindowWidth(unsigned width)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::SetAttachedWindowWidth(width), m_inspectedPageIdentifier);
}

void WebInspectorUI::openInNewTab(const String& url)
{
    if (m_backendConnection)
        m_backendConnection->send(Messages::WebInspector::OpenInNewTab(url), 0);
}

void WebInspectorUI::save(const WTF::String& filename, const WTF::String& content, bool base64Encoded, bool forceSaveAs)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::Save(filename, content, base64Encoded, forceSaveAs), m_inspectedPageIdentifier);
}

void WebInspectorUI::append(const WTF::String& filename, const WTF::String& content)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::Append(filename, content), m_inspectedPageIdentifier);
}

void WebInspectorUI::inspectedURLChanged(const String& urlString)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::InspectedURLChanged(urlString), m_inspectedPageIdentifier);
}

void WebInspectorUI::showConsole()
{
    m_frontendAPIDispatcher.dispatchCommand(ASCIILiteral("showConsole"));
}

void WebInspectorUI::showResources()
{
    m_frontendAPIDispatcher.dispatchCommand(ASCIILiteral("showResources"));
}

void WebInspectorUI::showTimelines()
{
    m_frontendAPIDispatcher.dispatchCommand(ASCIILiteral("showTimelines"));
}

void WebInspectorUI::showMainResourceForFrame(const String& frameIdentifier)
{
    m_frontendAPIDispatcher.dispatchCommand(ASCIILiteral("showMainResourceForFrame"), frameIdentifier);
}

void WebInspectorUI::startPageProfiling()
{
    m_frontendAPIDispatcher.dispatchCommand(ASCIILiteral("setTimelineProfilingEnabled"), true);
}

void WebInspectorUI::stopPageProfiling()
{
    m_frontendAPIDispatcher.dispatchCommand(ASCIILiteral("setTimelineProfilingEnabled"), false);
}

void WebInspectorUI::startElementSelection()
{
    m_frontendAPIDispatcher.dispatchCommand(ASCIILiteral("setElementSelectionEnabled"), true);
}

void WebInspectorUI::stopElementSelection()
{
    m_frontendAPIDispatcher.dispatchCommand(ASCIILiteral("setElementSelectionEnabled"), false);
}

void WebInspectorUI::didSave(const String& url)
{
    m_frontendAPIDispatcher.dispatchCommand(ASCIILiteral("savedURL"), url);
}

void WebInspectorUI::didAppend(const String& url)
{
    m_frontendAPIDispatcher.dispatchCommand(ASCIILiteral("appendedToURL"), url);
}

void WebInspectorUI::sendMessageToFrontend(const String& message)
{
    m_frontendAPIDispatcher.dispatchMessageAsync(message);
}

void WebInspectorUI::pagePaused()
{
    m_frontendAPIDispatcher.suspend();
}

void WebInspectorUI::pageUnpaused()
{
    m_frontendAPIDispatcher.unsuspend();
}

void WebInspectorUI::sendMessageToBackend(const String& message)
{
    if (m_backendConnection)
        m_backendConnection->send(Messages::WebInspector::SendMessageToBackend(message), 0);
}

} // namespace WebKit
