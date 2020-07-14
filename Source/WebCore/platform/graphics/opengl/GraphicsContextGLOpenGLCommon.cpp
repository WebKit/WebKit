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

#if ENABLE(GRAPHICS_CONTEXT_GL) && (USE(OPENGL) || USE(OPENGL_ES))

#if PLATFORM(IOS_FAMILY)
#include "GraphicsContextGLOpenGLESIOS.h"
#endif

#if !PLATFORM(COCOA) && USE(OPENGL_ES)
#include "ExtensionsGLOpenGLES.h"
#else
#include "ExtensionsGLOpenGL.h"
#endif
#include "ANGLEWebKitBridge.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Logging.h"
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

#if PLATFORM(COCOA)

#if USE(OPENGL_ES)
#import <OpenGLES/ES2/glext.h>
#import <OpenGLES/ES3/gl.h>
// From <OpenGLES/glext.h>
#define GL_RGBA32F_ARB                      0x8814
#define GL_RGB32F_ARB                       0x8815
#else
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#include <OpenGL/gl.h>
#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>
#undef GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#endif

#else

#if USE(LIBEPOXY)
#include "EpoxyShims.h"
#elif USE(OPENGL_ES)
#include "OpenGLESShims.h"
#elif PLATFORM(GTK) || PLATFORM(WIN)
#include "OpenGLShims.h"
#endif

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

void GraphicsContextGLOpenGL::validateDepthStencil(const char* packedDepthStencilExtension)
{
    auto attrs = contextAttributes();

    ExtensionsGL& extensions = getExtensions();
    if (attrs.stencil) {
        if (extensions.supports(packedDepthStencilExtension)) {
            extensions.ensureEnabled(packedDepthStencilExtension);
            // Force depth if stencil is true.
            attrs.depth = true;
        } else
            attrs.stencil = false;
        setContextAttributes(attrs);
    }
    if (attrs.antialias && !m_isForWebGL2) {
        if (!extensions.supports("GL_ANGLE_framebuffer_multisample")) {
            attrs.antialias = false;
            setContextAttributes(attrs);
        } else
            extensions.ensureEnabled("GL_ANGLE_framebuffer_multisample");
    }
}

void GraphicsContextGLOpenGL::paintRenderingResultsToCanvas(ImageBuffer* imageBuffer)
{
    Checked<int, RecordOverflow> rowBytes = Checked<int, RecordOverflow>(m_currentWidth) * 4;
    if (rowBytes.hasOverflowed())
        return;

    Checked<int, RecordOverflow> totalBytesChecked = rowBytes * m_currentHeight;
    if (totalBytesChecked.hasOverflowed())
        return;
    int totalBytes = totalBytesChecked.unsafeGet();

    auto pixels = makeUniqueArray<unsigned char>(totalBytes);
    if (!pixels)
        return;

    readRenderingResults(pixels.get(), totalBytes);

    if (!contextAttributes().premultipliedAlpha) {
        for (int i = 0; i < totalBytes; i += 4) {
            // Premultiply alpha.
            pixels[i + 0] = std::min(255, pixels[i + 0] * pixels[i + 3] / 255);
            pixels[i + 1] = std::min(255, pixels[i + 1] * pixels[i + 3] / 255);
            pixels[i + 2] = std::min(255, pixels[i + 2] * pixels[i + 3] / 255);
        }
    }

    paintToCanvas(pixels.get(), IntSize(m_currentWidth, m_currentHeight), imageBuffer->backendSize(), imageBuffer->context());

#if PLATFORM(COCOA) && USE(OPENGL_ES)
    presentRenderbuffer();
#endif
}

bool GraphicsContextGLOpenGL::paintCompositedResultsToCanvas(ImageBuffer*)
{
    // Not needed at the moment, so return that nothing was done.
    return false;
}

RefPtr<ImageData> GraphicsContextGLOpenGL::paintRenderingResultsToImageData()
{
    // Reading premultiplied alpha would involve unpremultiplying, which is
    // lossy.
    if (contextAttributes().premultipliedAlpha)
        return nullptr;

    auto imageData = ImageData::create(IntSize(m_currentWidth, m_currentHeight));
    unsigned char* pixels = imageData->data()->data();
    Checked<int, RecordOverflow> totalBytesChecked = 4 * Checked<int, RecordOverflow>(m_currentWidth) * Checked<int, RecordOverflow>(m_currentHeight);
    if (totalBytesChecked.hasOverflowed())
        return imageData;
    int totalBytes = totalBytesChecked.unsafeGet();

    readRenderingResults(pixels, totalBytes);

    // Convert to RGBA.
    for (int i = 0; i < totalBytes; i += 4)
        std::swap(pixels[i], pixels[i + 2]);

    return imageData;
}

void GraphicsContextGLOpenGL::prepareTexture()
{
    if (m_layerComposited)
        return;

    makeContextCurrent();

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

void GraphicsContextGLOpenGL::readRenderingResults(unsigned char *pixels, int pixelsSize)
{
    if (pixelsSize < m_currentWidth * m_currentHeight * 4)
        return;

    makeContextCurrent();

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

    GLint packAlignment = 4;
    bool mustRestorePackAlignment = false;
    ::glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
    if (packAlignment > 4) {
        ::glPixelStorei(GL_PACK_ALIGNMENT, 4);
        mustRestorePackAlignment = true;
    }

    readPixelsAndConvertToBGRAIfNecessary(0, 0, m_currentWidth, m_currentHeight, pixels);

    if (mustRestorePackAlignment)
        ::glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);

    if (mustRestoreFBO)
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_state.boundDrawFBO);
}

void GraphicsContextGLOpenGL::reshape(int width, int height)
{
    if (!platformGraphicsContextGL())
        return;

    if (width == m_currentWidth && height == m_currentHeight)
        return;

    ASSERT(width >= 0 && height >= 0);
    if (width < 0 || height < 0)
        return;

    markContextChanged();

    m_currentWidth = width;
    m_currentHeight = height;

    makeContextCurrent();
    validateAttributes();

    TemporaryOpenGLSetting scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
    TemporaryOpenGLSetting scopedDither(GL_DITHER, GL_FALSE);
    
    bool mustRestoreFBO = reshapeFBOs(IntSize(width, height));

    // Initialize renderbuffers to 0.
    GLfloat clearColor[] = {0, 0, 0, 0}, clearDepth = 0;
    GLint clearStencil = 0;
    GLboolean colorMask[] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE}, depthMask = GL_TRUE;
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
        const std::string& mappedName = entry.value.mappedName;
        vertexSymbolPrecisionMap.add(String(mappedName.c_str(), mappedName.length()), entry.value.precision);
    }

    for (const auto& entry : fragmentEntry.uniformMap) {
        const std::string& mappedName = entry.value.mappedName;
        const auto& vertexSymbol = vertexSymbolPrecisionMap.find(String(mappedName.c_str(), mappedName.length()));
        if (vertexSymbol != vertexSymbolPrecisionMap.end() && vertexSymbol->value != entry.value.precision)
            return false;
    }

    return true;
}

