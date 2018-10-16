/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "APINavigation.h"
#include "RemoteWebInspectorProxyMessages.h"
#include "RemoteWebInspectorUIMessages.h"
#include "WebInspectorProxy.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include <WebCore/NotImplemented.h>

namespace WebKit {
using namespace WebCore;

RemoteWebInspectorProxy::RemoteWebInspectorProxy()
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

void RemoteWebInspectorProxy::load(const String& debuggableType, const String& backendCommandsURL)
{
    createFrontendPageAndWindow();

    m_inspectorPage->process().send(Messages::RemoteWebInspectorUI::Initialize(debuggableType, backendCommandsURL), m_inspectorPage->pageID());
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
    m_inspectorPage->process().send(Messages::RemoteWebInspectorUI::SendMessageToFrontend(message), m_inspectorPage->pageID());
}

void RemoteWebInspectorProxy::frontendDidClose()
{
    Ref<RemoteWebInspectorProxy> protect(*this);

    if (m_client)
        m_client->closeFromFrontend();

    closeFrontendPageAndWindow();
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

void RemoteWebInspectorProxy::startWindowDrag()
{
    platformStartWindowDrag();
}

void RemoteWebInspectorProxy::openInNewTab(const String& url)
{
    platformOpenInNewTab(url);
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

    trackInspectorPage(m_inspectorPage);

    m_inspectorPage->process().addMessageReceiver(Messages::RemoteWebInspectorProxy::messageReceiverName(), m_inspectorPage->pageID(), *this);
    m_inspectorPage->process().assumeReadAccessToBaseURL(WebInspectorProxy::inspectorBaseURL());
}

void RemoteWebInspectorProxy::closeFrontendPageAndWindow()
{
    if (!m_inspectorPage)
        return;

    m_inspectorPage->process().removeMessageReceiver(Messages::RemoteWebInspectorProxy::messageReceiverName(), m_inspectorPage->pageID());

    untrackInspectorPage(m_inspectorPage);

    m_inspectorPage = nullptr;

    platformCloseFrontendPageAndWindow();
}

#if !ENABLE(REMOTE_INSPECTOR) || (!PLATFORM(MAC) && !PLATFORM(GTK))
WebPageProxy* RemoteWebInspectorProxy::platformCreateFrontendPageAndWindow()
{
    notImplemented();
    return nullptr;
}

void RemoteWebInspectorProxy::platformBringToFront() { }
void RemoteWebInspectorProxy::platformSave(const String&, const String&, bool, bool) { }
void RemoteWebInspectorProxy::platformAppend(const String&, const String&) { }
void RemoteWebInspectorProxy::platformStartWindowDrag() { }
void RemoteWebInspectorProxy::platformOpenInNewTab(const String&) { }
void RemoteWebInspectorProxy::platformCloseFrontendPageAndWindow() { }
#endif

} // namespace WebKit
