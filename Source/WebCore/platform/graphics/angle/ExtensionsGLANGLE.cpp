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

#if ENABLE(GRAPHICS_CONTEXT_GL) && USE(ANGLE)
#include "ExtensionsGLANGLE.h"

#include "GraphicsContextGLOpenGL.h"

#include <ANGLE/entry_points_gles_2_0_autogen.h>
#include <ANGLE/entry_points_gles_3_0_autogen.h>
// Skip the inclusion of ANGLE's explicit context entry points for now.
#define GL_ANGLE_explicit_context
#define GL_ANGLE_explicit_context_gles1
typedef void* GLeglContext;
#include <ANGLE/entry_points_gles_ext_autogen.h>

// Note: this file can't be compiled in the same unified source file
// as others which include the system's OpenGL headers.

namespace WebCore {

ExtensionsGLANGLE::ExtensionsGLANGLE(GraphicsContextGLOpenGL* context, bool useIndexedGetString)
    : m_initializedAvailableExtensions(false)
    , m_context(context)
    , m_isNVIDIA(false)
    , m_isAMD(false)
    , m_isIntel(false)
    , m_isImagination(false)
    , m_requiresBuiltInFunctionEmulation(false)
    , m_requiresRestrictedMaximumTextureSize(false)
    , m_useIndexedGetString(useIndexedGetString)
{
    // FIXME: ideally, remove this initialization altogether. ANGLE
    // subsumes the responsibility for graphics driver workarounds.
    m_vendor = String(reinterpret_cast<const char*>(gl::GetString(GL_VENDOR)));
    m_renderer = String(reinterpret_cast<const char*>(gl::GetString(GL_RENDERER)));

    Vector<String> vendorComponents = m_vendor.convertToASCIILowercase().split(' ');
    if (vendorComponents.contains("nvidia"))
        m_isNVIDIA = true;
    if (vendorComponents.contains("ati") || vendorComponents.contains("amd"))
        m_isAMD = true;
    if (vendorComponents.contains("intel"))
        m_isIntel = true;
    if (vendorComponents.contains("imagination"))
        m_isImagination = true;
}

ExtensionsGLANGLE::~ExtensionsGLANGLE() = default;

bool ExtensionsGLANGLE::supports(const String& name)
{
    if (!m_initializedAvailableExtensions)
        initializeAvailableExtensions();

    return supportsExtension(name);
}

void ExtensionsGLANGLE::ensureEnabled(const String& name)
{
    // Enable support in ANGLE (if not enabled already).
    if (m_requestableExtensions.contains(name) && !m_enabledExtensions.contains(name)) {
        m_context->makeContextCurrent();
        gl::RequestExtensionANGLE(name.ascii().data());
        m_enabledExtensions.add(name);
    }
}

bool ExtensionsGLANGLE::isEnabled(const String& name)
{
    return m_availableExtensions.contains(name) || m_enabledExtensions.contains(name);
}

int ExtensionsGLANGLE::getGraphicsResetStatusARB()
{
    return GraphicsContextGL::NO_ERROR;
}

String ExtensionsGLANGLE::getTranslatedShaderSourceANGLE(PlatformGLObject shader)
{
    int sourceLength = 0;
    m_context->getShaderiv(shader, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE, &sourceLength);
    if (!sourceLength)
        return emptyString();
    Vector<GLchar> name(sourceLength); // GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE includes null termination.
    int returnedLength = 0;
    gl::GetTranslatedShaderSourceANGLE(shader, sourceLength, &returnedLength, name.data());
    if (!returnedLength)
        return emptyString();
    // returnedLength does not include the null terminator.
    ASSERT(returnedLength == sourceLength - 1);
    return String(name.data(), returnedLength);
}

void ExtensionsGLANGLE::initializeAvailableExtensions()
{
    if (m_useIndexedGetString) {
        GLint numExtensions = 0;
        gl::GetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
        for (GLint i = 0; i < numExtensions; ++i)
            m_availableExtensions.add(gl::GetStringi(GL_EXTENSIONS, i));
        gl::GetIntegerv(GL_NUM_REQUESTABLE_EXTENSIONS_ANGLE, &numExtensions);
        for (GLint i = 0; i < numExtensions; ++i)
            m_requestableExtensions.add(gl::GetStringi(GL_REQUESTABLE_EXTENSIONS_ANGLE, i));
    } else {
        String extensionsString = getExtensions();
        for (auto& extension : extensionsString.split(' '))
            m_availableExtensions.add(extension);
        extensionsString = String(reinterpret_cast<const char*>(gl::GetString(GL_REQUESTABLE_EXTENSIONS_ANGLE)));
        for (auto& extension : extensionsString.split(' '))
            m_requestableExtensions.add(extension);
    }
    m_initializedAvailableExtensions = true;
}

void ExtensionsGLANGLE::readnPixelsEXT(int, int, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, GCGLsizei, void *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLANGLE::getnUniformfvEXT(GCGLuint, int, GCGLsizei, float *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLANGLE::getnUniformivEXT(GCGLuint, int, GCGLsizei, int *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLANGLE::blitFramebuffer(long srcX0, long srcY0, long srcX1, long srcY1, long dstX0, long dstY0, long dstX1, long dstY1, unsigned long mask, unsigned long filter)
{
    // FIXME: consider adding support for APPLE_framebuffer_multisample.
    gl::BlitFramebufferANGLE(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void ExtensionsGLANGLE::renderbufferStorageMultisample(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height)
{
    gl::RenderbufferStorageMultisampleANGLE(target, samples, internalformat, width, height);
}

PlatformGLObject ExtensionsGLANGLE::createVertexArrayOES()
{
    m_context->makeContextCurrent();
    GLuint array = 0;
    gl::GenVertexArraysOES(1, &array);
    return array;
}

void ExtensionsGLANGLE::deleteVertexArrayOES(PlatformGLObject array)
{
    if (!array)
        return;

    m_context->makeContextCurrent();
    gl::DeleteVertexArraysOES(1, &array);
}

GCGLboolean ExtensionsGLANGLE::isVertexArrayOES(PlatformGLObject array)
{
    if (!array)
        return GL_FALSE;

    m_context->makeContextCurrent();
    return gl::IsVertexArrayOES(array);
}

void ExtensionsGLANGLE::bindVertexArrayOES(PlatformGLObject array)
{
    m_context->makeContextCurrent();
    gl::BindVertexArrayOES(array);
}

void ExtensionsGLANGLE::insertEventMarkerEXT(const String&)
{
    // FIXME: implement this function and add GL_EXT_debug_marker in supports().
    return;
}

void ExtensionsGLANGLE::pushGroupMarkerEXT(const String&)
{
    // FIXME: implement this function and add GL_EXT_debug_marker in supports().
    return;
}

void ExtensionsGLANGLE::popGroupMarkerEXT(void)
{
    // FIXME: implement this function and add GL_EXT_debug_marker in supports().
    return;
}

bool ExtensionsGLANGLE::supportsExtension(const String& name)
{
    return m_availableExtensions.contains(name) || m_requestableExtensions.contains(name);
}

void ExtensionsGLANGLE::drawBuffersEXT(GCGLsizei n, const GCGLenum* bufs)
{
    gl::DrawBuffersEXT(n, bufs);
}

void ExtensionsGLANGLE::drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount)
{
    m_context->makeContextCurrent();
    gl::DrawArraysInstancedANGLE(mode, first, count, primcount);
}

void ExtensionsGLANGLE::drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, long long offset, GCGLsizei primcount)
{
    m_context->makeContextCurrent();
    gl::DrawElementsInstancedANGLE(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)), primcount);
}

void ExtensionsGLANGLE::vertexAttribDivisor(GCGLuint index, GCGLuint divisor)
{
    m_context->makeContextCurrent();
    gl::VertexAttribDivisorANGLE(index, divisor);
}

String ExtensionsGLANGLE::getExtensions()
{
    return String(reinterpret_cast<const char*>(gl::GetString(GL_EXTENSIONS)));
}

} // namespace WebCore

#endif // ENABLE(GRAPHICS_CONTEXT_GL) && USE(ANGLE)
