/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 ChangSeok Oh <shivamidow@gmail.com>
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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
#include "GraphicsContextGLOpenGL.h"

#if ENABLE(WEBGL) && !USE(ANGLE)

#include "ANGLEWebKitBridge.h"
#include "GLContext.h"
#include "GraphicsContext.h"
#include "GraphicsContextGLOpenGLManager.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Logging.h"
#include "PixelBuffer.h"
#include "TemporaryOpenGLSetting.h"
#include "WebGLRenderingContextBase.h"
#include <JavaScriptCore/RegularExpression.h>
#include <cstring>
#include <wtf/HexNumber.h>
#include <wtf/MainThread.h>
#include <wtf/ThreadSpecific.h>
#include <wtf/UniqueArray.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#if USE(OPENGL_ES)
#include "ExtensionsGLOpenGLES.h"
#else
#include "ExtensionsGLOpenGL.h"
#endif

#if USE(LIBEPOXY)
#include "EpoxyShims.h"
#elif USE(OPENGL_ES)
#include "OpenGLESShims.h"
#elif PLATFORM(GTK) || PLATFORM(WIN)
#include "OpenGLShims.h"
#endif

#if USE(NICOSIA)
#include "NicosiaGCGLLayer.h"
#endif

namespace WebCore {

static ThreadSpecific<ShaderNameHash*>& getCurrentNameHashMapForShader()
{
    static std::once_flag onceFlag;
    static ThreadSpecific<ShaderNameHash*>* sharedNameHash;
    std::call_once(onceFlag, [] {
        sharedNameHash = new ThreadSpecific<ShaderNameHash*>;
    });

    return *sharedNameHash;
}

static void setCurrentNameHashMapForShader(ShaderNameHash* shaderNameHash)
{
    *getCurrentNameHashMapForShader() = shaderNameHash;
}

// Hash function used by the ANGLE translator/compiler to do
// symbol name mangling. Since this is a static method, before
// calling compileShader we set currentNameHashMapForShader
// to point to the map kept by the current instance of GraphicsContextGLOpenGL.

static uint64_t nameHashForShader(const char* name, size_t length)
{
    if (!length)
        return 0;

    CString nameAsCString = CString(name);

    // Look up name in our local map.
    ShaderNameHash*& currentNameHashMapForShader = *getCurrentNameHashMapForShader();
    ShaderNameHash::iterator findResult = currentNameHashMapForShader->find(nameAsCString);
    if (findResult != currentNameHashMapForShader->end())
        return findResult->value;

    unsigned hashValue = nameAsCString.hash();

    // Convert the 32-bit hash from CString::hash into a 64-bit result
    // by shifting then adding the size of our table. Overflow would
    // only be a problem if we're already hashing to the same value (and
    // we're hoping that over the lifetime of the context we
    // don't have that many symbols).

    uint64_t result = hashValue;
    result = (result << 32) + (currentNameHashMapForShader->size() + 1);

    currentNameHashMapForShader->set(nameAsCString, result);
    return result;
}

GraphicsContextGLOpenGL::GraphicsContextGLOpenGL(GraphicsContextGLAttributes attributes)
    : GraphicsContextGL(attributes)
{
#if USE(NICOSIA)
    m_nicosiaLayer = makeUnique<Nicosia::GCGLLayer>(*this);
#else
    m_texmapLayer = makeUnique<TextureMapperGCGLPlatformLayer>(*this);
#endif

    bool success = makeContextCurrent();
    ASSERT_UNUSED(success, success);

    validateAttributes();
    attributes = contextAttributes(); // They may have changed during validation.

    // Create a texture to render into.
    ::glGenTextures(1, &m_texture);
    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    ::glBindTexture(GL_TEXTURE_2D, 0);

    // Create an FBO.
    ::glGenFramebuffers(1, &m_fbo);
    ::glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

#if USE(COORDINATED_GRAPHICS)
    ::glGenTextures(1, &m_compositorTexture);
    ::glBindTexture(GL_TEXTURE_2D, m_compositorTexture);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    ::glGenTextures(1, &m_intermediateTexture);
    ::glBindTexture(GL_TEXTURE_2D, m_intermediateTexture);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    ::glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    ::glBindTexture(GL_TEXTURE_2D, 0);
#endif

    // Create a multisample FBO.
    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (attributes.antialias) {
        ::glGenFramebuffers(1, &m_multisampleFBO);
        ::glBindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_multisampleFBO;
        ::glGenRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            ::glGenRenderbuffers(1, &m_multisampleDepthStencilBuffer);
    } else {
        // Bind canvas FBO.
        glBindFramebuffer(GraphicsContextGLOpenGL::FRAMEBUFFER, m_fbo);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_fbo;
#if USE(OPENGL_ES)
        if (attributes.depth)
            glGenRenderbuffers(1, &m_depthBuffer);
        if (attributes.stencil)
            glGenRenderbuffers(1, &m_stencilBuffer);
#endif
        if (attributes.stencil || attributes.depth)
            glGenRenderbuffers(1, &m_depthStencilBuffer);
    }

#if !USE(OPENGL_ES)
    ::glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    if (GLContext::current()->version() >= 320) {
        m_usingCoreProfile = true;

        // From version 3.2 on we use the OpenGL Core profile, so request that ouput to the shader compiler.
        // OpenGL version 3.2 uses GLSL version 1.50.
        m_compiler = ANGLEWebKitBridge(SH_GLSL_150_CORE_OUTPUT);

        // From version 3.2 on we use the OpenGL Core profile, and we need a VAO for rendering.
        // A VAO could be created and bound by each component using GL rendering (TextureMapper, WebGL, etc). This is
        // a simpler solution: the first GraphicsContextGLOpenGL created on a GLContext will create and bind a VAO for that context.
        GCGLint currentVAO = getInteger(GraphicsContextGLOpenGL::VERTEX_ARRAY_BINDING);
        if (!currentVAO) {
            m_vao = createVertexArray();
            bindVertexArray(m_vao);
        }
    } else {
        // For lower versions request the compatibility output to the shader compiler.
        m_compiler = ANGLEWebKitBridge(SH_GLSL_COMPATIBILITY_OUTPUT);

        // GL_POINT_SPRITE is needed in lower versions.
        ::glEnable(GL_POINT_SPRITE);
    }
#else
    // Adjust the shader specification depending on whether GLES3 (i.e. WebGL2 support) was requested.
#if ENABLE(WEBGL2)
    m_compiler = ANGLEWebKitBridge(SH_ESSL_OUTPUT, (attributes.webGLVersion == GraphicsContextGLWebGLVersion::WebGL2) ? SH_WEBGL2_SPEC : SH_WEBGL_SPEC);
#else
    m_compiler = ANGLEWebKitBridge(SH_ESSL_OUTPUT, SH_WEBGL_SPEC);
#endif
#endif

    // ANGLE initialization.
    ShBuiltInResources ANGLEResources;
    sh::InitBuiltInResources(&ANGLEResources);

    ANGLEResources.MaxVertexAttribs = getInteger(GraphicsContextGLOpenGL::MAX_VERTEX_ATTRIBS);
    ANGLEResources.MaxVertexUniformVectors = getInteger(GraphicsContextGLOpenGL::MAX_VERTEX_UNIFORM_VECTORS);
    ANGLEResources.MaxVaryingVectors = getInteger(GraphicsContextGLOpenGL::MAX_VARYING_VECTORS);
    ANGLEResources.MaxVertexTextureImageUnits = getInteger(GraphicsContextGLOpenGL::MAX_VERTEX_TEXTURE_IMAGE_UNITS);
    ANGLEResources.MaxCombinedTextureImageUnits = getInteger(GraphicsContextGLOpenGL::MAX_COMBINED_TEXTURE_IMAGE_UNITS);
    ANGLEResources.MaxTextureImageUnits = getInteger(GraphicsContextGLOpenGL::MAX_TEXTURE_IMAGE_UNITS);
    ANGLEResources.MaxFragmentUniformVectors = getInteger(GraphicsContextGLOpenGL::MAX_FRAGMENT_UNIFORM_VECTORS);

    // Always set to 1 for OpenGL ES.
    ANGLEResources.MaxDrawBuffers = 1;

    GCGLint range[2] { };
    GCGLint precision = 0;
    getShaderPrecisionFormat(GraphicsContextGLOpenGL::FRAGMENT_SHADER, GraphicsContextGLOpenGL::HIGH_FLOAT, range, &precision);
    ANGLEResources.FragmentPrecisionHigh = (range[0] || range[1] || precision);

    m_compiler.setResources(ANGLEResources);

    ::glClearColor(0, 0, 0, 0);
}

GraphicsContextGLOpenGL::~GraphicsContextGLOpenGL()
{
    GraphicsContextGLOpenGLManager::sharedManager().removeContext(this);
    bool success = makeContextCurrent();
    ASSERT_UNUSED(success, success);
    if (m_texture)
        ::glDeleteTextures(1, &m_texture);
#if USE(COORDINATED_GRAPHICS)
    if (m_compositorTexture)
        ::glDeleteTextures(1, &m_compositorTexture);
#endif

    auto attributes = contextAttributes();

    if (attributes.antialias) {
        ::glDeleteRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            ::glDeleteRenderbuffers(1, &m_multisampleDepthStencilBuffer);
        ::glDeleteFramebuffers(1, &m_multisampleFBO);
    } else if (attributes.stencil || attributes.depth) {
#if USE(OPENGL_ES)
        if (m_depthBuffer)
            glDeleteRenderbuffers(1, &m_depthBuffer);

        if (m_stencilBuffer)
            glDeleteRenderbuffers(1, &m_stencilBuffer);
#endif
        if (m_depthStencilBuffer)
            ::glDeleteRenderbuffers(1, &m_depthStencilBuffer);
    }
    ::glDeleteFramebuffers(1, &m_fbo);
#if USE(COORDINATED_GRAPHICS)
    ::glDeleteTextures(1, &m_intermediateTexture);
#endif

#if USE(CAIRO)
    if (m_vao)
        deleteVertexArray(m_vao);
#endif
}

bool GraphicsContextGLOpenGL::makeContextCurrent()
{
#if USE(NICOSIA)
    return m_nicosiaLayer->makeContextCurrent();
#else
    return m_texmapLayer->makeContextCurrent();
#endif
}

void GraphicsContextGLOpenGL::checkGPUStatus()
{
}

bool GraphicsContextGLOpenGL::isGLES2Compliant() const
{
#if USE(OPENGL_ES)
    return true;
#else
    return false;
#endif
}

#if PLATFORM(GTK) && USE(OPENGL_ES)
ExtensionsGLOpenGLES& GraphicsContextGLOpenGL::getExtensions()
{
    // glGetStringi is not available on GLES2.
    if (!m_extensions)
        m_extensions = makeUnique<ExtensionsGLOpenGLES>(this,  false);
    return *m_extensions;
}
#elif PLATFORM(GTK)
ExtensionsGLOpenGL& GraphicsContextGLOpenGL::getExtensions()
{
    // From OpenGL 3.2 on we use the Core profile, and there we must use glGetStringi.
    if (!m_extensions)
        m_extensions = makeUnique<ExtensionsGLOpenGL>(this, GLContext::current()->version() >= 320);
    return *m_extensions;
}
#endif

void GraphicsContextGLOpenGL::setContextVisibility(bool)
{
}

void GraphicsContextGLOpenGL::simulateEventForTesting(SimulatedEventForTesting event)
{
    if (event == SimulatedEventForTesting::GPUStatusFailure)
        m_failNextStatusCheck = true;
}

void GraphicsContextGLOpenGL::prepareForDisplay()
{
}

std::optional<PixelBuffer> GraphicsContextGLOpenGL::readCompositedResults()
{
    return readRenderingResults();
}

void GraphicsContextGLOpenGL::validateDepthStencil(const char* packedDepthStencilExtension)
{
    auto attrs = contextAttributes();

    if (attrs.stencil) {
        if (supportsExtension(packedDepthStencilExtension)) {
            ensureExtensionEnabled(packedDepthStencilExtension);
            // Force depth if stencil is true.
            attrs.depth = true;
        } else
            attrs.stencil = false;
        setContextAttributes(attrs);
    }
    if (attrs.antialias && !m_isForWebGL2) {
        if (!supportsExtension("GL_ANGLE_framebuffer_multisample")) {
            attrs.antialias = false;
            setContextAttributes(attrs);
        } else
            ensureExtensionEnabled("GL_ANGLE_framebuffer_multisample");
    }
}

void GraphicsContextGLOpenGL::prepareTexture()
{
    if (m_layerComposited)
        return;

    if (!makeContextCurrent())
        return;


#if !USE(COORDINATED_GRAPHICS)
    TemporaryOpenGLSetting scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
    TemporaryOpenGLSetting scopedDither(GL_DITHER, GL_FALSE);
#endif

    if (contextAttributes().antialias)
        resolveMultisamplingIfNecessary();

#if USE(COORDINATED_GRAPHICS)
    std::swap(m_texture, m_compositorTexture);
    std::swap(m_texture, m_intermediateTexture);
    ::glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    ::glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_texture, 0);
    glFlush();

    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (m_state.boundDrawFBO != m_fbo)
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_state.boundDrawFBO);
    else
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_fbo);
    return;
