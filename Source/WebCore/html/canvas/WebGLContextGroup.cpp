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
#include "WebGLContextGroup.h"

#if ENABLE(WEBGL)

#include "GraphicsContext3D.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLSharedObject.h"

namespace WebCore {

Ref<WebGLContextGroup> WebGLContextGroup::create()
{
    return adoptRef(*new WebGLContextGroup);
}

WebGLContextGroup::~WebGLContextGroup()
{
    detachAndRemoveAllObjects();
}

GraphicsContext3D& WebGLContextGroup::getAGraphicsContext3D()
{
    ASSERT(!m_contexts.isEmpty());
    return *(*m_contexts.begin())->graphicsContext3D();
}

void WebGLContextGroup::addContext(WebGLRenderingContextBase& context)
{
    m_contexts.add(&context);
}

void WebGLContextGroup::removeContext(WebGLRenderingContextBase& context)
{
    // We must call detachAndRemoveAllObjects before removing the last context.
    if (m_contexts.size() == 1 && m_contexts.contains(&context))
        detachAndRemoveAllObjects();

    m_contexts.remove(&context);
}

void WebGLContextGroup::removeObject(WebGLSharedObject& object)
{
    m_groupObjects.remove(&object);
}

void WebGLContextGroup::addObject(WebGLSharedObject& object)
{
    m_groupObjects.add(&object);
}

void WebGLContextGroup::detachAndRemoveAllObjects()
{
    while (!m_groupObjects.isEmpty())
        (*m_groupObjects.begin())->detachContextGroup();
}

void WebGLContextGroup::loseContextGroup(WebGLRenderingContextBase::LostContextMode mode)
{
    for (auto& context : m_contexts)
        context->loseContextImpl(mode);

    detachAndRemoveAllObjects();
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