IntSize GraphicsContextGLOpenGL::getInternalFramebufferSize() const
{
    return IntSize(m_currentWidth, m_currentHeight);
}

void GraphicsContextGLOpenGL::activeTexture(GCGLenum texture)
{
    makeContextCurrent();
    m_state.activeTextureUnit = texture;
    ::glActiveTexture(texture);
}

void GraphicsContextGLOpenGL::attachShader(PlatformGLObject program, PlatformGLObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    makeContextCurrent();
    m_shaderProgramSymbolCountMap.remove(program);
    ::glAttachShader(program, shader);
}

void GraphicsContextGLOpenGL::bindAttribLocation(PlatformGLObject program, GCGLuint index, const String& name)
{
    ASSERT(program);
    makeContextCurrent();

    String mappedName = mappedSymbolName(program, SHADER_SYMBOL_TYPE_ATTRIBUTE, name);
    LOG(WebGL, "::bindAttribLocation is mapping %s to %s", name.utf8().data(), mappedName.utf8().data());
    ::glBindAttribLocation(program, index, mappedName.utf8().data());
}

void GraphicsContextGLOpenGL::bindBuffer(GCGLenum target, PlatformGLObject buffer)
{
    makeContextCurrent();
    ::glBindBuffer(target, buffer);
}

void GraphicsContextGLOpenGL::bindFramebuffer(GCGLenum target, PlatformGLObject buffer)
{
    makeContextCurrent();
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
    makeContextCurrent();
    ::glBindRenderbufferEXT(target, renderbuffer);
}


void GraphicsContextGLOpenGL::bindTexture(GCGLenum target, PlatformGLObject texture)
{
    makeContextCurrent();
    m_state.setBoundTexture(m_state.activeTextureUnit, texture, target);
    ::glBindTexture(target, texture);
}

void GraphicsContextGLOpenGL::blendColor(GCGLclampf red, GCGLclampf green, GCGLclampf blue, GCGLclampf alpha)
{
    makeContextCurrent();
    ::glBlendColor(red, green, blue, alpha);
}

void GraphicsContextGLOpenGL::blendEquation(GCGLenum mode)
{
    makeContextCurrent();
    ::glBlendEquation(mode);
}

void GraphicsContextGLOpenGL::blendEquationSeparate(GCGLenum modeRGB, GCGLenum modeAlpha)
{
    makeContextCurrent();
    ::glBlendEquationSeparate(modeRGB, modeAlpha);
}


void GraphicsContextGLOpenGL::blendFunc(GCGLenum sfactor, GCGLenum dfactor)
{
    makeContextCurrent();
    ::glBlendFunc(sfactor, dfactor);
}       

void GraphicsContextGLOpenGL::blendFuncSeparate(GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha)
{
    makeContextCurrent();
    ::glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContextGLOpenGL::bufferData(GCGLenum target, GCGLsizeiptr size, GCGLenum usage)
{
    makeContextCurrent();
    ::glBufferData(target, size, 0, usage);
}

void GraphicsContextGLOpenGL::bufferData(GCGLenum target, GCGLsizeiptr size, const void* data, GCGLenum usage)
{
    makeContextCurrent();
    ::glBufferData(target, size, data, usage);
}

void GraphicsContextGLOpenGL::bufferSubData(GCGLenum target, GCGLintptr offset, GCGLsizeiptr size, const void* data)
{
    makeContextCurrent();
    ::glBufferSubData(target, offset, size, data);
}

GCGLenum GraphicsContextGLOpenGL::checkFramebufferStatus(GCGLenum target)
{
    makeContextCurrent();
    return ::glCheckFramebufferStatusEXT(target);
}

void GraphicsContextGLOpenGL::clearColor(GCGLclampf r, GCGLclampf g, GCGLclampf b, GCGLclampf a)
{
    makeContextCurrent();
    ::glClearColor(r, g, b, a);
}

void GraphicsContextGLOpenGL::clear(GCGLbitfield mask)
{
    makeContextCurrent();
    ::glClear(mask);
    checkGPUStatus();
}

void GraphicsContextGLOpenGL::clearStencil(GCGLint s)
{
    makeContextCurrent();
    ::glClearStencil(s);
}

void GraphicsContextGLOpenGL::colorMask(GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha)
{
    makeContextCurrent();
    ::glColorMask(red, green, blue, alpha);
}

void GraphicsContextGLOpenGL::compileShader(PlatformGLObject shader)
{
    ASSERT(shader);
    makeContextCurrent();

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
    makeContextCurrent();

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
    makeContextCurrent();
    auto attrs = contextAttributes();

    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (attrs.antialias && m_state.boundDrawFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_fbo);
    }
    ::glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
    if (attrs.antialias && m_state.boundDrawFBO == m_multisampleFBO)
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_multisampleFBO);

#if PLATFORM(MAC) && USE(OPENGL)
    if (getExtensions().isIntel())
        m_needsFlushBeforeDeleteTextures = true;
#endif
}

void GraphicsContextGLOpenGL::copyTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    makeContextCurrent();
    auto attrs = contextAttributes();

    ASSERT(m_state.boundReadFBO == m_state.boundDrawFBO);
    if (attrs.antialias && m_state.boundDrawFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_fbo);
    }
    ::glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    if (attrs.antialias && m_state.boundDrawFBO == m_multisampleFBO)
        ::glBindFramebufferEXT(GraphicsContextGL::FRAMEBUFFER, m_multisampleFBO);

#if PLATFORM(MAC) && USE(OPENGL)
    if (getExtensions().isIntel())
        m_needsFlushBeforeDeleteTextures = true;
#endif
}

void GraphicsContextGLOpenGL::cullFace(GCGLenum mode)
{
    makeContextCurrent();
    ::glCullFace(mode);
}

void GraphicsContextGLOpenGL::depthFunc(GCGLenum func)
{
    makeContextCurrent();
    ::glDepthFunc(func);
}

void GraphicsContextGLOpenGL::depthMask(GCGLboolean flag)
{
    makeContextCurrent();
    ::glDepthMask(flag);
}

void GraphicsContextGLOpenGL::detachShader(PlatformGLObject program, PlatformGLObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    makeContextCurrent();
    m_shaderProgramSymbolCountMap.remove(program);
    ::glDetachShader(program, shader);
}

void GraphicsContextGLOpenGL::disable(GCGLenum cap)
{
    makeContextCurrent();
    ::glDisable(cap);
}

void GraphicsContextGLOpenGL::disableVertexAttribArray(GCGLuint index)
{
    makeContextCurrent();
    ::glDisableVertexAttribArray(index);
}

void GraphicsContextGLOpenGL::drawArrays(GCGLenum mode, GCGLint first, GCGLsizei count)
{
    makeContextCurrent();
    ::glDrawArrays(mode, first, count);
    checkGPUStatus();
}

