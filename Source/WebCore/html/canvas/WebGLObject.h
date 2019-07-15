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

#pragma once

#if ENABLE(WEBGL)

#include "GraphicsContext3D.h"

namespace WebCore {

class WebGLContextGroup;
class WebGLRenderingContextBase;

class WebGLObject : public RefCounted<WebGLObject> {
public:
    virtual ~WebGLObject() = default;

    Platform3DObject object() const { return m_object; }

    // deleteObject may not always delete the OpenGL resource.  For programs and
    // shaders, deletion is delayed until they are no longer attached.
    // FIXME: revisit this when resource sharing between contexts are implemented.
    void deleteObject(GraphicsContext3D*);

    void onAttached() { ++m_attachmentCount; }
    void onDetached(GraphicsContext3D*);

    // This indicates whether the client side issue a delete call already, not
    // whether the OpenGL resource is deleted.
    // object()==0 indicates the OpenGL resource is deleted.
    bool isDeleted() { return m_deleted; }

    // True if this object belongs to the group or context.
    virtual bool validate(const WebGLContextGroup*, const WebGLRenderingContextBase&) const = 0;

protected:
    WebGLObject() = default;

    // setObject should be only called once right after creating a WebGLObject.
    void setObject(Platform3DObject);

    // deleteObjectImpl should be only called once to delete the OpenGL resource.
    virtual void deleteObjectImpl(GraphicsContext3D*, Platform3DObject) = 0;

    virtual bool hasGroupOrContext() const = 0;

    virtual void detach();

    virtual GraphicsContext3D* getAGraphicsContext3D() const = 0;

private:
    Platform3DObject m_object { 0 };
    unsigned m_attachmentCount { 0 };
    bool m_deleted { false };
};

inline Platform3DObject objectOrZero(WebGLObject* object)
{
    return object ? object->object() : 0;
}

} // namespace WebCore

#endif
