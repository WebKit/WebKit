/*
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
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
#include "WebXRHitTestSource.h"

#include "WebXRSession.h"
#include <wtf/Ref.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBXR_HIT_TEST)

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebXRHitTestSource);

Ref<WebXRHitTestSource> WebXRHitTestSource::create(WebXRSession& session, PlatformXR::HitTestSource&& source, WebXRSpace& space)
{
    return adoptRef(*new WebXRHitTestSource(session, WTF::move(source), space));
}

WebXRHitTestSource::WebXRHitTestSource(WebXRSession& session, PlatformXR::HitTestSource&& source, WebXRSpace& space)
    : m_session(session)
    , m_source(WTF::move(source))
    , m_space(space)
{
}

WebXRHitTestSource::~WebXRHitTestSource()
{
    cancel();
}

ExceptionOr<void> WebXRHitTestSource::cancel()
{
    if (!m_source)
        return Exception { ExceptionCode::InvalidStateError };
    RefPtr session = m_session.get();
    if (!session)
        return Exception { ExceptionCode::InvalidStateError };

    auto result = session->cancelHitTestSource(*m_source);
    m_source.reset();

    if (result.hasException())
        return result.releaseException();
    return { };
}

WebXRSpace& WebXRHitTestSource::space() const
{
    return m_space;
}

} // namespace WebCore

#endif // ENABLE(WEBXR_HIT_TEST)