#endif

    ::glActiveTexture(GL_TEXTURE0);
    ::glBindTexture(GL_TEXTURE_2D, m_state.boundTarget(GL_TEXTURE0) == GL_TEXTURE_2D ? m_state.boundTexture(GL_TEXTURE0) : 0);
    ::glActiveTexture(m_state.activeTextureUnit);
    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (m_state.boundDrawFBO != m_fbo)
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_state.boundDrawFBO);
    ::glFlush();
}

std::optional<PixelBuffer> GraphicsContextGLOpenGL::readRenderingResults()
{
    bool mustRestoreFBO = false;
    if (contextAttributes().antialias) {
        resolveMultisamplingIfNecessary();
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_fbo);
        mustRestoreFBO = true;
    } else {
        ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
        if (m_state.boundDrawFBO != m_fbo) {
            mustRestoreFBO = true;
            ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_fbo);
        }
    }
    auto result = readPixelsForPaintResults();
    if (mustRestoreFBO)
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_state.boundDrawFBO);
    return result;
}

void GraphicsContextGLOpenGL::reshape(int width, int height)
{
    if (width == m_currentWidth && height == m_currentHeight)
        return;

    ASSERT(width >= 0 && height >= 0);
    if (width < 0 || height < 0)
        return;

    if (!makeContextCurrent())
        return;

    markContextChanged();

    m_currentWidth = width;
    m_currentHeight = height;

    validateAttributes();

    TemporaryOpenGLSetting scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
    TemporaryOpenGLSetting scopedDither(GL_DITHER, GL_FALSE);
    
    bool mustRestoreFBO = reshapeFBOs(IntSize(width, height));

    // Initialize renderbuffers to 0.
    GLfloat clearColor[] = { 0, 0, 0, 0 }, clearDepth = 0;
    GLint clearStencil = 0;
    GLboolean colorMask[] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE }, depthMask = GL_TRUE;
    GLuint stencilMask = 0xffffffff, stencilMaskBack = 0xffffffff;
    GLbitfield clearMask = GL_COLOR_BUFFER_BIT;
    ::glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
    ::glClearColor(0, 0, 0, 0);
    ::glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    ::glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    auto attrs = contextAttributes();

    if (attrs.depth) {
        ::glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
        GraphicsContextGLOpenGL::clearDepth(1);
        ::glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        ::glDepthMask(GL_TRUE);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }
    if (attrs.stencil) {
        ::glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &clearStencil);
        ::glClearStencil(0);
        ::glGetIntegerv(GL_STENCIL_WRITEMASK, reinterpret_cast<GLint*>(&stencilMask));
        ::glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, reinterpret_cast<GLint*>(&stencilMaskBack));
        ::glStencilMaskSeparate(GL_FRONT, 0xffffffff);
        ::glStencilMaskSeparate(GL_BACK, 0xffffffff);
        clearMask |= GL_STENCIL_BUFFER_BIT;
    }

    ::glClear(clearMask);

    ::glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    ::glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    if (attrs.depth) {
        GraphicsContextGLOpenGL::clearDepth(clearDepth);
        ::glDepthMask(depthMask);
    }
    if (attrs.stencil) {
        ::glClearStencil(clearStencil);
        ::glStencilMaskSeparate(GL_FRONT, stencilMask);
        ::glStencilMaskSeparate(GL_BACK, stencilMaskBack);
    }

    if (mustRestoreFBO)
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_state.boundDrawFBO);

    auto error = ::glGetError();
    if (error != GL_NO_ERROR) {
        RELEASE_LOG(WebGL, "Fatal: OpenGL error during GraphicsContextGL buffer initialization (%d).", error);
        forceContextLost();
        return;
    }

    ::glFlush();
}

bool GraphicsContextGLOpenGL::checkVaryingsPacking(PlatformGLObject vertexShader, PlatformGLObject fragmentShader) const
{
    ASSERT(m_shaderSourceMap.contains(vertexShader));
    ASSERT(m_shaderSourceMap.contains(fragmentShader));
    const auto& vertexEntry = m_shaderSourceMap.find(vertexShader)->value;
    const auto& fragmentEntry = m_shaderSourceMap.find(fragmentShader)->value;

    HashMap<String, sh::ShaderVariable> combinedVaryings;
    for (const auto& vertexSymbol : vertexEntry.varyingMap) {
        const String& symbolName = vertexSymbol.key;
        // The varying map includes variables for each index of an array variable.
        // We only want a single variable to represent the array.
        if (symbolName.endsWith("]"))
            continue;

        // Don't count built in varyings.
        if (symbolName == "gl_FragCoord" || symbolName == "gl_FrontFacing" || symbolName == "gl_PointCoord")
            continue;

        const auto& fragmentSymbol = fragmentEntry.varyingMap.find(symbolName);
        if (fragmentSymbol != fragmentEntry.varyingMap.end())
            combinedVaryings.add(symbolName, fragmentSymbol->value);
    }

    size_t numVaryings = combinedVaryings.size();
    if (!numVaryings)
        return true;

    std::vector<sh::ShaderVariable> variables;
    variables.reserve(combinedVaryings.size());
    for (const auto& varyingSymbol : combinedVaryings.values())
        variables.push_back(varyingSymbol);

    GCGLint maxVaryingVectors = 0;
#if USE(OPENGL_ES)
    ::glGetIntegerv(MAX_VARYING_VECTORS, &maxVaryingVectors);
#else
    if (m_isForWebGL2)
        ::glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryingVectors);
    else {
        GCGLint maxVaryingFloats = 0;
        ::glGetIntegerv(GL_MAX_VARYING_FLOATS, &maxVaryingFloats);
        maxVaryingVectors = maxVaryingFloats / 4;
    }
#endif
    return sh::CheckVariablesWithinPackingLimits(maxVaryingVectors, variables);
}

bool GraphicsContextGLOpenGL::precisionsMatch(PlatformGLObject vertexShader, PlatformGLObject fragmentShader) const
{
    ASSERT(m_shaderSourceMap.contains(vertexShader));
    ASSERT(m_shaderSourceMap.contains(fragmentShader));
    const auto& vertexEntry = m_shaderSourceMap.find(vertexShader)->value;
    const auto& fragmentEntry = m_shaderSourceMap.find(fragmentShader)->value;

    HashMap<String, sh::GLenum> vertexSymbolPrecisionMap;

    for (const auto& entry : vertexEntry.uniformMap) {
        const std::string& mappedName = entry.value.get().mappedName;
        vertexSymbolPrecisionMap.add(String(mappedName.c_str(), mappedName.length()), entry.value.get().precision);
    }

    for (const auto& entry : fragmentEntry.uniformMap) {
        const std::string& mappedName = entry.value.get().mappedName;
        const auto& vertexSymbol = vertexSymbolPrecisionMap.find(String(mappedName.c_str(), mappedName.length()));
        if (vertexSymbol != vertexSymbolPrecisionMap.end() && vertexSymbol->value != entry.value.get().precision)
            return false;
    }

    return true;
}

void GraphicsContextGLOpenGL::activeTexture(GCGLenum texture)
{
    if (!makeContextCurrent())
        return;

    m_state.activeTextureUnit = texture;
    ::glActiveTexture(texture);
}

void GraphicsContextGLOpenGL::attachShader(PlatformGLObject program, PlatformGLObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    if (!makeContextCurrent())
        return;

    m_shaderProgramSymbolCountMap.remove(program);
    ::glAttachShader(program, shader);
}

void GraphicsContextGLOpenGL::bindAttribLocation(PlatformGLObject program, GCGLuint index, const String& name)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return;

    String mappedName = mappedSymbolName(program, SHADER_SYMBOL_TYPE_ATTRIBUTE, name);
    LOG(WebGL, "::bindAttribLocation is mapping %s to %s", name.utf8().data(), mappedName.utf8().data());
    ::glBindAttribLocation(program, index, mappedName.utf8().data());
}

void GraphicsContextGLOpenGL::bindBuffer(GCGLenum target, PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    ::glBindBuffer(target, buffer);
}

void GraphicsContextGLOpenGL::bindFramebuffer(GCGLenum target, PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    GLuint fbo;
    if (buffer)
        fbo = buffer;
    else
        fbo = (contextAttributes().antialias ? m_multisampleFBO : m_fbo);
    ASSERT(target == GL_FRAMEBUFFER);
    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (fbo != m_state.boundDrawFBO) {
        ::glBindFramebufferEXT(target, fbo);
        m_state.boundDrawFBO = m_state.boundReadFBO = fbo;
    }
}

void GraphicsContextGLOpenGL::bindRenderbuffer(GCGLenum target, PlatformGLObject renderbuffer)
{
    if (!makeContextCurrent())
        return;

    ::glBindRenderbufferEXT(target, renderbuffer);
}


void GraphicsContextGLOpenGL::bindTexture(GCGLenum target, PlatformGLObject texture)
{
    if (!makeContextCurrent())
        return;

    m_state.setBoundTexture(m_state.activeTextureUnit, texture, target);
    ::glBindTexture(target, texture);
}

void GraphicsContextGLOpenGL::blendColor(GCGLclampf red, GCGLclampf green, GCGLclampf blue, GCGLclampf alpha)
{
    if (!makeContextCurrent())
        return;

    ::glBlendColor(red, green, blue, alpha);
}

void GraphicsContextGLOpenGL::blendEquation(GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    ::glBlendEquation(mode);
}

void GraphicsContextGLOpenGL::blendEquationSeparate(GCGLenum modeRGB, GCGLenum modeAlpha)
{
    if (!makeContextCurrent())
        return;

    ::glBlendEquationSeparate(modeRGB, modeAlpha);
}


void GraphicsContextGLOpenGL::blendFunc(GCGLenum sfactor, GCGLenum dfactor)
{
    if (!makeContextCurrent())
        return;

    ::glBlendFunc(sfactor, dfactor);
}       

