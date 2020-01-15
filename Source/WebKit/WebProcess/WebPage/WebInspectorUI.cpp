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

#include "config.h"
#include "WebInspectorUI.h"

#include "WebCoreArgumentCoders.h"
#include "WebInspectorMessages.h"
#include "WebInspectorProxyMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/Chrome.h>
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/FloatRect.h>
#include <WebCore/InspectorController.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <WebCore/Settings.h>

namespace WebKit {
using namespace WebCore;

Ref<WebInspectorUI> WebInspectorUI::create(WebPage& page)
{
    return adoptRef(*new WebInspectorUI(page));
}

WebInspectorUI::WebInspectorUI(WebPage& page)
    : m_page(page)
    , m_frontendAPIDispatcher(page)
    , m_debuggableInfo(DebuggableInfoData::empty())
{
    RuntimeEnabledFeatures::sharedFeatures().setInspectorAdditionsEnabled(true);
    RuntimeEnabledFeatures::sharedFeatures().setImageBitmapEnabled(true);
#if ENABLE(WEBGL2)
    RuntimeEnabledFeatures::sharedFeatures().setWebGL2Enabled(true);
#endif
}

void WebInspectorUI::establishConnection(WebPageProxyIdentifier inspectedPageIdentifier, const DebuggableInfoData& debuggableInfo, bool underTest, unsigned inspectionLevel)
{
    m_inspectedPageIdentifier = inspectedPageIdentifier;
    m_debuggableInfo = debuggableInfo;
    m_underTest = underTest;
    m_inspectionLevel = inspectionLevel;

    m_frontendAPIDispatcher.reset();

    m_frontendController = &m_page.corePage()->inspectorController();
    m_frontendController->setInspectorFrontendClient(this);

    updateConnection();
}

void WebInspectorUI::updateConnection()
{
    if (m_backendConnection) {
        m_backendConnection->invalidate();
        m_backendConnection = nullptr;
    }

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
    m_backendConnection = IPC::Connection::createServerConnection(connectionIdentifier, *this);
    m_backendConnection->open();
#endif

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::SetFrontendConnection(connectionClientPort), m_inspectedPageIdentifier);
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

    if (m_backendConnection) {
        m_backendConnection->invalidate();
        m_backendConnection = nullptr;
    }

    if (m_frontendController) {
        m_frontendController->setInspectorFrontendClient(nullptr);
        m_frontendController = nullptr;
    }

    if (m_frontendHost)
        m_frontendHost->disconnectClient();

    m_inspectedPageIdentifier = { };
    m_underTest = false;
}

void WebInspectorUI::reopen()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::Reopen(), m_inspectedPageIdentifier);
}

void WebInspectorUI::resetState()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::ResetState(), m_inspectedPageIdentifier);
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
    ASCIILiteral sideString { ASCIILiteral::null() };

    switch (side) {
    case DockSide::Undocked:
        sideString = "undocked"_s;
        break;

    case DockSide::Right:
        sideString = "right"_s;
        break;

    case DockSide::Left:
        sideString = "left"_s;
        break;

    case DockSide::Bottom:
        sideString = "bottom"_s;
        break;
    }

    m_dockSide = side;

    m_frontendAPIDispatcher.dispatchCommand("setDockSide"_s, String(sideString));
}

void WebInspectorUI::setDockingUnavailable(bool unavailable)
{
    m_dockingUnavailable = unavailable;

    m_frontendAPIDispatcher.dispatchCommand("setDockingUnavailable"_s, unavailable);
}

void WebInspectorUI::setIsVisible(bool visible)
{
    m_isVisible = visible;

    m_frontendAPIDispatcher.dispatchCommand("setIsVisible"_s, visible);
}

void WebInspectorUI::changeAttachedWindowHeight(unsigned height)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::SetAttachedWindowHeight(height), m_inspectedPageIdentifier);
}

void WebInspectorUI::changeAttachedWindowWidth(unsigned width)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::SetAttachedWindowWidth(width), m_inspectedPageIdentifier);
}

void WebInspectorUI::changeSheetRect(const FloatRect& rect)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::SetSheetRect(rect), m_inspectedPageIdentifier);
}

void WebInspectorUI::openInNewTab(const String& url)
{
    if (m_backendConnection) {
        m_backendConnection->send(Messages::WebInspector::OpenInNewTab(url), 0);
        WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::BringInspectedPageToFront(), m_inspectedPageIdentifier);
    }
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

void WebInspectorUI::showCertificate(const CertificateInfo& certificateInfo)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::ShowCertificate(certificateInfo), m_inspectedPageIdentifier);
}

#if ENABLE(INSPECTOR_TELEMETRY)
bool WebInspectorUI::supportsDiagnosticLogging()
{
    return m_page.corePage()->settings().diagnosticLoggingEnabled();
}

void WebInspectorUI::logDiagnosticEvent(const String& eventName, const DiagnosticLoggingClient::ValueDictionary& dictionary)
{
    m_page.corePage()->diagnosticLoggingClient().logDiagnosticMessageWithValueDictionary(eventName, "Web Inspector Frontend Diagnostics"_s, dictionary, ShouldSample::No);
}

void WebInspectorUI::setDiagnosticLoggingAvailable(bool available)
{
    // Inspector's diagnostic logging client should never be used unless the page setting is also enabled.
    ASSERT(!available || supportsDiagnosticLogging());
    m_diagnosticLoggingAvailable = available;

    m_frontendAPIDispatcher.dispatchCommand("setDiagnosticLoggingAvailable"_s, m_diagnosticLoggingAvailable);
}
#endif

void WebInspectorUI::showConsole()
{
    m_frontendAPIDispatcher.dispatchCommand("showConsole"_s);
}

void WebInspectorUI::showResources()
{
    m_frontendAPIDispatcher.dispatchCommand("showResources"_s);
}

void WebInspectorUI::showMainResourceForFrame(const String& frameIdentifier)
{
    m_frontendAPIDispatcher.dispatchCommand("showMainResourceForFrame"_s, frameIdentifier);
}

void WebInspectorUI::startPageProfiling()
{
    m_frontendAPIDispatcher.dispatchCommand("setTimelineProfilingEnabled"_s, true);
}

void WebInspectorUI::stopPageProfiling()
{
    m_frontendAPIDispatcher.dispatchCommand("setTimelineProfilingEnabled"_s, false);
}

void WebInspectorUI::startElementSelection()
{
    m_frontendAPIDispatcher.dispatchCommand("setElementSelectionEnabled"_s, true);
}

void WebInspectorUI::stopElementSelection()
{
    m_frontendAPIDispatcher.dispatchCommand("setElementSelectionEnabled"_s, false);
}

void WebInspectorUI::didSave(const String& url)
{
    m_frontendAPIDispatcher.dispatchCommand("savedURL"_s, url);
}

void WebInspectorUI::didAppend(const String& url)
{
    m_frontendAPIDispatcher.dispatchCommand("appendedToURL"_s, url);
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
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::SendMessageToBackend(message), m_inspectedPageIdentifier);
}

String WebInspectorUI::targetPlatformName() const
{
    return m_debuggableInfo.targetPlatformName;
}

String WebInspectorUI::targetBuildVersion() const
{
    return m_debuggableInfo.targetBuildVersion;
}

String WebInspectorUI::targetProductVersion() const
{
    return m_debuggableInfo.targetProductVersion;
}

#if !PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(WIN)
bool WebInspectorUI::canSave()
{
    notImplemented();
    return false;
}

String WebInspectorUI::localizedStringsURL() const
{
    notImplemented();
    return emptyString();
}
#endif // !PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(WIN)

} // namespace WebKit
