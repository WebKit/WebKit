/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
#include "WebGLSharedObject.h"

#if ENABLE(WEBGL)

#include "WebGLContextGroup.h"
#include "WebGLRenderingContextBase.h"
#include <wtf/Lock.h>

namespace WebCore {

WebGLSharedObject::WebGLSharedObject(WebGLRenderingContextBase& context)
    : m_contextGroup(context.contextGroup())
{
}

WebGLSharedObject::~WebGLSharedObject()
{
    if (m_contextGroup)
        m_contextGroup->removeObject(*this);
}

void WebGLSharedObject::detachContextGroup(const AbstractLocker& locker)
{
    detach();
    if (m_contextGroup) {
        deleteObject(locker, nullptr);
        m_contextGroup->removeObject(*this);
        m_contextGroup = nullptr;
    }
}

void WebGLSharedObject::detachContextGroupWithoutDeletingObject()
{
    // This can be called during context teardown if the sole context
    // in the share group has already been removed. In this case, the
    // underlying WebGL object has already been implicitly deleted, so
    // it's not necessary to call deleteObject on it - which couldn't
    // be protected by the objectGraphLock.
    detach();
    if (m_contextGroup) {
        m_contextGroup->removeObject(*this);
        m_contextGroup = nullptr;
    }
}

bool WebGLSharedObject::hasGroupOrContext() const
{
    // Returning true from this implies that there's at least one (or,
    // since context sharing isn't currently implemented, exactly one)
    // viable context from which to grab the objectGraphLock.
    return m_contextGroup && m_contextGroup->hasAContext();
}

GraphicsContextGLOpenGL* WebGLSharedObject::getAGraphicsContextGL() const
{
    return m_contextGroup ? &m_contextGroup->getAGraphicsContextGL() : nullptr;
}

WTF::Lock& WebGLSharedObject::objectGraphLockForContext()
{
    // Should not call this if the object or context has been deleted.
    ASSERT(m_contextGroup);
    return m_contextGroup->objectGraphLockForAContext();
}

}

#endif // ENABLE(WEBGL)
