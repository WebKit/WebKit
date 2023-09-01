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

#include "GraphicsTypesGL.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class GraphicsContextGL;
class WebCoreOpaqueRoot;
class WebGLRenderingContextBase;

template<typename T, unsigned target = 0>
class WebGLBindingPoint {
    WTF_MAKE_NONCOPYABLE(WebGLBindingPoint);
public:
    WebGLBindingPoint() = default;
    explicit WebGLBindingPoint(RefPtr<T> object)
        : m_object(WTFMove(object))
    {
        if (m_object)
            didBind(*m_object);
    }
    WebGLBindingPoint(WebGLBindingPoint&&) = default;
    ~WebGLBindingPoint() = default;
    WebGLBindingPoint& operator=(WebGLBindingPoint&&) = default;

    WebGLBindingPoint& operator=(RefPtr<T> object)
    {
        if (m_object == object)
            return *this;
        m_object = WTFMove(object);
        if (m_object)
            didBind(*m_object);
        return *this;
    }
    bool operator==(const T* a) const { return a == m_object; }
    bool operator==(const RefPtr<T>& a) const { return a == m_object; }
    explicit operator bool() const { return m_object; }
    T* get() const { return m_object.get(); }
    T* operator->() const { return m_object.get(); }
    T& operator*() const { return *m_object; }
    operator RefPtr<T>() const { return m_object; }

private:
    void didBind(T& object)
    {
        if constexpr(!target)
            object.didBind();
        else
            object.didBind(target);
    }

    RefPtr<T> m_object;
};

class WebGLObject : public RefCounted<WebGLObject> {
public:
    virtual ~WebGLObject() = default;

    WebGLRenderingContextBase* context() const;
    GraphicsContextGL* graphicsContextGL() const;

    PlatformGLObject object() const { return m_object; }

    // deleteObject may not always delete the OpenGL resource.  For programs and
    // shaders, deletion is delayed until they are no longer attached.
    // The AbstractLocker argument enforces at compile time that the objectGraphLock
    // is held. This isn't necessary for all object types, but enough of them that
    // it's done for all of them.
    void deleteObject(const AbstractLocker&, GraphicsContextGL*);

    void onAttached() { ++m_attachmentCount; }
    void onDetached(const AbstractLocker&, GraphicsContextGL*);

    // This indicates whether the client side issue a delete call already, not
    // whether the OpenGL resource is deleted.
    // object()==0 indicates the OpenGL resource is deleted.
    bool isDeleted() const { return m_deleted; }

    // True if this object belongs to the context.
    bool validate(const WebGLRenderingContextBase&) const;

    Lock& objectGraphLockForContext();

protected:
    WebGLObject(WebGLRenderingContextBase&, PlatformGLObject);

    void runDestructor();

    // deleteObjectImpl should be only called once to delete the OpenGL resource.
    virtual void deleteObjectImpl(const AbstractLocker&, GraphicsContextGL*, PlatformGLObject) = 0;

    WeakPtr<WebGLRenderingContextBase> m_context;
private:
    PlatformGLObject m_object { 0 };
    unsigned m_attachmentCount { 0 };
    bool m_deleted { false };
};

template<typename T>
PlatformGLObject objectOrZero(const T& object)
{
    return object ? object->object() : 0;
}

WebCoreOpaqueRoot root(WebGLObject*);

} // namespace WebCore

#endif
