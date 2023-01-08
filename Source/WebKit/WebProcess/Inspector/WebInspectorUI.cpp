/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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
#include "WebInspectorUIProxyMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/Chrome.h>
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/ExceptionDetails.h>
#include <WebCore/FloatRect.h>
#include <WebCore/InspectorController.h>
#include <WebCore/InspectorFrontendHost.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Settings.h>

#if ENABLE(INSPECTOR_EXTENSIONS)
#include "WebInspectorUIExtensionController.h"
#endif

namespace WebKit {
using namespace WebCore;

Ref<WebInspectorUI> WebInspectorUI::create(WebPage& page)
{
    return adoptRef(*new WebInspectorUI(page));
}

void WebInspectorUI::enableFrontendFeatures(WebPage& page)
{
#if ENABLE(WEBGL2)
    page.corePage()->settings().setWebGL2Enabled(true);
#endif
    page.corePage()->settings().setCSSTypedOMEnabled(true);
}

WebInspectorUI::WebInspectorUI(WebPage& page)
    : m_page(page)
    , m_frontendAPIDispatcher(InspectorFrontendAPIDispatcher::create(*page.corePage()))
    , m_debuggableInfo(DebuggableInfoData::empty())
{
    WebInspectorUI::enableFrontendFeatures(page);
}

WebInspectorUI::~WebInspectorUI() = default;

void WebInspectorUI::establishConnection(WebPageProxyIdentifier inspectedPageIdentifier, const DebuggableInfoData& debuggableInfo, bool underTest, unsigned inspectionLevel)
{
    m_inspectedPageIdentifier = inspectedPageIdentifier;
    m_debuggableInfo = debuggableInfo;
    m_underTest = underTest;
    m_inspectionLevel = inspectionLevel;

#if ENABLE(INSPECTOR_EXTENSIONS)
    if (!m_extensionController)
        m_extensionController = makeUnique<WebInspectorUIExtensionController>(*this);
#endif

    m_frontendAPIDispatcher->reset();
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
    auto connectionIdentifiers = IPC::Connection::createConnectionIdentifierPair();
    if (!connectionIdentifiers)
        return;

    m_backendConnection = IPC::Connection::createServerConnection(connectionIdentifiers->server);
    m_backendConnection->open(*this);

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::SetFrontendConnection(connectionIdentifiers->client), m_inspectedPageIdentifier);
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
    m_frontendAPIDispatcher->frontendLoaded();

    // Tell the new frontend about the current dock state. If the window object
    // cleared due to a reload, the dock state won't be resent from UIProcess.
    setDockingUnavailable(m_dockingUnavailable);
    setDockSide(m_dockSide);
    setIsVisible(m_isVisible);

    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::FrontendLoaded(), m_inspectedPageIdentifier);

    bringToFront();
}

void WebInspectorUI::startWindowDrag()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::StartWindowDrag(), m_inspectedPageIdentifier);
}

void WebInspectorUI::moveWindowBy(float x, float y)
{
    FloatRect frameRect = m_page.corePage()->chrome().windowRect();
    frameRect.move(x, y);
    m_page.corePage()->chrome().setWindowRect(frameRect);
}

void WebInspectorUI::bringToFront()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::BringToFront(), m_inspectedPageIdentifier);
}

void WebInspectorUI::closeWindow()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::DidClose(), m_inspectedPageIdentifier);

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
    
#if ENABLE(INSPECTOR_EXTENSIONS)
    m_extensionController = nullptr;
#endif
}

void WebInspectorUI::reopen()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::Reopen(), m_inspectedPageIdentifier);
}

void WebInspectorUI::resetState()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::ResetState(), m_inspectedPageIdentifier);
}

void WebInspectorUI::setForcedAppearance(WebCore::InspectorFrontendClient::Appearance appearance)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::SetForcedAppearance(appearance), m_inspectedPageIdentifier);
}

