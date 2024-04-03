/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "RemoteWebInspectorUI.h"

#include "RemoteWebInspectorUIMessages.h"
#include "RemoteWebInspectorUIProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebInspectorUI.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/Chrome.h>
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/FloatRect.h>
#include <WebCore/InspectorController.h>
#include <WebCore/Page.h>
#include <WebCore/Settings.h>

#if ENABLE(INSPECTOR_EXTENSIONS)
#include "WebInspectorUIExtensionController.h"
#endif

#if !PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(WIN)
#include <WebCore/NotImplemented.h>
#endif

namespace WebKit {
using namespace WebCore;

Ref<RemoteWebInspectorUI> RemoteWebInspectorUI::create(WebPage& page)
{
    return adoptRef(*new RemoteWebInspectorUI(page));
}

RemoteWebInspectorUI::RemoteWebInspectorUI(WebPage& page)
    : m_page(page)
    , m_frontendAPIDispatcher(InspectorFrontendAPIDispatcher::create(*page.corePage()))
{
}

RemoteWebInspectorUI::~RemoteWebInspectorUI() = default;

void RemoteWebInspectorUI::initialize(DebuggableInfoData&& debuggableInfo, const String& backendCommandsURL)
{
#if ENABLE(INSPECTOR_EXTENSIONS)
    m_extensionController = makeUnique<WebInspectorUIExtensionController>(*this, m_page.identifier());
#endif

    m_debuggableInfo = WTFMove(debuggableInfo);
    m_backendCommandsURL = backendCommandsURL;

    m_page.corePage()->inspectorController().setInspectorFrontendClient(this);

    m_frontendAPIDispatcher->reset();
    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("setDockingUnavailable"_s, { JSON::Value::create(true) });
}

void RemoteWebInspectorUI::updateFindString(const String& findString)
{
    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("updateFindString"_s, { JSON::Value::create(findString) });
}

void RemoteWebInspectorUI::sendMessageToFrontend(const String& message)
{
    m_frontendAPIDispatcher->dispatchMessageAsync(message);
}

void RemoteWebInspectorUI::sendMessageToBackend(const String& message)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::RemoteWebInspectorUIProxy::SendMessageToBackend(message), m_page.identifier());
}

void RemoteWebInspectorUI::windowObjectCleared()
{
    if (m_frontendHost)
        m_frontendHost->disconnectClient();

    m_frontendHost = InspectorFrontendHost::create(this, m_page.corePage());
    m_frontendHost->addSelfToGlobalObjectInWorld(mainThreadNormalWorld());
}

void RemoteWebInspectorUI::frontendLoaded()
{
    m_frontendAPIDispatcher->frontendLoaded();

    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("setIsVisible"_s, { JSON::Value::create(true) });

    WebProcess::singleton().parentProcessConnection()->send(Messages::RemoteWebInspectorUIProxy::FrontendLoaded(), m_page.identifier());

    bringToFront();
}

void RemoteWebInspectorUI::pagePaused()
{
    m_frontendAPIDispatcher->suspend();
}

void RemoteWebInspectorUI::pageUnpaused()
{
    m_frontendAPIDispatcher->unsuspend();
}

void RemoteWebInspectorUI::changeSheetRect(const FloatRect& rect)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::RemoteWebInspectorUIProxy::SetSheetRect(rect), m_page.identifier());
}

void RemoteWebInspectorUI::setForcedAppearance(WebCore::InspectorFrontendClient::Appearance appearance)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::RemoteWebInspectorUIProxy::SetForcedAppearance(appearance), m_page.identifier());
}

void RemoteWebInspectorUI::startWindowDrag()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::RemoteWebInspectorUIProxy::StartWindowDrag(), m_page.identifier());
}

void RemoteWebInspectorUI::moveWindowBy(float x, float y)
{
    FloatRect frameRect = m_page.corePage()->chrome().windowRect();
    frameRect.move(x, y);
    m_page.corePage()->chrome().setWindowRect(frameRect);
}

WebCore::UserInterfaceLayoutDirection RemoteWebInspectorUI::userInterfaceLayoutDirection() const
{
    return m_page.corePage()->userInterfaceLayoutDirection();
}

bool RemoteWebInspectorUI::supportsDockSide(DockSide dockSide)
{
    switch (dockSide) {
    case DockSide::Undocked:
        return true;

    case DockSide::Right:
    case DockSide::Left:
    case DockSide::Bottom:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void RemoteWebInspectorUI::bringToFront()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::RemoteWebInspectorUIProxy::BringToFront(), m_page.identifier());
}

void RemoteWebInspectorUI::closeWindow()
{
    m_page.corePage()->inspectorController().setInspectorFrontendClient(nullptr);

#if ENABLE(INSPECTOR_EXTENSIONS)
    m_extensionController = nullptr;
#endif
    
    WebProcess::singleton().parentProcessConnection()->send(Messages::RemoteWebInspectorUIProxy::FrontendDidClose(), m_page.identifier());
}

void RemoteWebInspectorUI::reopen()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::RemoteWebInspectorUIProxy::Reopen(), m_page.identifier());
}

void RemoteWebInspectorUI::resetState()
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::RemoteWebInspectorUIProxy::ResetState(), m_page.identifier());
}

void RemoteWebInspectorUI::showConsole()
{
    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("showConsole"_s);
}

void RemoteWebInspectorUI::showResources()
{
    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("showResources"_s);
}

void RemoteWebInspectorUI::openURLExternally(const String& url)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::RemoteWebInspectorUIProxy::OpenURLExternally(url), m_page.identifier());
}

