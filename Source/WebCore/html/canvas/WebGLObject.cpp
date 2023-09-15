/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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
#include "WebGLObject.h"

#if ENABLE(WEBGL)

#include "WebCoreOpaqueRoot.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLLoseContext.h"
#include "WebGLRenderingContextBase.h"

namespace WebCore {

WebGLObject::WebGLObject(WebGLRenderingContextBase& context, PlatformGLObject object)
    : m_context(context.createRefForContextObject())
    , m_object(object)
{
}

WebGLRenderingContextBase* WebGLObject::context() const
{
    return m_context.get();
}

Lock& WebGLObject::objectGraphLockForContext()
{
    // Should not call this if the object or context has been deleted.
    ASSERT(m_context);
    return m_context->objectGraphLock();
}

GraphicsContextGL* WebGLObject::graphicsContextGL() const
{
    return m_context ? m_context->graphicsContextGL() : nullptr;
}

void WebGLObject::runDestructor()
{
    auto& lock = objectGraphLockForContext();
    if (lock.isHeld()) {
        // Destruction of WebGLObjects can happen in chains triggered from GC.
        // The lock must be held only once, at the beginning of the chain.
        auto locker = AbstractLocker(NoLockingNecessary);
        deleteObject(locker, nullptr);
    } else {
        Locker locker { lock };
        deleteObject(locker, nullptr);
    }
}

void WebGLObject::deleteObject(const AbstractLocker& locker, GraphicsContextGL* context3d)
{
    m_deleted = true;
    if (!m_object)
        return;

    if (!m_context)
        return;

    if (!m_attachmentCount) {
        if (!context3d)
            context3d = graphicsContextGL();

        if (context3d)
            deleteObjectImpl(locker, context3d, m_object);
    }

    if (!m_attachmentCount)
        m_object = 0;
}

void WebGLObject::onDetached(const AbstractLocker& locker, GraphicsContextGL* context3d)
{
    ASSERT(m_attachmentCount); // FIXME: handle attachment with WebGLAttachmentPoint RAII object and remove the ifs.
    if (m_attachmentCount)
        --m_attachmentCount;
    if (m_deleted)
        deleteObject(locker, context3d);
}

bool WebGLObject::validate(const WebGLRenderingContextBase& context) const
{
    return &context == m_context;
}

WebCoreOpaqueRoot root(WebGLObject* object)
{
    return WebCoreOpaqueRoot { object };
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
