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
#include "EXTDisjointTimerQuery.h"

#include "EventLoop.h"
#include "ScriptExecutionContext.h"
#include "WebGLRenderingContext.h"

#include <wtf/IsoMallocInlines.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(EXTDisjointTimerQuery);

EXTDisjointTimerQuery::EXTDisjointTimerQuery(WebGLRenderingContextBase& context)
    : WebGLExtension(context)
{
    context.graphicsContextGL()->ensureExtensionEnabled("GL_EXT_disjoint_timer_query"_s);
}

EXTDisjointTimerQuery::~EXTDisjointTimerQuery() = default;

WebGLExtension::ExtensionName EXTDisjointTimerQuery::getName() const
{
    return EXTDisjointTimerQueryName;
}

bool EXTDisjointTimerQuery::supported(GraphicsContextGL& context)
{
    return context.supportsExtension("GL_EXT_disjoint_timer_query"_s);
}

RefPtr<WebGLTimerQueryEXT> EXTDisjointTimerQuery::createQueryEXT()
{
    auto context = WebGLExtensionScopedContext(this);
    if (context.isLost())
        return nullptr;

    auto query = WebGLTimerQueryEXT::create(*context);
    context->addContextObject(query.get());
    return query;
}

void EXTDisjointTimerQuery::deleteQueryEXT(WebGLTimerQueryEXT* query)
{
    auto context = WebGLExtensionScopedContext(this);
    if (context.isLost())
        return;

    Locker locker { context->objectGraphLock() };

    if (!query)
        return;

    if (!query->validate(context->contextGroup(), *context)) {
        context->synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "delete", "object does not belong to this context");
        return;
    }

    if (query->isDeleted())
        return;

    if (query == context.downcast<WebGLRenderingContext>()->m_activeQuery) {
        context.downcast<WebGLRenderingContext>()->m_activeQuery = nullptr;
        ASSERT(query->target() == GraphicsContextGL::TIME_ELAPSED_EXT);
        context->graphicsContextGL()->endQueryEXT(GraphicsContextGL::TIME_ELAPSED_EXT);
    }

    query->deleteObject(locker, context->graphicsContextGL());
}

GCGLboolean EXTDisjointTimerQuery::isQueryEXT(WebGLTimerQueryEXT* query)
{
    auto context = WebGLExtensionScopedContext(this);
    if (context.isLost())
        return false;

    if (!query || !query->validate(context->contextGroup(), *context))
        return false;

    if (query->isDeleted())
        return false;

    return context->graphicsContextGL()->isQueryEXT(query->object());
}

void EXTDisjointTimerQuery::beginQueryEXT(GCGLenum target, WebGLTimerQueryEXT& query)
{
    auto context = WebGLExtensionScopedContext(this);
    if (context.isLost())
        return;

    Locker locker { context->objectGraphLock() };

    if (!context->validateWebGLObject("beginQueryEXT", &query))
        return;

    // The WebGL extension requires ending time elapsed queries when they are deleted.
    // Ending non-active queries is invalid so m_activeQuery is used to track them and
    // to defer query results until control is returned to the user agent's main loop.

    if (target != GraphicsContextGL::TIME_ELAPSED_EXT) {
        context->synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "beginQueryEXT", "invalid target");
        return;
    }

    if (query.target() && query.target() != target) {
        context->synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginQueryEXT", "query type does not match target");
        return;
    }

    if (context.downcast<WebGLRenderingContext>()->m_activeQuery) {
        context->synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginQueryEXT", "query object of target is already active");
        return;
    }

    query.setTarget(target);
    context.downcast<WebGLRenderingContext>()->m_activeQuery = &query;

    context->graphicsContextGL()->beginQueryEXT(target, query.object());
}

