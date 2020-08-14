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

#include "WebGLCompressedTextureS3TC.h"
#include "WebGLContextGroup.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLLoseContext.h"
#include "WebGLRenderingContextBase.h"

namespace WebCore {

void WebGLObject::setObject(PlatformGLObject object)
{
    ASSERT(!m_object);
    ASSERT(!m_deleted);
    m_object = object;
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
        auto locker = holdLock(lock);
        deleteObject(locker, nullptr);
    }
}

void WebGLObject::deleteObject(const AbstractLocker& locker, GraphicsContextGLOpenGL* context3d)
{
    m_deleted = true;
    if (!m_object)
        return;

    if (!hasGroupOrContext())
        return;

    // ANGLE correctly handles object deletion with WebGL semantics.
    // For other GL implementations, delay deletion until we know
    // the object is not attached anywhere.
#if USE(ANGLE)
    if (!m_calledDelete) {
        m_calledDelete = true;
#else
    if (!m_attachmentCount) {
#endif
        if (!context3d)
            context3d = getAGraphicsContextGL();

        if (context3d)
            deleteObjectImpl(locker, context3d, m_object);
    }

    if (!m_attachmentCount)
        m_object = 0;
}

void WebGLObject::detach()
{
    m_attachmentCount = 0; // Make sure OpenGL resource is deleted.
}

void WebGLObject::onDetached(const AbstractLocker& locker, GraphicsContextGLOpenGL* context3d)
{
    if (m_attachmentCount)
        --m_attachmentCount;
    if (m_deleted)
        deleteObject(locker, context3d);
}

}

#endif // ENABLE(WEBGL)
