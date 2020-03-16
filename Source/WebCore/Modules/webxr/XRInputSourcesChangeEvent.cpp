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
#include "XRInputSourcesChangeEvent.h"

#if ENABLE(WEBXR)

#include "WebXRInputSource.h"
#include "WebXRSession.h"

namespace WebCore {

Ref<XRInputSourcesChangeEvent> XRInputSourcesChangeEvent::create(const AtomString& type, const Init& initializer, IsTrusted isTrusted)
{
    return adoptRef(*new XRInputSourcesChangeEvent(type, initializer, isTrusted));
}

XRInputSourcesChangeEvent::XRInputSourcesChangeEvent(const AtomString& type, const Init& initializer, IsTrusted isTrusted)
    : Event(type, initializer, isTrusted)
    , m_session(*initializer.session)
    , m_added(initializer.added)
    , m_removed(initializer.removed)
{
}

XRInputSourcesChangeEvent::~XRInputSourcesChangeEvent() = default;

const WebXRSession& XRInputSourcesChangeEvent::session() const
{
    return m_session;
}

const Vector<RefPtr<WebXRInputSource>>& XRInputSourcesChangeEvent::added() const
{
    return m_added;
}

const Vector<RefPtr<WebXRInputSource>>& XRInputSourcesChangeEvent::removed() const
{
    return m_removed;
}

} // namespace WebCore

#endif // ENABLE(WEBXR)