void GraphicsContextGLOpenGL::drawElements(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset)
{
    makeContextCurrent();
    ::glDrawElements(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
    checkGPUStatus();
}

void GraphicsContextGLOpenGL::enable(GCGLenum cap)
{
    makeContextCurrent();
    ::glEnable(cap);
}

void GraphicsContextGLOpenGL::enableVertexAttribArray(GCGLuint index)
{
    makeContextCurrent();
    ::glEnableVertexAttribArray(index);
}

void GraphicsContextGLOpenGL::finish()
{
    makeContextCurrent();
    ::glFinish();
#if PLATFORM(MAC) && USE(OPENGL)
    m_needsFlushBeforeDeleteTextures = false;
#endif
}

void GraphicsContextGLOpenGL::flush()
{
    makeContextCurrent();
    ::glFlush();
#if PLATFORM(MAC) && USE(OPENGL)
    m_needsFlushBeforeDeleteTextures = false;
#endif
}

void GraphicsContextGLOpenGL::framebufferRenderbuffer(GCGLenum target, GCGLenum attachment, GCGLenum renderbuffertarget, PlatformGLObject buffer)
{
    makeContextCurrent();
    ::glFramebufferRenderbufferEXT(target, attachment, renderbuffertarget, buffer);
}

void GraphicsContextGLOpenGL::framebufferTexture2D(GCGLenum target, GCGLenum attachment, GCGLenum textarget, PlatformGLObject texture, GCGLint level)
{
    makeContextCurrent();
    ::glFramebufferTexture2DEXT(target, attachment, textarget, texture, level);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContextGLOpenGL::frontFace(GCGLenum mode)
{
    makeContextCurrent();
    ::glFrontFace(mode);
}

void GraphicsContextGLOpenGL::generateMipmap(GCGLenum target)
{
    makeContextCurrent();
    ::glGenerateMipmap(target);
}

bool GraphicsContextGLOpenGL::getActiveAttribImpl(PlatformGLObject program, GCGLuint index, ActiveInfo& info)
{
    if (!program) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }
    makeContextCurrent();
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

    makeContextCurrent();
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
    makeContextCurrent();
    ::glGetAttachedShaders(program, maxCount, count, shaders);
}

static String generateHashedName(const String& name)
{
    if (name.isEmpty())
        return name;
    uint64_t number = nameHashForShader(name.utf8().data(), name.length());
    return makeString("webgl_", hex(number, Lowercase));
}

Optional<String> GraphicsContextGLOpenGL::mappedSymbolInShaderSourceMap(PlatformGLObject shader, ANGLEShaderSymbolType symbolType, const String& name)
{
    auto result = m_shaderSourceMap.find(shader);
    if (result == m_shaderSourceMap.end())
        return WTF::nullopt;

    const auto& symbolMap = result->value.symbolMap(symbolType);
    auto symbolEntry = symbolMap.find(name);
    if (symbolEntry == symbolMap.end())
        return WTF::nullopt;

    auto& mappedName = symbolEntry->value.mappedName;
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

Optional<String> GraphicsContextGLOpenGL::originalSymbolInShaderSourceMap(PlatformGLObject shader, ANGLEShaderSymbolType symbolType, const String& name)
{
    auto result = m_shaderSourceMap.find(shader);
    if (result == m_shaderSourceMap.end())
        return WTF::nullopt;

    const auto& symbolMap = result->value.symbolMap(symbolType);
    for (const auto& symbolEntry : symbolMap) {
        if (name == symbolEntry.value.mappedName.c_str())
            return symbolEntry.key;
    }
    return WTF::nullopt;
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
                if (name == symbolEntry.value.mappedName.c_str())
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

    makeContextCurrent();

    String mappedName = mappedSymbolName(program, SHADER_SYMBOL_TYPE_ATTRIBUTE, name);
    LOG(WebGL, "::glGetAttribLocation is mapping %s to %s", name.utf8().data(), mappedName.utf8().data());
    return ::glGetAttribLocation(program, mappedName.utf8().data());
}

int GraphicsContextGLOpenGL::getAttribLocationDirect(PlatformGLObject program, const String& name)
{
    if (!program)
        return -1;

    makeContextCurrent();

    return ::glGetAttribLocation(program, name.utf8().data());
}

bool GraphicsContextGLOpenGL::moveErrorsToSyntheticErrorList()
{
    makeContextCurrent();
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

    makeContextCurrent();
    return ::glGetError();
}

String GraphicsContextGLOpenGL::getString(GCGLenum name)
{
    makeContextCurrent();
    return String(reinterpret_cast<const char*>(::glGetString(name)));
}

void GraphicsContextGLOpenGL::hint(GCGLenum target, GCGLenum mode)
{
    makeContextCurrent();
    ::glHint(target, mode);
}

GCGLboolean GraphicsContextGLOpenGL::isBuffer(PlatformGLObject buffer)
{
    if (!buffer)
        return GL_FALSE;

    makeContextCurrent();
    return ::glIsBuffer(buffer);
}

GCGLboolean GraphicsContextGLOpenGL::isEnabled(GCGLenum cap)
{
    makeContextCurrent();
    return ::glIsEnabled(cap);
}

GCGLboolean GraphicsContextGLOpenGL::isFramebuffer(PlatformGLObject framebuffer)
{
    if (!framebuffer)
        return GL_FALSE;

    makeContextCurrent();
    return ::glIsFramebufferEXT(framebuffer);
}

GCGLboolean GraphicsContextGLOpenGL::isProgram(PlatformGLObject program)
{
    if (!program)
        return GL_FALSE;

    makeContextCurrent();
    return ::glIsProgram(program);
}

GCGLboolean GraphicsContextGLOpenGL::isRenderbuffer(PlatformGLObject renderbuffer)
{
    if (!renderbuffer)
        return GL_FALSE;

    makeContextCurrent();
    return ::glIsRenderbufferEXT(renderbuffer);
}

GCGLboolean GraphicsContextGLOpenGL::isShader(PlatformGLObject shader)
{
    if (!shader)
        return GL_FALSE;

    makeContextCurrent();
    return ::glIsShader(shader);
}

GCGLboolean GraphicsContextGLOpenGL::isTexture(PlatformGLObject texture)
{
    if (!texture)
        return GL_FALSE;

    makeContextCurrent();
    return ::glIsTexture(texture);
}

void GraphicsContextGLOpenGL::lineWidth(GCGLfloat width)
{
    makeContextCurrent();
    ::glLineWidth(width);
}

void GraphicsContextGLOpenGL::linkProgram(PlatformGLObject program)
{
    ASSERT(program);
    makeContextCurrent();

    GCGLsizei count = 0;
    PlatformGLObject shaders[2] = { };
    getAttachedShaders(program, 2, &count, shaders);

    if (count == 2)
        m_linkedShaderMap.set(program, std::make_pair(shaders[0], shaders[1]));

    ::glLinkProgram(program);
}

void GraphicsContextGLOpenGL::pixelStorei(GCGLenum pname, GCGLint param)
{
    makeContextCurrent();
    ::glPixelStorei(pname, param);
}

void GraphicsContextGLOpenGL::polygonOffset(GCGLfloat factor, GCGLfloat units)
{
    makeContextCurrent();
    ::glPolygonOffset(factor, units);
}

void GraphicsContextGLOpenGL::sampleCoverage(GCGLclampf value, GCGLboolean invert)
{
    makeContextCurrent();
    ::glSampleCoverage(value, invert);
}

void GraphicsContextGLOpenGL::scissor(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    makeContextCurrent();
    ::glScissor(x, y, width, height);
}

void GraphicsContextGLOpenGL::shaderSource(PlatformGLObject shader, const String& string)
{
    ASSERT(shader);

    makeContextCurrent();

    ShaderSourceEntry entry;

    entry.source = string;

    m_shaderSourceMap.set(shader, entry);
}

void GraphicsContextGLOpenGL::stencilFunc(GCGLenum func, GCGLint ref, GCGLuint mask)
{
    makeContextCurrent();
    ::glStencilFunc(func, ref, mask);
}

void GraphicsContextGLOpenGL::stencilFuncSeparate(GCGLenum face, GCGLenum func, GCGLint ref, GCGLuint mask)
{
    makeContextCurrent();
    ::glStencilFuncSeparate(face, func, ref, mask);
}

void GraphicsContextGLOpenGL::stencilMask(GCGLuint mask)
{
    makeContextCurrent();
    ::glStencilMask(mask);
}

void GraphicsContextGLOpenGL::stencilMaskSeparate(GCGLenum face, GCGLuint mask)
{
    makeContextCurrent();
    ::glStencilMaskSeparate(face, mask);
}

void GraphicsContextGLOpenGL::stencilOp(GCGLenum fail, GCGLenum zfail, GCGLenum zpass)
{
    makeContextCurrent();
    ::glStencilOp(fail, zfail, zpass);
}

void GraphicsContextGLOpenGL::stencilOpSeparate(GCGLenum face, GCGLenum fail, GCGLenum zfail, GCGLenum zpass)
{
    makeContextCurrent();
    ::glStencilOpSeparate(face, fail, zfail, zpass);
}

void GraphicsContextGLOpenGL::texParameterf(GCGLenum target, GCGLenum pname, GCGLfloat value)
{
    makeContextCurrent();
    ::glTexParameterf(target, pname, value);
}

void GraphicsContextGLOpenGL::texParameteri(GCGLenum target, GCGLenum pname, GCGLint value)
{
    makeContextCurrent();
    ::glTexParameteri(target, pname, value);
}

void GraphicsContextGLOpenGL::uniform1f(GCGLint location, GCGLfloat v0)
{
    makeContextCurrent();
    ::glUniform1f(location, v0);
}

void GraphicsContextGLOpenGL::uniform1fv(GCGLint location, GCGLsizei size, const GCGLfloat* array)
{
    makeContextCurrent();
    ::glUniform1fv(location, size, array);
}

void GraphicsContextGLOpenGL::uniform2f(GCGLint location, GCGLfloat v0, GCGLfloat v1)
{
    makeContextCurrent();
    ::glUniform2f(location, v0, v1);
}

void GraphicsContextGLOpenGL::uniform2fv(GCGLint location, GCGLsizei size, const GCGLfloat* array)
{
    // FIXME: length needs to be a multiple of 2.
    makeContextCurrent();
    ::glUniform2fv(location, size, array);
}

void GraphicsContextGLOpenGL::uniform3f(GCGLint location, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2)
{
    makeContextCurrent();
    ::glUniform3f(location, v0, v1, v2);
}

void GraphicsContextGLOpenGL::uniform3fv(GCGLint location, GCGLsizei size, const GCGLfloat* array)
{
    // FIXME: length needs to be a multiple of 3.
    makeContextCurrent();
    ::glUniform3fv(location, size, array);
}

void GraphicsContextGLOpenGL::uniform4f(GCGLint location, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2, GCGLfloat v3)
{
    makeContextCurrent();
    ::glUniform4f(location, v0, v1, v2, v3);
}

void GraphicsContextGLOpenGL::uniform4fv(GCGLint location, GCGLsizei size, const GCGLfloat* array)
{
    // FIXME: length needs to be a multiple of 4.
    makeContextCurrent();
    ::glUniform4fv(location, size, array);
}

void GraphicsContextGLOpenGL::uniform1i(GCGLint location, GCGLint v0)
{
    makeContextCurrent();
    ::glUniform1i(location, v0);
}

void GraphicsContextGLOpenGL::uniform1iv(GCGLint location, GCGLsizei size, const GCGLint* array)
{
    makeContextCurrent();
    ::glUniform1iv(location, size, array);
}

void GraphicsContextGLOpenGL::uniform2i(GCGLint location, GCGLint v0, GCGLint v1)
{
    makeContextCurrent();
    ::glUniform2i(location, v0, v1);
}

void GraphicsContextGLOpenGL::uniform2iv(GCGLint location, GCGLsizei size, const GCGLint* array)
{
    // FIXME: length needs to be a multiple of 2.
    makeContextCurrent();
    ::glUniform2iv(location, size, array);
}

void GraphicsContextGLOpenGL::uniform3i(GCGLint location, GCGLint v0, GCGLint v1, GCGLint v2)
{
    makeContextCurrent();
    ::glUniform3i(location, v0, v1, v2);
}

void GraphicsContextGLOpenGL::uniform3iv(GCGLint location, GCGLsizei size, const GCGLint* array)
{
    // FIXME: length needs to be a multiple of 3.
    makeContextCurrent();
    ::glUniform3iv(location, size, array);
}

void GraphicsContextGLOpenGL::uniform4i(GCGLint location, GCGLint v0, GCGLint v1, GCGLint v2, GCGLint v3)
{
    makeContextCurrent();
    ::glUniform4i(location, v0, v1, v2, v3);
}

void GraphicsContextGLOpenGL::uniform4iv(GCGLint location, GCGLsizei size, const GCGLint* array)
{
    // FIXME: length needs to be a multiple of 4.
    makeContextCurrent();
    ::glUniform4iv(location, size, array);
}

void GraphicsContextGLOpenGL::uniformMatrix2fv(GCGLint location, GCGLsizei size, GCGLboolean transpose, const GCGLfloat* array)
{
    // FIXME: length needs to be a multiple of 4.
    makeContextCurrent();
    ::glUniformMatrix2fv(location, size, transpose, array);
}

void GraphicsContextGLOpenGL::uniformMatrix3fv(GCGLint location, GCGLsizei size, GCGLboolean transpose, const GCGLfloat* array)
{
    // FIXME: length needs to be a multiple of 9.
    makeContextCurrent();
    ::glUniformMatrix3fv(location, size, transpose, array);
}

void GraphicsContextGLOpenGL::uniformMatrix4fv(GCGLint location, GCGLsizei size, GCGLboolean transpose, const GCGLfloat* array)
{
    // FIXME: length needs to be a multiple of 16.
    makeContextCurrent();
    ::glUniformMatrix4fv(location, size, transpose, array);
}

void GraphicsContextGLOpenGL::useProgram(PlatformGLObject program)
{
    makeContextCurrent();
    ::glUseProgram(program);
}

void GraphicsContextGLOpenGL::validateProgram(PlatformGLObject program)
{
    ASSERT(program);

    makeContextCurrent();
    ::glValidateProgram(program);
}

void GraphicsContextGLOpenGL::vertexAttrib1f(GCGLuint index, GCGLfloat v0)
{
    makeContextCurrent();
    ::glVertexAttrib1f(index, v0);
}

void GraphicsContextGLOpenGL::vertexAttrib1fv(GCGLuint index, const GCGLfloat* array)
{
    makeContextCurrent();
    ::glVertexAttrib1fv(index, array);
}

void GraphicsContextGLOpenGL::vertexAttrib2f(GCGLuint index, GCGLfloat v0, GCGLfloat v1)
{
    makeContextCurrent();
    ::glVertexAttrib2f(index, v0, v1);
}

void GraphicsContextGLOpenGL::vertexAttrib2fv(GCGLuint index, const GCGLfloat* array)
{
    makeContextCurrent();
    ::glVertexAttrib2fv(index, array);
}

void GraphicsContextGLOpenGL::vertexAttrib3f(GCGLuint index, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2)
{
    makeContextCurrent();
    ::glVertexAttrib3f(index, v0, v1, v2);
}

void GraphicsContextGLOpenGL::vertexAttrib3fv(GCGLuint index, const GCGLfloat* array)
{
    makeContextCurrent();
    ::glVertexAttrib3fv(index, array);
}

void GraphicsContextGLOpenGL::vertexAttrib4f(GCGLuint index, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2, GCGLfloat v3)
{
    makeContextCurrent();
    ::glVertexAttrib4f(index, v0, v1, v2, v3);
}

void GraphicsContextGLOpenGL::vertexAttrib4fv(GCGLuint index, const GCGLfloat* array)
{
    makeContextCurrent();
    ::glVertexAttrib4fv(index, array);
}

void GraphicsContextGLOpenGL::vertexAttribPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLboolean normalized, GCGLsizei stride, GCGLintptr offset)
{
    makeContextCurrent();
    ::glVertexAttribPointer(index, size, type, normalized, stride, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContextGLOpenGL::viewport(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    makeContextCurrent();
    ::glViewport(x, y, width, height);
}

PlatformGLObject GraphicsContextGLOpenGL::createVertexArray()
{
    makeContextCurrent();
    GLuint array = 0;
#if (!USE(OPENGL_ES) && (PLATFORM(GTK) || PLATFORM(WIN))) || PLATFORM(COCOA)
    ::glGenVertexArrays(1, &array);
#endif
    return array;
}

void GraphicsContextGLOpenGL::deleteVertexArray(PlatformGLObject array)
{
    if (!array)
        return;
    
    makeContextCurrent();
#if (!USE(OPENGL_ES) && (PLATFORM(GTK) || PLATFORM(WIN))) || PLATFORM(COCOA)
    ::glDeleteVertexArrays(1, &array);
#endif
}

GCGLboolean GraphicsContextGLOpenGL::isVertexArray(PlatformGLObject array)
{
    if (!array)
        return GL_FALSE;
    
    makeContextCurrent();
#if (!USE(OPENGL_ES) && (PLATFORM(GTK) || PLATFORM(WIN))) || PLATFORM(COCOA)
    return ::glIsVertexArray(array);
#endif
    return GL_FALSE;
}

void GraphicsContextGLOpenGL::bindVertexArray(PlatformGLObject array)
{
    makeContextCurrent();
#if (!USE(OPENGL_ES) && (PLATFORM(GTK) || PLATFORM(WIN))) || PLATFORM(COCOA)
    ::glBindVertexArray(array);
#else
    UNUSED_PARAM(array);
#endif
}

void GraphicsContextGLOpenGL::getBooleanv(GCGLenum pname, GCGLboolean* value)
{
    makeContextCurrent();
    ::glGetBooleanv(pname, value);
}

void GraphicsContextGLOpenGL::getBufferParameteriv(GCGLenum target, GCGLenum pname, GCGLint* value)
{
    makeContextCurrent();
    ::glGetBufferParameteriv(target, pname, value);
}

void GraphicsContextGLOpenGL::getFloatv(GCGLenum pname, GCGLfloat* value)
{
    makeContextCurrent();
    ::glGetFloatv(pname, value);
}

void GraphicsContextGLOpenGL::getIntegeri_v(GCGLenum pname, GCGLuint index, GCGLint* value)
{
    UNUSED_PARAM(pname);
    UNUSED_PARAM(index);
    makeContextCurrent();
    *value = 0;
    // FIXME 141178: Before enabling this we must first switch over to using gl3.h and creating and initialing the WebGL2 context using OpenGL ES 3.0.
    // ::glGetIntegeri_v(pname, index, value);
}

void GraphicsContextGLOpenGL::getInteger64v(GCGLenum pname, GCGLint64* value)
{
    UNUSED_PARAM(pname);
    makeContextCurrent();
    *value = 0;
    // FIXME 141178: Before enabling this we must first switch over to using gl3.h and creating and initialing the WebGL2 context using OpenGL ES 3.0.
    // ::glGetInteger64v(pname, value);
}

void GraphicsContextGLOpenGL::getInteger64i_v(GCGLenum pname, GCGLuint index, GCGLint64* value)
{
    UNUSED_PARAM(pname);
    UNUSED_PARAM(index);
    makeContextCurrent();
    *value = 0;
    // FIXME 141178: Before enabling this we must first switch over to using gl3.h and creating and initialing the WebGL2 context using OpenGL ES 3.0.
    // ::glGetInteger64i_v(pname, index, value);
}

void GraphicsContextGLOpenGL::getFramebufferAttachmentParameteriv(GCGLenum target, GCGLenum attachment, GCGLenum pname, GCGLint* value)
{
    makeContextCurrent();
    if (attachment == DEPTH_STENCIL_ATTACHMENT)
        attachment = DEPTH_ATTACHMENT; // Or STENCIL_ATTACHMENT, either works.
    ::glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, value);
}

void GraphicsContextGLOpenGL::getProgramiv(PlatformGLObject program, GCGLenum pname, GCGLint* value)
{
    makeContextCurrent();
    ::glGetProgramiv(program, pname, value);
}

void GraphicsContextGLOpenGL::getNonBuiltInActiveSymbolCount(PlatformGLObject program, GCGLenum pname, GCGLint* value)
{
    ASSERT(ACTIVE_ATTRIBUTES == pname || ACTIVE_UNIFORMS == pname);
    if (!value)
        return;

    makeContextCurrent();
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

    makeContextCurrent();
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

void GraphicsContextGLOpenGL::getRenderbufferParameteriv(GCGLenum target, GCGLenum pname, GCGLint* value)
{
    makeContextCurrent();
    ::glGetRenderbufferParameterivEXT(target, pname, value);
}

void GraphicsContextGLOpenGL::getShaderiv(PlatformGLObject shader, GCGLenum pname, GCGLint* value)
{
    ASSERT(shader);

    makeContextCurrent();

    const auto& result = m_shaderSourceMap.find(shader);
    
    switch (pname) {
    case DELETE_STATUS:
    case SHADER_TYPE:
        ::glGetShaderiv(shader, pname, value);
        break;
    case COMPILE_STATUS:
        if (result == m_shaderSourceMap.end()) {
            *value = static_cast<int>(false);
            return;
        }
        *value = static_cast<int>(result->value.isValid);
        break;
    case INFO_LOG_LENGTH:
        if (result == m_shaderSourceMap.end()) {
            *value = 0;
            return;
        }
        *value = getShaderInfoLog(shader).length();
        break;
    case SHADER_SOURCE_LENGTH:
        *value = getShaderSource(shader).length();
        break;
    default:
        synthesizeGLError(INVALID_ENUM);
    }
}

String GraphicsContextGLOpenGL::getShaderInfoLog(PlatformGLObject shader)
{
    ASSERT(shader);

    makeContextCurrent();

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

    makeContextCurrent();

    const auto& result = m_shaderSourceMap.find(shader);
    if (result == m_shaderSourceMap.end())
        return String(); 

    return result->value.source;
}

void GraphicsContextGLOpenGL::getTexParameterfv(GCGLenum target, GCGLenum pname, GCGLfloat* value)
{
    makeContextCurrent();
    ::glGetTexParameterfv(target, pname, value);
}

void GraphicsContextGLOpenGL::getTexParameteriv(GCGLenum target, GCGLenum pname, GCGLint* value)
{
    makeContextCurrent();
    ::glGetTexParameteriv(target, pname, value);
}

void GraphicsContextGLOpenGL::getUniformfv(PlatformGLObject program, GCGLint location, GCGLfloat* value)
{
    makeContextCurrent();
    ::glGetUniformfv(program, location, value);
}

void GraphicsContextGLOpenGL::getUniformiv(PlatformGLObject program, GCGLint location, GCGLint* value)
{
    makeContextCurrent();
    ::glGetUniformiv(program, location, value);
}

void GraphicsContextGLOpenGL::getUniformuiv(PlatformGLObject program, GCGLint location, GCGLuint* value)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(location);
    UNUSED_PARAM(value);
}

GCGLint GraphicsContextGLOpenGL::getUniformLocation(PlatformGLObject program, const String& name)
{
    ASSERT(program);

    makeContextCurrent();

    String mappedName = mappedSymbolName(program, SHADER_SYMBOL_TYPE_UNIFORM, name);
    LOG(WebGL, "::getUniformLocation is mapping %s to %s", name.utf8().data(), mappedName.utf8().data());
    return ::glGetUniformLocation(program, mappedName.utf8().data());
}

void GraphicsContextGLOpenGL::getVertexAttribfv(GCGLuint index, GCGLenum pname, GCGLfloat* value)
{
    makeContextCurrent();
    ::glGetVertexAttribfv(index, pname, value);
}

void GraphicsContextGLOpenGL::getVertexAttribiv(GCGLuint index, GCGLenum pname, GCGLint* value)
{
    makeContextCurrent();
    ::glGetVertexAttribiv(index, pname, value);
}

GCGLsizeiptr GraphicsContextGLOpenGL::getVertexAttribOffset(GCGLuint index, GCGLenum pname)
{
    makeContextCurrent();

    GLvoid* pointer = 0;
    ::glGetVertexAttribPointerv(index, pname, &pointer);
    return static_cast<GCGLsizeiptr>(reinterpret_cast<intptr_t>(pointer));
}

void GraphicsContextGLOpenGL::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoff, GCGLint yoff, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, const void* pixels)
{
    makeContextCurrent();

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
    ::glTexSubImage2D(target, level, xoff, yoff, width, height, format, type, pixels);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContextGLOpenGL::compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLsizei imageSize, const void* data)
{
    makeContextCurrent();
    ::glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContextGLOpenGL::compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, const void* data)
{
    makeContextCurrent();
    ::glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

PlatformGLObject GraphicsContextGLOpenGL::createBuffer()
{
    makeContextCurrent();
    GLuint o = 0;
    glGenBuffers(1, &o);
    return o;
}

PlatformGLObject GraphicsContextGLOpenGL::createFramebuffer()
{
    makeContextCurrent();
    GLuint o = 0;
    glGenFramebuffersEXT(1, &o);
    return o;
}

PlatformGLObject GraphicsContextGLOpenGL::createProgram()
{
    makeContextCurrent();
    return glCreateProgram();
}

PlatformGLObject GraphicsContextGLOpenGL::createRenderbuffer()
{
    makeContextCurrent();
    GLuint o = 0;
    glGenRenderbuffersEXT(1, &o);
    return o;
}

PlatformGLObject GraphicsContextGLOpenGL::createShader(GCGLenum type)
{
    makeContextCurrent();
    return glCreateShader((type == FRAGMENT_SHADER) ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
}

PlatformGLObject GraphicsContextGLOpenGL::createTexture()
{
    makeContextCurrent();
    GLuint o = 0;
    glGenTextures(1, &o);
    m_state.textureSeedCount.add(o);
    return o;
}

void GraphicsContextGLOpenGL::deleteBuffer(PlatformGLObject buffer)
{
    makeContextCurrent();
    glDeleteBuffers(1, &buffer);
}

void GraphicsContextGLOpenGL::deleteFramebuffer(PlatformGLObject framebuffer)
{
    makeContextCurrent();
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
    makeContextCurrent();
    m_shaderProgramSymbolCountMap.remove(program);
    glDeleteProgram(program);
}

void GraphicsContextGLOpenGL::deleteRenderbuffer(PlatformGLObject renderbuffer)
{
    makeContextCurrent();
    glDeleteRenderbuffersEXT(1, &renderbuffer);
}

void GraphicsContextGLOpenGL::deleteShader(PlatformGLObject shader)
{
    makeContextCurrent();
    glDeleteShader(shader);
}

void GraphicsContextGLOpenGL::deleteTexture(PlatformGLObject texture)
{
    makeContextCurrent();
#if PLATFORM(MAC) && USE(OPENGL)
    if (m_needsFlushBeforeDeleteTextures)
        flush();
#endif
    m_state.boundTextureMap.removeIf([texture] (auto& keyValue) {
        return keyValue.value.first == texture;
    });
    glDeleteTextures(1, &texture);
    m_state.textureSeedCount.removeAll(texture);
}

void GraphicsContextGLOpenGL::synthesizeGLError(GCGLenum error)
{
    // Need to move the current errors to the synthetic error list to
    // preserve the order of errors, so a caller to getError will get
    // any errors from glError before the error we are synthesizing.
    moveErrorsToSyntheticErrorList();
    m_syntheticErrors.add(error);
}

void GraphicsContextGLOpenGL::markContextChanged()
{
    m_layerComposited = false;
}

void GraphicsContextGLOpenGL::markLayerComposited()
{
    m_layerComposited = true;

    for (auto* client : copyToVector(m_clients))
        client->didComposite();
}

bool GraphicsContextGLOpenGL::layerComposited() const
{
    return m_layerComposited;
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
    makeContextCurrent();
    ::glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContextGLOpenGL::drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount)
{
    getExtensions().drawArraysInstanced(mode, first, count, primcount);
    checkGPUStatus();
}

void GraphicsContextGLOpenGL::drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset, GCGLsizei primcount)
{
    getExtensions().drawElementsInstanced(mode, count, type, offset, primcount);
    checkGPUStatus();
}

void GraphicsContextGLOpenGL::vertexAttribDivisor(GCGLuint index, GCGLuint divisor)
{
    getExtensions().vertexAttribDivisor(index, divisor);
}

#if HAVE(OPENGL_4) && ENABLE(WEBGL2)
void GraphicsContextGLOpenGL::primitiveRestartIndex(GCGLuint index)
{
    makeContextCurrent();
    ::glPrimitiveRestartIndex(index);
}
#endif

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
    makeContextCurrent();
    ::glCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}
#elif !USE(ANGLE)
void GraphicsContextGLOpenGL::copyBufferSubData(GCGLenum, GCGLenum, GCGLintptr, GCGLintptr, GCGLsizeiptr)
{
}
#endif

void GraphicsContextGLOpenGL::getBufferSubData(GCGLenum target, GCGLintptr srcByteOffset, const void* dstData, GCGLuint dstOffset, GCGLuint length)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(srcByteOffset);
    UNUSED_PARAM(dstData);
    UNUSED_PARAM(dstOffset);
    UNUSED_PARAM(length);
}

