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
#include "RemoteDOMWindow.h"
#include "RemoteFrameClient.h"
#include "RemoteFrameView.h"

namespace WebCore {

Ref<RemoteFrame> RemoteFrame::createMainFrame(Page& page, UniqueRef<RemoteFrameClient>&& client, FrameIdentifier identifier)
{
    return adoptRef(*new RemoteFrame(page, WTFMove(client), identifier, nullptr, nullptr, std::nullopt));
}

Ref<RemoteFrame> RemoteFrame::createSubframe(Page& page, UniqueRef<RemoteFrameClient>&& client, FrameIdentifier identifier, Frame& parent)
{
    return adoptRef(*new RemoteFrame(page, WTFMove(client), identifier, nullptr, &parent, std::nullopt));
}

Ref<RemoteFrame> RemoteFrame::createSubframeWithContentsInAnotherProcess(Page& page, UniqueRef<RemoteFrameClient>&& client, FrameIdentifier identifier, HTMLFrameOwnerElement& ownerElement, std::optional<LayerHostingContextIdentifier> layerHostingContextIdentifier)
{
    return adoptRef(*new RemoteFrame(page, WTFMove(client), identifier, &ownerElement, ownerElement.document().frame(), layerHostingContextIdentifier));
}

RemoteFrame::RemoteFrame(Page& page, UniqueRef<RemoteFrameClient>&& client, FrameIdentifier frameID, HTMLFrameOwnerElement* ownerElement, Frame* parent, Markable<LayerHostingContextIdentifier> layerHostingContextIdentifier)
    : Frame(page, frameID, FrameType::Remote, ownerElement, parent)
    , m_window(RemoteDOMWindow::create(*this, GlobalWindowIdentifier { Process::identifier(), WindowIdentifier::generate() }))
    , m_client(WTFMove(client))
    , m_layerHostingContextIdentifier(layerHostingContextIdentifier)
{
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

void RemoteFrame::didFinishLoadInAnotherProcess()
{
    // FIXME: We also need to set this to false if the load fails in another process.
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

FrameView* RemoteFrame::virtualView() const
{
    return m_view.get();
}

void RemoteFrame::setView(RefPtr<RemoteFrameView>&& view)
{
    m_view = WTFMove(view);
}

void RemoteFrame::frameDetached()
{
    m_client->frameDetached();
}

String RemoteFrame::renderTreeAsText(size_t baseIndent, OptionSet<RenderAsTextFlag> behavior)
{
    return m_client->renderTreeAsText(baseIndent, behavior);
}

} // namespace WebCore