void GraphicsContextGLOpenGL::blendFuncSeparate(GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha)
{
    if (!makeContextCurrent())
        return;

    ::glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContextGLOpenGL::bufferData(GCGLenum target, GCGLsizeiptr size, GCGLenum usage)
{
    if (!makeContextCurrent())
        return;

    ::glBufferData(target, size, 0, usage);
}

void GraphicsContextGLOpenGL::bufferData(GCGLenum target, GCGLSpan<const void> data, GCGLenum usage)
{
    if (!makeContextCurrent())
        return;

    ::glBufferData(target, data.bufSize, data.data, usage);
}

void GraphicsContextGLOpenGL::bufferSubData(GCGLenum target, GCGLintptr offset, GCGLSpan<const GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;

    ::glBufferSubData(target, offset, data.bufSize, data.data);
}

GCGLenum GraphicsContextGLOpenGL::checkFramebufferStatus(GCGLenum target)
{
    if (!makeContextCurrent())
        return GL_INVALID_OPERATION;

    return ::glCheckFramebufferStatusEXT(target);
}

void GraphicsContextGLOpenGL::clearColor(GCGLclampf r, GCGLclampf g, GCGLclampf b, GCGLclampf a)
{
    if (!makeContextCurrent())
        return;

    ::glClearColor(r, g, b, a);
}

void GraphicsContextGLOpenGL::clear(GCGLbitfield mask)
{
    if (!makeContextCurrent())
        return;

    ::glClear(mask);
    checkGPUStatus();
}

void GraphicsContextGLOpenGL::clearStencil(GCGLint s)
{
    if (!makeContextCurrent())
        return;

    ::glClearStencil(s);
}

void GraphicsContextGLOpenGL::colorMask(GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha)
{
    if (!makeContextCurrent())
        return;

    ::glColorMask(red, green, blue, alpha);
}

void GraphicsContextGLOpenGL::compileShader(PlatformGLObject shader)
{
    ASSERT(shader);
    if (!makeContextCurrent())
        return;

    // Turn on name mapping. Due to the way ANGLE name hashing works, we
    // point a global hashmap to the map owned by this context.
    ShBuiltInResources ANGLEResources = m_compiler.getResources();
    ShHashFunction64 previousHashFunction = ANGLEResources.HashFunction;
    ANGLEResources.HashFunction = nameHashForShader;

    if (!nameHashMapForShaders)
        nameHashMapForShaders = makeUnique<ShaderNameHash>();
    setCurrentNameHashMapForShader(nameHashMapForShaders.get());
    m_compiler.setResources(ANGLEResources);

    String translatedShaderSource = m_extensions->getTranslatedShaderSourceANGLE(shader);

    ANGLEResources.HashFunction = previousHashFunction;
    m_compiler.setResources(ANGLEResources);
    setCurrentNameHashMapForShader(nullptr);

    if (!translatedShaderSource.length())
        return;

    const CString& translatedShaderCString = translatedShaderSource.utf8();
    const char* translatedShaderPtr = translatedShaderCString.data();
    int translatedShaderLength = translatedShaderCString.length();

    LOG(WebGL, "--- begin original shader source ---\n%s\n--- end original shader source ---\n", getShaderSource(shader).utf8().data());
    LOG(WebGL, "--- begin translated shader source ---\n%s\n--- end translated shader source ---", translatedShaderPtr);

    ::glShaderSource(shader, 1, &translatedShaderPtr, &translatedShaderLength);
    
    ::glCompileShader(shader);
    
    int compileStatus;
    
    ::glGetShaderiv(shader, COMPILE_STATUS, &compileStatus);

    ShaderSourceMap::iterator result = m_shaderSourceMap.find(shader);
    ShaderSourceEntry& entry = result->value;

    // Populate the shader log
    GLint length = 0;
    ::glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    if (length) {
        GLsizei size = 0;
        Vector<GLchar> info(length);
        ::glGetShaderInfoLog(shader, length, &size, info.data());

        PlatformGLObject shaders[2] = { shader, 0 };
        entry.log = getUnmangledInfoLog(shaders, 1, String(info.data(), size));
    }

    if (compileStatus != GL_TRUE) {
        entry.isValid = false;
        LOG(WebGL, "Error: shader translator produced a shader that OpenGL would not compile.");
    }
}

void GraphicsContextGLOpenGL::compileShaderDirect(PlatformGLObject shader)
{
    ASSERT(shader);
    if (!makeContextCurrent())
        return;

    HashMap<PlatformGLObject, ShaderSourceEntry>::iterator result = m_shaderSourceMap.find(shader);

    if (result == m_shaderSourceMap.end())
        return;

    ShaderSourceEntry& entry = result->value;

    const CString& shaderSourceCString = entry.source.utf8();
    const char* shaderSourcePtr = shaderSourceCString.data();
    int shaderSourceLength = shaderSourceCString.length();

    LOG(WebGL, "--- begin direct shader source ---\n%s\n--- end direct shader source ---\n", shaderSourcePtr);

    ::glShaderSource(shader, 1, &shaderSourcePtr, &shaderSourceLength);

    ::glCompileShader(shader);

    int compileStatus;

    ::glGetShaderiv(shader, COMPILE_STATUS, &compileStatus);

    if (compileStatus == GL_TRUE) {
        entry.isValid = true;
        LOG(WebGL, "Direct compilation of shader succeeded.");
    } else {
        entry.isValid = false;
        LOG(WebGL, "Error: direct compilation of shader failed.");
    }
}

void GraphicsContextGLOpenGL::copyTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLint border)
{
    if (!makeContextCurrent())
        return;

    auto attrs = contextAttributes();

    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (attrs.antialias && m_state.boundDrawFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_fbo);
    }
    ::glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
    if (attrs.antialias && m_state.boundDrawFBO == m_multisampleFBO)
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_multisampleFBO);
}

void GraphicsContextGLOpenGL::copyTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    auto attrs = contextAttributes();

    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (attrs.antialias && m_state.boundDrawFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_fbo);
    }
    ::glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    if (attrs.antialias && m_state.boundDrawFBO == m_multisampleFBO)
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_multisampleFBO);
}

void GraphicsContextGLOpenGL::cullFace(GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    ::glCullFace(mode);
}

void GraphicsContextGLOpenGL::depthFunc(GCGLenum func)
{
    if (!makeContextCurrent())
        return;

    ::glDepthFunc(func);
}

void GraphicsContextGLOpenGL::depthMask(GCGLboolean flag)
{
    if (!makeContextCurrent())
        return;

    ::glDepthMask(flag);
}

void GraphicsContextGLOpenGL::detachShader(PlatformGLObject program, PlatformGLObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    if (!makeContextCurrent())
        return;

    m_shaderProgramSymbolCountMap.remove(program);
    ::glDetachShader(program, shader);
}

void GraphicsContextGLOpenGL::disable(GCGLenum cap)
{
    if (!makeContextCurrent())
        return;

    ::glDisable(cap);
}

void GraphicsContextGLOpenGL::disableVertexAttribArray(GCGLuint index)
{
    if (!makeContextCurrent())
        return;

    ::glDisableVertexAttribArray(index);
}

void GraphicsContextGLOpenGL::drawArrays(GCGLenum mode, GCGLint first, GCGLsizei count)
{
    if (!makeContextCurrent())
        return;

    ::glDrawArrays(mode, first, count);
    checkGPUStatus();
}

void GraphicsContextGLOpenGL::drawElements(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;

    ::glDrawElements(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
    checkGPUStatus();
}

void GraphicsContextGLOpenGL::enable(GCGLenum cap)
{
    if (!makeContextCurrent())
        return;

    ::glEnable(cap);
}

void GraphicsContextGLOpenGL::enableVertexAttribArray(GCGLuint index)
{
    if (!makeContextCurrent())
        return;

    ::glEnableVertexAttribArray(index);
}

void GraphicsContextGLOpenGL::finish()
{
    if (!makeContextCurrent())
        return;

    ::glFinish();
}

void GraphicsContextGLOpenGL::flush()
{
    if (!makeContextCurrent())
        return;

    ::glFlush();
}

void GraphicsContextGLOpenGL::framebufferRenderbuffer(GCGLenum target, GCGLenum attachment, GCGLenum renderbuffertarget, PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    ::glFramebufferRenderbufferEXT(target, attachment, renderbuffertarget, buffer);
}

void GraphicsContextGLOpenGL::framebufferTexture2D(GCGLenum target, GCGLenum attachment, GCGLenum textarget, PlatformGLObject texture, GCGLint level)
{
    if (!makeContextCurrent())
        return;

    ::glFramebufferTexture2DEXT(target, attachment, textarget, texture, level);
}

void GraphicsContextGLOpenGL::frontFace(GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    ::glFrontFace(mode);
}

void GraphicsContextGLOpenGL::generateMipmap(GCGLenum target)
{
    if (!makeContextCurrent())
        return;

    ::glGenerateMipmap(target);
}

bool GraphicsContextGLOpenGL::getActiveAttribImpl(PlatformGLObject program, GCGLuint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    if (!makeContextCurrent())
        return false;

    GLint maxAttributeSize = 0;
    ::glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxAttributeSize);
    Vector<GLchar> name(maxAttributeSize); // GL_ACTIVE_ATTRIBUTE_MAX_LENGTH includes null termination.
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    ::glGetActiveAttrib(program, index, maxAttributeSize, &nameLength, &size, &type, name.data());
    if (!nameLength)
        return false;
    
    String originalName = originalSymbolName(program, SHADER_SYMBOL_TYPE_ATTRIBUTE, String(name.data(), nameLength));
    
#ifndef NDEBUG
    String uniformName(name.data(), nameLength);
    LOG(WebGL, "Program %d is mapping active attribute %d from '%s' to '%s'", program, index, uniformName.utf8().data(), originalName.utf8().data());
#endif

    info.name = originalName;
    info.type = type;
    info.size = size;
    return true;
}

bool GraphicsContextGLOpenGL::getActiveAttrib(PlatformGLObject program, GCGLuint index, ActiveInfo& info)
{
    GCGLint symbolCount;
    auto result = m_shaderProgramSymbolCountMap.find(program);
    if (result == m_shaderProgramSymbolCountMap.end()) {
        getNonBuiltInActiveSymbolCount(program, GraphicsContextGL::ACTIVE_ATTRIBUTES, &symbolCount);
        result = m_shaderProgramSymbolCountMap.find(program);
    }
    
    ActiveShaderSymbolCounts& symbolCounts = result->value;
    GCGLuint rawIndex = (index < symbolCounts.filteredToActualAttributeIndexMap.size()) ? symbolCounts.filteredToActualAttributeIndexMap[index] : -1;

    return getActiveAttribImpl(program, rawIndex, info);
}

bool GraphicsContextGLOpenGL::getActiveUniformImpl(PlatformGLObject program, GCGLuint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }

    if (!makeContextCurrent())
        return false;

    GLint maxUniformSize = 0;
    ::glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxUniformSize);

    Vector<GLchar> name(maxUniformSize); // GL_ACTIVE_UNIFORM_MAX_LENGTH includes null termination.
    GLsizei nameLength = 0;
    GLint size = 0;
    GLenum type = 0;
    ::glGetActiveUniform(program, index, maxUniformSize, &nameLength, &size, &type, name.data());
    if (!nameLength)
        return false;
    
    String originalName = originalSymbolName(program, SHADER_SYMBOL_TYPE_UNIFORM, String(name.data(), nameLength));
    
#ifndef NDEBUG
    String uniformName(name.data(), nameLength);
    LOG(WebGL, "Program %d is mapping active uniform %d from '%s' to '%s'", program, index, uniformName.utf8().data(), originalName.utf8().data());
#endif
    
    info.name = originalName;
    info.type = type;
    info.size = size;
    return true;
}

bool GraphicsContextGLOpenGL::getActiveUniform(PlatformGLObject program, GCGLuint index, ActiveInfo& info)
{
    GCGLint symbolCount;
    auto result = m_shaderProgramSymbolCountMap.find(program);
    if (result == m_shaderProgramSymbolCountMap.end()) {
        getNonBuiltInActiveSymbolCount(program, GraphicsContextGL::ACTIVE_UNIFORMS, &symbolCount);
        result = m_shaderProgramSymbolCountMap.find(program);
    }
    
    ActiveShaderSymbolCounts& symbolCounts = result->value;
    GCGLuint rawIndex = (index < symbolCounts.filteredToActualUniformIndexMap.size()) ? symbolCounts.filteredToActualUniformIndexMap[index] : -1;
    
    return getActiveUniformImpl(program, rawIndex, info);
}

void GraphicsContextGLOpenGL::getAttachedShaders(PlatformGLObject program, GCGLsizei maxCount, GCGLsizei* count, PlatformGLObject* shaders)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return;
    }
    if (!makeContextCurrent())
        return;

    ::glGetAttachedShaders(program, maxCount, count, shaders);
}

