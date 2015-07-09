/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include <JavaScriptCore/ScriptValue.h>
#include <WebCore/Chrome.h>
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/InspectorController.h>
#include <WebCore/MainFrame.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ScriptController.h>
#include <WebCore/ScriptGlobalObject.h>
#include <WebCore/ScriptState.h>

using namespace WebCore;

namespace WebKit {

Ref<WebInspectorUI> WebInspectorUI::create(WebPage* page)
{
    return adoptRef(*new WebInspectorUI(page));
}

WebInspectorUI::WebInspectorUI(WebPage* page)
    : m_page(page)
    , m_inspectedPageIdentifier(0)
    , m_underTest(false)
    , m_frontendLoaded(false)
    , m_dockSide(DockSide::Undocked)
#if PLATFORM(COCOA)
    , m_hasLocalizedStringsURL(false)
#endif
{
}

void WebInspectorUI::establishConnection(IPC::Attachment encodedConnectionIdentifier, uint64_t inspectedPageIdentifier, bool underTest)
{
#if OS(DARWIN)
    IPC::Connection::Identifier connectionIdentifier(encodedConnectionIdentifier.port());
#elif USE(UNIX_DOMAIN_SOCKETS)
    IPC::Connection::Identifier connectionIdentifier(encodedConnectionIdentifier.releaseFileDescriptor());
#else
    notImplemented();
    return;
#endif

    if (IPC::Connection::identifierIsNull(connectionIdentifier))
        return;

    m_inspectedPageIdentifier = inspectedPageIdentifier;
    m_frontendLoaded = false;
    m_underTest = underTest;

    m_page->corePage()->inspectorController().setInspectorFrontendClient(this);

    m_backendConnection = IPC::Connection::createClientConnection(connectionIdentifier, *this);
    m_backendConnection->open();
}

void WebInspectorUI::windowObjectCleared()
{
    if (m_frontendHost)
        m_frontendHost->disconnectClient();

    m_frontendHost = InspectorFrontendHost::create(this, m_page->corePage());
    ScriptGlobalObject::set(execStateFromPage(mainThreadNormalWorld(), m_page->corePage()), ASCIILiteral("InspectorFrontendHost"), m_frontendHost.get());
}

void WebInspectorUI::frontendLoaded()
{
    m_frontendLoaded = true;

    evaluatePendingExpressions();
    bringToFront();
}

void WebInspectorUI::startWindowDrag()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::StartWindowDrag(), m_inspectedPageIdentifier);
}

void WebInspectorUI::moveWindowBy(float x, float y)
{
    FloatRect frameRect = m_page->corePage()->chrome().windowRect();
    frameRect.move(x, y);
    m_page->corePage()->chrome().setWindowRect(frameRect);
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

    m_inspectedPageIdentifier = 0;
    m_underTest = false;
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

    case DockSide::Bottom:
        sideString = "bottom";
        break;
    }

    m_dockSide = side;

    evaluateCommandOnLoad(ASCIILiteral("setDockSide"), ASCIILiteral(sideString));
}

void WebInspectorUI::setDockingUnavailable(bool unavailable)
{
    evaluateCommandOnLoad(ASCIILiteral("setDockingUnavailable"), unavailable);
}

void WebInspectorUI::changeAttachedWindowHeight(unsigned height)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::SetAttachedWindowHeight(height), m_inspectedPageIdentifier);
}

void WebInspectorUI::changeAttachedWindowWidth(unsigned width)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::SetAttachedWindowWidth(width), m_inspectedPageIdentifier);
}

void WebInspectorUI::setToolbarHeight(unsigned height)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorProxy::SetToolbarHeight(height), m_inspectedPageIdentifier);
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
    evaluateCommandOnLoad(ASCIILiteral("showConsole"));
}

void WebInspectorUI::showResources()
{
    evaluateCommandOnLoad(ASCIILiteral("showResources"));
}

void WebInspectorUI::showMainResourceForFrame(const String& frameIdentifier)
{
    evaluateCommandOnLoad(ASCIILiteral("showMainResourceForFrame"), frameIdentifier);
}

void WebInspectorUI::startPageProfiling()
{
    evaluateCommandOnLoad(ASCIILiteral("setTimelineProfilingEnabled"), true);
}

void WebInspectorUI::stopPageProfiling()
{
    evaluateCommandOnLoad(ASCIILiteral("setTimelineProfilingEnabled"), false);
}

void WebInspectorUI::didSave(const String& url)
{
    evaluateCommandOnLoad(ASCIILiteral("savedURL"), url);
}

void WebInspectorUI::didAppend(const String& url)
{
    evaluateCommandOnLoad(ASCIILiteral("appendedToURL"), url);
}

void WebInspectorUI::sendMessageToFrontend(const String& message)
{
    evaluateExpressionOnLoad(makeString("InspectorFrontendAPI.dispatchMessageAsync(", message, ")"));
}

void WebInspectorUI::sendMessageToBackend(const String& message)
{
    if (m_backendConnection)
        m_backendConnection->send(Messages::WebInspector::SendMessageToBackend(message), 0);
}

void WebInspectorUI::evaluateCommandOnLoad(const String& command, const String& argument)
{
    if (argument.isNull())
        evaluateExpressionOnLoad(makeString("InspectorFrontendAPI.dispatch([\"", command, "\"])"));
    else
        evaluateExpressionOnLoad(makeString("InspectorFrontendAPI.dispatch([\"", command, "\", \"", argument, "\"])"));
}

void WebInspectorUI::evaluateCommandOnLoad(const String& command, bool argument)
{
    evaluateExpressionOnLoad(makeString("InspectorFrontendAPI.dispatch([\"", command, "\", ", ASCIILiteral(argument ? "true" : "false"), "])"));
}

void WebInspectorUI::evaluateExpressionOnLoad(const String& expression)
{
    if (m_frontendLoaded) {
        ASSERT(m_queue.isEmpty());
        m_page->corePage()->mainFrame().script().executeScript(expression);
        return;
    }

    m_queue.append(expression);
}

void WebInspectorUI::evaluatePendingExpressions()
{
    ASSERT(m_frontendLoaded);

    for (const String& expression : m_queue)
        m_page->corePage()->mainFrame().script().executeScript(expression);

    m_queue.clear();
}

} // namespace WebKit
