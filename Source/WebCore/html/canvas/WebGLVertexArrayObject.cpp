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

#if ENABLE(WEBGL) && ENABLE(WEBGL2)
#include "WebGLVertexArrayObject.h"

#include "WebGL2RenderingContext.h"
#include "WebGLContextGroup.h"

namespace WebCore {
    
Ref<WebGLVertexArrayObject> WebGLVertexArrayObject::create(WebGLRenderingContextBase* ctx, VAOType type)
{
    return adoptRef(*new WebGLVertexArrayObject(ctx, type));
}

WebGLVertexArrayObject::~WebGLVertexArrayObject()
{
    deleteObject(0);
}

WebGLVertexArrayObject::WebGLVertexArrayObject(WebGLRenderingContextBase* ctx, VAOType type)
    : WebGLVertexArrayObjectBase(ctx, type)
{
    switch (m_type) {
    case VAOTypeDefault:
        break;
    default:
        setObject(context()->graphicsContext3D()->createVertexArray());
        break;
    }
}

void WebGLVertexArrayObject::deleteObjectImpl(GraphicsContext3D* context3d, Platform3DObject object)
{
    switch (m_type) {
    case VAOTypeDefault:
        break;
    default:
        context3d->deleteVertexArray(object);
        break;
    }
    
    if (m_boundElementArrayBuffer)
        m_boundElementArrayBuffer->onDetached(context3d);
    
    for (auto& state : m_vertexAttribState) {
        if (state.bufferBinding)
            state.bufferBinding->onDetached(context3d);
    }
}

}

#endif // ENABLE(WEBGL)
