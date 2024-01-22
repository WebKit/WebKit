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
#include "WebGLRenderingContextBase.h"
#include <wtf/Lock.h>
#include <wtf/Locker.h>

namespace WebCore {

RefPtr<WebGLSync> WebGLSync::create(WebGLRenderingContextBase& context)
{
    auto object = context.protectedGraphicsContextGL()->fenceSync(GraphicsContextGL::SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (!object)
        return nullptr;
    return adoptRef(*new WebGLSync { context, object });
}

WebGLSync::~WebGLSync()
{
    if (!m_context)
        return;

    runDestructor();
}

WebGLSync::WebGLSync(WebGLRenderingContextBase& context, GCGLsync object)
    : WebGLObject(context, static_cast<PlatformGLObject>(-1)) // This value is unused because the sync object is a pointer type, but it needs to be non-zero or other parts of the code will assume the object is invalid.
    , m_sync(object)
{
}

void WebGLSync::deleteObjectImpl(const AbstractLocker&, GraphicsContextGL* context3d, PlatformGLObject)
{
    context3d->deleteSync(m_sync);
    m_sync = nullptr;
}

void WebGLSync::updateCache(WebGLRenderingContextBase& context)
{
    if (m_syncStatus == GraphicsContextGL::SIGNALED || !m_allowCacheUpdate)
        return;

    m_allowCacheUpdate = false;
    m_syncStatus = context.protectedGraphicsContextGL()->getSynci(m_sync, GraphicsContextGL::SYNC_STATUS);
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
    context.canvasBase().queueTaskKeepingObjectAlive(TaskSource::WebGL, [protectedThis = Ref { *this }]() {
        protectedThis->m_allowCacheUpdate = true;
    });
}

}

#endif // ENABLE(WEBGL)