static String generateHashedName(const String& name)
{
    if (name.isEmpty())
        return name;
    uint64_t number = nameHashForShader(name.utf8().data(), name.length());
    return makeString("webgl_", hex(number, Lowercase));
}

std::optional<String> GraphicsContextGLOpenGL::mappedSymbolInShaderSourceMap(PlatformGLObject shader, ANGLEShaderSymbolType symbolType, const String& name)
{
    auto result = m_shaderSourceMap.find(shader);
    if (result == m_shaderSourceMap.end())
        return std::nullopt;

    const auto& symbolMap = result->value.symbolMap(symbolType);
    auto symbolEntry = symbolMap.find(name);
    if (symbolEntry == symbolMap.end())
        return std::nullopt;

    auto& mappedName = symbolEntry->value.get().mappedName;
    return String(mappedName.c_str(), mappedName.length());
}

String GraphicsContextGLOpenGL::mappedSymbolName(PlatformGLObject program, ANGLEShaderSymbolType symbolType, const String& name)
{
    GCGLsizei count = 0;
    PlatformGLObject shaders[2] = { };
    getAttachedShaders(program, 2, &count, shaders);

    for (GCGLsizei i = 0; i < count; ++i) {
        auto mappedName = mappedSymbolInShaderSourceMap(shaders[i], symbolType, name);
        if (mappedName)
            return mappedName.value();
    }

    // We might have detached or deleted the shaders after linking.
    auto result = m_linkedShaderMap.find(program);
    if (result != m_linkedShaderMap.end()) {
        auto linkedShaders = result->value;
        auto mappedName = mappedSymbolInShaderSourceMap(linkedShaders.first, symbolType, name);
        if (mappedName)
            return mappedName.value();
        mappedName = mappedSymbolInShaderSourceMap(linkedShaders.second, symbolType, name);
        if (mappedName)
            return mappedName.value();
    }

    if (symbolType == SHADER_SYMBOL_TYPE_ATTRIBUTE && !name.isEmpty()) {
        // Attributes are a special case: they may be requested before any shaders have been compiled,
        // and aren't even required to be used in any shader program.
        if (!nameHashMapForShaders)
            nameHashMapForShaders = makeUnique<ShaderNameHash>();
        setCurrentNameHashMapForShader(nameHashMapForShaders.get());

        auto generatedName = generateHashedName(name);

        setCurrentNameHashMapForShader(nullptr);

        m_possiblyUnusedAttributeMap.set(generatedName, name);

        return generatedName;
    }

    return name;
}

std::optional<String> GraphicsContextGLOpenGL::originalSymbolInShaderSourceMap(PlatformGLObject shader, ANGLEShaderSymbolType symbolType, const String& name)
{
    auto result = m_shaderSourceMap.find(shader);
    if (result == m_shaderSourceMap.end())
        return std::nullopt;

    const auto& symbolMap = result->value.symbolMap(symbolType);
    for (const auto& symbolEntry : symbolMap) {
        if (name == symbolEntry.value.get().mappedName.c_str())
            return symbolEntry.key;
    }
    return std::nullopt;
}

String GraphicsContextGLOpenGL::originalSymbolName(PlatformGLObject program, ANGLEShaderSymbolType symbolType, const String& name)
{
    GCGLsizei count;
    PlatformGLObject shaders[2];
    getAttachedShaders(program, 2, &count, shaders);
    
    for (GCGLsizei i = 0; i < count; ++i) {
        auto originalName = originalSymbolInShaderSourceMap(shaders[i], symbolType, name);
        if (originalName)
            return originalName.value();
    }

    // We might have detached or deleted the shaders after linking.
    auto result = m_linkedShaderMap.find(program);
    if (result != m_linkedShaderMap.end()) {
        auto linkedShaders = result->value;
        auto originalName = originalSymbolInShaderSourceMap(linkedShaders.first, symbolType, name);
        if (originalName)
            return originalName.value();
        originalName = originalSymbolInShaderSourceMap(linkedShaders.second, symbolType, name);
        if (originalName)
            return originalName.value();
    }

    if (symbolType == SHADER_SYMBOL_TYPE_ATTRIBUTE && !name.isEmpty()) {
        // Attributes are a special case: they may be requested before any shaders have been compiled,
        // and aren't even required to be used in any shader program.

        const auto& cached = m_possiblyUnusedAttributeMap.find(name);
        if (cached != m_possiblyUnusedAttributeMap.end())
            return cached->value;
    }

    return name;
}

String GraphicsContextGLOpenGL::mappedSymbolName(PlatformGLObject shaders[2], size_t count, const String& name)
{
    for (size_t symbolType = 0; symbolType <= static_cast<size_t>(SHADER_SYMBOL_TYPE_VARYING); ++symbolType) {
        for (size_t i = 0; i < count; ++i) {
            ShaderSourceMap::iterator result = m_shaderSourceMap.find(shaders[i]);
            if (result == m_shaderSourceMap.end())
                continue;
            
            const ShaderSymbolMap& symbolMap = result->value.symbolMap(static_cast<enum ANGLEShaderSymbolType>(symbolType));
            for (const auto& symbolEntry : symbolMap) {
                if (name == symbolEntry.value.get().mappedName.c_str())
                    return symbolEntry.key;
            }
        }
    }
    return name;
}

int GraphicsContextGLOpenGL::getAttribLocation(PlatformGLObject program, const String& name)
{
    if (!program)
        return -1;

    if (!makeContextCurrent())
        return -1;


    String mappedName = mappedSymbolName(program, SHADER_SYMBOL_TYPE_ATTRIBUTE, name);
    LOG(WebGL, "::glGetAttribLocation is mapping %s to %s", name.utf8().data(), mappedName.utf8().data());
    return ::glGetAttribLocation(program, mappedName.utf8().data());
}

int GraphicsContextGLOpenGL::getAttribLocationDirect(PlatformGLObject program, const String& name)
{
    if (!program)
        return -1;

    if (!makeContextCurrent())
        return -1;


    return ::glGetAttribLocation(program, name.utf8().data());
}

bool GraphicsContextGLOpenGL::moveErrorsToSyntheticErrorList()
{
    if (!makeContextCurrent())
        return false;

    bool movedAnError = false;

    // Set an arbitrary limit of 100 here to avoid creating a hang if
    // a problem driver has a bug that causes it to never clear the error.
    // Otherwise, we would just loop until we got NO_ERROR.
    for (unsigned i = 0; i < 100; ++i) {
        GCGLenum error = glGetError();
        if (error == NO_ERROR)
            break;
        m_syntheticErrors.add(error);
        movedAnError = true;
    }

    return movedAnError;
}

GCGLenum GraphicsContextGLOpenGL::getError()
{
    if (!m_syntheticErrors.isEmpty()) {
        // Need to move the current errors to the synthetic error list in case
        // that error is already there, since the expected behavior of both
        // glGetError and getError is to only report each error code once.
        moveErrorsToSyntheticErrorList();
        return m_syntheticErrors.takeFirst();
    }

    if (!makeContextCurrent())
        return GL_INVALID_OPERATION;

    return ::glGetError();
}

String GraphicsContextGLOpenGL::getString(GCGLenum name)
{
    if (!makeContextCurrent())
        return String();

    return String(reinterpret_cast<const char*>(::glGetString(name)));
}

void GraphicsContextGLOpenGL::hint(GCGLenum target, GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    ::glHint(target, mode);
}

GCGLboolean GraphicsContextGLOpenGL::isBuffer(PlatformGLObject buffer)
{
    if (!buffer)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return ::glIsBuffer(buffer);
}

GCGLboolean GraphicsContextGLOpenGL::isEnabled(GCGLenum cap)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return ::glIsEnabled(cap);
}

GCGLboolean GraphicsContextGLOpenGL::isFramebuffer(PlatformGLObject framebuffer)
{
    if (!framebuffer)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return ::glIsFramebufferEXT(framebuffer);
}

GCGLboolean GraphicsContextGLOpenGL::isProgram(PlatformGLObject program)
{
    if (!program)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return ::glIsProgram(program);
}

GCGLboolean GraphicsContextGLOpenGL::isRenderbuffer(PlatformGLObject renderbuffer)
{
    if (!renderbuffer)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return ::glIsRenderbufferEXT(renderbuffer);
}

GCGLboolean GraphicsContextGLOpenGL::isShader(PlatformGLObject shader)
{
    if (!shader)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return ::glIsShader(shader);
}

GCGLboolean GraphicsContextGLOpenGL::isTexture(PlatformGLObject texture)
{
    if (!texture)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return ::glIsTexture(texture);
}

void GraphicsContextGLOpenGL::lineWidth(GCGLfloat width)
{
    if (!makeContextCurrent())
        return;

    ::glLineWidth(width);
}

void GraphicsContextGLOpenGL::linkProgram(PlatformGLObject program)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return;


    GCGLsizei count = 0;
    PlatformGLObject shaders[2] = { };
    getAttachedShaders(program, 2, &count, shaders);

    if (count == 2)
        m_linkedShaderMap.set(program, std::make_pair(shaders[0], shaders[1]));

    ::glLinkProgram(program);
}

void GraphicsContextGLOpenGL::pixelStorei(GCGLenum pname, GCGLint param)
{
    if (!makeContextCurrent())
        return;

    ::glPixelStorei(pname, param);
}

void GraphicsContextGLOpenGL::polygonOffset(GCGLfloat factor, GCGLfloat units)
{
    if (!makeContextCurrent())
        return;

    ::glPolygonOffset(factor, units);
}

void GraphicsContextGLOpenGL::sampleCoverage(GCGLclampf value, GCGLboolean invert)
{
    if (!makeContextCurrent())
        return;

    ::glSampleCoverage(value, invert);
}

void GraphicsContextGLOpenGL::scissor(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    ::glScissor(x, y, width, height);
}

void GraphicsContextGLOpenGL::shaderSource(PlatformGLObject shader, const String& string)
{
    ASSERT(shader);

    if (!makeContextCurrent())
        return;


    ShaderSourceEntry entry;

    entry.source = string;

    m_shaderSourceMap.set(shader, WTFMove(entry));
}

void GraphicsContextGLOpenGL::stencilFunc(GCGLenum func, GCGLint ref, GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    ::glStencilFunc(func, ref, mask);
}

void GraphicsContextGLOpenGL::stencilFuncSeparate(GCGLenum face, GCGLenum func, GCGLint ref, GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    ::glStencilFuncSeparate(face, func, ref, mask);
}

void GraphicsContextGLOpenGL::stencilMask(GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    ::glStencilMask(mask);
}

void GraphicsContextGLOpenGL::stencilMaskSeparate(GCGLenum face, GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    ::glStencilMaskSeparate(face, mask);
}

void GraphicsContextGLOpenGL::stencilOp(GCGLenum fail, GCGLenum zfail, GCGLenum zpass)
{
    if (!makeContextCurrent())
        return;

    ::glStencilOp(fail, zfail, zpass);
}

void GraphicsContextGLOpenGL::stencilOpSeparate(GCGLenum face, GCGLenum fail, GCGLenum zfail, GCGLenum zpass)
{
    if (!makeContextCurrent())
        return;

    ::glStencilOpSeparate(face, fail, zfail, zpass);
}

void GraphicsContextGLOpenGL::texParameterf(GCGLenum target, GCGLenum pname, GCGLfloat value)
{
    if (!makeContextCurrent())
        return;

    ::glTexParameterf(target, pname, value);
}

void GraphicsContextGLOpenGL::texParameteri(GCGLenum target, GCGLenum pname, GCGLint value)
{
    if (!makeContextCurrent())
        return;

    ::glTexParameteri(target, pname, value);
}

void GraphicsContextGLOpenGL::uniform1f(GCGLint location, GCGLfloat v0)
{
    if (!makeContextCurrent())
        return;

    ::glUniform1f(location, v0);
}