#if HAVE(OPENGL_4) || HAVE(OPENGL_ES_3)
void* GraphicsContextGLOpenGL::mapBufferRange(GCGLenum target, GCGLintptr offset, GCGLsizeiptr length, GCGLbitfield access)
{
    makeContextCurrent();
    return ::glMapBufferRange(target, offset, length, access);
}

GCGLboolean GraphicsContextGLOpenGL::unmapBuffer(GCGLenum target)
{
    makeContextCurrent();
    return ::glUnmapBuffer(target);
}
#elif !USE(ANGLE)
void* GraphicsContextGLOpenGL::mapBufferRange(GCGLenum, GCGLintptr, GCGLsizeiptr, GCGLbitfield)
{
    return nullptr;
}

GCGLboolean GraphicsContextGLOpenGL::unmapBuffer(GCGLenum)
{
    return false;
}
#endif

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

void GraphicsContextGLOpenGL::invalidateFramebuffer(GCGLenum target, const Vector<GCGLenum>& attachments)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(attachments);
}

void GraphicsContextGLOpenGL::invalidateSubFramebuffer(GCGLenum target, const Vector<GCGLenum>& attachments, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
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
void GraphicsContextGLOpenGL::getInternalformativ(GCGLenum target, GCGLenum internalformat, GCGLenum pname, GCGLsizei bufSize, GCGLint* params)
{
#if USE(OPENGL_ES)
    makeContextCurrent();
    ::glGetInternalformativ(target, internalformat, pname, bufSize, params);
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
    makeContextCurrent();
    ::glRenderbufferStorageMultisample(target, samples, internalformat, width, height);
}

void GraphicsContextGLOpenGL::texStorage2D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    makeContextCurrent();
    ::glTexStorage2D(target, levels, internalformat, width, height);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContextGLOpenGL::texStorage3D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth)
{
    makeContextCurrent();
    ::glTexStorage3D(target, levels, internalformat, width, height, depth);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}
