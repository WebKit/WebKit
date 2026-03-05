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

#pragma once

#if ENABLE(WEBXR_HIT_TEST)

#include "PlatformXR.h"
#include <wtf/Ref.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class WebXRSession;
class WebXRSpace;
template<typename> class ExceptionOr;

class WebXRHitTestSource : public RefCountedAndCanMakeWeakPtr<WebXRHitTestSource> {
    WTF_MAKE_TZONE_ALLOCATED(WebXRHitTestSource);
public:
    static Ref<WebXRHitTestSource> create(WebXRSession&, PlatformXR::HitTestSource&&, WebXRSpace&);
    ~WebXRHitTestSource();
    ExceptionOr<void> cancel();

    std::optional<PlatformXR::HitTestSource> handle() const { return m_source; }

    WebXRSpace& space() const;

private:
    WebXRHitTestSource(WebXRSession&, PlatformXR::HitTestSource&&, WebXRSpace&);

    WeakPtr<WebXRSession> m_session;
    std::optional<PlatformXR::HitTestSource> m_source;
    Ref<WebXRSpace> m_space;
};

} // namespace WebCore

#endif // ENABLE(WEBXR_HIT_TEST)