void GraphicsContextGLOpenGL::uniform1fv(GCGLint location, GCGLSpan<const GCGLfloat> array)
{
    if (!makeContextCurrent())
        return;

    ::glUniform1fv(location, array.bufSize, array.data);
}

void GraphicsContextGLOpenGL::uniform2f(GCGLint location, GCGLfloat v0, GCGLfloat v1)
{
    if (!makeContextCurrent())
        return;

    ::glUniform2f(location, v0, v1);
}

void GraphicsContextGLOpenGL::uniform2fv(GCGLint location, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 2));
    if (!makeContextCurrent())
        return;

    ::glUniform2fv(location, array.bufSize / 2, array.data);
}

void GraphicsContextGLOpenGL::uniform3f(GCGLint location, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2)
{
    if (!makeContextCurrent())
        return;

    ::glUniform3f(location, v0, v1, v2);
}

void GraphicsContextGLOpenGL::uniform3fv(GCGLint location, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 3));
    if (!makeContextCurrent())
        return;

    ::glUniform3fv(location, array.bufSize / 3, array.data);
}

void GraphicsContextGLOpenGL::uniform4f(GCGLint location, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2, GCGLfloat v3)
{
    if (!makeContextCurrent())
        return;

    ::glUniform4f(location, v0, v1, v2, v3);
}

void GraphicsContextGLOpenGL::uniform4fv(GCGLint location, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 4));
    if (!makeContextCurrent())
        return;

    ::glUniform4fv(location, array.bufSize / 4, array.data);
}

void GraphicsContextGLOpenGL::uniform1i(GCGLint location, GCGLint v0)
{
    if (!makeContextCurrent())
        return;

    ::glUniform1i(location, v0);
}

void GraphicsContextGLOpenGL::uniform1iv(GCGLint location, GCGLSpan<const GCGLint> array)
{
    if (!makeContextCurrent())
        return;

    ::glUniform1iv(location, array.bufSize, array.data);
}

void GraphicsContextGLOpenGL::uniform2i(GCGLint location, GCGLint v0, GCGLint v1)
{
    if (!makeContextCurrent())
        return;

    ::glUniform2i(location, v0, v1);
}

void GraphicsContextGLOpenGL::uniform2iv(GCGLint location, GCGLSpan<const GCGLint> array)
{
    ASSERT(!(array.bufSize % 2));
    if (!makeContextCurrent())
        return;

    ::glUniform2iv(location, array.bufSize / 2, array.data);
}

void GraphicsContextGLOpenGL::uniform3i(GCGLint location, GCGLint v0, GCGLint v1, GCGLint v2)
{
    if (!makeContextCurrent())
        return;

    ::glUniform3i(location, v0, v1, v2);
}

void GraphicsContextGLOpenGL::uniform3iv(GCGLint location, GCGLSpan<const GCGLint> array)
{
    ASSERT(!(array.bufSize % 3));
    if (!makeContextCurrent())
        return;

    ::glUniform3iv(location, array.bufSize / 3, array.data);
}

void GraphicsContextGLOpenGL::uniform4i(GCGLint location, GCGLint v0, GCGLint v1, GCGLint v2, GCGLint v3)
{
    if (!makeContextCurrent())
        return;

    ::glUniform4i(location, v0, v1, v2, v3);
}

void GraphicsContextGLOpenGL::uniform4iv(GCGLint location, GCGLSpan<const GCGLint> array)
{
    ASSERT(!(array.bufSize % 4));
    if (!makeContextCurrent())
        return;

    ::glUniform4iv(location, array.bufSize / 4, array.data);
}

void GraphicsContextGLOpenGL::uniformMatrix2fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 4));
    if (!makeContextCurrent())
        return;

    ::glUniformMatrix2fv(location, array.bufSize / 4, transpose, array.data);
}

void GraphicsContextGLOpenGL::uniformMatrix3fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 9));
    if (!makeContextCurrent())
        return;

    ::glUniformMatrix3fv(location, array.bufSize / 9, transpose, array.data);
}

void GraphicsContextGLOpenGL::uniformMatrix4fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> array)
{
    ASSERT(!(array.bufSize % 16));
    if (!makeContextCurrent())
        return;

    ::glUniformMatrix4fv(location, array.bufSize / 16, transpose, array.data);
}

void GraphicsContextGLOpenGL::useProgram(PlatformGLObject program)
{
    if (!makeContextCurrent())
        return;

    ::glUseProgram(program);
}

void GraphicsContextGLOpenGL::validateProgram(PlatformGLObject program)
{
    ASSERT(program);

    if (!makeContextCurrent())
        return;

    ::glValidateProgram(program);
}

void GraphicsContextGLOpenGL::vertexAttrib1f(GCGLuint index, GCGLfloat v0)
{
    if (!makeContextCurrent())
        return;

    ::glVertexAttrib1f(index, v0);
}

void GraphicsContextGLOpenGL::vertexAttrib1fv(GCGLuint index, GCGLSpan<const GCGLfloat, 1> array)
{
    if (!makeContextCurrent())
        return;

    ::glVertexAttrib1fv(index, array.data);
}

void GraphicsContextGLOpenGL::vertexAttrib2f(GCGLuint index, GCGLfloat v0, GCGLfloat v1)
{
    if (!makeContextCurrent())
        return;

    ::glVertexAttrib2f(index, v0, v1);
}

void GraphicsContextGLOpenGL::vertexAttrib2fv(GCGLuint index, GCGLSpan<const GCGLfloat, 2> array)
{
    if (!makeContextCurrent())
        return;

    ::glVertexAttrib2fv(index, array.data);
}

void GraphicsContextGLOpenGL::vertexAttrib3f(GCGLuint index, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2)
{
    if (!makeContextCurrent())
        return;

    ::glVertexAttrib3f(index, v0, v1, v2);
}

void GraphicsContextGLOpenGL::vertexAttrib3fv(GCGLuint index, GCGLSpan<const GCGLfloat, 3> array)
{
    if (!makeContextCurrent())
        return;

    ::glVertexAttrib3fv(index, array.data);
}

void GraphicsContextGLOpenGL::vertexAttrib4f(GCGLuint index, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2, GCGLfloat v3)
{
    if (!makeContextCurrent())
        return;

    ::glVertexAttrib4f(index, v0, v1, v2, v3);
}

void GraphicsContextGLOpenGL::vertexAttrib4fv(GCGLuint index, GCGLSpan<const GCGLfloat, 4> array)
{
    if (!makeContextCurrent())
        return;

    ::glVertexAttrib4fv(index, array.data);
}

void GraphicsContextGLOpenGL::vertexAttribPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLboolean normalized, GCGLsizei stride, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;

    ::glVertexAttribPointer(index, size, type, normalized, stride, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContextGLOpenGL::viewport(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    ::glViewport(x, y, width, height);
}

PlatformGLObject GraphicsContextGLOpenGL::createVertexArray()
{
    return getExtensions().createVertexArrayOES();
}

void GraphicsContextGLOpenGL::deleteVertexArray(PlatformGLObject array)
{
    getExtensions().deleteVertexArrayOES(array);
}

GCGLboolean GraphicsContextGLOpenGL::isVertexArray(PlatformGLObject array)
{
    return getExtensions().isVertexArrayOES(array);
}

void GraphicsContextGLOpenGL::bindVertexArray(PlatformGLObject array)
{
    getExtensions().bindVertexArrayOES(array);
}

void GraphicsContextGLOpenGL::getBooleanv(GCGLenum pname, GCGLSpan<GCGLboolean> value)
{
    if (!makeContextCurrent())
        return;

    ::glGetBooleanv(pname, value.data);
}

GCGLint GraphicsContextGLOpenGL::getBufferParameteri(GCGLenum target, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;

    ::glGetBufferParameteriv(target, pname, &value);
    return value;
}

void GraphicsContextGLOpenGL::getFloatv(GCGLenum pname, GCGLSpan<GCGLfloat> value)
{
    if (!makeContextCurrent())
        return;

    ::glGetFloatv(pname, value.data);
}

GCGLint64 GraphicsContextGLOpenGL::getInteger64(GCGLenum pname)
{
    UNUSED_PARAM(pname);
    if (!makeContextCurrent())
        return 0;

    GCGLint64 value = 0;
    // FIXME 141178: Before enabling this we must first switch over to using gl3.h and creating and initialing the WebGL2 context using OpenGL ES 3.0.
    // ::glGetInteger64v(pname, value);
    return value;
}

GCGLint64 GraphicsContextGLOpenGL::getInteger64i(GCGLenum pname, GCGLuint index)
{
    UNUSED_PARAM(pname);
    UNUSED_PARAM(index);
    if (!makeContextCurrent())
        return 0;

    GCGLint64 value = 0;
    // FIXME 141178: Before enabling this we must first switch over to using gl3.h and creating and initialing the WebGL2 context using OpenGL ES 3.0.
    // ::glGetInteger64i_v(pname, index, value);
    return value;
}

GCGLint GraphicsContextGLOpenGL::getFramebufferAttachmentParameteri(GCGLenum target, GCGLenum attachment, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return 0;
    if (attachment == DEPTH_STENCIL_ATTACHMENT)
        attachment = DEPTH_ATTACHMENT; // Or STENCIL_ATTACHMENT, either works.
    ::glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, &value);
    return value;
}

GCGLint GraphicsContextGLOpenGL::getProgrami(PlatformGLObject program, GCGLenum pname)
{
    if (!makeContextCurrent())
        return 0;
    GCGLint value = 0;
    ::glGetProgramiv(program, pname, &value);
    return value;
}

void GraphicsContextGLOpenGL::getNonBuiltInActiveSymbolCount(PlatformGLObject program, GCGLenum pname, GCGLint* value)
{
    ASSERT(ACTIVE_ATTRIBUTES == pname || ACTIVE_UNIFORMS == pname);
    if (!value)
        return;

    if (!makeContextCurrent())
        return;

    const auto& result = m_shaderProgramSymbolCountMap.find(program);
    if (result != m_shaderProgramSymbolCountMap.end()) {
        *value = result->value.countForType(pname);
        return;
    }

    m_shaderProgramSymbolCountMap.set(program, ActiveShaderSymbolCounts());
    ActiveShaderSymbolCounts& symbolCounts = m_shaderProgramSymbolCountMap.find(program)->value;

    // Retrieve the active attributes, build a filtered count, and a mapping of
    // our internal attributes indexes to the real unfiltered indexes inside OpenGL.
    GCGLint attributeCount = 0;
    ::glGetProgramiv(program, ACTIVE_ATTRIBUTES, &attributeCount);
    for (GCGLint i = 0; i < attributeCount; ++i) {
        ActiveInfo info;
        getActiveAttribImpl(program, i, info);
        if (info.name.startsWith("gl_"))
            continue;

        symbolCounts.filteredToActualAttributeIndexMap.append(i);
    }
    
    // Do the same for uniforms.
    GCGLint uniformCount = 0;
    ::glGetProgramiv(program, ACTIVE_UNIFORMS, &uniformCount);
    for (GCGLint i = 0; i < uniformCount; ++i) {
        ActiveInfo info;
        getActiveUniformImpl(program, i, info);
        if (info.name.startsWith("gl_"))
            continue;
        
        symbolCounts.filteredToActualUniformIndexMap.append(i);
    }
    
    *value = symbolCounts.countForType(pname);
}