void RemoteWebInspectorUI::revealFileExternally(const String& path)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::RemoteWebInspectorUIProxy::RevealFileExternally(path), m_page.identifier());
}

void RemoteWebInspectorUI::save(Vector<WebCore::InspectorFrontendClient::SaveData>&& saveDatas, bool forceSaveAs)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::RemoteWebInspectorUIProxy::Save(WTFMove(saveDatas), forceSaveAs), m_page.identifier());
}

void RemoteWebInspectorUI::load(const String& path, CompletionHandler<void(const String&)>&& completionHandler)
{
    WebProcess::singleton().parentProcessConnection()->sendWithAsyncReply(Messages::RemoteWebInspectorUIProxy::Load(path), WTFMove(completionHandler), m_page.identifier());
}

void RemoteWebInspectorUI::pickColorFromScreen(CompletionHandler<void(const std::optional<WebCore::Color>&)>&& completionHandler)
{
    WebProcess::singleton().parentProcessConnection()->sendWithAsyncReply(Messages::RemoteWebInspectorUIProxy::PickColorFromScreen(), WTFMove(completionHandler), m_page.identifier());
}

void RemoteWebInspectorUI::inspectedURLChanged(const String& urlString)
{
    // Do nothing. The remote side can know if the main resource changed.
}

void RemoteWebInspectorUI::showCertificate(const CertificateInfo& certificateInfo)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::RemoteWebInspectorUIProxy::ShowCertificate(certificateInfo), m_page.identifier());
}

void RemoteWebInspectorUI::setInspectorPageDeveloperExtrasEnabled(bool enabled)
{
    WebProcess::singleton().parentProcessConnection()->send(Messages::RemoteWebInspectorUIProxy::SetInspectorPageDeveloperExtrasEnabled(enabled), m_page.identifier());
}

Inspector::DebuggableType RemoteWebInspectorUI::debuggableType() const
{
    return m_debuggableInfo.debuggableType;
}

String RemoteWebInspectorUI::targetPlatformName() const
{
    return m_debuggableInfo.targetPlatformName;
}

String RemoteWebInspectorUI::targetBuildVersion() const
{
    return m_debuggableInfo.targetBuildVersion;
}

String RemoteWebInspectorUI::targetProductVersion() const
{
    return m_debuggableInfo.targetProductVersion;
}

bool RemoteWebInspectorUI::targetIsSimulator() const
{
    return m_debuggableInfo.targetIsSimulator;
}

#if ENABLE(INSPECTOR_TELEMETRY)
bool RemoteWebInspectorUI::supportsDiagnosticLogging()
{
    return m_page.corePage()->settings().diagnosticLoggingEnabled();
}

void RemoteWebInspectorUI::logDiagnosticEvent(const String& eventName,  const DiagnosticLoggingClient::ValueDictionary& dictionary)
{
    m_page.corePage()->diagnosticLoggingClient().logDiagnosticMessageWithValueDictionary(eventName, "Remote Web Inspector Frontend Diagnostics"_s, dictionary, ShouldSample::No);
}

void RemoteWebInspectorUI::setDiagnosticLoggingAvailable(bool available)
{
    // Inspector's diagnostic logging client should never be used unless the page setting is also enabled.
    ASSERT(!available || supportsDiagnosticLogging());
    m_diagnosticLoggingAvailable = available;

    m_frontendAPIDispatcher->dispatchCommandWithResultAsync("setDiagnosticLoggingAvailable"_s, { JSON::Value::create(m_diagnosticLoggingAvailable) });
}
#endif // ENABLE(INSPECTOR_TELEMETRY)

#if ENABLE(INSPECTOR_EXTENSIONS)
bool RemoteWebInspectorUI::supportsWebExtensions()
{
    return true;
}

void RemoteWebInspectorUI::didShowExtensionTab(const Inspector::ExtensionID& extensionID, const Inspector::ExtensionTabID& extensionTabID, const WebCore::FrameIdentifier& frameID)
{
    if (!m_extensionController)
        return;

    m_extensionController->didShowExtensionTab(extensionID, extensionTabID, frameID);
}

void RemoteWebInspectorUI::didHideExtensionTab(const Inspector::ExtensionID& extensionID, const Inspector::ExtensionTabID& extensionTabID)
{
    if (!m_extensionController)
        return;

    m_extensionController->didHideExtensionTab(extensionID, extensionTabID);
}

void RemoteWebInspectorUI::didNavigateExtensionTab(const Inspector::ExtensionID& extensionID, const Inspector::ExtensionTabID& extensionTabID, const URL& newURL)
{
    if (!m_extensionController)
        return;

    m_extensionController->didNavigateExtensionTab(extensionID, extensionTabID, newURL);
}

void RemoteWebInspectorUI::inspectedPageDidNavigate(const URL& newURL)
{
    if (!m_extensionController)
        return;

    m_extensionController->inspectedPageDidNavigate(newURL);
}
#endif // ENABLE(INSPECTOR_EXTENSIONS)

WebCore::Page* RemoteWebInspectorUI::frontendPage()
{
    return m_page.corePage();
}


#if !PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(WIN)
bool RemoteWebInspectorUI::canSave(InspectorFrontendClient::SaveMode)
{
    notImplemented();
    return false;
}

bool RemoteWebInspectorUI::canLoad()
{
    notImplemented();
    return false;
}

bool RemoteWebInspectorUI::canPickColorFromScreen()
{
    notImplemented();
    return false;
}

String RemoteWebInspectorUI::localizedStringsURL() const
{
    notImplemented();
    return emptyString();
}
#endif // !PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(WIN)

} // namespace WebKit
