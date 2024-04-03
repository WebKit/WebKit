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
#include "WebGLTimerQueryEXT.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(EXTDisjointTimerQuery);

EXTDisjointTimerQuery::EXTDisjointTimerQuery(WebGLRenderingContext& context)
    : WebGLExtension(context, WebGLExtensionName::EXTDisjointTimerQuery)
{
    context.protectedGraphicsContextGL()->ensureExtensionEnabled("GL_EXT_disjoint_timer_query"_s);
}

EXTDisjointTimerQuery::~EXTDisjointTimerQuery() = default;

bool EXTDisjointTimerQuery::supported(GraphicsContextGL& context)
{
    return context.supportsExtension("GL_EXT_disjoint_timer_query"_s);
}

RefPtr<WebGLTimerQueryEXT> EXTDisjointTimerQuery::createQueryEXT()
{
    if (isContextLost())
        return nullptr;
    auto& context = this->context();
    return WebGLTimerQueryEXT::create(context);
}

void EXTDisjointTimerQuery::deleteQueryEXT(WebGLTimerQueryEXT* query)
{
    if (isContextLost())
        return;
    auto& context = this->context();

    Locker locker { context.objectGraphLock() };

    if (!query)
        return;

    if (!query->validate(context)) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "delete", "object does not belong to this context");
        return;
    }

    if (query->isDeleted())
        return;

    if (query == context.m_activeQuery) {
        context.m_activeQuery = nullptr;
        ASSERT(query->target() == GraphicsContextGL::TIME_ELAPSED_EXT);
        context.protectedGraphicsContextGL()->endQueryEXT(GraphicsContextGL::TIME_ELAPSED_EXT);
    }

    query->deleteObject(locker, context.protectedGraphicsContextGL().get());
}

GCGLboolean EXTDisjointTimerQuery::isQueryEXT(WebGLTimerQueryEXT* query)
{
    if (isContextLost())
        return false;
    auto& context = this->context();
    if (!context.validateIsWebGLObject(query))
        return false;
    return context.protectedGraphicsContextGL()->isQueryEXT(query->object());
}

void EXTDisjointTimerQuery::beginQueryEXT(GCGLenum target, WebGLTimerQueryEXT& query)
{
    if (isContextLost())
        return;
    auto& context = this->context();

    Locker locker { context.objectGraphLock() };

    if (!context.validateWebGLObject("beginQueryEXT", query))
        return;

    // The WebGL extension requires ending time elapsed queries when they are deleted.
    // Ending non-active queries is invalid so m_activeQuery is used to track them and
    // to defer query results until control is returned to the user agent's main loop.

    if (target != GraphicsContextGL::TIME_ELAPSED_EXT) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "beginQueryEXT", "invalid target");
        return;
    }

    if (query.target() && query.target() != target) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginQueryEXT", "query type does not match target");
        return;
    }

    if (context.m_activeQuery) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginQueryEXT", "query object of target is already active");
        return;
    }

    context.m_activeQuery = &query;

    context.protectedGraphicsContextGL()->beginQueryEXT(target, query.object());
}

void EXTDisjointTimerQuery::endQueryEXT(GCGLenum target)
{
    if (isContextLost())
        return;
    auto& context = this->context();
    if (!context.scriptExecutionContext())
        return;

    Locker locker { context.objectGraphLock() };

    if (target != GraphicsContextGL::TIME_ELAPSED_EXT) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "endQueryEXT", "invalid target");
        return;
    }

    if (!context.m_activeQuery) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "endQueryEXT", "query object of target is not active");
        return;
    }

    context.protectedGraphicsContextGL()->endQueryEXT(target);

    // A query's result must not be made available until control has returned to the user agent's main loop.
    context.scriptExecutionContext()->eventLoop().queueMicrotask([query = WTFMove(context.m_activeQuery)] {
        query->makeResultAvailable();
    });
}

void EXTDisjointTimerQuery::queryCounterEXT(WebGLTimerQueryEXT& query, GCGLenum target)
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

WebGLAny EXTDisjointTimerQuery::getQueryEXT(GCGLenum target, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;
    auto& context = this->context();

    if (target != GraphicsContextGL::TIME_ELAPSED_EXT && target != GraphicsContextGL::TIMESTAMP_EXT) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getQueryEXT", "invalid target");
        return nullptr;
    }

    switch (pname) {
    case GraphicsContextGL::CURRENT_QUERY_EXT:
        if (target == GraphicsContextGL::TIME_ELAPSED_EXT)
            return context.m_activeQuery;
        return nullptr;
    case GraphicsContextGL::QUERY_COUNTER_BITS_EXT:
        return context.protectedGraphicsContextGL()->getQueryiEXT(target, pname);
    }
    context.synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getQueryEXT", "invalid parameter name");
    return nullptr;
}

WebGLAny EXTDisjointTimerQuery::getQueryObjectEXT(WebGLTimerQueryEXT& query, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;
    auto& context = this->context();

    if (!context.validateWebGLObject("getQueryObjectEXT", query))
        return nullptr;

    if (!query.target()) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getQueryObjectEXT", "query has not been used");
        return nullptr;
    }

    if (&query == context.m_activeQuery) {
        context.synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getQueryObjectEXT", "query is currently active");
        return nullptr;
    }

    switch (pname) {
    case GraphicsContextGL::QUERY_RESULT_EXT:
        if (!query.isResultAvailable())
            return 0;
        return static_cast<unsigned long long>(context.protectedGraphicsContextGL()->getQueryObjectui64EXT(query.object(), pname));
    case GraphicsContextGL::QUERY_RESULT_AVAILABLE_EXT:
        if (!query.isResultAvailable())
            return false;
        return static_cast<bool>(context.protectedGraphicsContextGL()->getQueryObjectiEXT(query.object(), pname));
    }
    context.synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getQueryObjectEXT", "invalid parameter name");
    return nullptr;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
