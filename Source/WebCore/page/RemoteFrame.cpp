/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "RemoteFrame.h"

#include "AXObjectCache.h"
#include "AutoplayPolicy.h"
#include "Document.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLFrameOwnerElement.h"
#include "FrameInlines.h"
#include "NodeDocument.h"
#include "NodeInlines.h"
#include "RemoteDOMWindow.h"
#include "RemoteFrameClient.h"
#include "RemoteFrameView.h"
#include "SecurityOrigin.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

Ref<RemoteFrame> RemoteFrame::createMainFrame(Page& page, ClientCreator&& clientCreator, FrameIdentifier identifier, Frame* opener, Ref<FrameTreeSyncData>&& frameTreeSyncData)
{
    return adoptRef(*new RemoteFrame(page, WTF::move(clientCreator), identifier, nullptr, nullptr, std::nullopt, opener, WTF::move(frameTreeSyncData)));
}

Ref<RemoteFrame> RemoteFrame::createSubframe(Page& page, ClientCreator&& clientCreator, FrameIdentifier identifier, Frame& parent, Frame* opener, Ref<FrameTreeSyncData>&& frameTreeSyncData, AddToFrameTree addToFrameTree)
{
    return adoptRef(*new RemoteFrame(page, WTF::move(clientCreator), identifier, nullptr, &parent, std::nullopt, opener, WTF::move(frameTreeSyncData), addToFrameTree));
}

Ref<RemoteFrame> RemoteFrame::createSubframeWithContentsInAnotherProcess(Page& page, ClientCreator&& clientCreator, FrameIdentifier identifier, HTMLFrameOwnerElement& ownerElement, std::optional<LayerHostingContextIdentifier> layerHostingContextIdentifier, Ref<FrameTreeSyncData>&& frameTreeSyncData)
{
    return adoptRef(*new RemoteFrame(page, WTF::move(clientCreator), identifier, &ownerElement, ownerElement.document().frame(), layerHostingContextIdentifier, nullptr, WTF::move(frameTreeSyncData), AddToFrameTree::No));
}

RemoteFrame::RemoteFrame(Page& page, ClientCreator&& clientCreator, FrameIdentifier frameID, HTMLFrameOwnerElement* ownerElement, Frame* parent, Markable<LayerHostingContextIdentifier> layerHostingContextIdentifier, Frame* opener, Ref<FrameTreeSyncData>&& frameTreeSyncData, AddToFrameTree addToFrameTree)
    : Frame(page, frameID, FrameType::Remote, ownerElement, parent, opener, WTF::move(frameTreeSyncData), addToFrameTree)
    , m_window(RemoteDOMWindow::create(*this, GlobalWindowIdentifier { Process::identifier(), WindowIdentifier::generate() }))
    , m_client(clientCreator(*this))
    , m_layerHostingContextIdentifier(layerHostingContextIdentifier)
    , m_autoplayPolicy(AutoplayPolicy::Default)
{
    setView(RemoteFrameView::create(*this));
}

RemoteFrame::~RemoteFrame() = default;

DOMWindow* RemoteFrame::virtualWindow() const
{
    return &window();
}

RemoteDOMWindow& RemoteFrame::window() const
{
    return m_window.get();
}

void RemoteFrame::disconnectView()
{
    m_view = nullptr;
}

void RemoteFrame::didFinishLoadInAnotherProcess()
{
    m_preventsParentFromBeingComplete = false;

    if (RefPtr ownerElement = this->ownerElement())
        ownerElement->document().checkCompleted();
}

bool RemoteFrame::preventsParentFromBeingComplete() const
{
    return m_preventsParentFromBeingComplete;
}

void RemoteFrame::changeLocation(FrameLoadRequest&& request)
{
    m_client->changeLocation(WTF::move(request));
}

void RemoteFrame::loadFrameRequest(FrameLoadRequest&& request, Event*)
{
    m_client->changeLocation(WTF::move(request));
}

