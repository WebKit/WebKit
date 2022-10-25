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

#include "AbstractFrame.h"
#include <wtf/RefPtr.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

class RemoteDOMWindow;
class WeakPtrImplWithEventTargetData;

class RemoteFrame final : public AbstractFrame {
public:
    static Ref<RemoteFrame> create(Page& page, FrameIdentifier frameID, AbstractFrame* parent)
    {
        return adoptRef(*new RemoteFrame(page, frameID, parent));
    }
    ~RemoteFrame();

    void setWindow(RemoteDOMWindow*);
    RemoteDOMWindow* window() const;

    void setOpener(AbstractFrame* opener) { m_opener = opener; }
    AbstractFrame* opener() const { return m_opener.get(); }

private:
    WEBCORE_EXPORT explicit RemoteFrame(Page&, FrameIdentifier, AbstractFrame* parent);

    FrameType frameType() const final { return FrameType::Remote; }

    AbstractDOMWindow* virtualWindow() const final;

    WeakPtr<RemoteDOMWindow, WeakPtrImplWithEventTargetData> m_window;

    RefPtr<AbstractFrame> m_opener;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::RemoteFrame)
static bool isType(const WebCore::AbstractFrame& frame) { return frame.frameType() == WebCore::AbstractFrame::FrameType::Remote; }
SPECIALIZE_TYPE_TRAITS_END()
