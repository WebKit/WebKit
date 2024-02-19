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

#include "Document.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLFrameOwnerElement.h"
#include "Page.h"
#include "RemoteDOMWindow.h"
#include "RemoteFrameClient.h"
#include "RemoteFrameView.h"

namespace WebCore {

Ref<RemoteFrame> RemoteFrame::createMainFrame(Page& page, UniqueRef<RemoteFrameClient>&& client, FrameIdentifier identifier, Frame* opener)
{
    ALWAYS_LOG_WITH_STREAM(stream << "**GS** RemoteFrame::createMainFrame(pageID=" << page.identifier() << ", frameId=" << identifier << ", opener=" << opener << ")");
    return adoptRef(*new RemoteFrame(page, WTFMove(client), identifier, nullptr, nullptr, std::nullopt, opener));
}

Ref<RemoteFrame> RemoteFrame::createSubframe(Page& page, UniqueRef<RemoteFrameClient>&& client, FrameIdentifier identifier, Frame& parent)
{
    ALWAYS_LOG_WITH_STREAM(stream << "**GS** RemoteFrame::createSubframe(pageID=" << page.identifier() << ", frameId=" << identifier << ")");
    return adoptRef(*new RemoteFrame(page, WTFMove(client), identifier, nullptr, &parent, std::nullopt));
}

Ref<RemoteFrame> RemoteFrame::createSubframeWithContentsInAnotherProcess(Page& page, UniqueRef<RemoteFrameClient>&& client, FrameIdentifier identifier, HTMLFrameOwnerElement& ownerElement, std::optional<LayerHostingContextIdentifier> layerHostingContextIdentifier)
{
    ALWAYS_LOG_WITH_STREAM(stream << "**GS** RemoteFrame::createSubframeWithContentsInAnotherProcess(pageID=" << page.identifier() << ", frameId=" << identifier << ", layerHostingContextId=" << layerHostingContextIdentifier << ")");
    return adoptRef(*new RemoteFrame(page, WTFMove(client), identifier, &ownerElement, ownerElement.document().frame(), layerHostingContextIdentifier));
}

RemoteFrame::RemoteFrame(Page& page, UniqueRef<RemoteFrameClient>&& client, FrameIdentifier frameID, HTMLFrameOwnerElement* ownerElement, Frame* parent, Markable<LayerHostingContextIdentifier> layerHostingContextIdentifier, Frame* opener)
    : Frame(page, frameID, FrameType::Remote, ownerElement, parent)
    , m_window(RemoteDOMWindow::create(*this, GlobalWindowIdentifier { Process::identifier(), WindowIdentifier::generate() }))
    , m_opener(opener, this, "RemoteFrame::m_opener"_s)
    , m_client(WTFMove(client))
    , m_layerHostingContextIdentifier(layerHostingContextIdentifier)
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

    if (auto* ownerElement = this->ownerElement())
        ownerElement->document().checkCompleted();
}

bool RemoteFrame::preventsParentFromBeingComplete() const
{
    return m_preventsParentFromBeingComplete;
}

void RemoteFrame::changeLocation(FrameLoadRequest&& request)
{
    m_client->changeLocation(WTFMove(request));
}

void RemoteFrame::broadcastFrameRemovalToOtherProcesses()
{
    m_client->broadcastFrameRemovalToOtherProcesses();
}

void RemoteFrame::updateRemoteFrameAccessibilityOffset(IntPoint offset)
{
    m_client->updateRemoteFrameAccessibilityOffset(frameID(), offset);
}

void RemoteFrame::unbindRemoteAccessibilityFrames(int processIdentifier)
{
    m_client->unbindRemoteAccessibilityFrames(processIdentifier);
}

void RemoteFrame::bindRemoteAccessibilityFrames(int processIdentifier, std::span<const uint8_t> dataToken, CompletionHandler<void(const std::span<const uint8_t>, int)>&& completionHandler)
{
    return m_client->bindRemoteAccessibilityFrames(processIdentifier, frameID(), dataToken, WTFMove(completionHandler));
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
    m_view = WTFMove(view);
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

} // namespace WebCore
