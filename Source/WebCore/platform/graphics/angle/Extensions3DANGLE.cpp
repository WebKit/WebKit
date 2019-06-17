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

#if ENABLE(GRAPHICS_CONTEXT_3D) && USE(ANGLE)
#include "Extensions3DANGLE.h"

#include "GraphicsContext3D.h"

#include <ANGLE/entry_points_gles_2_0_autogen.h>
#include <ANGLE/entry_points_gles_3_0_autogen.h>

// Note: this file can't be compiled in the same unified source file
// as others which include the system's OpenGL headers.

namespace WebCore {

Extensions3DANGLE::Extensions3DANGLE(GraphicsContext3D* context, bool useIndexedGetString)
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
    m_vendor = String(reinterpret_cast<const char*>(::glGetString(GL_VENDOR)));
    m_renderer = String(reinterpret_cast<const char*>(::glGetString(GL_RENDERER)));

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

Extensions3DANGLE::~Extensions3DANGLE() = default;

bool Extensions3DANGLE::supports(const String& name)
{
    if (!m_initializedAvailableExtensions)
        initializeAvailableExtensions();

    // We explicitly do not support this extension until
    // we fix the following bug:
    // https://bugs.webkit.org/show_bug.cgi?id=149734
    // FIXME: given that ANGLE is in use, rewrite this in terms of
    // ANGLE queries and enable this extension.
    if (name == "GL_ANGLE_translated_shader_source")
        return false;

    return supportsExtension(name);
}

void Extensions3DANGLE::ensureEnabled(const String& name)
{
    if (name == "GL_OES_standard_derivatives") {
        // Enable support in ANGLE (if not enabled already).
        ANGLEWebKitBridge& compiler = m_context->m_compiler;
        ShBuiltInResources ANGLEResources = compiler.getResources();
        if (!ANGLEResources.OES_standard_derivatives) {
            ANGLEResources.OES_standard_derivatives = 1;
            compiler.setResources(ANGLEResources);
        }
    } else if (name == "GL_EXT_draw_buffers") {
        // Enable support in ANGLE (if not enabled already).
        ANGLEWebKitBridge& compiler = m_context->m_compiler;
        ShBuiltInResources ANGLEResources = compiler.getResources();
        if (!ANGLEResources.EXT_draw_buffers) {
            ANGLEResources.EXT_draw_buffers = 1;
            m_context->getIntegerv(Extensions3D::MAX_DRAW_BUFFERS_EXT, &ANGLEResources.MaxDrawBuffers);
            compiler.setResources(ANGLEResources);
        }
    } else if (name == "GL_EXT_shader_texture_lod") {
        // Enable support in ANGLE (if not enabled already).
        ANGLEWebKitBridge& compiler = m_context->m_compiler;
        ShBuiltInResources ANGLEResources = compiler.getResources();
        if (!ANGLEResources.EXT_shader_texture_lod) {
            ANGLEResources.EXT_shader_texture_lod = 1;
            compiler.setResources(ANGLEResources);
        }
    } else if (name == "GL_EXT_frag_depth") {
        // Enable support in ANGLE (if not enabled already).
        ANGLEWebKitBridge& compiler = m_context->m_compiler;
        ShBuiltInResources ANGLEResources = compiler.getResources();
        if (!ANGLEResources.EXT_frag_depth) {
            ANGLEResources.EXT_frag_depth = 1;
            compiler.setResources(ANGLEResources);
        }
    }
}

bool Extensions3DANGLE::isEnabled(const String& name)
{
    if (name == "GL_OES_standard_derivatives") {
        ANGLEWebKitBridge& compiler = m_context->m_compiler;
        return compiler.getResources().OES_standard_derivatives;
    }
    return supports(name);
}

int Extensions3DANGLE::getGraphicsResetStatusARB()
{
    return GraphicsContext3D::NO_ERROR;
}

String Extensions3DANGLE::getTranslatedShaderSourceANGLE(Platform3DObject shader)
{
    // FIXME: port to use ANGLE's implementation directly.
    ASSERT(shader);
    int GLshaderType;
    ANGLEShaderType shaderType;

    ANGLEWebKitBridge& compiler = m_context->m_compiler;

    m_context->getShaderiv(shader, GraphicsContext3D::SHADER_TYPE, &GLshaderType);

    if (GLshaderType == GraphicsContext3D::VERTEX_SHADER)
        shaderType = SHADER_TYPE_VERTEX;
    else if (GLshaderType == GraphicsContext3D::FRAGMENT_SHADER)
        shaderType = SHADER_TYPE_FRAGMENT;
    else
        return emptyString(); // Invalid shader type.

    HashMap<Platform3DObject, GraphicsContext3D::ShaderSourceEntry>::iterator result = m_context->m_shaderSourceMap.find(shader);

    if (result == m_context->m_shaderSourceMap.end())
        return emptyString();

    GraphicsContext3D::ShaderSourceEntry& entry = result->value;

    String translatedShaderSource;
    String shaderInfoLog;
    uint64_t extraCompileOptions = SH_CLAMP_INDIRECT_ARRAY_BOUNDS | SH_UNFOLD_SHORT_CIRCUIT | SH_INIT_OUTPUT_VARIABLES | SH_ENFORCE_PACKING_RESTRICTIONS | SH_LIMIT_EXPRESSION_COMPLEXITY | SH_LIMIT_CALL_STACK_DEPTH | SH_INITIALIZE_UNINITIALIZED_LOCALS;

    if (m_requiresBuiltInFunctionEmulation)
        extraCompileOptions |= SH_EMULATE_ABS_INT_FUNCTION;

    Vector<std::pair<ANGLEShaderSymbolType, sh::ShaderVariable>> symbols;
    bool isValid = compiler.compileShaderSource(entry.source.utf8().data(), shaderType, translatedShaderSource, shaderInfoLog, symbols, extraCompileOptions);

    entry.log = shaderInfoLog;
    entry.isValid = isValid;

    for (const std::pair<ANGLEShaderSymbolType, sh::ShaderVariable>& pair : symbols) {
        const std::string& name = pair.second.name;
        entry.symbolMap(pair.first).set(String(name.c_str(), name.length()), pair.second);
    }

    if (!isValid)
        return emptyString();

    return translatedShaderSource;
}

void Extensions3DANGLE::initializeAvailableExtensions()
{
    if (m_useIndexedGetString) {
        GLint numExtensions = 0;
        gl::GetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
        for (GLint i = 0; i < numExtensions; ++i)
            m_availableExtensions.add(glGetStringi(GL_EXTENSIONS, i));
    } else {
        String extensionsString = getExtensions();
        for (auto& extension : extensionsString.split(' '))
            m_availableExtensions.add(extension);
    }
    m_initializedAvailableExtensions = true;
}

void Extensions3DANGLE::readnPixelsEXT(int, int, GC3Dsizei, GC3Dsizei, GC3Denum, GC3Denum, GC3Dsizei, void *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void Extensions3DANGLE::getnUniformfvEXT(GC3Duint, int, GC3Dsizei, float *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void Extensions3DANGLE::getnUniformivEXT(GC3Duint, int, GC3Dsizei, int *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void Extensions3DANGLE::blitFramebuffer(long srcX0, long srcY0, long srcX1, long srcY1, long dstX0, long dstY0, long dstX1, long dstY1, unsigned long mask, unsigned long filter)
{
    // FIXME: consider adding support for APPLE_framebuffer_multisample.
    gl::BlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void Extensions3DANGLE::renderbufferStorageMultisample(unsigned long target, unsigned long samples, unsigned long internalformat, unsigned long width, unsigned long height)
{
    gl::RenderbufferStorageMultisample(target, samples, internalformat, width, height);
}

Platform3DObject Extensions3DANGLE::createVertexArrayOES()
{
    m_context->makeContextCurrent();
    GLuint array = 0;
    gl::GenVertexArrays(1, &array);
    return array;
}

void Extensions3DANGLE::deleteVertexArrayOES(Platform3DObject array)
{
    if (!array)
        return;

    m_context->makeContextCurrent();
    gl::DeleteVertexArrays(1, &array);
}

GC3Dboolean Extensions3DANGLE::isVertexArrayOES(Platform3DObject array)
{
    if (!array)
        return GL_FALSE;

    m_context->makeContextCurrent();
    return gl::IsVertexArray(array);
}

void Extensions3DANGLE::bindVertexArrayOES(Platform3DObject array)
{
    m_context->makeContextCurrent();
    gl::BindVertexArray(array);
}

void Extensions3DANGLE::insertEventMarkerEXT(const String&)
{
    // FIXME: implement this function and add GL_EXT_debug_marker in supports().
    return;
}

void Extensions3DANGLE::pushGroupMarkerEXT(const String&)
{
    // FIXME: implement this function and add GL_EXT_debug_marker in supports().
    return;
}

void Extensions3DANGLE::popGroupMarkerEXT(void)
{
    // FIXME: implement this function and add GL_EXT_debug_marker in supports().
    return;
}

bool Extensions3DANGLE::supportsExtension(const String& name)
{
    return m_availableExtensions.contains(name);
}

void Extensions3DANGLE::drawBuffersEXT(GC3Dsizei n, const GC3Denum* bufs)
{
    gl::DrawBuffers(n, bufs);
}

void Extensions3DANGLE::drawArraysInstanced(GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei primcount)
{
    m_context->makeContextCurrent();
    gl::DrawArraysInstanced(mode, first, count, primcount);
}

void Extensions3DANGLE::drawElementsInstanced(GC3Denum mode, GC3Dsizei count, GC3Denum type, long long offset, GC3Dsizei primcount)
{
    m_context->makeContextCurrent();
    gl::DrawElementsInstanced(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)), primcount);
}

void Extensions3DANGLE::vertexAttribDivisor(GC3Duint index, GC3Duint divisor)
{
    m_context->makeContextCurrent();
    gl::VertexAttribDivisor(index, divisor);
}

String Extensions3DANGLE::getExtensions()
{
    ASSERT(!m_useIndexedGetString);
    return String(reinterpret_cast<const char*>(gl::GetString(GL_EXTENSIONS)));
}

} // namespace WebCore

#endif // ENABLE(GRAPHICS_CONTEXT_3D) && USE(ANGLE)
