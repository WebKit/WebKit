/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEBGL)
#include "WebGLSync.h"

#include "HTMLCanvasElement.h"
#include "WebGLContextGroup.h"
#include "WebGLRenderingContextBase.h"

namespace WebCore {

Ref<WebGLSync> WebGLSync::create(WebGLRenderingContextBase& ctx)
{
    return adoptRef(*new WebGLSync(ctx));
}

WebGLSync::~WebGLSync()
{
    deleteObject(0);
}

WebGLSync::WebGLSync(WebGLRenderingContextBase& ctx)
    : WebGLSharedObject(ctx)
    , m_sync(ctx.graphicsContextGL()->fenceSync(GraphicsContextGL::SYNC_GPU_COMMANDS_COMPLETE, 0))
{
    // This value is unused because the sync object is a pointer type, but it needs to be non-zero
    // or other parts of the code will assume the object is invalid.
    setObject(-1);
}

void WebGLSync::deleteObjectImpl(GraphicsContextGLOpenGL* context3d, PlatformGLObject object)
{
    UNUSED_PARAM(object);
    context3d->deleteSync(m_sync);
    m_sync = nullptr;
}

void WebGLSync::updateCache(WebGLRenderingContextBase& context)
{
    if (m_syncStatus == GraphicsContextGL::SIGNALED || !m_allowCacheUpdate)
        return;

    m_allowCacheUpdate = false;
    context.graphicsContextGL()->getSynciv(m_sync, GraphicsContextGL::SYNC_STATUS, sizeof(m_syncStatus), &m_syncStatus);
    if (m_syncStatus == GraphicsContextGL::UNSIGNALED)
        scheduleAllowCacheUpdate(context);
}

GCGLint WebGLSync::getCachedResult(GCGLenum pname) const
{
    switch (pname) {
    case GraphicsContextGL::OBJECT_TYPE:
        return GraphicsContextGL::SYNC_FENCE;
    case GraphicsContextGL::SYNC_STATUS:
        return m_syncStatus;
    case GraphicsContextGL::SYNC_CONDITION:
        return GraphicsContextGL::SYNC_GPU_COMMANDS_COMPLETE;
    case GraphicsContextGL::SYNC_FLAGS:
        return 0;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

bool WebGLSync::isSignaled() const
{
    return m_syncStatus == GraphicsContextGL::SIGNALED;
}

void WebGLSync::scheduleAllowCacheUpdate(WebGLRenderingContextBase& context)
{
    auto* canvas = context.htmlCanvas();
    if (!canvas)
        return;

    context.queueTaskKeepingObjectAlive(*canvas, TaskSource::WebGL, [protectedThis = makeRef(*this)]() {
        protectedThis->m_allowCacheUpdate = true;
    });
}

}

#endif // ENABLE(WEBGL)
