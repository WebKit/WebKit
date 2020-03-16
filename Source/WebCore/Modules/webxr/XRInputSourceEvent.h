/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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

#if ENABLE(WEBXR)

#include "Event.h"
#include <wtf/Optional.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class WebXRFrame;
class WebXRInputSource;

class XRInputSourceEvent final : public Event {
public:
    struct Init : EventInit {
        RefPtr<WebXRFrame> frame;
        RefPtr<WebXRInputSource> inputSource;
        Optional<int> buttonIndex;
    };

    static Ref<XRInputSourceEvent> create(const AtomString&, const Init&, IsTrusted = IsTrusted::No);
    virtual ~XRInputSourceEvent();

    const WebXRFrame& frame() const;
    const WebXRInputSource& inputSource() const;
    Optional<int> buttonIndex() const;

private:
    XRInputSourceEvent(const AtomString&, const Init&, IsTrusted);

    RefPtr<WebXRFrame> m_frame;
    RefPtr<WebXRInputSource> m_inputSource;
    Optional<int> m_buttonIndex;
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