String GraphicsContextGLOpenGL::getUnmangledInfoLog(PlatformGLObject shaders[2], GCGLsizei count, const String& log)
{
    LOG(WebGL, "Original ShaderInfoLog:\n%s", log.utf8().data());

    JSC::Yarr::RegularExpression regExp("webgl_[0123456789abcdefABCDEF]+");

    StringBuilder processedLog;
    
    // ANGLE inserts a "#extension" line into the shader source that
    // causes a warning in some compilers. There is no point showing
    // this warning to the user since they didn't write the code that
    // is causing it.
    static const NeverDestroyed<String> angleWarning { "WARNING: 0:1: extension 'GL_ARB_gpu_shader5' is not supported\n"_s };
    int startFrom = log.startsWith(angleWarning) ? angleWarning.get().length() : 0;
    int matchedLength = 0;

    do {
        int start = regExp.match(log, startFrom, &matchedLength);
        if (start == -1)
            break;

        processedLog.append(log.substring(startFrom, start - startFrom));
        startFrom = start + matchedLength;

        const String& mangledSymbol = log.substring(start, matchedLength);
        const String& mappedSymbol = mappedSymbolName(shaders, count, mangledSymbol);
        LOG(WebGL, "Demangling: %s to %s", mangledSymbol.utf8().data(), mappedSymbol.utf8().data());
        processedLog.append(mappedSymbol);
    } while (startFrom < static_cast<int>(log.length()));

    processedLog.append(log.substring(startFrom, log.length() - startFrom));

    LOG(WebGL, "Unmangled ShaderInfoLog:\n%s", processedLog.toString().utf8().data());
    return processedLog.toString();
}

String GraphicsContextGLOpenGL::getProgramInfoLog(PlatformGLObject program)
{
    ASSERT(program);

    if (!makeContextCurrent())
        return String();

    GLint length = 0;
    ::glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (!length)
        return String(); 

    GLsizei size = 0;
    Vector<GLchar> info(length);
    ::glGetProgramInfoLog(program, length, &size, info.data());

    GCGLsizei count;
    PlatformGLObject shaders[2];
    getAttachedShaders(program, 2, &count, shaders);

    return getUnmangledInfoLog(shaders, count, String(info.data(), size));
}

GCGLint GraphicsContextGLOpenGL::getRenderbufferParameteri(GCGLenum target, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    ::glGetRenderbufferParameterivEXT(target, pname, &value);
    return value;
}

GCGLint GraphicsContextGLOpenGL::getShaderi(PlatformGLObject shader, GCGLenum pname)
{
    ASSERT(shader);
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;


    const auto& result = m_shaderSourceMap.find(shader);
    
    switch (pname) {
    case DELETE_STATUS:
    case SHADER_TYPE:
        ::glGetShaderiv(shader, pname, &value);
        break;
    case COMPILE_STATUS:
        if (result != m_shaderSourceMap.end())
            value = static_cast<int>(result->value.isValid);
        break;
    case INFO_LOG_LENGTH:
        if (result != m_shaderSourceMap.end())
            value = getShaderInfoLog(shader).length();
        break;
    case SHADER_SOURCE_LENGTH:
        value = getShaderSource(shader).length();
        break;
    default:
        synthesizeGLError(INVALID_ENUM);
    }
    return value;
}

String GraphicsContextGLOpenGL::getShaderInfoLog(PlatformGLObject shader)
{
    ASSERT(shader);

    if (!makeContextCurrent())
        return String();

    const auto& result = m_shaderSourceMap.find(shader);
    if (result == m_shaderSourceMap.end())
        return String(); 

    const ShaderSourceEntry& entry = result->value;
    if (!entry.isValid)
        return entry.log;

    GLint length = 0;
    ::glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (!length)
        return String(); 

    GLsizei size = 0;
    Vector<GLchar> info(length);
    ::glGetShaderInfoLog(shader, length, &size, info.data());

    PlatformGLObject shaders[2] = { shader, 0 };
    return getUnmangledInfoLog(shaders, 1, String(info.data(), size));
}

String GraphicsContextGLOpenGL::getShaderSource(PlatformGLObject shader)
{
    ASSERT(shader);

    if (!makeContextCurrent())
        return String();

    const auto& result = m_shaderSourceMap.find(shader);
    if (result == m_shaderSourceMap.end())
        return String(); 

    return result->value.source;
}

GCGLfloat GraphicsContextGLOpenGL::getTexParameterf(GCGLenum target, GCGLenum pname)
{
    GCGLfloat value = 0.f;
    if (!makeContextCurrent())
        return value;
    ::glGetTexParameterfv(target, pname, &value);
    return value;
}

GCGLint GraphicsContextGLOpenGL::getTexParameteri(GCGLenum target, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    ::glGetTexParameteriv(target, pname, &value);
    return value;
}

void GraphicsContextGLOpenGL::getUniformfv(PlatformGLObject program, GCGLint location, GCGLSpan<GCGLfloat> value)
{
    if (!makeContextCurrent())
        return;

    ::glGetUniformfv(program, location, value.data);
}

void GraphicsContextGLOpenGL::getUniformiv(PlatformGLObject program, GCGLint location, GCGLSpan<GCGLint> value)
{
    if (!makeContextCurrent())
        return;

    ::glGetUniformiv(program, location, value.data);
}

void GraphicsContextGLOpenGL::getUniformuiv(PlatformGLObject program, GCGLint location, GCGLSpan<GCGLuint> value)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(location);
    UNUSED_PARAM(value);
}

GCGLint GraphicsContextGLOpenGL::getUniformLocation(PlatformGLObject program, const String& name)
{
    ASSERT(program);

    if (!makeContextCurrent())
        return -1;

    String mappedName = mappedSymbolName(program, SHADER_SYMBOL_TYPE_UNIFORM, name);
    LOG(WebGL, "::getUniformLocation is mapping %s to %s", name.utf8().data(), mappedName.utf8().data());
    return ::glGetUniformLocation(program, mappedName.utf8().data());
}

GCGLsizeiptr GraphicsContextGLOpenGL::getVertexAttribOffset(GCGLuint index, GCGLenum pname)
{
    if (!makeContextCurrent())
        return 0;

    GLvoid* pointer = 0;
    ::glGetVertexAttribPointerv(index, pname, &pointer);
    return static_cast<GCGLsizeiptr>(reinterpret_cast<intptr_t>(pointer));
}

void GraphicsContextGLOpenGL::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoff, GCGLint yoff, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels)
{
    if (!makeContextCurrent())
        return;

#if !USE(OPENGL_ES)
    if (type == HALF_FLOAT_OES)
        type = GL_HALF_FLOAT_ARB;
#endif

    if (m_usingCoreProfile)  {
        // There are some format values used in WebGL that are deprecated when using a core profile, so we need
        // to adapt them, as we do in GraphicsContextGLOpenGL::texImage2D().
        switch (format) {
        case ALPHA:
            // We are using GL_RED to back GL_ALPHA, so do it here as well.
            format = RED;
            break;
        case LUMINANCE_ALPHA:
            // We are using GL_RG to back GL_LUMINANCE_ALPHA, so do it here as well.
            format = RG;
            break;
        default:
            break;
        }
    }

    // FIXME: we will need to deal with PixelStore params when dealing with image buffers that differ from the subimage size.
    ::glTexSubImage2D(target, level, xoff, yoff, width, height, format, type, pixels.data);
}

void GraphicsContextGLOpenGL::compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLsizei imageSize, GCGLSpan<const GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;

    ::glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data.data);
}

void GraphicsContextGLOpenGL::compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, GCGLSpan<const GCGLvoid> data)
{
    if (!makeContextCurrent())
        return;

    ::glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data.data);
}

PlatformGLObject GraphicsContextGLOpenGL::createBuffer()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    glGenBuffers(1, &o);
    return o;
}

PlatformGLObject GraphicsContextGLOpenGL::createFramebuffer()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    glGenFramebuffersEXT(1, &o);
    return o;
}

PlatformGLObject GraphicsContextGLOpenGL::createProgram()
{
    if (!makeContextCurrent())
        return 0;

    return glCreateProgram();
}

PlatformGLObject GraphicsContextGLOpenGL::createRenderbuffer()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    glGenRenderbuffersEXT(1, &o);
    return o;
}

PlatformGLObject GraphicsContextGLOpenGL::createShader(GCGLenum type)
{
    if (!makeContextCurrent())
        return 0;

    return glCreateShader((type == FRAGMENT_SHADER) ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
}

PlatformGLObject GraphicsContextGLOpenGL::createTexture()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    glGenTextures(1, &o);
    return o;
}

void GraphicsContextGLOpenGL::deleteBuffer(PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    glDeleteBuffers(1, &buffer);
}

void GraphicsContextGLOpenGL::deleteFramebuffer(PlatformGLObject framebuffer)
{
    if (!makeContextCurrent())
        return;

    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (framebuffer == m_state.boundDrawFBO) {
        // Make sure the framebuffer is not going to be used for drawing
        // operations after it gets deleted.
        bindFramebuffer(FRAMEBUFFER, 0);
    }
    glDeleteFramebuffersEXT(1, &framebuffer);
}

void GraphicsContextGLOpenGL::deleteProgram(PlatformGLObject program)
{
    if (!makeContextCurrent())
        return;

    m_shaderProgramSymbolCountMap.remove(program);
    glDeleteProgram(program);
}

void GraphicsContextGLOpenGL::deleteRenderbuffer(PlatformGLObject renderbuffer)
{
    if (!makeContextCurrent())
        return;

    glDeleteRenderbuffersEXT(1, &renderbuffer);
}

void GraphicsContextGLOpenGL::deleteShader(PlatformGLObject shader)
{
    if (!makeContextCurrent())
        return;

    glDeleteShader(shader);
}

void GraphicsContextGLOpenGL::deleteTexture(PlatformGLObject texture)
{
    if (!makeContextCurrent())
        return;

    m_state.boundTextureMap.removeIf([texture] (auto& keyValue) {
        return keyValue.value.first == texture;
    });
    glDeleteTextures(1, &texture);
}

void GraphicsContextGLOpenGL::synthesizeGLError(GCGLenum error)
{
    // Need to move the current errors to the synthetic error list to
    // preserve the order of errors, so a caller to getError will get
    // any errors from glError before the error we are synthesizing.
    moveErrorsToSyntheticErrorList();
    m_syntheticErrors.add(error);
}

void GraphicsContextGLOpenGL::forceContextLost()
{
    for (auto* client : copyToVector(m_clients))
        client->forceContextLost();
}

void GraphicsContextGLOpenGL::recycleContext()
{
    for (auto* client : copyToVector(m_clients))
        client->recycleContext();
}

void GraphicsContextGLOpenGL::dispatchContextChangedNotification()
{
    for (auto* client : copyToVector(m_clients))
        client->dispatchContextChangedNotification();
}

void GraphicsContextGLOpenGL::texImage2DDirect(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, const void* pixels)
{
    if (!makeContextCurrent())
        return;

    ::glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void GraphicsContextGLOpenGL::drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount)
{
    getExtensions().drawArraysInstancedANGLE(mode, first, count, primcount);
    checkGPUStatus();
}

void GraphicsContextGLOpenGL::drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset, GCGLsizei primcount)
{
    getExtensions().drawElementsInstancedANGLE(mode, count, type, offset, primcount);
    checkGPUStatus();
}

void GraphicsContextGLOpenGL::vertexAttribDivisor(GCGLuint index, GCGLuint divisor)
{
    getExtensions().vertexAttribDivisorANGLE(index, divisor);
}

void GraphicsContextGLOpenGL::bufferData(GCGLenum target, const void* data, GCGLenum usage, GCGLuint srcOffset, GCGLuint length)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(data);
    UNUSED_PARAM(usage);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(length);
}

void GraphicsContextGLOpenGL::bufferSubData(GCGLenum target, GCGLintptr dstByteOffset, const void* srcData, GCGLuint srcOffset, GCGLuint length)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(dstByteOffset);
    UNUSED_PARAM(srcData);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(length);
}

#if HAVE(OPENGL_4) || HAVE(OPENGL_ES_3)
void GraphicsContextGLOpenGL::copyBufferSubData(GCGLenum readTarget, GCGLenum writeTarget, GCGLintptr readOffset, GCGLintptr writeOffset, GCGLsizeiptr size)
{
    if (!makeContextCurrent())
        return;

    ::glCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}
#else
void GraphicsContextGLOpenGL::copyBufferSubData(GCGLenum, GCGLenum, GCGLintptr, GCGLintptr, GCGLsizeiptr)
{
}
#endif

