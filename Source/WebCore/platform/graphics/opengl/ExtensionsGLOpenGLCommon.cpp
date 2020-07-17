/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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

#if ENABLE(GRAPHICS_CONTEXT_GL) && (USE(OPENGL) || USE(OPENGL_ES))
#include "ExtensionsGLOpenGLCommon.h"

#include "ANGLEWebKitBridge.h"
#include "GraphicsContextGLOpenGL.h"

#if PLATFORM(COCOA)

#if USE(OPENGL_ES)
#include <OpenGLES/ES2/glext.h>
#include <OpenGLES/ES3/gl.h>
#else
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#include <OpenGL/gl.h>
#include <OpenGL/gl3.h>
#undef GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#endif

#else

#if USE(LIBEPOXY)
#include "EpoxyShims.h"
#elif USE(OPENGL_ES)
#include "OpenGLESShims.h"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#elif PLATFORM(GTK) || PLATFORM(WIN)
#include "OpenGLShims.h"
#endif

#endif

#include <wtf/MainThread.h>
#include <wtf/Vector.h>

namespace WebCore {

ExtensionsGLOpenGLCommon::ExtensionsGLOpenGLCommon(GraphicsContextGLOpenGL* context, bool useIndexedGetString)
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

#if PLATFORM(MAC)
    if (m_isAMD || m_isIntel)
        m_requiresBuiltInFunctionEmulation = true;

    // Intel HD 3000 devices have problems with large textures. <rdar://problem/16649140>
    m_requiresRestrictedMaximumTextureSize = m_renderer.startsWith("Intel HD Graphics 3000");
#endif
}

ExtensionsGLOpenGLCommon::~ExtensionsGLOpenGLCommon() = default;

bool ExtensionsGLOpenGLCommon::supports(const String& name)
{
    if (!m_initializedAvailableExtensions)
        initializeAvailableExtensions();

    // We explicitly do not support this extension until
    // we fix the following bug:
    // https://bugs.webkit.org/show_bug.cgi?id=149734
    if (name == "GL_ANGLE_translated_shader_source")
        return false;

    return supportsExtension(name);
}

void ExtensionsGLOpenGLCommon::ensureEnabled(const String& name)
{
    if (name == "GL_OES_standard_derivatives") {
        // Enable support in ANGLE (if not enabled already)
        ANGLEWebKitBridge& compiler = m_context->m_compiler;
        ShBuiltInResources ANGLEResources = compiler.getResources();
        if (!ANGLEResources.OES_standard_derivatives) {
            ANGLEResources.OES_standard_derivatives = 1;
            compiler.setResources(ANGLEResources);
        }
    } else if (name == "GL_EXT_draw_buffers") {
        // Enable support in ANGLE (if not enabled already)
        ANGLEWebKitBridge& compiler = m_context->m_compiler;
        ShBuiltInResources ANGLEResources = compiler.getResources();
        if (!ANGLEResources.EXT_draw_buffers) {
            ANGLEResources.EXT_draw_buffers = 1;
            m_context->getIntegerv(ExtensionsGL::MAX_DRAW_BUFFERS_EXT, &ANGLEResources.MaxDrawBuffers);
            compiler.setResources(ANGLEResources);
        }
    } else if (name == "GL_EXT_shader_texture_lod") {
        // Enable support in ANGLE (if not enabled already)
        ANGLEWebKitBridge& compiler = m_context->m_compiler;
        ShBuiltInResources ANGLEResources = compiler.getResources();
        if (!ANGLEResources.EXT_shader_texture_lod) {
            ANGLEResources.EXT_shader_texture_lod = 1;
            compiler.setResources(ANGLEResources);
        }
    } else if (name == "GL_EXT_frag_depth") {
        // Enable support in ANGLE (if not enabled already)
        ANGLEWebKitBridge& compiler = m_context->m_compiler;
        ShBuiltInResources ANGLEResources = compiler.getResources();
        if (!ANGLEResources.EXT_frag_depth) {
            ANGLEResources.EXT_frag_depth = 1;
            compiler.setResources(ANGLEResources);
        }
    }
}

bool ExtensionsGLOpenGLCommon::isEnabled(const String& name)
{
    if (name == "GL_OES_standard_derivatives") {
        ANGLEWebKitBridge& compiler = m_context->m_compiler;
        return compiler.getResources().OES_standard_derivatives;
    }
    return supports(name);
}

int ExtensionsGLOpenGLCommon::getGraphicsResetStatusARB()
{
    return GraphicsContextGL::NO_ERROR;
}

