/*
 * Copyright 2023 The Chromium Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEBGL)
#include "EXTDisjointTimerQueryWebGL2.h"

#include "EventLoop.h"
#include "ScriptExecutionContext.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(EXTDisjointTimerQueryWebGL2);

EXTDisjointTimerQueryWebGL2::EXTDisjointTimerQueryWebGL2(WebGLRenderingContextBase& context)
    : WebGLExtension(context, WebGLExtensionName::EXTDisjointTimerQueryWebGL2)
{
    context.protectedGraphicsContextGL()->ensureExtensionEnabled("GL_EXT_disjoint_timer_query"_s);
}

EXTDisjointTimerQueryWebGL2::~EXTDisjointTimerQueryWebGL2() = default;

bool EXTDisjointTimerQueryWebGL2::supported(GraphicsContextGL& context)
{
    return context.supportsExtension("GL_EXT_disjoint_timer_query"_s);
}

void EXTDisjointTimerQueryWebGL2::queryCounterEXT(WebGLQuery& query, GCGLenum target)
{
    if (isContextLost())
        return;
    auto& context = this->context();
    if (!context.scriptExecutionContext())
        return;

    if (!context.validateWebGLObject("queryCounterEXT", query))
        return;

    if (target != GraphicsContextGL::TIMESTAMP_EXT) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "queryCounterEXT", "invalid target");
        return;
    }

    if (query.target() && query.target() != target) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "queryCounterEXT", "query type does not match target");
        return;
    }

    query.setTarget(target);

    context.protectedGraphicsContextGL()->queryCounterEXT(query.object(), target);

    // A query's result must not be made available until control has returned to the user agent's main loop.
    context.scriptExecutionContext()->eventLoop().queueMicrotask([&] {
        query.makeResultAvailable();
    });
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
