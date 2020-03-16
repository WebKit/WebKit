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

#include "config.h"
#include "XRInputSourceEvent.h"

#if ENABLE(WEBXR)

#include "WebXRFrame.h"
#include "WebXRInputSource.h"

namespace WebCore {

Ref<XRInputSourceEvent> XRInputSourceEvent::create(const AtomString& type, const Init& initializer, IsTrusted isTrusted)
{
    return adoptRef(*new XRInputSourceEvent(type, initializer, isTrusted));
}

XRInputSourceEvent::XRInputSourceEvent(const AtomString& type, const Init& initializer, IsTrusted isTrusted)
    : Event(type, initializer, isTrusted)
    , m_frame(initializer.frame)
    , m_inputSource(initializer.inputSource)
    , m_buttonIndex(initializer.buttonIndex)
{
    ASSERT(m_frame);
    ASSERT(m_inputSource);
}

XRInputSourceEvent::~XRInputSourceEvent() = default;

const WebXRFrame& XRInputSourceEvent::frame() const
{
    return *m_frame;
}

const WebXRInputSource& XRInputSourceEvent::inputSource() const
{
    return *m_inputSource;
}

Optional<int> XRInputSourceEvent::buttonIndex() const
{
    return m_buttonIndex;
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
