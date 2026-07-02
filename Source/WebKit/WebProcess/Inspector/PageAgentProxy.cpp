/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015-2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PageAgentProxy.h"

#include "ProxyingPageAgentMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Document.h>
#include <WebCore/ElementInlines.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameInlines.h>
#include <WebCore/FrameTree.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/InspectorIdentifierRegistry.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/Page.h>
#include <WebCore/PageInspectorController.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/Stopwatch.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;
using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PageAgentProxy);

PageAgentProxy::PageAgentProxy(WebAgentContext& context, WebPage& page)
    : PageAgentInstrumentation(context)
    , m_inspectedPage(*page.corePage())
    , m_page(page)
{
}

PageAgentProxy::~PageAgentProxy()
{
    // Clear the enabledPageProxy slot on our InstrumentingAgents so a later frame commit
    // doesn't dereference this freed proxy from InspectorInstrumentation. Mirrors
    // FrameNetworkAgentProxy::~FrameNetworkAgentProxy().
    disable();
}

void PageAgentProxy::didCreateFrontendAndBackend()
{
}

void PageAgentProxy::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    disable();
}

CommandResult<void> PageAgentProxy::enable()
{
    Ref agents = m_instrumentingAgents.get();
    if (agents->enabledPageProxy() == this)
        return { };

    agents->setEnabledPageProxy(this);
    return { };
}

CommandResult<void> PageAgentProxy::disable()
{
    Ref agents = m_instrumentingAgents.get();
    if (agents->enabledPageProxy() != this)
        return { };

    agents->setEnabledPageProxy(nullptr);
    return { };
}

void PageAgentProxy::domContentEventFired()
{
    Ref inspectedPage = m_inspectedPage.get();

    Ref stopwatch = inspectedPage->inspectorController().executionStopwatch();
    double timestamp = stopwatch->elapsedTime().seconds();

    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingPageAgent::DomContentEventFired(timestamp),
        m_page->identifier());
}

void PageAgentProxy::loadEventFired()
{
    Ref inspectedPage = m_inspectedPage.get();

    Ref stopwatch = inspectedPage->inspectorController().executionStopwatch();
    double timestamp = stopwatch->elapsedTime().seconds();

    protect(WebProcess::singleton().parentProcessConnection())->send(
        Messages::ProxyingPageAgent::LoadEventFired(timestamp),
        m_page->identifier());
}

void PageAgentProxy::frameNavigated(LocalFrame& frame)
{
    auto frameID = frame.frameID();
    RefPtr document = frame.document();
    if (!document)
        return;

    URL url = document->url();
    String mimeType = document->suggestedMIMEType();
    auto securityOrigin = document->securityOrigin().data();

    std::optional<FrameIdentifier> parentFrameID;
    if (auto* parentFrame = frame.tree().parent())
        parentFrameID = parentFrame->frameID();

    String name;
    if (RefPtr ownerElement = frame.ownerElement()) {
        name = ownerElement->getNameAttribute();
        if (name.isEmpty())
            name = ownerElement->attributeWithoutSynchronization(WebCore::HTMLNames::idAttr);
    }

    // Send the committed document's ScriptExecutionContextIdentifier as the loaderId; the
    // UIProcess derives the deterministic, hosting-process-qualified protocol loaderId string
    // from it (consistent across processes, unlike the per-process IdentifierRegistry loaderId).
    // See webkit.org/b/308895.
    auto loaderId = document->identifier();

    RefPtr connection = WebProcess::singleton().parentProcessConnection();
    if (!connection)
        return;
    connection->send(
        Messages::ProxyingPageAgent::FrameNavigated(frameID, url, mimeType, securityOrigin, parentFrameID, name, loaderId),
        m_page->identifier());
}

void PageAgentProxy::frameDetached(LocalFrame& frame)
{
    RefPtr connection = WebProcess::singleton().parentProcessConnection();
    if (!connection)
        return;
    connection->send(
        Messages::ProxyingPageAgent::FrameDetached(frame.frameID()),
        m_page->identifier());
}

void PageAgentProxy::loaderDetachedFromFrame(DocumentLoader&)
{
}

void PageAgentProxy::accessibilitySettingsDidChange()
{
}

void PageAgentProxy::defaultUserPreferencesDidChange()
{
}

#if ENABLE(DARK_MODE_CSS)
void PageAgentProxy::defaultAppearanceDidChange()
{
}
#endif

void PageAgentProxy::applyUserAgentOverride(String&)
{
}

void PageAgentProxy::applyEmulatedMedia(AtomString&)
{
}

void PageAgentProxy::didClearWindowObjectInWorld(LocalFrame&, DOMWrapperWorld&)
{
}

void PageAgentProxy::didPaint(RenderObject&, const LayoutRect&)
{
}

void PageAgentProxy::didLayout()
{
}

void PageAgentProxy::didScroll()
{
}

void PageAgentProxy::didRecalculateStyle()
{
}

} // namespace WebKit