void GraphicsContextGLOpenGL::getBufferSubData(GCGLenum target, GCGLintptr offset, GCGLSpan<GCGLvoid> data)
{
#if HAVE(OPENGL_4) || HAVE(OPENGL_ES_3)
    if (!makeContextCurrent())
        return;
    GCGLvoid* ptr = ::glMapBufferRange(target, offset, data.bufSize, GraphicsContextGL::MAP_READ_BIT);
    if (!ptr)
        return;
    memcpy(data.data, ptr, data.bufSize);
    if (!::glUnmapBuffer(target))
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION);
#else
    UNUSED_PARAM(target);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(data);
    synthesizeGLError(GraphicsContextGL::INVALID_OPERATION);
#endif
}


void GraphicsContextGLOpenGL::blitFramebuffer(GCGLint srcX0, GCGLint srcY0, GCGLint srcX1, GCGLint srcY1, GCGLint dstX0, GCGLint dstY0, GCGLint dstX1, GCGLint dstY1, GCGLbitfield mask, GCGLenum filter)
{
    UNUSED_PARAM(srcX0);
    UNUSED_PARAM(srcY0);
    UNUSED_PARAM(srcX1);
    UNUSED_PARAM(srcY1);
    UNUSED_PARAM(dstX0);
    UNUSED_PARAM(dstY0);
    UNUSED_PARAM(dstX1);
    UNUSED_PARAM(dstY1);
    UNUSED_PARAM(mask);
    UNUSED_PARAM(filter);
}

void GraphicsContextGLOpenGL::framebufferTextureLayer(GCGLenum target, GCGLenum attachment, PlatformGLObject texture, GCGLint level, GCGLint layer)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(attachment);
    UNUSED_PARAM(texture);
    UNUSED_PARAM(level);
    UNUSED_PARAM(layer);
}

void GraphicsContextGLOpenGL::invalidateFramebuffer(GCGLenum target, GCGLSpan<const GCGLenum> attachments)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(attachments);
}

void GraphicsContextGLOpenGL::invalidateSubFramebuffer(GCGLenum target, GCGLSpan<const GCGLenum> attachments, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(attachments);
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
}

void GraphicsContextGLOpenGL::readBuffer(GCGLenum src)
{
    UNUSED_PARAM(src);
}

#if HAVE(OPENGL_4) || HAVE(OPENGL_ES_3)
void GraphicsContextGLOpenGL::getInternalformativ(GCGLenum target, GCGLenum internalformat, GCGLenum pname, GCGLSpan<GCGLint> data)
{
#if USE(OPENGL_ES)
    if (!makeContextCurrent())
        return;

    ::glGetInternalformativ(target, internalformat, pname, data.bufSize, data.data);
#else
    UNUSED_PARAM(target);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(pname);
    UNUSED_PARAM(bufSize);
    UNUSED_PARAM(params);
#endif
}

void GraphicsContextGLOpenGL::renderbufferStorageMultisample(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    ::glRenderbufferStorageMultisample(target, samples, internalformat, width, height);
}

void GraphicsContextGLOpenGL::texStorage2D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    ::glTexStorage2D(target, levels, internalformat, width, height);
}

void GraphicsContextGLOpenGL::texStorage3D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth)
{
    if (!makeContextCurrent())
        return;

    ::glTexStorage3D(target, levels, internalformat, width, height, depth);
}
#else
void GraphicsContextGLOpenGL::getInternalformativ(GCGLenum, GCGLenum, GCGLenum, GCGLSpan<GCGLint>)
{
}

void GraphicsContextGLOpenGL::renderbufferStorageMultisample(GCGLenum, GCGLsizei, GCGLenum, GCGLsizei, GCGLsizei)
{
}

void GraphicsContextGLOpenGL::texStorage2D(GCGLenum, GCGLsizei, GCGLenum, GCGLsizei, GCGLsizei)
{
}

void GraphicsContextGLOpenGL::texStorage3D(GCGLenum, GCGLsizei, GCGLenum, GCGLsizei, GCGLsizei, GCGLsizei)
{
}
#endif

void GraphicsContextGLOpenGL::copyTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(zoffset);
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
}

GCGLint GraphicsContextGLOpenGL::getFragDataLocation(PlatformGLObject program, const String& name)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(name);

    return 0;
}

void GraphicsContextGLOpenGL::uniform1ui(GCGLint location, GCGLuint v0)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(v0);
}

void GraphicsContextGLOpenGL::uniform2ui(GCGLint location, GCGLuint v0, GCGLuint v1)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(v0);
    UNUSED_PARAM(v1);
}

void GraphicsContextGLOpenGL::uniform3ui(GCGLint location, GCGLuint v0, GCGLuint v1, GCGLuint v2)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(v0);
    UNUSED_PARAM(v1);
    UNUSED_PARAM(v2);
}

void GraphicsContextGLOpenGL::uniform4ui(GCGLint location, GCGLuint v0, GCGLuint v1, GCGLuint v2, GCGLuint v3)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(v0);
    UNUSED_PARAM(v1);
    UNUSED_PARAM(v2);
    UNUSED_PARAM(v3);
}

void GraphicsContextGLOpenGL::uniform1uiv(GCGLint location, GCGLSpan<const GCGLuint> data)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
}

void GraphicsContextGLOpenGL::uniform2uiv(GCGLint location, GCGLSpan<const GCGLuint> data)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
}

void GraphicsContextGLOpenGL::uniform3uiv(GCGLint location, GCGLSpan<const GCGLuint> data)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
}

void GraphicsContextGLOpenGL::uniform4uiv(GCGLint location, GCGLSpan<const GCGLuint> data)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
}

void GraphicsContextGLOpenGL::uniformMatrix2x3fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
}

void GraphicsContextGLOpenGL::uniformMatrix3x2fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
}

void GraphicsContextGLOpenGL::uniformMatrix2x4fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
}

void GraphicsContextGLOpenGL::uniformMatrix4x2fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
}

void GraphicsContextGLOpenGL::uniformMatrix3x4fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
}

void GraphicsContextGLOpenGL::uniformMatrix4x3fv(GCGLint location, GCGLboolean transpose, GCGLSpan<const GCGLfloat> data)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
}

void GraphicsContextGLOpenGL::vertexAttribI4i(GCGLuint index, GCGLint x, GCGLint y, GCGLint z, GCGLint w)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(z);
    UNUSED_PARAM(w);
}

void GraphicsContextGLOpenGL::vertexAttribI4iv(GCGLuint index, GCGLSpan<const GCGLint, 4> values)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(values);
}

void GraphicsContextGLOpenGL::vertexAttribI4ui(GCGLuint index, GCGLuint x, GCGLuint y, GCGLuint z, GCGLuint w)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(z);
    UNUSED_PARAM(w);
}

void GraphicsContextGLOpenGL::vertexAttribI4uiv(GCGLuint index, GCGLSpan<const GCGLuint, 4> values)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(values);
}

void GraphicsContextGLOpenGL::vertexAttribIPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLsizei stride, GCGLintptr offset)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(size);
    UNUSED_PARAM(type);
    UNUSED_PARAM(stride);
    UNUSED_PARAM(offset);
}

void GraphicsContextGLOpenGL::drawRangeElements(GCGLenum mode, GCGLuint start, GCGLuint end, GCGLsizei count, GCGLenum type, GCGLintptr offset)
{
    UNUSED_PARAM(mode);
    UNUSED_PARAM(start);
    UNUSED_PARAM(end);
    UNUSED_PARAM(count);
    UNUSED_PARAM(type);
    UNUSED_PARAM(offset);
}

void GraphicsContextGLOpenGL::drawBuffers(GCGLSpan<const GCGLenum> bufs)
{
    UNUSED_PARAM(bufs);
}

void GraphicsContextGLOpenGL::clearBufferiv(GCGLenum buffer, GCGLint drawbuffer, GCGLSpan<const GCGLint> values)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(drawbuffer);
    UNUSED_PARAM(values);
}

void GraphicsContextGLOpenGL::clearBufferuiv(GCGLenum buffer, GCGLint drawbuffer, GCGLSpan<const GCGLuint> values)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(drawbuffer);
    UNUSED_PARAM(values);
}

void GraphicsContextGLOpenGL::clearBufferfv(GCGLenum buffer, GCGLint drawbuffer, GCGLSpan<const GCGLfloat> values)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(drawbuffer);
    UNUSED_PARAM(values);
}

void GraphicsContextGLOpenGL::clearBufferfi(GCGLenum buffer, GCGLint drawbuffer, GCGLfloat depth, GCGLint stencil)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(drawbuffer);
    UNUSED_PARAM(depth);
    UNUSED_PARAM(stencil);
}

PlatformGLObject GraphicsContextGLOpenGL::createQuery()
{
    return 0;
}

void GraphicsContextGLOpenGL::deleteQuery(PlatformGLObject query)
{
    UNUSED_PARAM(query);
}

GCGLboolean GraphicsContextGLOpenGL::isQuery(PlatformGLObject query)
{
    UNUSED_PARAM(query);

    return false;
}

void GraphicsContextGLOpenGL::beginQuery(GCGLenum target, PlatformGLObject query)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(query);
}

void GraphicsContextGLOpenGL::endQuery(GCGLenum target)
{
    UNUSED_PARAM(target);
}

PlatformGLObject GraphicsContextGLOpenGL::getQuery(GCGLenum target, GCGLenum pname)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(pname);

    return 0;
}

GCGLuint GraphicsContextGLOpenGL::getQueryObjectui(PlatformGLObject query, GCGLenum pname)
{
    UNUSED_PARAM(query);
    UNUSED_PARAM(pname);
    return 0;
}

PlatformGLObject GraphicsContextGLOpenGL::createSampler()
{
    return 0;
}

void GraphicsContextGLOpenGL::deleteSampler(PlatformGLObject sampler)
{
    UNUSED_PARAM(sampler);
}

GCGLboolean GraphicsContextGLOpenGL::isSampler(PlatformGLObject sampler)
{
    UNUSED_PARAM(sampler);

    return false;
}

void GraphicsContextGLOpenGL::bindSampler(GCGLuint unit, PlatformGLObject sampler)
{
    UNUSED_PARAM(unit);
    UNUSED_PARAM(sampler);
}

void GraphicsContextGLOpenGL::samplerParameteri(PlatformGLObject sampler, GCGLenum pname, GCGLint param)
{
    UNUSED_PARAM(sampler);
    UNUSED_PARAM(pname);
    UNUSED_PARAM(param);
}

void GraphicsContextGLOpenGL::samplerParameterf(PlatformGLObject sampler, GCGLenum pname, GCGLfloat param)
{
    UNUSED_PARAM(sampler);
    UNUSED_PARAM(pname);
    UNUSED_PARAM(param);
}

GCGLfloat GraphicsContextGLOpenGL::getSamplerParameterf(PlatformGLObject sampler, GCGLenum pname)
{
    UNUSED_PARAM(sampler);
    UNUSED_PARAM(pname);
    return 0.f;
}

GCGLint GraphicsContextGLOpenGL::getSamplerParameteri(PlatformGLObject sampler, GCGLenum pname)

{
    UNUSED_PARAM(sampler);
    UNUSED_PARAM(pname);
    return 0;
}

GCGLsync GraphicsContextGLOpenGL::fenceSync(GCGLenum condition, GCGLbitfield flags)
{
    UNUSED_PARAM(condition);
    UNUSED_PARAM(flags);

    return 0;
}

GCGLboolean GraphicsContextGLOpenGL::isSync(GCGLsync sync)
{
    UNUSED_PARAM(sync);

    return false;
}

void GraphicsContextGLOpenGL::deleteSync(GCGLsync sync)
{
    UNUSED_PARAM(sync);
}

GCGLenum GraphicsContextGLOpenGL::clientWaitSync(GCGLsync sync, GCGLbitfield flags, GCGLuint64 timeout)
{
    UNUSED_PARAM(sync);
    UNUSED_PARAM(flags);
    UNUSED_PARAM(timeout);

    return 0;
}

void GraphicsContextGLOpenGL::waitSync(GCGLsync sync, GCGLbitfield flags, GCGLint64 timeout)
{
    UNUSED_PARAM(sync);
    UNUSED_PARAM(flags);
    UNUSED_PARAM(timeout);
}