String ExtensionsGLOpenGLCommon::getTranslatedShaderSourceANGLE(PlatformGLObject shader)
{
    ASSERT(shader);
    int GLshaderType;
    ANGLEShaderType shaderType;

    ANGLEWebKitBridge& compiler = m_context->m_compiler;

    m_context->getShaderiv(shader, GraphicsContextGL::SHADER_TYPE, &GLshaderType);

    if (GLshaderType == GraphicsContextGL::VERTEX_SHADER)
        shaderType = SHADER_TYPE_VERTEX;
    else if (GLshaderType == GraphicsContextGL::FRAGMENT_SHADER)
        shaderType = SHADER_TYPE_FRAGMENT;
    else
        return ""; // Invalid shader type.

    HashMap<PlatformGLObject, GraphicsContextGLOpenGL::ShaderSourceEntry>::iterator result = m_context->m_shaderSourceMap.find(shader);

    if (result == m_context->m_shaderSourceMap.end())
        return "";

    GraphicsContextGLOpenGL::ShaderSourceEntry& entry = result->value;

    String translatedShaderSource;
    String shaderInfoLog;
    GCGLuint64 extraCompileOptions = SH_CLAMP_INDIRECT_ARRAY_BOUNDS | SH_UNFOLD_SHORT_CIRCUIT | SH_INIT_OUTPUT_VARIABLES | SH_ENFORCE_PACKING_RESTRICTIONS | SH_LIMIT_EXPRESSION_COMPLEXITY | SH_LIMIT_CALL_STACK_DEPTH | SH_INITIALIZE_UNINITIALIZED_LOCALS;

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
        return "";

    return translatedShaderSource;
}

void ExtensionsGLOpenGLCommon::initializeAvailableExtensions()
{
#if (PLATFORM(COCOA) && USE(OPENGL)) || (PLATFORM(GTK) && !USE(OPENGL_ES))
    if (m_useIndexedGetString) {
        GLint numExtensions = 0;
        ::glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
        for (GLint i = 0; i < numExtensions; ++i)
            m_availableExtensions.add(glGetStringi(GL_EXTENSIONS, i));

        if (!m_availableExtensions.contains("GL_ARB_texture_storage"_s)) {
            GLint majorVersion;
            glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
            GLint minorVersion;
            glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
            if (majorVersion > 4 || (majorVersion == 4 && minorVersion >= 2))
                m_availableExtensions.add("GL_ARB_texture_storage"_s);
        }
    } else
#endif
    {
        String extensionsString = getExtensions();
        for (auto& extension : extensionsString.split(' '))
            m_availableExtensions.add(extension);
    }
    m_initializedAvailableExtensions = true;
}

