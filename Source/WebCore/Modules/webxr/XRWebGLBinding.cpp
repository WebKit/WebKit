/*
 * Copyright (C) 2024 Apple, Inc. All rights reserved.
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
#include "XRWebGLBinding.h"

#if ENABLE(WEBXR_LAYERS)

#include "ExceptionOr.h"
#include "WebGL2RenderingContext.h"
#include "WebGLRenderingContext.h"
#include "WebGLRenderingContextBase.h"
#include "WebXRSession.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(XRWebGLBinding);

ExceptionOr<Ref<XRWebGLBinding>> XRWebGLBinding::create(Ref<WebXRSession>&& session, WebXRWebGLRenderingContext&& context)
{
    if (session->ended())
        return Exception { ExceptionCode::InvalidStateError, "Cannot create an XRWebGLBinding with an XRSession that has ended."_s };

    return WTF::switchOn(WTF::move(context),
        [&](auto&& context) -> ExceptionOr<Ref<XRWebGLBinding>> {
            if (context->isContextLost())
                return Exception { ExceptionCode::InvalidStateError, "Cannot create an XRWebGLBinding with a lost WebGL context."_s };

            if (!isImmersive(session->mode()))
                return Exception { ExceptionCode::InvalidStateError, "Cannot create an XRWebGLBinding for non immersive sessions."_s };

            if (!context->isXRCompatible())
                return Exception { ExceptionCode::InvalidStateError, "Cannot create an XRWebGLBinding with a non XR compatible WebGL context."_s };

            return adoptRef(*new XRWebGLBinding(WTF::move(session), WTF::move(context)));
        }
    );
}

XRWebGLBinding::XRWebGLBinding(Ref<WebXRSession>&& session, WebXRWebGLRenderingContext&& context)
    : m_session(WTF::move(session))
    , m_context(WTF::move(context))
{
}

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)

