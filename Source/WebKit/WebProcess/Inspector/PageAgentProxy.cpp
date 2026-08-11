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
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentView.h>
#include <WebCore/ElementInlines.h>
#include <WebCore/FloatRect.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameTree.h>
#include <WebCore/HTMLFrameOwnerElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/InspectorBackendClient.h>
#include <WebCore/InspectorIdentifierRegistry.h>
#include <WebCore/InstrumentingAgents.h>
#include <WebCore/LayoutRect.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Page.h>
#include <WebCore/PageInspectorController.h>
#include <WebCore/RenderObjectInlines.h>
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

    // Send the loaderId for the just-committed DocumentLoader. The registry memoizes one string
    // per loader, so this matches the id the network path already reported for the same loader.
    Ref inspectedPage = m_inspectedPage.get();
    Ref registry = inspectedPage->inspectorController().identifierRegistry();
    RefPtr documentLoader = frame.loader().documentLoader();
    auto loaderId = registry->loaderId(documentLoader.get());

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

void PageAgentProxy::didPaint(RenderObject& renderer, const LayoutRect& rect)
{
    if (!m_showPaintRects)
        return;

    // The main-frame process has a real, enabled InspectorPageAgent that already drew this paint via
    // the enabledPageAgent() branch in didPaintImpl; drawing again here would double-flash it. This
    // proxy only draws where no real page agent is enabled -- the cross-origin subframe processes.
    Ref agents = m_instrumentingAgents.get();
    if (agents->enabledPageAgent())
        return;

    Ref inspectedPage = m_inspectedPage.get();
    auto* client = inspectedPage->inspectorController().inspectorBackendClient();
    if (!client)
        return;

    RefPtr view = renderer.document().view();
    if (!view)
        return;

    // Draw in this frame's own contents coordinate space. The overlay's layer lives inside the
    // scrolled-contents layer that the compositor already offsets by -scrollPosition, so passing
    // contents coords applies that offset exactly once. Do NOT convert with contentsToRootView()
    // (double-counts scroll) or remap through the main frame (it is a RemoteFrame here).
    // FIXME: localToAbsoluteQuad is frame-local, so coords are in the painting frame's own space --
    // correct only when that frame is its own local root. A same-site child frame nested inside a
    // local root in the same process, or a local subframe reached through a remote ancestor, is
    // drawn at the wrong offset. See webkit.org/b/308899.
    LayoutRect absoluteRect = LayoutRect(renderer.localToAbsoluteQuad(FloatRect(rect)).boundingBox());

    // Scope to this frame's local root: the overlay for that root is attached to its own compositing
    // tree, so sibling local roots in one process each flash independently.
    Ref rootFrame = view->frame().rootFrame();
    client->showPaintRect(rootFrame.get(), snappedIntRect(absoluteRect));
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