void ExtensionsGLOpenGLCommon::readnPixelsEXT(int, int, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, GCGLsizei, void *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getnUniformfvEXT(GCGLuint, int, GCGLsizei, GCGLfloat *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getnUniformivEXT(GCGLuint, int, GCGLsizei, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getBooleanvRobustANGLE(GCGLenum, GCGLsizei, GCGLsizei *, GCGLboolean *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getBufferParameterivRobustANGLE(GCGLenum, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getFloatvRobustANGLE(GCGLenum, GCGLsizei, GCGLsizei *, GCGLfloat *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getFramebufferAttachmentParameterivRobustANGLE(GCGLenum, GCGLenum, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getIntegervRobustANGLE(GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getProgramivRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getRenderbufferParameterivRobustANGLE(GCGLenum, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getShaderivRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getTexParameterfvRobustANGLE(GCGLenum, GCGLenum, GCGLsizei, GCGLsizei *, GCGLfloat *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getTexParameterivRobustANGLE(GCGLenum, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getUniformfvRobustANGLE(GCGLuint, int, GCGLsizei, GCGLsizei *, GCGLfloat *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getUniformivRobustANGLE(GCGLuint, int, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getVertexAttribfvRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, GCGLfloat *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getVertexAttribivRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getVertexAttribPointervRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, void **)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::readPixelsRobustANGLE(int, int, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, GCGLsizei, GCGLsizei *, GCGLsizei *, GCGLsizei *, void *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::texImage2DRobustANGLE(GCGLenum, int, int, GCGLsizei, GCGLsizei, int, GCGLenum, GCGLenum, GCGLsizei, const void *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::texParameterfvRobustANGLE(GCGLenum, GCGLenum, GCGLsizei, const GCGLfloat *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::texParameterivRobustANGLE(GCGLenum, GCGLenum, GCGLsizei, const GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::texSubImage2DRobustANGLE(GCGLenum, int, int, int, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, GCGLsizei, const void *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::compressedTexImage2DRobustANGLE(GCGLenum, int, GCGLenum, GCGLsizei, GCGLsizei, int, GCGLsizei, GCGLsizei, const void*)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::compressedTexSubImage2DRobustANGLE(GCGLenum, int, int, int, GCGLsizei, GCGLsizei, GCGLenum, GCGLsizei, GCGLsizei, const void*)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::compressedTexImage3DRobustANGLE(GCGLenum, int, GCGLenum, GCGLsizei, GCGLsizei, GCGLsizei, int, GCGLsizei, GCGLsizei, const void*)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::compressedTexSubImage3DRobustANGLE(GCGLenum, int, int, int, int, GCGLsizei, GCGLsizei, GCGLsizei, GCGLenum, GCGLsizei, GCGLsizei, const void*)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}


void ExtensionsGLOpenGLCommon::texImage3DRobustANGLE(GCGLenum, int, int, GCGLsizei, GCGLsizei, GCGLsizei, int, GCGLenum, GCGLenum, GCGLsizei, const void *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::texSubImage3DRobustANGLE(GCGLenum, int, int, int, int, GCGLsizei, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, GCGLsizei, const void *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getQueryivRobustANGLE(GCGLenum, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getQueryObjectuivRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, GCGLuint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getBufferPointervRobustANGLE(GCGLenum, GCGLenum, GCGLsizei, GCGLsizei *, void **)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getIntegeri_vRobustANGLE(GCGLenum, GCGLuint, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getInternalformativRobustANGLE(GCGLenum, GCGLenum, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getVertexAttribIivRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getVertexAttribIuivRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, GCGLuint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getUniformuivRobustANGLE(GCGLuint, int, GCGLsizei, GCGLsizei *, GCGLuint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getActiveUniformBlockivRobustANGLE(GCGLuint, GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getInteger64vRobustANGLE(GCGLenum, GCGLsizei, GCGLsizei *, GCGLint64 *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getInteger64i_vRobustANGLE(GCGLenum, GCGLuint, GCGLsizei, GCGLsizei *, GCGLint64 *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getBufferParameteri64vRobustANGLE(GCGLenum, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint64 *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::samplerParameterivRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, const GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::samplerParameterfvRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, const GCGLfloat *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getSamplerParameterivRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getSamplerParameterfvRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, GCGLfloat *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}


void ExtensionsGLOpenGLCommon::getFramebufferParameterivRobustANGLE(GCGLenum, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getProgramInterfaceivRobustANGLE(GCGLuint, GCGLenum, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getBooleani_vRobustANGLE(GCGLenum, GCGLuint, GCGLsizei, GCGLsizei *, GCGLboolean *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getMultisamplefvRobustANGLE(GCGLenum, GCGLuint, GCGLsizei, GCGLsizei *, GCGLfloat *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getTexLevelParameterivRobustANGLE(GCGLenum, int, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getTexLevelParameterfvRobustANGLE(GCGLenum, int, GCGLenum, GCGLsizei, GCGLsizei *, GCGLfloat *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}


void ExtensionsGLOpenGLCommon::getPointervRobustANGLERobustANGLE(GCGLenum, GCGLsizei, GCGLsizei *, void **)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::readnPixelsRobustANGLE(int, int, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, GCGLsizei, GCGLsizei *, GCGLsizei *, GCGLsizei *, void *, bool)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getnUniformfvRobustANGLE(GCGLuint, int, GCGLsizei, GCGLsizei *, GCGLfloat *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getnUniformivRobustANGLE(GCGLuint, int, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getnUniformuivRobustANGLE(GCGLuint, int, GCGLsizei, GCGLsizei *, GCGLuint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::texParameterIivRobustANGLE(GCGLenum, GCGLenum, GCGLsizei, const GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::texParameterIuivRobustANGLE(GCGLenum, GCGLenum, GCGLsizei, const GCGLuint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getTexParameterIivRobustANGLE(GCGLenum, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getTexParameterIuivRobustANGLE(GCGLenum, GCGLenum, GCGLsizei, GCGLsizei *, GCGLuint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::samplerParameterIivRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, const GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::samplerParameterIuivRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, const GCGLuint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getSamplerParameterIivRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getSamplerParameterIuivRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, GCGLuint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}


void ExtensionsGLOpenGLCommon::getQueryObjectivRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getQueryObjecti64vRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, GCGLint64 *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}

void ExtensionsGLOpenGLCommon::getQueryObjectui64vRobustANGLE(GCGLuint, GCGLenum, GCGLsizei, GCGLsizei *, GCGLuint64 *)
{
    m_context->synthesizeGLError(GL_INVALID_OPERATION);
}


} // namespace WebCore

#endif // ENABLE(GRAPHICS_CONTEXT_GL) && (USE(OPENGL) || USE(OPENGL_ES))