#elif !USE(ANGLE)
void GraphicsContextGLOpenGL::getInternalformativ(GCGLenum, GCGLenum, GCGLenum, GCGLsizei, GCGLint*)
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

void GraphicsContextGLOpenGL::texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLintptr pboOffset)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(depth);
    UNUSED_PARAM(border);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(pboOffset);
}

void GraphicsContextGLOpenGL::texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, const void* pixels)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(depth);
    UNUSED_PARAM(border);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(pixels);
}

void GraphicsContextGLOpenGL::texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, const void* srcData, GCGLuint srcOffset)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(depth);
    UNUSED_PARAM(border);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(srcData);
    UNUSED_PARAM(srcOffset);
}

void GraphicsContextGLOpenGL::texSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, GCGLintptr pboOffset)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(zoffset);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(depth);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(pboOffset);
}

void GraphicsContextGLOpenGL::texSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, const void* srcData, GCGLuint srcOffset)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(zoffset);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(depth);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(srcData);
    UNUSED_PARAM(srcOffset);
}

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

void GraphicsContextGLOpenGL::compressedTexImage3D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLsizei imageSize, GCGLintptr offset)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(depth);
    UNUSED_PARAM(border);
    UNUSED_PARAM(imageSize);
    UNUSED_PARAM(offset);
}