WebCore::UserInterfaceLayoutDirection WebInspectorUI::userInterfaceLayoutDirection() const
{
    return m_page.corePage()->userInterfaceLayoutDirection();
}

bool WebInspectorUI::supportsDockSide(DockSide dockSide)
{
    switch (dockSide) {
    case DockSide::Undocked:
    case DockSide::Right:
    case DockSide::Left:
    case DockSide::Bottom:
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void WebInspectorUI::requestSetDockSide(DockSide dockSide)
{
    auto& webProcess = WebProcess::singleton();
    switch (dockSide) {
    case DockSide::Undocked:
        webProcess.parentProcessConnection()->send(Messages::WebInspectorUIProxy::Detach(), m_inspectedPageIdentifier);
        break;
    case DockSide::Right:
        webProcess.parentProcessConnection()->send(Messages::WebInspectorUIProxy::AttachRight(), m_inspectedPageIdentifier);
        break;
    case DockSide::Left:
        webProcess.parentProcessConnection()->send(Messages::WebInspectorUIProxy::AttachLeft(), m_inspectedPageIdentifier);
        break;
    case DockSide::Bottom:
        webProcess.parentProcessConnection()->send(Messages::WebInspectorUIProxy::AttachBottom(), m_inspectedPageIdentifier);
        break;
    }
}

void WebInspectorUI::setDockSide(DockSide dockSide)
{
    ASCIILiteral dockSideString;

    switch (dockSide) {
    case DockSide::Undocked:
        dockSideString = "undocked"_s;
        break;

    case DockSide::Right:
        dockSideString = "right"_s;
        break;

    case DockSide::Left:
        dockSideString = "left"_s;
        break;

    case DockSide::Bottom:
        dockSideString = "bottom"_s;
        break;
    }

    m_dockSide = dockSide;

    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("setDockSide"_s, { JSON::Value::create(String(dockSideString)) });
}

void WebInspectorUI::setDockingUnavailable(bool unavailable)
{
    m_dockingUnavailable = unavailable;

    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("setDockingUnavailable"_s, { JSON::Value::create(unavailable) });
}

void WebInspectorUI::setIsVisible(bool visible)
{
    m_isVisible = visible;

    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("setIsVisible"_s, { JSON::Value::create(visible) });
}

void WebInspectorUI::updateFindString(const String& findString)
{
    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("updateFindString"_s, { JSON::Value::create(findString) });
}

void WebInspectorUI::changeAttachedWindowHeight(unsigned height)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::SetAttachedWindowHeight(height), m_inspectedPageIdentifier);
}

void WebInspectorUI::changeAttachedWindowWidth(unsigned width)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::SetAttachedWindowWidth(width), m_inspectedPageIdentifier);
}

void WebInspectorUI::changeSheetRect(const FloatRect& rect)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::SetSheetRect(rect), m_inspectedPageIdentifier);
}

void WebInspectorUI::openURLExternally(const String& url)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::OpenURLExternally(url), m_inspectedPageIdentifier);
}

void WebInspectorUI::revealFileExternally(const String& path)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::RevealFileExternally(path), m_inspectedPageIdentifier);
}

void WebInspectorUI::save(Vector<InspectorFrontendClient::SaveData>&& saveDatas, bool forceSaveAs)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::Save(WTFMove(saveDatas), forceSaveAs), m_inspectedPageIdentifier);
}

void WebInspectorUI::load(const WTF::String& path, CompletionHandler<void(const String&)>&& completionHandler)
{
    WebProcess::singleton().parentProcessConnection()->sendWithAsyncReply(Messages::WebInspectorUIProxy::Load(path), WTFMove(completionHandler), m_inspectedPageIdentifier);
}

void WebInspectorUI::pickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&& completionHandler)
{
    WebProcess::singleton().parentProcessConnection()->sendWithAsyncReply(Messages::WebInspectorUIProxy::PickColorFromScreen(), WTFMove(completionHandler), m_inspectedPageIdentifier);
}