void EXTDisjointTimerQuery::endQueryEXT(GCGLenum target)
{
    auto context = WebGLExtensionScopedContext(this);
    if (context.isLost() || !context->scriptExecutionContext())
        return;

    Locker locker { context->objectGraphLock() };

    if (target != GraphicsContextGL::TIME_ELAPSED_EXT) {
        context->synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "endQueryEXT", "invalid target");
        return;
    }

    if (!context.downcast<WebGLRenderingContext>()->m_activeQuery) {
        context->synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "endQueryEXT", "query object of target is not active");
        return;
    }

    context->graphicsContextGL()->endQueryEXT(target);

    // A query's result must not be made available until control has returned to the user agent's main loop.
    context->scriptExecutionContext()->eventLoop().queueMicrotask([query = WTFMove(context.downcast<WebGLRenderingContext>()->m_activeQuery)] {
        query->makeResultAvailable();
    });
}

void EXTDisjointTimerQuery::queryCounterEXT(WebGLTimerQueryEXT& query, GCGLenum target)
{
    auto context = WebGLExtensionScopedContext(this);
    if (context.isLost() || !context->scriptExecutionContext())
        return;

    if (!context->validateWebGLObject("queryCounterEXT", &query))
        return;

    if (target != GraphicsContextGL::TIMESTAMP_EXT) {
        context->synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "queryCounterEXT", "invalid target");
        return;
    }

    if (query.target() && query.target() != target) {
        context->synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "queryCounterEXT", "query type does not match target");
        return;
    }

    query.setTarget(target);

    context->graphicsContextGL()->queryCounterEXT(query.object(), target);

    // A query's result must not be made available until control has returned to the user agent's main loop.
    context->scriptExecutionContext()->eventLoop().queueMicrotask([&] {
        query.makeResultAvailable();
    });
}

WebGLAny EXTDisjointTimerQuery::getQueryEXT(GCGLenum target, GCGLenum pname)
{
    auto context = WebGLExtensionScopedContext(this);
    if (context.isLost())
        return nullptr;

    if (target != GraphicsContextGL::TIME_ELAPSED_EXT && target != GraphicsContextGL::TIMESTAMP_EXT) {
        context->synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getQueryEXT", "invalid target");
        return nullptr;
    }

    switch (pname) {
    case GraphicsContextGL::CURRENT_QUERY_EXT:
        if (target == GraphicsContextGL::TIME_ELAPSED_EXT)
            return context.downcast<WebGLRenderingContext>()->m_activeQuery;
        return nullptr;
    case GraphicsContextGL::QUERY_COUNTER_BITS_EXT:
        return context->graphicsContextGL()->getQueryiEXT(target, pname);
    }
    context->synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getQueryEXT", "invalid parameter name");
    return nullptr;
}

WebGLAny EXTDisjointTimerQuery::getQueryObjectEXT(WebGLTimerQueryEXT& query, GCGLenum pname)
{
    auto context = WebGLExtensionScopedContext(this);
    if (context.isLost())
        return nullptr;

    if (!context->validateWebGLObject("getQueryObjectEXT", &query))
        return nullptr;

    if (!query.target()) {
        context->synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getQueryObjectEXT", "query has not been used");
        return nullptr;
    }

    if (&query == context.downcast<WebGLRenderingContext>()->m_activeQuery) {
        context->synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getQueryObjectEXT", "query is currently active");
        return nullptr;
    }

    switch (pname) {
    case GraphicsContextGL::QUERY_RESULT_EXT:
        if (!query.isResultAvailable())
            return 0;
        return static_cast<unsigned long long>(context->graphicsContextGL()->getQueryObjectui64EXT(query.object(), pname));
    case GraphicsContextGL::QUERY_RESULT_AVAILABLE_EXT:
        if (!query.isResultAvailable())
            return false;
        return static_cast<bool>(context->graphicsContextGL()->getQueryObjectiEXT(query.object(), pname));
    }
    context->synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getQueryObjectEXT", "invalid parameter name");
    return nullptr;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