void GraphicsContextGLOpenGL::compressedTexImage3D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, const void* srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(depth);
    UNUSED_PARAM(border);
    UNUSED_PARAM(srcData);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLengthOverride);
}

void GraphicsContextGLOpenGL::compressedTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(zoffset);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(depth);
    UNUSED_PARAM(format);
    UNUSED_PARAM(imageSize);
    UNUSED_PARAM(offset);
}

void GraphicsContextGLOpenGL::compressedTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, const void* srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(zoffset);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(depth);
    UNUSED_PARAM(format);
    UNUSED_PARAM(srcData);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLengthOverride);
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

void GraphicsContextGLOpenGL::uniform1uiv(GCGLint location, const GCGLuint* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniform2uiv(GCGLint location, const GCGLuint* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniform3uiv(GCGLint location, const GCGLuint* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniform4uiv(GCGLint location, const GCGLuint* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniformMatrix2x3fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniformMatrix3x2fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniformMatrix2x4fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniformMatrix4x2fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniformMatrix3x4fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniformMatrix4x3fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::vertexAttribI4i(GCGLuint index, GCGLint x, GCGLint y, GCGLint z, GCGLint w)
{
    UNUSED_PARAM(index);
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(z);
    UNUSED_PARAM(w);
}

void GraphicsContextGLOpenGL::vertexAttribI4iv(GCGLuint index, const GCGLint* values)
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

void GraphicsContextGLOpenGL::vertexAttribI4uiv(GCGLuint index, const GCGLuint* values)
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

void GraphicsContextGLOpenGL::drawBuffers(const Vector<GCGLenum>& buffers)
{
    UNUSED_PARAM(buffers);
}

void GraphicsContextGLOpenGL::clearBufferiv(GCGLenum buffer, GCGLint drawbuffer, const GCGLint* values, GCGLuint srcOffset)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(drawbuffer);
    UNUSED_PARAM(values);
    UNUSED_PARAM(srcOffset);
}

void GraphicsContextGLOpenGL::clearBufferuiv(GCGLenum buffer, GCGLint drawbuffer, const GCGLuint* values, GCGLuint srcOffset)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(drawbuffer);
    UNUSED_PARAM(values);
    UNUSED_PARAM(srcOffset);
}

void GraphicsContextGLOpenGL::clearBufferfv(GCGLenum buffer, GCGLint drawbuffer, const GCGLfloat* values, GCGLuint srcOffset)
{
    UNUSED_PARAM(buffer);
    UNUSED_PARAM(drawbuffer);
    UNUSED_PARAM(values);
    UNUSED_PARAM(srcOffset);
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

void GraphicsContextGLOpenGL::getQueryObjectuiv(PlatformGLObject query, GCGLenum pname, GCGLuint* value)
{
    UNUSED_PARAM(query);
    UNUSED_PARAM(pname);
    UNUSED_PARAM(value);
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

void GraphicsContextGLOpenGL::getSamplerParameterfv(PlatformGLObject sampler, GCGLenum pname, GCGLfloat* value)
{
    UNUSED_PARAM(sampler);
    UNUSED_PARAM(pname);
    UNUSED_PARAM(value);
}

void GraphicsContextGLOpenGL::getSamplerParameteriv(PlatformGLObject sampler, GCGLenum pname, GCGLint* value)
{
    UNUSED_PARAM(sampler);
    UNUSED_PARAM(pname);
    UNUSED_PARAM(value);
}

PlatformGLObject GraphicsContextGLOpenGL::fenceSync(GCGLenum condition, GCGLbitfield flags)
{
    UNUSED_PARAM(condition);
    UNUSED_PARAM(flags);

    return 0;
}

GCGLboolean GraphicsContextGLOpenGL::isSync(PlatformGLObject sync)
{
    UNUSED_PARAM(sync);

    return false;
}

void GraphicsContextGLOpenGL::deleteSync(PlatformGLObject sync)
{
    UNUSED_PARAM(sync);
}

GCGLenum GraphicsContextGLOpenGL::clientWaitSync(PlatformGLObject sync, GCGLbitfield flags, GCGLuint64 timeout)
{
    UNUSED_PARAM(sync);
    UNUSED_PARAM(flags);
    UNUSED_PARAM(timeout);

    return 0;
}

void GraphicsContextGLOpenGL::waitSync(PlatformGLObject sync, GCGLbitfield flags, GCGLint64 timeout)
{
    UNUSED_PARAM(sync);
    UNUSED_PARAM(flags);
    UNUSED_PARAM(timeout);
}

void GraphicsContextGLOpenGL::getSynciv(PlatformGLObject sync, GCGLenum pname, GCGLsizei bufSize, GCGLint *value)
{
    UNUSED_PARAM(sync);
    UNUSED_PARAM(pname);
    UNUSED_PARAM(bufSize);
    UNUSED_PARAM(value);
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

#if HAVE(OPENGL_4) || HAVE(OPENGL_ES_3)
void GraphicsContextGLOpenGL::getActiveUniforms(PlatformGLObject program, const Vector<GCGLuint>& uniformIndices, GCGLenum pname, Vector<GCGLint>& params)
{
    ASSERT(program);
    makeContextCurrent();

    ::glGetActiveUniformsiv(program, uniformIndices.size(), uniformIndices.data(), pname, params.data());
}

GCGLuint GraphicsContextGLOpenGL::getUniformBlockIndex(PlatformGLObject program, const String& uniformBlockName)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformBlockName);

    return 0;
}

void GraphicsContextGLOpenGL::getActiveUniformBlockiv(PlatformGLObject program, GCGLuint uniformBlockIndex, GCGLenum pname, GCGLint* params)
{
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformBlockIndex);
    UNUSED_PARAM(pname);
    UNUSED_PARAM(params);
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
#elif !USE(ANGLE)
void GraphicsContextGLOpenGL::getActiveUniforms(PlatformGLObject, const Vector<GCGLuint>&, GCGLenum, Vector<GCGLint>&)
{
}

GCGLuint GraphicsContextGLOpenGL::getUniformBlockIndex(PlatformGLObject, const String&)
{
    return 0;
}

void GraphicsContextGLOpenGL::getActiveUniformBlockiv(PlatformGLObject, GCGLuint, GCGLenum, GCGLint*)
{
}

String GraphicsContextGLOpenGL::getActiveUniformBlockName(PlatformGLObject, GCGLuint)
{
    return emptyString();
}

void GraphicsContextGLOpenGL::uniformBlockBinding(PlatformGLObject, GCGLuint, GCGLuint)
{
}
#endif

void GraphicsContextGLOpenGL::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr pboOffset)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(pboOffset);
}

void GraphicsContextGLOpenGL::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, const void* srcData, GCGLuint srcOffset)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(srcData);
    UNUSED_PARAM(srcOffset);
}

