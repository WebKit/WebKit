/*
 * Copyright (C) 2019 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEBGL) && USE(ANGLE)
#include "ExtensionsGLANGLE.h"

#include "ANGLEHeaders.h"
#include "GraphicsContextGLOpenGL.h"

namespace WebCore {

ExtensionsGLANGLE::ExtensionsGLANGLE(GraphicsContextGLOpenGL* context)
    : m_initializedAvailableExtensions(false)
    , m_context(context)
{
}

ExtensionsGLANGLE::~ExtensionsGLANGLE() = default;

bool ExtensionsGLANGLE::supports(const String& name)
{
    if (!m_initializedAvailableExtensions) {
        if (!m_context->makeContextCurrent())
            return false;
        initializeAvailableExtensions();
    }
    return supportsExtension(name);
}

void ExtensionsGLANGLE::ensureEnabled(const String& name)
{
    // Enable support in ANGLE (if not enabled already).
    if (m_requestableExtensions.contains(name) && !m_enabledExtensions.contains(name)) {
        if (!m_context->makeContextCurrent())
            return;
        gl::RequestExtensionANGLE(name.ascii().data());
        m_enabledExtensions.add(name);

        if (name == "GL_CHROMIUM_color_buffer_float_rgba"_s)
            m_webglColorBufferFloatRGBA = true;
        else if (name == "GL_CHROMIUM_color_buffer_float_rgb"_s)
            m_webglColorBufferFloatRGB = true;
    }
}

bool ExtensionsGLANGLE::isEnabled(const String& name)
{
    return m_availableExtensions.contains(name) || m_enabledExtensions.contains(name);
}

GLint ExtensionsGLANGLE::getGraphicsResetStatusARB()
{
    return GraphicsContextGL::NO_ERROR;
}

String ExtensionsGLANGLE::getTranslatedShaderSourceANGLE(PlatformGLObject shader)
{
    if (!m_context->makeContextCurrent())
        return String();

    int sourceLength = m_context->getShaderi(shader, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE);

    if (!sourceLength)
        return emptyString();
    Vector<GLchar> name(sourceLength); // GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE includes null termination.
    GCGLint returnedLength = 0;
    gl::GetTranslatedShaderSourceANGLE(shader, sourceLength, &returnedLength, name.data());
    if (!returnedLength)
        return emptyString();
    // returnedLength does not include the null terminator.
    ASSERT(returnedLength == sourceLength - 1);
    return String(name.data(), returnedLength);
}

void ExtensionsGLANGLE::initializeAvailableExtensions()
{
    String extensionsString = getExtensions();
    for (auto& extension : extensionsString.split(' '))
        m_availableExtensions.add(extension);
    extensionsString = String(reinterpret_cast<const char*>(gl::GetString(GL_REQUESTABLE_EXTENSIONS_ANGLE)));
    for (auto& extension : extensionsString.split(' '))
        m_requestableExtensions.add(extension);
    m_initializedAvailableExtensions = true;
}

bool ExtensionsGLANGLE::supportsExtension(const String& name)
{
    return m_availableExtensions.contains(name) || m_requestableExtensions.contains(name);
}

void ExtensionsGLANGLE::drawBuffersEXT(GCGLSpan<const GCGLenum> bufs)
{
    if (!m_context->makeContextCurrent())
        return;

    gl::DrawBuffersEXT(bufs.bufSize, bufs.data);
}

String ExtensionsGLANGLE::getExtensions()
{
    return String(reinterpret_cast<const char*>(gl::GetString(GL_EXTENSIONS)));
}

GCGLenum ExtensionsGLANGLE::adjustWebGL1TextureInternalFormat(GCGLenum internalformat, GCGLenum format, GCGLenum type)
{
    // The implementation of WEBGL_color_buffer_float for WebGL 1.0 / ES 2.0 requires a sized
    // internal format. Adjust it if necessary at this lowest level.
    if (type == GL_FLOAT) {
        if (m_webglColorBufferFloatRGBA && format == GL_RGBA && internalformat == GL_RGBA)
            return GL_RGBA32F;
        if (m_webglColorBufferFloatRGB && format == GL_RGB && internalformat == GL_RGB)
            return GL_RGB32F;
    }
    return internalformat;
}

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(ANGLE)