GCGLint GraphicsContextGLOpenGL::getSynci(GCGLsync sync, GCGLenum pname)
{
    UNUSED_PARAM(sync);
    UNUSED_PARAM(pname);
    return 0;
}

PlatformGLObject GraphicsContextGLOpenGL::createTransformFeedback()
{
    return 0;
}

void GraphicsContextGLOpenGL::deleteTransformFeedback(PlatformGLObject id)
{
    UNUSED_PARAM(id);
}

GCGLboolean GraphicsContextGLOpenGL::isTransformFeedback(PlatformGLObject id)
{
    UNUSED_PARAM(id);

    return false;
}

void GraphicsContextGLOpenGL::bindTransformFeedback(GCGLenum target, PlatformGLObject id)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(id);
}

void GraphicsContextGLOpenGL::beginTransformFeedback(GCGLenum primitiveMode)
{
    UNUSED_PARAM(primitiveMode);
}

void GraphicsContextGLOpenGL::endTransformFeedback()
{
}

void GraphicsContextGLOpenGL::transformFeedbackVaryings(PlatformGLObject program, const Vector<String>& varyings, GCGLenum bufferMode)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(varyings);
    UNUSED_PARAM(bufferMode);
}

void GraphicsContextGLOpenGL::getTransformFeedbackVarying(PlatformGLObject program, GCGLuint index, ActiveInfo&)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(index);
}

void GraphicsContextGLOpenGL::pauseTransformFeedback()
{
}

void GraphicsContextGLOpenGL::resumeTransformFeedback()
{
}

void GraphicsContextGLOpenGL::bindBufferBase(GCGLenum target, GCGLuint index, PlatformGLObject buffer)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(index);
    UNUSED_PARAM(buffer);
}

void GraphicsContextGLOpenGL::bindBufferRange(GCGLenum target, GCGLuint index, PlatformGLObject buffer, GCGLintptr offset, GCGLsizeiptr size)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(index);
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(offset);
    UNUSED_PARAM(size);
}

Vector<GCGLuint> GraphicsContextGLOpenGL::getUniformIndices(PlatformGLObject program, const Vector<String>& uniformNames)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformNames);

    return { };
}

Vector<GCGLint> GraphicsContextGLOpenGL::getActiveUniforms(PlatformGLObject program, const Vector<GCGLuint>& uniformIndices, GCGLenum pname)
{
    Vector<GCGLint> result(uniformIndices.size(), 0);
#if HAVE(OPENGL_4) || HAVE(OPENGL_ES_3)
    ASSERT(program);
    if (!makeContextCurrent())
        return result;

    ::glGetActiveUniformsiv(program, uniformIndices.size(), uniformIndices.data(), pname, result.data());
#else
    UNUSED_PARAM(program);
    UNUSED_PARAM(pname);
#endif
    return result;
}

GCGLuint GraphicsContextGLOpenGL::getUniformBlockIndex(PlatformGLObject program, const String& uniformBlockName)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformBlockName);
    return 0;
}

String GraphicsContextGLOpenGL::getActiveUniformBlockName(PlatformGLObject program, GCGLuint uniformBlockIndex)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformBlockIndex);
    return emptyString();
}

void GraphicsContextGLOpenGL::uniformBlockBinding(PlatformGLObject program, GCGLuint uniformBlockIndex, GCGLuint uniformBlockBinding)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformBlockIndex);
    UNUSED_PARAM(uniformBlockBinding);
}

void GraphicsContextGLOpenGL::readnPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(offset);
}

void GraphicsContextGLOpenGL::getActiveUniformBlockiv(GCGLuint program, GCGLuint uniformBlockIndex, GCGLenum pname, GCGLSpan<GCGLint> params)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformBlockIndex);
    UNUSED_PARAM(pname);
    UNUSED_PARAM(params);
}

void GraphicsContextGLOpenGL::texImage2D(GCGLenum, GCGLint, GCGLenum, GCGLsizei, GCGLsizei, GCGLint, GCGLenum, GCGLenum , GCGLintptr)
{
}

void GraphicsContextGLOpenGL::texSubImage2D(GCGLenum, GCGLint, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, GCGLintptr)
{
}

void GraphicsContextGLOpenGL::compressedTexImage2D(GCGLenum, GCGLint, GCGLenum, GCGLsizei, GCGLsizei, GCGLint, GCGLsizei, GCGLintptr)
{
}

void GraphicsContextGLOpenGL::compressedTexSubImage2D(GCGLenum, GCGLint, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLenum, GCGLsizei, GCGLintptr)
{
}

void GraphicsContextGLOpenGL::texImage3D(GCGLenum, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLsizei, GCGLint, GCGLenum, GCGLenum, GCGLSpan<const GCGLvoid>)
{
}

void GraphicsContextGLOpenGL::texImage3D(GCGLenum, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLsizei, GCGLint, GCGLenum, GCGLenum, GCGLintptr)
{
}

void GraphicsContextGLOpenGL::texSubImage3D(GCGLenum, GCGLint, GCGLint, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, GCGLSpan<const GCGLvoid>)
{
}

void GraphicsContextGLOpenGL::texSubImage3D(GCGLenum, GCGLint, GCGLint, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, GCGLintptr)
{
}

void GraphicsContextGLOpenGL::compressedTexImage3D(GCGLenum, GCGLint, GCGLenum, GCGLsizei, GCGLsizei, GCGLsizei, GCGLint, GCGLsizei, GCGLSpan<const GCGLvoid>)
{
}

void GraphicsContextGLOpenGL::compressedTexImage3D(GCGLenum, GCGLint, GCGLenum, GCGLsizei, GCGLsizei, GCGLsizei, GCGLint, GCGLsizei, GCGLintptr)
{
}

void GraphicsContextGLOpenGL::compressedTexSubImage3D(GCGLenum, GCGLint, GCGLint, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLsizei, GCGLenum, GCGLsizei, GCGLSpan<const GCGLvoid>)
{
}

void GraphicsContextGLOpenGL::compressedTexSubImage3D(GCGLenum, GCGLint, GCGLint, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLsizei, GCGLenum, GCGLsizei, GCGLintptr)
{
}

void GraphicsContextGLOpenGL::multiDrawArraysANGLE(GCGLenum, GCGLSpan<const GCGLint>, GCGLSpan<const GCGLsizei>, GCGLsizei)
{
    synthesizeGLError(GraphicsContextGL::INVALID_OPERATION);
}

void GraphicsContextGLOpenGL::multiDrawArraysInstancedANGLE(GCGLenum, GCGLSpan<const GCGLint>, GCGLSpan<const GCGLsizei>, GCGLSpan<const GCGLsizei>, GCGLsizei)
{
    synthesizeGLError(GraphicsContextGL::INVALID_OPERATION);
}

void GraphicsContextGLOpenGL::multiDrawElementsANGLE(GCGLenum, GCGLSpan<const GCGLsizei>, GCGLenum, GCGLSpan<const GCGLint>, GCGLsizei)
{
    synthesizeGLError(GraphicsContextGL::INVALID_OPERATION);
}

void GraphicsContextGLOpenGL::multiDrawElementsInstancedANGLE(GCGLenum, GCGLSpan<const GCGLsizei>, GCGLenum, GCGLSpan<const GCGLint>, GCGLSpan<const GCGLsizei>, GCGLsizei)
{
    synthesizeGLError(GraphicsContextGL::INVALID_OPERATION);
}

bool GraphicsContextGLOpenGL::supportsExtension(const String& name)
{
    return getExtensions().supports(name);
}

void GraphicsContextGLOpenGL::ensureExtensionEnabled(const String& name)
{
    getExtensions().ensureEnabled(name);
}

bool GraphicsContextGLOpenGL::isExtensionEnabled(const String& name)
{
    return getExtensions().isEnabled(name);
}

GLint GraphicsContextGLOpenGL::getGraphicsResetStatusARB()
{
    return getExtensions().getGraphicsResetStatusARB();
}

void GraphicsContextGLOpenGL::drawBuffersEXT(GCGLSpan<const GCGLenum> buffers)
{
    return getExtensions().drawBuffersEXT(buffers);
}

String GraphicsContextGLOpenGL::getTranslatedShaderSourceANGLE(PlatformGLObject shader)
{
    return getExtensions().getTranslatedShaderSourceANGLE(shader);
}

bool GraphicsContextGLOpenGL::texImage2DResourceSafe(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLint unpackAlignment)
{
    ASSERT(unpackAlignment == 1 || unpackAlignment == 2 || unpackAlignment == 4 || unpackAlignment == 8);
    UniqueArray<unsigned char> zero;
    unsigned size = 0;
    if (width > 0 && height > 0) {
        PixelStoreParams params;
        params.alignment = unpackAlignment;
        GCGLenum error = computeImageSizeInBytes(format, type, width, height, 1, params, &size, nullptr, nullptr);
        if (error != GraphicsContextGL::NO_ERROR) {
            synthesizeGLError(error);
            return false;
        }
        zero = makeUniqueArray<unsigned char>(size);
        if (!zero) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE);
            return false;
        }
        memset(zero.get(), 0, size);
    }
    texImage2D(target, level, internalformat, width, height, border, format, type, makeGCGLSpan(zero.get(), size));
    return true;
}

void GraphicsContextGLOpenGL::paintRenderingResultsToCanvas(ImageBuffer& imageBuffer)
{
    if (!makeContextCurrent())
        return;
    if (getInternalFramebufferSize().isEmpty())
        return;
    auto pixelBuffer = readRenderingResults();
    if (!pixelBuffer)
        return;
    paintToCanvas(contextAttributes(), WTFMove(*pixelBuffer), imageBuffer.backendSize(), imageBuffer.context());
}

void GraphicsContextGLOpenGL::paintCompositedResultsToCanvas(ImageBuffer& imageBuffer)
{
    if (!makeContextCurrent())
        return;
    if (getInternalFramebufferSize().isEmpty())
        return;
    auto pixelBuffer = readCompositedResults();
    if (!pixelBuffer)
        return;
    paintToCanvas(contextAttributes(), WTFMove(*pixelBuffer), imageBuffer.backendSize(), imageBuffer.context());
}

std::optional<PixelBuffer> GraphicsContextGLOpenGL::paintRenderingResultsToPixelBuffer()
{
    // Reading premultiplied alpha would involve unpremultiplying, which is lossy.
    if (contextAttributes().premultipliedAlpha)
        return std::nullopt;
    auto results = readRenderingResultsForPainting();
    if (results && !results->size().isEmpty()) {
        ASSERT(results->format().pixelFormat == PixelFormat::RGBA8 || results->format().pixelFormat == PixelFormat::BGRA8);
        // FIXME: Make PixelBufferConversions support negative rowBytes and in-place conversions.
        const auto size = results->size();
        const size_t rowStride = size.width() * 4;
        uint8_t* top = results->data().data();
        uint8_t* bottom = top + (size.height() - 1) * rowStride;
        std::unique_ptr<uint8_t[]> temp(new uint8_t[rowStride]);
        for (; top < bottom; top += rowStride, bottom -= rowStride) {
            memcpy(temp.get(), bottom, rowStride);
            memcpy(bottom, top, rowStride);
            memcpy(top, temp.get(), rowStride);
        }
    }
    return results;
}

std::optional<PixelBuffer> GraphicsContextGLOpenGL::readRenderingResultsForPainting()
{
    if (!makeContextCurrent())
        return std::nullopt;
    if (getInternalFramebufferSize().isEmpty())
        return std::nullopt;
    return readRenderingResults();
}

std::optional<PixelBuffer> GraphicsContextGLOpenGL::readCompositedResultsForPainting()
{
    if (!makeContextCurrent())
        return std::nullopt;
    if (getInternalFramebufferSize().isEmpty())
        return std::nullopt;
    return readCompositedResults();
}

}

#endif // ENABLE(WEBGL) && !USE(ANGLE)
