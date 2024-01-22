/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "WebGLVertexArrayObjectOES.h"

#include "WebGLRenderingContextBase.h"

namespace WebCore {

Ref<WebGLVertexArrayObjectOES> WebGLVertexArrayObjectOES::createDefault(WebGLRenderingContextBase& context)
{
    return adoptRef(*new WebGLVertexArrayObjectOES(context, 0, Type::Default));
}

RefPtr<WebGLVertexArrayObjectOES> WebGLVertexArrayObjectOES::createUser(WebGLRenderingContextBase& context)
{
    auto object = context.protectedGraphicsContextGL()->createVertexArray();
    if (!object)
        return nullptr;
    return adoptRef(*new WebGLVertexArrayObjectOES { context, object, Type::User });
}

WebGLVertexArrayObjectOES::WebGLVertexArrayObjectOES(WebGLRenderingContextBase& context, PlatformGLObject object, Type type)
    : WebGLVertexArrayObjectBase(context, object, type)
{
}

WebGLVertexArrayObjectOES::~WebGLVertexArrayObjectOES()
{
    if (!context())
        return;

    runDestructor();
}

void WebGLVertexArrayObjectOES::deleteObjectImpl(const AbstractLocker& locker, GraphicsContextGL* context3d, PlatformGLObject object)
{
    switch (m_type) {
    case Type::Default:
        break;
    case Type::User:
        context3d->deleteVertexArray(object);
        break;
    }

    if (m_boundElementArrayBuffer)
        m_boundElementArrayBuffer->onDetached(locker, context3d);

    for (auto& state : m_vertexAttribState) {
        if (state.bufferBinding)
            state.bufferBinding->onDetached(locker, context3d);
    }
}
}

#endif // ENABLE(WEBGL)
