/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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
#include "RemoteWebInspectorUIProxy.h"

#include "APIDebuggableInfo.h"
#include "APINavigation.h"
#include "RemoteWebInspectorUIMessages.h"
#include "RemoteWebInspectorUIProxyMessages.h"
#include "WebInspectorUIProxy.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/NotImplemented.h>

#if ENABLE(INSPECTOR_EXTENSIONS)
#include "WebInspectorUIExtensionControllerProxy.h"
#endif

namespace WebKit {
using namespace WebCore;

RemoteWebInspectorUIProxy::RemoteWebInspectorUIProxy()
    : m_debuggableInfo(API::DebuggableInfo::create(DebuggableInfoData::empty()))
{
}

RemoteWebInspectorUIProxy::~RemoteWebInspectorUIProxy()
{
    ASSERT(!m_inspectorPage);
}

void RemoteWebInspectorUIProxy::invalidate()
{
    closeFrontendPageAndWindow();
}

void RemoteWebInspectorUIProxy::setDiagnosticLoggingAvailable(bool available)
{
#if ENABLE(INSPECTOR_TELEMETRY)
    m_inspectorPage->send(Messages::RemoteWebInspectorUI::SetDiagnosticLoggingAvailable(available));
#else
    UNUSED_PARAM(available);
#endif
}

void RemoteWebInspectorUIProxy::load(Ref<API::DebuggableInfo>&& debuggableInfo, const String& backendCommandsURL)
{
    m_debuggableInfo = WTFMove(debuggableInfo);
    m_backendCommandsURL = backendCommandsURL;

    createFrontendPageAndWindow();

    m_inspectorPage->send(Messages::RemoteWebInspectorUI::Initialize(m_debuggableInfo->debuggableInfoData(), backendCommandsURL));
    m_inspectorPage->loadRequest(URL(URL(), WebInspectorUIProxy::inspectorPageURL()));
}

void RemoteWebInspectorUIProxy::closeFromBackend()
{
    closeFrontendPageAndWindow();
}

void RemoteWebInspectorUIProxy::closeFromCrash()
{
    // Behave as if the frontend just closed, so clients are informed the frontend is gone.
    frontendDidClose();
}

void RemoteWebInspectorUIProxy::show()
{
    bringToFront();
}

void RemoteWebInspectorUIProxy::showConsole()
{
    m_inspectorPage->send(Messages::RemoteWebInspectorUI::ShowConsole { });
}

void RemoteWebInspectorUIProxy::showResources()
{
    m_inspectorPage->send(Messages::RemoteWebInspectorUI::ShowResources { });
}

void RemoteWebInspectorUIProxy::sendMessageToFrontend(const String& message)
{
    m_inspectorPage->send(Messages::RemoteWebInspectorUI::SendMessageToFrontend(message));
}

void RemoteWebInspectorUIProxy::frontendLoaded()
{
#if ENABLE(INSPECTOR_EXTENSIONS)
    m_extensionController->inspectorFrontendLoaded();
#endif
}

void RemoteWebInspectorUIProxy::frontendDidClose()
{
    Ref<RemoteWebInspectorUIProxy> protect(*this);

    if (m_client)
        m_client->closeFromFrontend();

    closeFrontendPageAndWindow();
}

void RemoteWebInspectorUIProxy::reopen()
{
    ASSERT(!m_backendCommandsURL.isEmpty());

    closeFrontendPageAndWindow();
    load(m_debuggableInfo.copyRef(), m_backendCommandsURL);
}

void RemoteWebInspectorUIProxy::resetState()
{
    platformResetState();
}

void RemoteWebInspectorUIProxy::bringToFront()
{
    platformBringToFront();
}

void RemoteWebInspectorUIProxy::save(const String& suggestedURL, const String& content, bool base64Encoded, bool forceSaveDialog)
{
    platformSave(suggestedURL, content, base64Encoded, forceSaveDialog);
}

void RemoteWebInspectorUIProxy::append(const String& suggestedURL, const String& content)
{
    platformAppend(suggestedURL, content);
}

void RemoteWebInspectorUIProxy::setSheetRect(const FloatRect& rect)
{
    platformSetSheetRect(rect);
}

void RemoteWebInspectorUIProxy::setForcedAppearance(InspectorFrontendClient::Appearance appearance)
{
    platformSetForcedAppearance(appearance);
}

void RemoteWebInspectorUIProxy::startWindowDrag()
{
    platformStartWindowDrag();
}

void RemoteWebInspectorUIProxy::openURLExternally(const String& url)
{
    platformOpenURLExternally(url);
}

void RemoteWebInspectorUIProxy::showCertificate(const CertificateInfo& certificateInfo)
{
    platformShowCertificate(certificateInfo);
}

void RemoteWebInspectorUIProxy::sendMessageToBackend(const String& message)
{
    if (m_client)
        m_client->sendMessageToBackend(message);
}

void RemoteWebInspectorUIProxy::createFrontendPageAndWindow()
{
    if (m_inspectorPage)
        return;

    m_inspectorPage = platformCreateFrontendPageAndWindow();

    trackInspectorPage(m_inspectorPage, nullptr);

    m_inspectorPage->process().addMessageReceiver(Messages::RemoteWebInspectorUIProxy::messageReceiverName(), m_inspectorPage->webPageID(), *this);

#if ENABLE(INSPECTOR_EXTENSIONS)
    m_extensionController = WebInspectorUIExtensionControllerProxy::create(*m_inspectorPage);
#endif
}

void RemoteWebInspectorUIProxy::closeFrontendPageAndWindow()
{
    if (!m_inspectorPage)
        return;

    m_inspectorPage->process().removeMessageReceiver(Messages::RemoteWebInspectorUIProxy::messageReceiverName(), m_inspectorPage->webPageID());

    untrackInspectorPage(m_inspectorPage);

#if ENABLE(INSPECTOR_EXTENSIONS)
    // This extension controller may be kept alive by the IPC dispatcher beyond the point
    // when m_inspectorPage is cleared below. Notify the controller so it can clean up before then.
    m_extensionController->inspectorFrontendWillClose();
    m_extensionController = nullptr;
#endif

    m_inspectorPage = nullptr;

    platformCloseFrontendPageAndWindow();
}

#if !ENABLE(REMOTE_INSPECTOR) || (!PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(WIN))
WebPageProxy* RemoteWebInspectorUIProxy::platformCreateFrontendPageAndWindow()
{
    notImplemented();
    return nullptr;
}

void RemoteWebInspectorUIProxy::platformResetState() { }
void RemoteWebInspectorUIProxy::platformBringToFront() { }
void RemoteWebInspectorUIProxy::platformSave(const String&, const String&, bool, bool) { }
void RemoteWebInspectorUIProxy::platformAppend(const String&, const String&) { }
void RemoteWebInspectorUIProxy::platformSetSheetRect(const FloatRect&) { }
void RemoteWebInspectorUIProxy::platformSetForcedAppearance(InspectorFrontendClient::Appearance) { }
void RemoteWebInspectorUIProxy::platformStartWindowDrag() { }
void RemoteWebInspectorUIProxy::platformOpenURLExternally(const String&) { }
void RemoteWebInspectorUIProxy::platformShowCertificate(const CertificateInfo&) { }
void RemoteWebInspectorUIProxy::platformCloseFrontendPageAndWindow() { }
#endif // !ENABLE(REMOTE_INSPECTOR) || (!PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(WIN))

} // namespace WebKit
