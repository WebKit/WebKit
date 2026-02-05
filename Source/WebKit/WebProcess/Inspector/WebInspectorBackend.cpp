/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "WebInspectorBackend.h"

#include "WebFrame.h"
#include "WebInspectorBackendMessages.h"
#include "WebInspectorBackendProxyMessages.h"
#include "WebInspectorUIMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Chrome.h>
#include <WebCore/DocumentView.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/InspectorFrontendClient.h>
#include <WebCore/InspectorPageAgent.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/NavigationAction.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PageInspectorController.h>
#include <WebCore/ScriptController.h>
#include <WebCore/WindowFeatures.h>

static const float minimumAttachedHeight = 250;
static const float maximumAttachedHeightRatio = 0.75;
static const float minimumAttachedWidth = 500;

namespace WebKit {
using namespace WebCore;

Ref<WebInspectorBackend> WebInspectorBackend::create(WebPage& page)
{
    return adoptRef(*new WebInspectorBackend(page));
}

WebInspectorBackend::WebInspectorBackend(WebPage& page)
    : m_page(page)
{
}

WebInspectorBackend::~WebInspectorBackend()
{
    if (RefPtr frontendConnection = m_frontendConnection)
        frontendConnection->invalidate();
}

WebPage* WebInspectorBackend::page() const
{
    return m_page.get();
}

void WebInspectorBackend::openLocalInspectorFrontend()
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::RequestOpenLocalInspectorFrontend(), m_page->identifier());
}

void WebInspectorBackend::setFrontendConnection(IPC::Connection::Handle&& connectionHandle)
{
    // We might receive multiple updates if this web process got swapped into a WebPageProxy
    // shortly after another process established the connection.
    if (RefPtr frontendConnection = std::exchange(m_frontendConnection, nullptr))
        frontendConnection->invalidate();

    if (!connectionHandle)
        return;

    Ref frontendConnection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTF::move(connectionHandle) });
    m_frontendConnection = frontendConnection.copyRef();
    frontendConnection->open(*this);

    for (auto& callback : m_frontendConnectionActions)
        callback(frontendConnection.get());
    m_frontendConnectionActions.clear();
}

void WebInspectorBackend::closeFrontendConnection()
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::DidClose(), m_page->identifier());

    // If we tried to close the frontend before it was created, then no connection exists yet.
    if (RefPtr frontendConnection = m_frontendConnection) {
        frontendConnection->invalidate();
        m_frontendConnection = nullptr;
    }

    m_frontendConnectionActions.clear();

    m_attached = false;
    m_previousCanAttach = false;
}

void WebInspectorBackend::bringToFront()
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::BringToFront(), m_page->identifier());
}

void WebInspectorBackend::whenFrontendConnectionEstablished(Function<void(IPC::Connection&)>&& callback)
{
    if (RefPtr connection = m_frontendConnection) {
        callback(*connection);
        return;
    }

    m_frontendConnectionActions.append(WTF::move(callback));
}

// Called by WebInspectorBackend messages
void WebInspectorBackend::show(CompletionHandler<void()>&& completionHandler)
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().show();
    completionHandler();
}

void WebInspectorBackend::close()
{
    if (!m_page->corePage())
        return;

    // Close could be called multiple times during teardown.
    if (!m_frontendConnection)
        return;

    closeFrontendConnection();
}

void WebInspectorBackend::evaluateScriptForTest(const String& script)
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().evaluateForTestInFrontend(script);
}

void WebInspectorBackend::showConsole()
{
    if (!m_page->corePage())
        return;

    whenFrontendConnectionEstablished([](auto& frontendConnection) {
        frontendConnection.send(Messages::WebInspectorUI::ShowConsole(), 0);
    });
}

void WebInspectorBackend::showResources()
{
    if (!m_page->corePage())
        return;

    whenFrontendConnectionEstablished([](auto& frontendConnection) {
        frontendConnection.send(Messages::WebInspectorUI::ShowResources(), 0);
    });
}

void WebInspectorBackend::showMainResourceForFrame(WebCore::FrameIdentifier frameIdentifier)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameIdentifier);
    if (!frame)
        return;

    if (!m_page->corePage())
        return;

    String inspectorFrameIdentifier = CheckedRef { m_page->corePage()->inspectorController().ensurePageAgent() }->frameId(protect(frame->coreLocalFrame()).get());

    whenFrontendConnectionEstablished([inspectorFrameIdentifier](auto& frontendConnection) {
        frontendConnection.send(Messages::WebInspectorUI::ShowMainResourceForFrame(inspectorFrameIdentifier), 0);
    });
}

void WebInspectorBackend::startPageProfiling()
{
    if (!m_page->corePage())
        return;

    whenFrontendConnectionEstablished([](auto& frontendConnection) {
        frontendConnection.send(Messages::WebInspectorUI::StartPageProfiling(), 0);
    });
}

void WebInspectorBackend::stopPageProfiling()
{
    if (!m_page->corePage())
        return;

    whenFrontendConnectionEstablished([](auto& frontendConnection) {
        frontendConnection.send(Messages::WebInspectorUI::StopPageProfiling(), 0);
    });
}

void WebInspectorBackend::startElementSelection()
{
    if (!m_page->corePage())
        return;

    whenFrontendConnectionEstablished([](auto& frontendConnection) {
        frontendConnection.send(Messages::WebInspectorUI::StartElementSelection(), 0);
    });
}

void WebInspectorBackend::stopElementSelection()
{
    if (!m_page->corePage())
        return;

    whenFrontendConnectionEstablished([](auto& frontendConnection) {
        frontendConnection.send(Messages::WebInspectorUI::StopElementSelection(), 0);
    });
}

void WebInspectorBackend::elementSelectionChanged(bool active)
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::ElementSelectionChanged(active), m_page->identifier());
}

void WebInspectorBackend::timelineRecordingChanged(bool active)
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::TimelineRecordingChanged(active), m_page->identifier());
}

void WebInspectorBackend::setDeveloperPreferenceOverride(InspectorBackendClient::DeveloperPreference developerPreference, std::optional<bool> overrideValue)
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::SetDeveloperPreferenceOverride(developerPreference, overrideValue), m_page->identifier());
}

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

void WebInspectorBackend::setEmulatedConditions(std::optional<int64_t>&& bytesPerSecondLimit)
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::SetEmulatedConditions(WTF::move(bytesPerSecondLimit)), m_page->identifier());
}

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

// FIXME <https://webkit.org/b/283435>: Remove this unused canAttachWindow function. Its return value is no longer used
// or respected by the UI process.
bool WebInspectorBackend::canAttachWindow()
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
    RefPtr localMainFrame = RefPtr { m_page.get() }->localMainFrame();
    if (!localMainFrame)
        return false;
    unsigned inspectedPageHeight = localMainFrame->protectedView()->visibleHeight();
    unsigned inspectedPageWidth = localMainFrame->protectedView()->visibleWidth();
    unsigned maximumAttachedHeight = inspectedPageHeight * maximumAttachedHeightRatio;
    return minimumAttachedHeight <= maximumAttachedHeight && minimumAttachedWidth <= inspectedPageWidth;
}

void WebInspectorBackend::updateDockingAvailability()
{
    if (m_attached)
        return;

    bool canAttachWindow = this->canAttachWindow();
    if (m_previousCanAttach == canAttachWindow)
        return;

    m_previousCanAttach = canAttachWindow;

    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::AttachAvailabilityChanged(canAttachWindow), m_page->identifier());
}

} // namespace WebKit