void GraphicsContextGLOpenGL::compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLsizei imageSize, GCGLintptr offset)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(border);
    UNUSED_PARAM(imageSize);
    UNUSED_PARAM(offset);
}

void GraphicsContextGLOpenGL::compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, const void* srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(border);
    UNUSED_PARAM(srcData);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLengthOverride);
}

void GraphicsContextGLOpenGL::compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(format);
    UNUSED_PARAM(imageSize);
    UNUSED_PARAM(offset);
}

void GraphicsContextGLOpenGL::compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, const void* srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(xoffset);
    UNUSED_PARAM(yoffset);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(format);
    UNUSED_PARAM(srcData);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLengthOverride);
}

void GraphicsContextGLOpenGL::uniform1fv(GCGLint location, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniform2fv(GCGLint location, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniform3fv(GCGLint location, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniform4fv(GCGLint location, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniform1iv(GCGLint location, const GCGLint* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniform2iv(GCGLint location, const GCGLint* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniform3iv(GCGLint location, const GCGLint* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniform4iv(GCGLint location, const GCGLint* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniformMatrix2fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniformMatrix3fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::uniformMatrix4fv(GCGLint location, GCGLboolean transpose, const GCGLfloat* data, GCGLuint srcOffset, GCGLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);
}

void GraphicsContextGLOpenGL::readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(offset);
}

void GraphicsContextGLOpenGL::readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, const void* dstData, GCGLuint dstOffset)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(dstData);
    UNUSED_PARAM(dstOffset);
}


}

#endif // ENABLE(GRAPHICS_CONTEXT_GL) && (USE(OPENGL) || USE(OPENGL_ES))
