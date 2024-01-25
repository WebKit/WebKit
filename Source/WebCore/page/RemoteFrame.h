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

#pragma once

#include "Frame.h"
#include "LayerHostingContextIdentifier.h"
#include <wtf/RefPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class RemoteDOMWindow;
class RemoteFrameClient;
class RemoteFrameView;
class WeakPtrImplWithEventTargetData;

enum class RenderAsTextFlag : uint16_t;

class RemoteFrame final : public Frame {
public:
    WEBCORE_EXPORT static Ref<RemoteFrame> createMainFrame(Page&, UniqueRef<RemoteFrameClient>&&, FrameIdentifier, Frame* opener = nullptr);
    WEBCORE_EXPORT static Ref<RemoteFrame> createSubframe(Page&, UniqueRef<RemoteFrameClient>&&, FrameIdentifier, Frame& parent);
    WEBCORE_EXPORT static Ref<RemoteFrame> createSubframeWithContentsInAnotherProcess(Page&, UniqueRef<RemoteFrameClient>&&, FrameIdentifier, HTMLFrameOwnerElement&, std::optional<LayerHostingContextIdentifier>);
    ~RemoteFrame();

    RemoteDOMWindow& window() const;

    void setOpener(Frame* opener) final { m_opener = opener; }
    Frame* opener() final { return m_opener.get(); }
    const Frame* opener() const final { return m_opener.get(); }

    const RemoteFrameClient& client() const { return m_client.get(); }
    RemoteFrameClient& client() { return m_client.get(); }

    RemoteFrameView* view() const { return m_view.get(); }
    WEBCORE_EXPORT void setView(RefPtr<RemoteFrameView>&&);

    Markable<LayerHostingContextIdentifier> layerHostingContextIdentifier() const { return m_layerHostingContextIdentifier; }

    String renderTreeAsText(size_t baseIndent, OptionSet<RenderAsTextFlag>);

private:
    WEBCORE_EXPORT explicit RemoteFrame(Page&, UniqueRef<RemoteFrameClient>&&, FrameIdentifier, HTMLFrameOwnerElement*, Frame* parent, Markable<LayerHostingContextIdentifier>, Frame* opener = nullptr);

    void frameDetached() final;
    bool preventsParentFromBeingComplete() const final;
    void changeLocation(FrameLoadRequest&&) final;
    void broadcastFrameRemovalToOtherProcesses() final;
    void didFinishLoadInAnotherProcess() final;
    bool isRootFrame() const final { return false; }

    FrameView* virtualView() const final;
    void disconnectView() final;
    DOMWindow* virtualWindow() const final;
    FrameLoaderClient& loaderClient() final;

    Ref<RemoteDOMWindow> m_window;
    RefPtr<Frame> m_opener;
    RefPtr<RemoteFrameView> m_view;
    UniqueRef<RemoteFrameClient> m_client;
    Markable<LayerHostingContextIdentifier> m_layerHostingContextIdentifier;
    bool m_preventsParentFromBeingComplete { true };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::RemoteFrame)
static bool isType(const WebCore::Frame& frame) { return frame.frameType() == WebCore::Frame::FrameType::Remote; }
SPECIALIZE_TYPE_TRAITS_END()
