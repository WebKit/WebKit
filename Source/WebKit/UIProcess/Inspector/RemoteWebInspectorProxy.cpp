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
#include "RemoteWebInspectorProxy.h"

#include "APIDebuggableInfo.h"
#include "APINavigation.h"
#include "RemoteWebInspectorProxyMessages.h"
#include "RemoteWebInspectorUIMessages.h"
#include "WebInspectorProxy.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/NotImplemented.h>

namespace WebKit {
using namespace WebCore;

RemoteWebInspectorProxy::RemoteWebInspectorProxy()
    : m_debuggableInfo(API::DebuggableInfo::create(DebuggableInfoData::empty()))
{
}

RemoteWebInspectorProxy::~RemoteWebInspectorProxy()
{
    ASSERT(!m_inspectorPage);
}

void RemoteWebInspectorProxy::invalidate()
{
    closeFrontendPageAndWindow();
}

void RemoteWebInspectorProxy::setDiagnosticLoggingAvailable(bool available)
{
#if ENABLE(INSPECTOR_TELEMETRY)
    m_inspectorPage->send(Messages::RemoteWebInspectorUI::SetDiagnosticLoggingAvailable(available));
#else
    UNUSED_PARAM(available);
#endif
}

void RemoteWebInspectorProxy::load(Ref<API::DebuggableInfo>&& debuggableInfo, const String& backendCommandsURL)
{
    createFrontendPageAndWindow();

    m_debuggableInfo = WTFMove(debuggableInfo);
    m_backendCommandsURL = backendCommandsURL;

    m_inspectorPage->send(Messages::RemoteWebInspectorUI::Initialize(m_debuggableInfo->debuggableInfoData(), backendCommandsURL));
    m_inspectorPage->loadRequest(URL(URL(), WebInspectorProxy::inspectorPageURL()));
}

void RemoteWebInspectorProxy::closeFromBackend()
{
    closeFrontendPageAndWindow();
}

void RemoteWebInspectorProxy::closeFromCrash()
{
    // Behave as if the frontend just closed, so clients are informed the frontend is gone.
    frontendDidClose();
}

void RemoteWebInspectorProxy::show()
{
    bringToFront();
}

void RemoteWebInspectorProxy::sendMessageToFrontend(const String& message)
{
    m_inspectorPage->send(Messages::RemoteWebInspectorUI::SendMessageToFrontend(message));
}

void RemoteWebInspectorProxy::frontendDidClose()
{
    Ref<RemoteWebInspectorProxy> protect(*this);

    if (m_client)
        m_client->closeFromFrontend();

    closeFrontendPageAndWindow();
}

void RemoteWebInspectorProxy::reopen()
{
    ASSERT(!m_backendCommandsURL.isEmpty());

    closeFrontendPageAndWindow();
    load(m_debuggableInfo.copyRef(), m_backendCommandsURL);
}

void RemoteWebInspectorProxy::resetState()
{
    platformResetState();
}

void RemoteWebInspectorProxy::bringToFront()
{
    platformBringToFront();
}

void RemoteWebInspectorProxy::save(const String& suggestedURL, const String& content, bool base64Encoded, bool forceSaveDialog)
{
    platformSave(suggestedURL, content, base64Encoded, forceSaveDialog);
}

void RemoteWebInspectorProxy::append(const String& suggestedURL, const String& content)
{
    platformAppend(suggestedURL, content);
}

void RemoteWebInspectorProxy::setSheetRect(const FloatRect& rect)
{
    platformSetSheetRect(rect);
}

void RemoteWebInspectorProxy::setForcedAppearance(InspectorFrontendClient::Appearance appearance)
{
    platformSetForcedAppearance(appearance);
}

void RemoteWebInspectorProxy::startWindowDrag()
{
    platformStartWindowDrag();
}

void RemoteWebInspectorProxy::openURLExternally(const String& url)
{
    platformOpenURLExternally(url);
}

void RemoteWebInspectorProxy::showCertificate(const CertificateInfo& certificateInfo)
{
    platformShowCertificate(certificateInfo);
}

void RemoteWebInspectorProxy::sendMessageToBackend(const String& message)
{
    if (m_client)
        m_client->sendMessageToBackend(message);
}

void RemoteWebInspectorProxy::createFrontendPageAndWindow()
{
    if (m_inspectorPage)
        return;

    m_inspectorPage = platformCreateFrontendPageAndWindow();

    trackInspectorPage(m_inspectorPage, nullptr);

    m_inspectorPage->process().addMessageReceiver(Messages::RemoteWebInspectorProxy::messageReceiverName(), m_inspectorPage->webPageID(), *this);
    m_inspectorPage->process().assumeReadAccessToBaseURL(*m_inspectorPage, WebInspectorProxy::inspectorBaseURL());
}

void RemoteWebInspectorProxy::closeFrontendPageAndWindow()
{
    if (!m_inspectorPage)
        return;

    m_inspectorPage->process().removeMessageReceiver(Messages::RemoteWebInspectorProxy::messageReceiverName(), m_inspectorPage->webPageID());

    untrackInspectorPage(m_inspectorPage);

    m_inspectorPage = nullptr;

    platformCloseFrontendPageAndWindow();
}

#if !ENABLE(REMOTE_INSPECTOR) || (!PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(WIN))
WebPageProxy* RemoteWebInspectorProxy::platformCreateFrontendPageAndWindow()
{
    notImplemented();
    return nullptr;
}

void RemoteWebInspectorProxy::platformResetState() { }
void RemoteWebInspectorProxy::platformBringToFront() { }
void RemoteWebInspectorProxy::platformSave(const String&, const String&, bool, bool) { }
void RemoteWebInspectorProxy::platformAppend(const String&, const String&) { }
void RemoteWebInspectorProxy::platformSetSheetRect(const FloatRect&) { }
void RemoteWebInspectorProxy::platformSetForcedAppearance(InspectorFrontendClient::Appearance) { }
void RemoteWebInspectorProxy::platformStartWindowDrag() { }
void RemoteWebInspectorProxy::platformOpenURLExternally(const String&) { }
void RemoteWebInspectorProxy::platformShowCertificate(const CertificateInfo&) { }
void RemoteWebInspectorProxy::platformCloseFrontendPageAndWindow() { }
#endif // !ENABLE(REMOTE_INSPECTOR) || (!PLATFORM(MAC) && !PLATFORM(GTK) && !PLATFORM(WIN))

} // namespace WebKit
