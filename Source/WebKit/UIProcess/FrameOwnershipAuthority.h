/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "Untrusted.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/FrameIdentifier.h>
#include <wtf/WeakPtr.h>

namespace WebKit {

// Vouches for a frame identifier a web content process named: the frame exists, it belongs to the
// page the message was addressed to, and the process that sent the message hosts it.
//
// This is the procedure for the frame a message acts on. A message that acts on a frame's parent -
// because the attribute it carries lives on the owner element rather than on the frame - is the
// parent's to speak for, and needs a procedure of its own.
class FrameHostedBySenderAuthority : public IPC::CanValidateUntrusted<FrameHostedBySenderAuthority> {
public:
    FrameHostedBySenderAuthority(const WebProcessProxy& sender, const WebPageProxy& page)
        : m_sender(sender)
        , m_page(page)
    {
    }

    std::optional<IPC::ValidationFailure> checkUntrusted(const WebCore::FrameIdentifier& frameID) const
    {
        RefPtr sender = m_sender.get();
        RefPtr page = m_page.get();
        if (!sender || !page)
            return IPC::ValidationFailure::Ignore;

        // Either the frame was destroyed while this message was in flight, or the sender named a
        // frame that has never existed. Both leave nothing to act on, and the first is routine, so
        // this is not by itself evidence of the second.
        RefPtr frame = WebFrameProxy::webFrame(frameID);
        if (!frame)
            return IPC::ValidationFailure::Ignore;

        // A frame outlives its page only while the page is being torn down.
        RefPtr framePage = frame->page();
        if (!framePage)
            return IPC::ValidationFailure::Ignore;

        // A frame never moves between pages, so naming a frame of some other page is not something
        // a race can produce. WebPageProxy::focusedFrameChanged already terminates for this.
        if (framePage.get() != page.get())
            return IPC::ValidationFailure::Terminate;

        // With site isolation the process hosting a frame changes as that frame navigates, and a
        // message the previous process sent before the swap is still legitimately in flight. So a
        // mismatch here is a lost race rather than a claim the sender could not have made honestly;
        // didSameDocumentNavigationForFrameViaJS already reaches that conclusion by hand.
        if (&frame->process() != sender.get())
            return IPC::ValidationFailure::Ignore;

        return std::nullopt;
    }

private:
    // Weak, so that constructing a validator cannot extend the life of a process or a page, and a
    // validator that outlives either drops the message rather than crashing.
    WeakPtr<const WebProcessProxy> m_sender;
    WeakPtr<const WebPageProxy> m_page;
};

} // namespace WebKit

namespace IPC {

template<> struct IsValidationProcedureFor<WebKit::FrameHostedBySenderAuthority, WebCore::FrameIdentifier> : std::true_type { };

} // namespace IPC
