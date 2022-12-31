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
#include "HTMLFrameOwnerElement.h"
#include "RemoteDOMWindow.h"
#include "RemoteFrameClient.h"
#include "RemoteFrameView.h"

namespace WebCore {

RemoteFrame::RemoteFrame(Page& page, FrameIdentifier frameID, HTMLFrameOwnerElement* ownerElement, UniqueRef<RemoteFrameClient>&& client)
    : AbstractFrame(page, frameID, ownerElement)
    , m_window(RemoteDOMWindow::create(*this, GlobalWindowIdentifier { Process::identifier(), WindowIdentifier::generate() }))
    , m_client(WTFMove(client))
{
}

RemoteFrame::~RemoteFrame() = default;

AbstractDOMWindow* RemoteFrame::virtualWindow() const
{
    return &window();
}

RemoteDOMWindow& RemoteFrame::window() const
{
    return m_window.get();
}

void RemoteFrame::didFinishLoadInAnotherProcess()
{
    auto* ownerElement = this->ownerElement();
    if (!ownerElement)
        return;

    // FIXME: Do something so that this would not have caused the load event to fire before a state change
    // but now does cause the load event to fire.
    ownerElement->document().checkCompleted();
}

AbstractFrameView* RemoteFrame::virtualView() const
{
    return m_view.get();
}

} // namespace WebCore