void WebInspectorUI::inspectedURLChanged(const String& urlString)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::InspectedURLChanged(urlString), m_inspectedPageIdentifier);
}

void WebInspectorUI::showCertificate(const CertificateInfo& certificateInfo)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::ShowCertificate(certificateInfo), m_inspectedPageIdentifier);
}

void WebInspectorUI::setInspectorPageDeveloperExtrasEnabled(bool enabled)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::SetInspectorPageDeveloperExtrasEnabled(enabled), m_inspectedPageIdentifier);
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

    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("setDiagnosticLoggingAvailable"_s, { JSON::Value::create(m_diagnosticLoggingAvailable) });
}
#endif // ENABLE(INSPECTOR_TELEMETRY)

#if ENABLE(INSPECTOR_EXTENSIONS)
bool WebInspectorUI::supportsWebExtensions()
{
    return true;
}

void WebInspectorUI::didShowExtensionTab(const String& extensionID, const String& extensionTabID, WebCore::FrameIdentifier frameID)
{
    if (!m_extensionController)
        return;

    m_extensionController->didShowExtensionTab(extensionID, extensionTabID, frameID);
}

void WebInspectorUI::didHideExtensionTab(const String& extensionID, const String& extensionTabID)
{
    if (!m_extensionController)
        return;

    m_extensionController->didHideExtensionTab(extensionID, extensionTabID);
}

void WebInspectorUI::didNavigateExtensionTab(const String& extensionID, const String& extensionTabID, const URL& newURL)
{
    if (!m_extensionController)
        return;

    m_extensionController->didNavigateExtensionTab(extensionID, extensionTabID, newURL);
}

void WebInspectorUI::inspectedPageDidNavigate(const URL& newURL)
{
    if (!m_extensionController)
        return;

    m_extensionController->inspectedPageDidNavigate(newURL);
}
#endif // ENABLE(INSPECTOR_EXTENSIONS)

void WebInspectorUI::showConsole()
{
    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("showConsole"_s);
}

void WebInspectorUI::showResources()
{
    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("showResources"_s);
}

void WebInspectorUI::showMainResourceForFrame(const String& frameIdentifier)
{
    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("showMainResourceForFrame"_s, { JSON::Value::create(frameIdentifier) });
}

void WebInspectorUI::startPageProfiling()
{
    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("setTimelineProfilingEnabled"_s, { JSON::Value::create(true) });
}

void WebInspectorUI::stopPageProfiling()
{
    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("setTimelineProfilingEnabled"_s, { JSON::Value::create(false) });
}

void WebInspectorUI::startElementSelection()
{
    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("setElementSelectionEnabled"_s, { JSON::Value::create(true) });
}

void WebInspectorUI::stopElementSelection()
{
    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("setElementSelectionEnabled"_s, { JSON::Value::create(false) });
}

void WebInspectorUI::sendMessageToFrontend(const String& message)
{
    m_frontendAPIDispatcher->dispatchMessageAsync(message);
}

void WebInspectorUI::evaluateInFrontendForTesting(const String& expression)
{
    m_frontendAPIDispatcher->evaluateExpressionForTesting(expression);
}

void WebInspectorUI::pagePaused()
{
    m_frontendAPIDispatcher->suspend();
}

void WebInspectorUI::pageUnpaused()
{
    m_frontendAPIDispatcher->unsuspend();
}

void WebInspectorUI::sendMessageToBackend(const String& message)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::WebInspectorUIProxy::SendMessageToBackend(message), m_inspectedPageIdentifier);
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

WebCore::Page* WebInspectorUI::frontendPage()
{
    return m_page.corePage();
}


#if !PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(WIN)
bool WebInspectorUI::canSave(InspectorFrontendClient::SaveMode)
{
    notImplemented();
    return false;
}

bool WebInspectorUI::canLoad()
{
    notImplemented();
    return false;
}

bool WebInspectorUI::canPickColorFromScreen()
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