void RemoteFrame::updateRemoteFrameAccessibilityOffset(IntPoint offset)
{
    m_client->updateRemoteFrameAccessibilityOffset(frameID(), offset);
}

#if ENABLE(ACCESSIBILITY_LOCAL_FRAME)
void RemoteFrame::updateRemoteFrameAccessibilityInheritedState(const InheritedFrameState& state)
{
    m_client->updateRemoteFrameAccessibilityInheritedState(frameID(), state);
}
#endif

void RemoteFrame::unbindRemoteAccessibilityFrames(int processIdentifier)
{
    m_client->unbindRemoteAccessibilityFrames(processIdentifier);
}

void RemoteFrame::bindRemoteAccessibilityFrames(int processIdentifier, AccessibilityRemoteToken dataToken, CompletionHandler<void(AccessibilityRemoteToken, int)>&& completionHandler)
{
    return m_client->bindRemoteAccessibilityFrames(processIdentifier, frameID(), dataToken, WTF::move(completionHandler));
}

FrameView* RemoteFrame::virtualView() const
{
    return m_view.get();
}

FrameLoaderClient& RemoteFrame::loaderClient()
{
    return m_client.get();
}

void RemoteFrame::setView(RefPtr<RemoteFrameView>&& view)
{
    m_view = WTF::move(view);
}

void RemoteFrame::frameDetached()
{
    m_client->frameDetached();
    m_window->frameDetached();
}

String RemoteFrame::renderTreeAsText(size_t baseIndent, OptionSet<RenderAsTextFlag> behavior)
{
    return m_client->renderTreeAsText(baseIndent, behavior);
}

String RemoteFrame::customUserAgent() const
{
    return m_customUserAgent;
}

String RemoteFrame::customUserAgentAsSiteSpecificQuirks() const
{
    return m_customUserAgentAsSiteSpecificQuirks;
}

String RemoteFrame::customNavigatorPlatform() const
{
    return m_customNavigatorPlatform;
}

void RemoteFrame::documentURLForConsoleLog(CompletionHandler<void(const URL&)>&& completionHandler)
{
    m_client->documentURLForConsoleLog(WTF::move(completionHandler));
}

OptionSet<AdvancedPrivacyProtections> RemoteFrame::advancedPrivacyProtections() const
{
    return m_advancedPrivacyProtections;
}

void RemoteFrame::updateScrollingMode()
{
    if (RefPtr ownerElement = this->ownerElement())
        m_client->updateScrollingMode(ownerElement->scrollingMode());
}

void RemoteFrame::reportMixedContentViolation(bool blocked, const URL& target) const
{
    m_client->reportMixedContentViolation(blocked, target);
}

SecurityOrigin* RemoteFrame::frameDocumentSecurityOrigin() const
{
    return frameTreeSyncData().frameDocumentSecurityOrigin.get();
}

std::optional<DocumentSecurityPolicy> RemoteFrame::frameDocumentSecurityPolicy() const
{
    return frameTreeSyncData().frameDocumentSecurityPolicy;
}

String RemoteFrame::frameURLProtocol() const
{
    return frameTreeSyncData().frameURLProtocol;
}

const SecurityOrigin& RemoteFrame::frameDocumentSecurityOriginOrOpaque() const
{
    if (auto* securityOrigin = frameDocumentSecurityOrigin())
        return *securityOrigin;
    return SecurityOrigin::opaqueOrigin();
}

AutoplayPolicy RemoteFrame::autoplayPolicy() const
{
    return m_autoplayPolicy;
}

float RemoteFrame::usedZoomForChild(const Frame& child) const
{
    auto maybeInfo = frameTreeSyncData().childrenFrameLayoutInfo.getOptional(child.frameID());
    return maybeInfo.transform([] (auto& info) { return info.usedZoom; }).value_or(1.0);
}

} // namespace WebCore
