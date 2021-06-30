/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGL) && USE(ANGLE)

#include "ANGLEHeaders.h"
#include "GraphicsTypesGL.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class ScopedRestoreTextureBinding {
    WTF_MAKE_NONCOPYABLE(ScopedRestoreTextureBinding);
public:
    ScopedRestoreTextureBinding(GLenum bindingPointQuery, GLenum bindingPoint, bool condition = true)
    {
        ASSERT(bindingPoint != static_cast<GLenum>(0u));
        if (condition) {
            m_bindingPoint = bindingPoint;
            gl::GetIntegerv(bindingPointQuery, reinterpret_cast<GLint*>(&m_bindingValue));
        }
    }

    ~ScopedRestoreTextureBinding()
    {
        if (m_bindingPoint)
            gl::BindTexture(m_bindingPoint, m_bindingValue);
    }

private:
    GLenum m_bindingPoint { 0 };
    GLuint m_bindingValue { 0u };
};


class ScopedBufferBinding {
    WTF_MAKE_NONCOPYABLE(ScopedBufferBinding);
public:
    ScopedBufferBinding(GLenum bindingPoint, GLuint bindingValue, bool condition = true)
    {
        if (!condition)
            return;
        gl::GetIntegerv(query(bindingPoint), reinterpret_cast<GLint*>(&m_bindingValue));
        if (bindingValue == m_bindingValue)
            return;
        m_bindingPoint = bindingPoint;
        gl::BindBuffer(m_bindingPoint, bindingValue);
    }

    ~ScopedBufferBinding()
    {
        if (m_bindingPoint)
            gl::BindBuffer(m_bindingPoint, m_bindingValue);
    }

private:
    static constexpr GLenum query(GLenum bindingPoint)
    {
        if (bindingPoint == GL_PIXEL_PACK_BUFFER)
            return GL_PIXEL_PACK_BUFFER_BINDING;
        ASSERT(bindingPoint == GL_PIXEL_UNPACK_BUFFER);
        return GL_PIXEL_UNPACK_BUFFER_BINDING;
    }
    GLint m_bindingPoint { 0 };
    GLuint m_bindingValue { 0u };
};
class ScopedRestoreReadFramebufferBinding {
    WTF_MAKE_NONCOPYABLE(ScopedRestoreReadFramebufferBinding);
public:
    ScopedRestoreReadFramebufferBinding(bool isForWebGL2, GLuint restoreValue)
        : m_framebufferTarget(isForWebGL2 ? GL_READ_FRAMEBUFFER : GL_FRAMEBUFFER)
        , m_bindingValue(restoreValue)
    {
    }
    ScopedRestoreReadFramebufferBinding(bool isForWebGL2, GLuint restoreValue, GLuint value)
        : m_framebufferTarget(isForWebGL2 ? GL_READ_FRAMEBUFFER : GL_FRAMEBUFFER)
        , m_bindingValue(restoreValue)
    {
        bindFramebuffer(value);
    }
    void markBindingChanged()
    {
        m_bindingChanged = true;
    }
    void bindFramebuffer(GLuint bindingValue)
    {
        if (!m_bindingChanged && m_bindingValue == bindingValue)
            return;
        gl::BindFramebuffer(m_framebufferTarget, bindingValue);
        m_bindingChanged = m_bindingValue != bindingValue;
    }
    GLuint framebufferTarget() const { return m_framebufferTarget; }
    ~ScopedRestoreReadFramebufferBinding()
    {
        if (m_bindingChanged)
            gl::BindFramebuffer(m_framebufferTarget, m_bindingValue);
    }
private:
    const GLenum m_framebufferTarget;
    GLuint m_bindingValue { 0u };
    bool m_bindingChanged { false };
};

class ScopedPixelStorageMode {
    WTF_MAKE_NONCOPYABLE(ScopedPixelStorageMode);
public:
    explicit ScopedPixelStorageMode(GLenum name, bool condition = true)
        : m_name(condition ? name : 0)
    {
        if (m_name)
            gl::GetIntegerv(m_name, &m_originalValue);
    }
    ScopedPixelStorageMode(GLenum name, GLint value, bool condition = true)
        : m_name(condition ? name : 0)
    {
        if (m_name) {
            gl::GetIntegerv(m_name, &m_originalValue);
            pixelStore(value);
        }
    }
    ~ScopedPixelStorageMode()
    {
        if (m_name && m_valueChanged)
            gl::PixelStorei(m_name, m_originalValue);
    }
    void pixelStore(GLint value)
    {
        ASSERT(m_name);
        if (!m_valueChanged && m_originalValue == value)
            return;
        gl::PixelStorei(m_name, value);
        m_valueChanged = m_originalValue != value;
    }
    operator GLint() const
    {
        ASSERT(m_name && !m_valueChanged);
        return m_originalValue;
    }
private:
    const GLenum m_name;
    bool m_valueChanged { false };
    GLint m_originalValue { 0 };
};

class ScopedTexture {
    WTF_MAKE_NONCOPYABLE(ScopedTexture);
public:
    ScopedTexture()
    {
        gl::GenTextures(1, &m_object);
    }
    ~ScopedTexture()
    {
        gl::DeleteTextures(1, &m_object);
    }
    operator GLuint() const
    {
        return m_object;
    }
private:
    GLuint m_object { 0u };
};

class ScopedFramebuffer {
    WTF_MAKE_NONCOPYABLE(ScopedFramebuffer);
public:
    ScopedFramebuffer()
    {
        gl::GenFramebuffers(1, &m_object);
    }
    ~ScopedFramebuffer()
    {
        gl::DeleteFramebuffers(1, &m_object);
    }
    operator GLuint() const
    {
        return m_object;
    }
private:
    GLuint m_object { 0 };
};

class ScopedGLFence {
    WTF_MAKE_NONCOPYABLE(ScopedGLFence);
public:
    ScopedGLFence() = default;
    ScopedGLFence(ScopedGLFence&& other)
        : m_object(std::exchange(other.m_object, { }))
    {
    }
    ~ScopedGLFence() { reset(); }
    ScopedGLFence& operator=(ScopedGLFence&& other)
    {
        if (this != &other) {
            reset();
            m_object = std::exchange(other.m_object, { });
        }
        return *this;
    }
    void reset()
    {
        if (m_object) {
            gl::DeleteSync(m_object);
            m_object = { };
        }
    }
    void abandon() { m_object = { }; }
    void fenceSync()
    {
        reset();
        m_object = gl::FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }
    operator GLsync() const { return m_object; }
    operator bool() const { return m_object; }
private:
    GLsync m_object { };
};

}

#endif
