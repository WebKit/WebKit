/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "ANGLEUtilities.h"

#if ENABLE(WEBGL)
#include "ANGLEHeaders.h"

namespace WebCore {

#if !PLATFORM(COCOA)
bool platformIsANGLEAvailable()
{
    return true;
}
#endif

ScopedRestoreTextureBinding::ScopedRestoreTextureBinding(GCGLenum bindingPointQuery, GCGLenum bindingPoint, bool condition)
{
    ASSERT(bindingPoint != static_cast<GCGLenum>(0u));
    if (condition) {
        m_bindingPoint = bindingPoint;
        GL_GetIntegerv(bindingPointQuery, reinterpret_cast<GCGLint*>(&m_bindingValue));
    }
}

ScopedRestoreTextureBinding::~ScopedRestoreTextureBinding()
{
    if (m_bindingPoint)
        GL_BindTexture(m_bindingPoint, m_bindingValue);
}

ScopedBufferBinding::ScopedBufferBinding(GCGLenum bindingPoint, GCGLuint bindingValue, bool condition)
{
    if (!condition)
        return;
    GL_GetIntegerv(query(bindingPoint), reinterpret_cast<GCGLint*>(&m_bindingValue));
    if (bindingValue == m_bindingValue)
        return;
    m_bindingPoint = bindingPoint;
    GL_BindBuffer(m_bindingPoint, bindingValue);
}

ScopedBufferBinding::~ScopedBufferBinding()
{
    if (m_bindingPoint)
        GL_BindBuffer(m_bindingPoint, m_bindingValue);
}

void ScopedRestoreReadFramebufferBinding::bindFramebuffer(GCGLuint bindingValue)
{
    if (!m_bindingChanged && m_bindingValue == bindingValue)
        return;
    GL_BindFramebuffer(m_framebufferTarget, bindingValue);
    m_bindingChanged = m_bindingValue != bindingValue;
}

ScopedRestoreReadFramebufferBinding::~ScopedRestoreReadFramebufferBinding()
{
    if (m_bindingChanged)
        GL_BindFramebuffer(m_framebufferTarget, m_bindingValue);
}

ScopedPixelStorageMode::ScopedPixelStorageMode(GCGLenum name, bool condition)
    : m_name(condition ? name : 0)
{
    if (m_name)
        GL_GetIntegerv(m_name, &m_originalValue);
}

ScopedPixelStorageMode::ScopedPixelStorageMode(GCGLenum name, GCGLint value, bool condition)
    : m_name(condition ? name : 0)
{
    if (m_name) {
        GL_GetIntegerv(m_name, &m_originalValue);
        pixelStore(value);
    }
}

ScopedPixelStorageMode::~ScopedPixelStorageMode()
{
    if (m_name && m_valueChanged)
        GL_PixelStorei(m_name, m_originalValue);
}

void ScopedPixelStorageMode::pixelStore(GCGLint value)
{
    ASSERT(m_name);
    if (!m_valueChanged && m_originalValue == value)
        return;
    GL_PixelStorei(m_name, value);
    m_valueChanged = m_originalValue != value;
}

ScopedTexture::ScopedTexture()
{
    GL_GenTextures(1, &m_object);
}

ScopedTexture::~ScopedTexture()
{
    GL_DeleteTextures(1, &m_object);
}

ScopedFramebuffer::ScopedFramebuffer()
{
    GL_GenFramebuffers(1, &m_object);
}

ScopedFramebuffer::~ScopedFramebuffer()
{
    GL_DeleteFramebuffers(1, &m_object);
}

void ScopedGLFence::reset()
{
    if (m_object) {
        GL_DeleteSync(static_cast<GLsync>(m_object));
        m_object = { };
    }
}

void ScopedGLFence::fenceSync()
{
    reset();
    m_object = GL_FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}


ScopedGLCapability::ScopedGLCapability(GCGLenum capability, bool enable)
    : m_capability(capability)
    , m_original(GL_IsEnabled(m_capability) == enable ? std::nullopt : std::optional<bool>(!enable))
{
    if (!m_original)
        return;
    if (enable)
        GL_Enable(m_capability);
    else
        GL_Disable(m_capability);
}

ScopedGLCapability::~ScopedGLCapability()
{
    if (!m_original)
        return;
    if (*m_original)
        GL_Enable(m_capability);
    else
        GL_Disable(m_capability);
}

}

#endif
