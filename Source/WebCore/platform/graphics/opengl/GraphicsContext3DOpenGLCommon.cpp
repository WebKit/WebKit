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

#if ENABLE(GRAPHICS_CONTEXT_3D)

#include "GraphicsContext3D.h"
#if PLATFORM(IOS_FAMILY)
#include "GraphicsContext3DIOS.h"
#endif

#if !PLATFORM(COCOA) && USE(OPENGL_ES)
#include "Extensions3DOpenGLES.h"
#else
#include "Extensions3DOpenGL.h"
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
// to point to the map kept by the current instance of GraphicsContext3D.

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

void GraphicsContext3D::validateDepthStencil(const char* packedDepthStencilExtension)
{
    Extensions3D& extensions = getExtensions();
    if (m_attrs.stencil) {
        if (extensions.supports(packedDepthStencilExtension)) {
            extensions.ensureEnabled(packedDepthStencilExtension);
            // Force depth if stencil is true.
            m_attrs.depth = true;
        } else
            m_attrs.stencil = false;
    }
    if (m_attrs.antialias) {
        if (!extensions.supports("GL_ANGLE_framebuffer_multisample") || isGLES2Compliant())
            m_attrs.antialias = false;
        else
            extensions.ensureEnabled("GL_ANGLE_framebuffer_multisample");
    }
}

void GraphicsContext3D::paintRenderingResultsToCanvas(ImageBuffer* imageBuffer)
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

    if (!m_attrs.premultipliedAlpha) {
        for (int i = 0; i < totalBytes; i += 4) {
            // Premultiply alpha.
            pixels[i + 0] = std::min(255, pixels[i + 0] * pixels[i + 3] / 255);
            pixels[i + 1] = std::min(255, pixels[i + 1] * pixels[i + 3] / 255);
            pixels[i + 2] = std::min(255, pixels[i + 2] * pixels[i + 3] / 255);
        }
    }

    paintToCanvas(pixels.get(), IntSize(m_currentWidth, m_currentHeight), imageBuffer->internalSize(), imageBuffer->context());

#if PLATFORM(COCOA) && USE(OPENGL_ES)
    presentRenderbuffer();
#endif
}

bool GraphicsContext3D::paintCompositedResultsToCanvas(ImageBuffer*)
{
    // Not needed at the moment, so return that nothing was done.
    return false;
}

RefPtr<ImageData> GraphicsContext3D::paintRenderingResultsToImageData()
{
    // Reading premultiplied alpha would involve unpremultiplying, which is
    // lossy.
    if (m_attrs.premultipliedAlpha)
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

void GraphicsContext3D::prepareTexture()
{
    if (m_layerComposited)
        return;

    makeContextCurrent();

#if !USE(COORDINATED_GRAPHICS_THREADED)
    TemporaryOpenGLSetting scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
    TemporaryOpenGLSetting scopedDither(GL_DITHER, GL_FALSE);
#endif

    if (m_attrs.antialias)
        resolveMultisamplingIfNecessary();

#if USE(COORDINATED_GRAPHICS_THREADED)
    std::swap(m_texture, m_compositorTexture);
    std::swap(m_texture, m_intermediateTexture);
    ::glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    ::glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_texture, 0);
    glFlush();

    if (m_state.boundFBO != m_fbo)
        ::glBindFramebufferEXT(GraphicsContext3D::FRAMEBUFFER, m_state.boundFBO);
    else
        ::glBindFramebufferEXT(GraphicsContext3D::FRAMEBUFFER, m_fbo);
    return;
#endif

    ::glActiveTexture(GL_TEXTURE0);
    ::glBindTexture(GL_TEXTURE_2D, m_state.boundTarget(GL_TEXTURE0) == GL_TEXTURE_2D ? m_state.boundTexture(GL_TEXTURE0) : 0);
    ::glActiveTexture(m_state.activeTextureUnit);
    if (m_state.boundFBO != m_fbo)
        ::glBindFramebufferEXT(GraphicsContext3D::FRAMEBUFFER, m_state.boundFBO);
    ::glFlush();
}

void GraphicsContext3D::readRenderingResults(unsigned char *pixels, int pixelsSize)
{
    if (pixelsSize < m_currentWidth * m_currentHeight * 4)
        return;

    makeContextCurrent();

    bool mustRestoreFBO = false;
    if (m_attrs.antialias) {
        resolveMultisamplingIfNecessary();
        ::glBindFramebufferEXT(GraphicsContext3D::FRAMEBUFFER, m_fbo);
        mustRestoreFBO = true;
    } else {
        if (m_state.boundFBO != m_fbo) {
            mustRestoreFBO = true;
            ::glBindFramebufferEXT(GraphicsContext3D::FRAMEBUFFER, m_fbo);
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
        ::glBindFramebufferEXT(GraphicsContext3D::FRAMEBUFFER, m_state.boundFBO);
}

void GraphicsContext3D::reshape(int width, int height)
{
    if (!platformGraphicsContext3D())
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
    if (m_attrs.depth) {
        ::glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
        GraphicsContext3D::clearDepth(1);
        ::glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        ::glDepthMask(GL_TRUE);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }
    if (m_attrs.stencil) {
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
    if (m_attrs.depth) {
        GraphicsContext3D::clearDepth(clearDepth);
        ::glDepthMask(depthMask);
    }
    if (m_attrs.stencil) {
        ::glClearStencil(clearStencil);
        ::glStencilMaskSeparate(GL_FRONT, stencilMask);
        ::glStencilMaskSeparate(GL_BACK, stencilMaskBack);
    }

    if (mustRestoreFBO)
        ::glBindFramebufferEXT(GraphicsContext3D::FRAMEBUFFER, m_state.boundFBO);

    ::glFlush();
}

bool GraphicsContext3D::checkVaryingsPacking(Platform3DObject vertexShader, Platform3DObject fragmentShader) const
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

    GC3Dint maxVaryingVectors = 0;
#if USE(OPENGL_ES)
    ::glGetIntegerv(MAX_VARYING_VECTORS, &maxVaryingVectors);
#else
    if (m_isForWebGL2)
        ::glGetIntegerv(GL_MAX_VARYING_VECTORS, &maxVaryingVectors);
    else {
        GC3Dint maxVaryingFloats = 0;
        ::glGetIntegerv(GL_MAX_VARYING_FLOATS, &maxVaryingFloats);
        maxVaryingVectors = maxVaryingFloats / 4;
    }
#endif
    return sh::CheckVariablesWithinPackingLimits(maxVaryingVectors, variables);
}

bool GraphicsContext3D::precisionsMatch(Platform3DObject vertexShader, Platform3DObject fragmentShader) const
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

IntSize GraphicsContext3D::getInternalFramebufferSize() const
{
    return IntSize(m_currentWidth, m_currentHeight);
}

void GraphicsContext3D::activeTexture(GC3Denum texture)
{
    makeContextCurrent();
    m_state.activeTextureUnit = texture;
    ::glActiveTexture(texture);
}

void GraphicsContext3D::attachShader(Platform3DObject program, Platform3DObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    makeContextCurrent();
    m_shaderProgramSymbolCountMap.remove(program);
    ::glAttachShader(program, shader);
}

void GraphicsContext3D::bindAttribLocation(Platform3DObject program, GC3Duint index, const String& name)
{
    ASSERT(program);
    makeContextCurrent();

    String mappedName = mappedSymbolName(program, SHADER_SYMBOL_TYPE_ATTRIBUTE, name);
    LOG(WebGL, "::bindAttribLocation is mapping %s to %s", name.utf8().data(), mappedName.utf8().data());
    ::glBindAttribLocation(program, index, mappedName.utf8().data());
}

void GraphicsContext3D::bindBuffer(GC3Denum target, Platform3DObject buffer)
{
    makeContextCurrent();
    ::glBindBuffer(target, buffer);
}

void GraphicsContext3D::bindFramebuffer(GC3Denum target, Platform3DObject buffer)
{
    makeContextCurrent();
    GLuint fbo;
    if (buffer)
        fbo = buffer;
    else
        fbo = (m_attrs.antialias ? m_multisampleFBO : m_fbo);
    if (fbo != m_state.boundFBO) {
        ::glBindFramebufferEXT(target, fbo);
        m_state.boundFBO = fbo;
    }
}

void GraphicsContext3D::bindRenderbuffer(GC3Denum target, Platform3DObject renderbuffer)
{
    makeContextCurrent();
    ::glBindRenderbufferEXT(target, renderbuffer);
}


void GraphicsContext3D::bindTexture(GC3Denum target, Platform3DObject texture)
{
    makeContextCurrent();
    m_state.setBoundTexture(m_state.activeTextureUnit, texture, target);
    ::glBindTexture(target, texture);
}

void GraphicsContext3D::blendColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha)
{
    makeContextCurrent();
    ::glBlendColor(red, green, blue, alpha);
}

void GraphicsContext3D::blendEquation(GC3Denum mode)
{
    makeContextCurrent();
    ::glBlendEquation(mode);
}

void GraphicsContext3D::blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha)
{
    makeContextCurrent();
    ::glBlendEquationSeparate(modeRGB, modeAlpha);
}


void GraphicsContext3D::blendFunc(GC3Denum sfactor, GC3Denum dfactor)
{
    makeContextCurrent();
    ::glBlendFunc(sfactor, dfactor);
}       

void GraphicsContext3D::blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB, GC3Denum srcAlpha, GC3Denum dstAlpha)
{
    makeContextCurrent();
    ::glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, GC3Denum usage)
{
    makeContextCurrent();
    ::glBufferData(target, size, 0, usage);
}

void GraphicsContext3D::bufferData(GC3Denum target, GC3Dsizeiptr size, const void* data, GC3Denum usage)
{
    makeContextCurrent();
    ::glBufferData(target, size, data, usage);
}

void GraphicsContext3D::bufferSubData(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr size, const void* data)
{
    makeContextCurrent();
    ::glBufferSubData(target, offset, size, data);
}

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY) || PLATFORM(WPE)
void* GraphicsContext3D::mapBufferRange(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr length, GC3Dbitfield access)
{
    makeContextCurrent();
    return ::glMapBufferRange(target, offset, length, access);
}

GC3Dboolean GraphicsContext3D::unmapBuffer(GC3Denum target)
{
    makeContextCurrent();
    return ::glUnmapBuffer(target);
}

void GraphicsContext3D::copyBufferSubData(GC3Denum readTarget, GC3Denum writeTarget, GC3Dintptr readOffset, GC3Dintptr writeOffset, GC3Dsizeiptr size)
{
    makeContextCurrent();
    ::glCopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}

void GraphicsContext3D::getInternalformativ(GC3Denum target, GC3Denum internalformat, GC3Denum pname, GC3Dsizei bufSize, GC3Dint* params)
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

void GraphicsContext3D::renderbufferStorageMultisample(GC3Denum target, GC3Dsizei samples, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    ::glRenderbufferStorageMultisample(target, samples, internalformat, width, height);
}

void GraphicsContext3D::texStorage2D(GC3Denum target, GC3Dsizei levels, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    ::glTexStorage2D(target, levels, internalformat, width, height);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContext3D::texStorage3D(GC3Denum target, GC3Dsizei levels, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dsizei depth)
{
    makeContextCurrent();
    ::glTexStorage3D(target, levels, internalformat, width, height, depth);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContext3D::getActiveUniforms(Platform3DObject program, const Vector<GC3Duint>& uniformIndices, GC3Denum pname, Vector<GC3Dint>& params)
{
    ASSERT(program);
    makeContextCurrent();

    ::glGetActiveUniformsiv(program, uniformIndices.size(), uniformIndices.data(), pname, params.data());
}
#endif

GC3Denum GraphicsContext3D::checkFramebufferStatus(GC3Denum target)
{
    makeContextCurrent();
    return ::glCheckFramebufferStatusEXT(target);
}

void GraphicsContext3D::clearColor(GC3Dclampf r, GC3Dclampf g, GC3Dclampf b, GC3Dclampf a)
{
    makeContextCurrent();
    ::glClearColor(r, g, b, a);
}

void GraphicsContext3D::clear(GC3Dbitfield mask)
{
    makeContextCurrent();
    ::glClear(mask);
    checkGPUStatus();
}

void GraphicsContext3D::clearStencil(GC3Dint s)
{
    makeContextCurrent();
    ::glClearStencil(s);
}

void GraphicsContext3D::colorMask(GC3Dboolean red, GC3Dboolean green, GC3Dboolean blue, GC3Dboolean alpha)
{
    makeContextCurrent();
    ::glColorMask(red, green, blue, alpha);
}

void GraphicsContext3D::compileShader(Platform3DObject shader)
{
    ASSERT(shader);
    makeContextCurrent();

    // Turn on name mapping. Due to the way ANGLE name hashing works, we
    // point a global hashmap to the map owned by this context.
    ShBuiltInResources ANGLEResources = m_compiler.getResources();
    ShHashFunction64 previousHashFunction = ANGLEResources.HashFunction;
    ANGLEResources.HashFunction = nameHashForShader;

    if (!nameHashMapForShaders)
        nameHashMapForShaders = std::make_unique<ShaderNameHash>();
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
    GraphicsContext3D::ShaderSourceEntry& entry = result->value;

    // Populate the shader log
    GLint length = 0;
    ::glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    if (length) {
        GLsizei size = 0;
        Vector<GLchar> info(length);
        ::glGetShaderInfoLog(shader, length, &size, info.data());

        Platform3DObject shaders[2] = { shader, 0 };
        entry.log = getUnmangledInfoLog(shaders, 1, String(info.data(), size));
    }

    if (compileStatus != GL_TRUE) {
        entry.isValid = false;
        LOG(WebGL, "Error: shader translator produced a shader that OpenGL would not compile.");
    }
}

void GraphicsContext3D::compileShaderDirect(Platform3DObject shader)
{
    ASSERT(shader);
    makeContextCurrent();

    HashMap<Platform3DObject, ShaderSourceEntry>::iterator result = m_shaderSourceMap.find(shader);

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

void GraphicsContext3D::copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border)
{
    makeContextCurrent();
    if (m_attrs.antialias && m_state.boundFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        ::glBindFramebufferEXT(GraphicsContext3D::FRAMEBUFFER, m_fbo);
    }
    ::glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
    if (m_attrs.antialias && m_state.boundFBO == m_multisampleFBO)
        ::glBindFramebufferEXT(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO);
}

void GraphicsContext3D::copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    if (m_attrs.antialias && m_state.boundFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        ::glBindFramebufferEXT(GraphicsContext3D::FRAMEBUFFER, m_fbo);
    }
    ::glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    if (m_attrs.antialias && m_state.boundFBO == m_multisampleFBO)
        ::glBindFramebufferEXT(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO);
}

void GraphicsContext3D::cullFace(GC3Denum mode)
{
    makeContextCurrent();
    ::glCullFace(mode);
}

void GraphicsContext3D::depthFunc(GC3Denum func)
{
    makeContextCurrent();
    ::glDepthFunc(func);
}

void GraphicsContext3D::depthMask(GC3Dboolean flag)
{
    makeContextCurrent();
    ::glDepthMask(flag);
}

void GraphicsContext3D::detachShader(Platform3DObject program, Platform3DObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    makeContextCurrent();
    m_shaderProgramSymbolCountMap.remove(program);
    ::glDetachShader(program, shader);
}

void GraphicsContext3D::disable(GC3Denum cap)
{
    makeContextCurrent();
    ::glDisable(cap);
}

void GraphicsContext3D::disableVertexAttribArray(GC3Duint index)
{
    makeContextCurrent();
    ::glDisableVertexAttribArray(index);
}

void GraphicsContext3D::drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count)
{
    makeContextCurrent();
    ::glDrawArrays(mode, first, count);
    checkGPUStatus();
}

void GraphicsContext3D::drawElements(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset)
{
    makeContextCurrent();
    ::glDrawElements(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
    checkGPUStatus();
}

void GraphicsContext3D::enable(GC3Denum cap)
{
    makeContextCurrent();
    ::glEnable(cap);
}

void GraphicsContext3D::enableVertexAttribArray(GC3Duint index)
{
    makeContextCurrent();
    ::glEnableVertexAttribArray(index);
}

void GraphicsContext3D::finish()
{
    makeContextCurrent();
    ::glFinish();
}

void GraphicsContext3D::flush()
{
    makeContextCurrent();
    ::glFlush();
}

void GraphicsContext3D::framebufferRenderbuffer(GC3Denum target, GC3Denum attachment, GC3Denum renderbuffertarget, Platform3DObject buffer)
{
    makeContextCurrent();
    ::glFramebufferRenderbufferEXT(target, attachment, renderbuffertarget, buffer);
}

void GraphicsContext3D::framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum textarget, Platform3DObject texture, GC3Dint level)
{
    makeContextCurrent();
    ::glFramebufferTexture2DEXT(target, attachment, textarget, texture, level);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContext3D::frontFace(GC3Denum mode)
{
    makeContextCurrent();
    ::glFrontFace(mode);
}

void GraphicsContext3D::generateMipmap(GC3Denum target)
{
    makeContextCurrent();
    ::glGenerateMipmap(target);
}

bool GraphicsContext3D::getActiveAttribImpl(Platform3DObject program, GC3Duint index, ActiveInfo& info)
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

bool GraphicsContext3D::getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    GC3Dint symbolCount;
    auto result = m_shaderProgramSymbolCountMap.find(program);
    if (result == m_shaderProgramSymbolCountMap.end()) {
        getNonBuiltInActiveSymbolCount(program, GraphicsContext3D::ACTIVE_ATTRIBUTES, &symbolCount);
        result = m_shaderProgramSymbolCountMap.find(program);
    }
    
    ActiveShaderSymbolCounts& symbolCounts = result->value;
    GC3Duint rawIndex = (index < symbolCounts.filteredToActualAttributeIndexMap.size()) ? symbolCounts.filteredToActualAttributeIndexMap[index] : -1;

    return getActiveAttribImpl(program, rawIndex, info);
}

bool GraphicsContext3D::getActiveUniformImpl(Platform3DObject program, GC3Duint index, ActiveInfo& info)
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

bool GraphicsContext3D::getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo& info)
{
    GC3Dint symbolCount;
    auto result = m_shaderProgramSymbolCountMap.find(program);
    if (result == m_shaderProgramSymbolCountMap.end()) {
        getNonBuiltInActiveSymbolCount(program, GraphicsContext3D::ACTIVE_UNIFORMS, &symbolCount);
        result = m_shaderProgramSymbolCountMap.find(program);
    }
    
    ActiveShaderSymbolCounts& symbolCounts = result->value;
    GC3Duint rawIndex = (index < symbolCounts.filteredToActualUniformIndexMap.size()) ? symbolCounts.filteredToActualUniformIndexMap[index] : -1;
    
    return getActiveUniformImpl(program, rawIndex, info);
}

void GraphicsContext3D::getAttachedShaders(Platform3DObject program, GC3Dsizei maxCount, GC3Dsizei* count, Platform3DObject* shaders)
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
    StringBuilder builder;
    builder.appendLiteral("webgl_");
    appendUnsigned64AsHex(number, builder, Lowercase);
    return builder.toString();
}

Optional<String> GraphicsContext3D::mappedSymbolInShaderSourceMap(Platform3DObject shader, ANGLEShaderSymbolType symbolType, const String& name)
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

String GraphicsContext3D::mappedSymbolName(Platform3DObject program, ANGLEShaderSymbolType symbolType, const String& name)
{
    GC3Dsizei count = 0;
    Platform3DObject shaders[2] = { };
    getAttachedShaders(program, 2, &count, shaders);

    for (GC3Dsizei i = 0; i < count; ++i) {
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
            nameHashMapForShaders = std::make_unique<ShaderNameHash>();
        setCurrentNameHashMapForShader(nameHashMapForShaders.get());

        auto generatedName = generateHashedName(name);

        setCurrentNameHashMapForShader(nullptr);

        m_possiblyUnusedAttributeMap.set(generatedName, name);

        return generatedName;
    }

    return name;
}

Optional<String> GraphicsContext3D::originalSymbolInShaderSourceMap(Platform3DObject shader, ANGLEShaderSymbolType symbolType, const String& name)
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

String GraphicsContext3D::originalSymbolName(Platform3DObject program, ANGLEShaderSymbolType symbolType, const String& name)
{
    GC3Dsizei count;
    Platform3DObject shaders[2];
    getAttachedShaders(program, 2, &count, shaders);
    
    for (GC3Dsizei i = 0; i < count; ++i) {
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

String GraphicsContext3D::mappedSymbolName(Platform3DObject shaders[2], size_t count, const String& name)
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

int GraphicsContext3D::getAttribLocation(Platform3DObject program, const String& name)
{
    if (!program)
        return -1;

    makeContextCurrent();

    String mappedName = mappedSymbolName(program, SHADER_SYMBOL_TYPE_ATTRIBUTE, name);
    LOG(WebGL, "::glGetAttribLocation is mapping %s to %s", name.utf8().data(), mappedName.utf8().data());
    return ::glGetAttribLocation(program, mappedName.utf8().data());
}

int GraphicsContext3D::getAttribLocationDirect(Platform3DObject program, const String& name)
{
    if (!program)
        return -1;

    makeContextCurrent();

    return ::glGetAttribLocation(program, name.utf8().data());
}

GraphicsContext3DAttributes GraphicsContext3D::getContextAttributes()
{
    return m_attrs;
}

bool GraphicsContext3D::moveErrorsToSyntheticErrorList()
{
    makeContextCurrent();
    bool movedAnError = false;

    // Set an arbitrary limit of 100 here to avoid creating a hang if
    // a problem driver has a bug that causes it to never clear the error.
    // Otherwise, we would just loop until we got NO_ERROR.
    for (unsigned i = 0; i < 100; ++i) {
        GC3Denum error = glGetError();
        if (error == NO_ERROR)
            break;
        m_syntheticErrors.add(error);
        movedAnError = true;
    }

    return movedAnError;
}

GC3Denum GraphicsContext3D::getError()
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

String GraphicsContext3D::getString(GC3Denum name)
{
    makeContextCurrent();
    return String(reinterpret_cast<const char*>(::glGetString(name)));
}

void GraphicsContext3D::hint(GC3Denum target, GC3Denum mode)
{
    makeContextCurrent();
    ::glHint(target, mode);
}

GC3Dboolean GraphicsContext3D::isBuffer(Platform3DObject buffer)
{
    if (!buffer)
        return GL_FALSE;

    makeContextCurrent();
    return ::glIsBuffer(buffer);
}

GC3Dboolean GraphicsContext3D::isEnabled(GC3Denum cap)
{
    makeContextCurrent();
    return ::glIsEnabled(cap);
}

GC3Dboolean GraphicsContext3D::isFramebuffer(Platform3DObject framebuffer)
{
    if (!framebuffer)
        return GL_FALSE;

    makeContextCurrent();
    return ::glIsFramebufferEXT(framebuffer);
}

GC3Dboolean GraphicsContext3D::isProgram(Platform3DObject program)
{
    if (!program)
        return GL_FALSE;

    makeContextCurrent();
    return ::glIsProgram(program);
}

GC3Dboolean GraphicsContext3D::isRenderbuffer(Platform3DObject renderbuffer)
{
    if (!renderbuffer)
        return GL_FALSE;

    makeContextCurrent();
    return ::glIsRenderbufferEXT(renderbuffer);
}

GC3Dboolean GraphicsContext3D::isShader(Platform3DObject shader)
{
    if (!shader)
        return GL_FALSE;

    makeContextCurrent();
    return ::glIsShader(shader);
}

GC3Dboolean GraphicsContext3D::isTexture(Platform3DObject texture)
{
    if (!texture)
        return GL_FALSE;

    makeContextCurrent();
    return ::glIsTexture(texture);
}

void GraphicsContext3D::lineWidth(GC3Dfloat width)
{
    makeContextCurrent();
    ::glLineWidth(width);
}

void GraphicsContext3D::linkProgram(Platform3DObject program)
{
    ASSERT(program);
    makeContextCurrent();

    GC3Dsizei count = 0;
    Platform3DObject shaders[2] = { };
    getAttachedShaders(program, 2, &count, shaders);

    if (count == 2)
        m_linkedShaderMap.set(program, std::make_pair(shaders[0], shaders[1]));

    ::glLinkProgram(program);
}

void GraphicsContext3D::pixelStorei(GC3Denum pname, GC3Dint param)
{
    makeContextCurrent();
    ::glPixelStorei(pname, param);
}

void GraphicsContext3D::polygonOffset(GC3Dfloat factor, GC3Dfloat units)
{
    makeContextCurrent();
    ::glPolygonOffset(factor, units);
}

void GraphicsContext3D::sampleCoverage(GC3Dclampf value, GC3Dboolean invert)
{
    makeContextCurrent();
    ::glSampleCoverage(value, invert);
}

void GraphicsContext3D::scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    ::glScissor(x, y, width, height);
}

void GraphicsContext3D::shaderSource(Platform3DObject shader, const String& string)
{
    ASSERT(shader);

    makeContextCurrent();

    ShaderSourceEntry entry;

    entry.source = string;

    m_shaderSourceMap.set(shader, entry);
}

void GraphicsContext3D::stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    makeContextCurrent();
    ::glStencilFunc(func, ref, mask);
}

void GraphicsContext3D::stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask)
{
    makeContextCurrent();
    ::glStencilFuncSeparate(face, func, ref, mask);
}

void GraphicsContext3D::stencilMask(GC3Duint mask)
{
    makeContextCurrent();
    ::glStencilMask(mask);
}

void GraphicsContext3D::stencilMaskSeparate(GC3Denum face, GC3Duint mask)
{
    makeContextCurrent();
    ::glStencilMaskSeparate(face, mask);
}

void GraphicsContext3D::stencilOp(GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    makeContextCurrent();
    ::glStencilOp(fail, zfail, zpass);
}

void GraphicsContext3D::stencilOpSeparate(GC3Denum face, GC3Denum fail, GC3Denum zfail, GC3Denum zpass)
{
    makeContextCurrent();
    ::glStencilOpSeparate(face, fail, zfail, zpass);
}

void GraphicsContext3D::texParameterf(GC3Denum target, GC3Denum pname, GC3Dfloat value)
{
    makeContextCurrent();
    ::glTexParameterf(target, pname, value);
}

void GraphicsContext3D::texParameteri(GC3Denum target, GC3Denum pname, GC3Dint value)
{
    makeContextCurrent();
    ::glTexParameteri(target, pname, value);
}

void GraphicsContext3D::uniform1f(GC3Dint location, GC3Dfloat v0)
{
    makeContextCurrent();
    ::glUniform1f(location, v0);
}

void GraphicsContext3D::uniform1fv(GC3Dint location, GC3Dsizei size, const GC3Dfloat* array)
{
    makeContextCurrent();
    ::glUniform1fv(location, size, array);
}

void GraphicsContext3D::uniform2f(GC3Dint location, GC3Dfloat v0, GC3Dfloat v1)
{
    makeContextCurrent();
    ::glUniform2f(location, v0, v1);
}

void GraphicsContext3D::uniform2fv(GC3Dint location, GC3Dsizei size, const GC3Dfloat* array)
{
    // FIXME: length needs to be a multiple of 2.
    makeContextCurrent();
    ::glUniform2fv(location, size, array);
}

void GraphicsContext3D::uniform3f(GC3Dint location, GC3Dfloat v0, GC3Dfloat v1, GC3Dfloat v2)
{
    makeContextCurrent();
    ::glUniform3f(location, v0, v1, v2);
}

void GraphicsContext3D::uniform3fv(GC3Dint location, GC3Dsizei size, const GC3Dfloat* array)
{
    // FIXME: length needs to be a multiple of 3.
    makeContextCurrent();
    ::glUniform3fv(location, size, array);
}

void GraphicsContext3D::uniform4f(GC3Dint location, GC3Dfloat v0, GC3Dfloat v1, GC3Dfloat v2, GC3Dfloat v3)
{
    makeContextCurrent();
    ::glUniform4f(location, v0, v1, v2, v3);
}

void GraphicsContext3D::uniform4fv(GC3Dint location, GC3Dsizei size, const GC3Dfloat* array)
{
    // FIXME: length needs to be a multiple of 4.
    makeContextCurrent();
    ::glUniform4fv(location, size, array);
}

void GraphicsContext3D::uniform1i(GC3Dint location, GC3Dint v0)
{
    makeContextCurrent();
    ::glUniform1i(location, v0);
}

void GraphicsContext3D::uniform1iv(GC3Dint location, GC3Dsizei size, const GC3Dint* array)
{
    makeContextCurrent();
    ::glUniform1iv(location, size, array);
}

void GraphicsContext3D::uniform2i(GC3Dint location, GC3Dint v0, GC3Dint v1)
{
    makeContextCurrent();
    ::glUniform2i(location, v0, v1);
}

void GraphicsContext3D::uniform2iv(GC3Dint location, GC3Dsizei size, const GC3Dint* array)
{
    // FIXME: length needs to be a multiple of 2.
    makeContextCurrent();
    ::glUniform2iv(location, size, array);
}

void GraphicsContext3D::uniform3i(GC3Dint location, GC3Dint v0, GC3Dint v1, GC3Dint v2)
{
    makeContextCurrent();
    ::glUniform3i(location, v0, v1, v2);
}

void GraphicsContext3D::uniform3iv(GC3Dint location, GC3Dsizei size, const GC3Dint* array)
{
    // FIXME: length needs to be a multiple of 3.
    makeContextCurrent();
    ::glUniform3iv(location, size, array);
}

void GraphicsContext3D::uniform4i(GC3Dint location, GC3Dint v0, GC3Dint v1, GC3Dint v2, GC3Dint v3)
{
    makeContextCurrent();
    ::glUniform4i(location, v0, v1, v2, v3);
}

void GraphicsContext3D::uniform4iv(GC3Dint location, GC3Dsizei size, const GC3Dint* array)
{
    // FIXME: length needs to be a multiple of 4.
    makeContextCurrent();
    ::glUniform4iv(location, size, array);
}

void GraphicsContext3D::uniformMatrix2fv(GC3Dint location, GC3Dsizei size, GC3Dboolean transpose, const GC3Dfloat* array)
{
    // FIXME: length needs to be a multiple of 4.
    makeContextCurrent();
    ::glUniformMatrix2fv(location, size, transpose, array);
}

void GraphicsContext3D::uniformMatrix3fv(GC3Dint location, GC3Dsizei size, GC3Dboolean transpose, const GC3Dfloat* array)
{
    // FIXME: length needs to be a multiple of 9.
    makeContextCurrent();
    ::glUniformMatrix3fv(location, size, transpose, array);
}

void GraphicsContext3D::uniformMatrix4fv(GC3Dint location, GC3Dsizei size, GC3Dboolean transpose, const GC3Dfloat* array)
{
    // FIXME: length needs to be a multiple of 16.
    makeContextCurrent();
    ::glUniformMatrix4fv(location, size, transpose, array);
}

void GraphicsContext3D::useProgram(Platform3DObject program)
{
    makeContextCurrent();
    ::glUseProgram(program);
}

void GraphicsContext3D::validateProgram(Platform3DObject program)
{
    ASSERT(program);

    makeContextCurrent();
    ::glValidateProgram(program);
}

void GraphicsContext3D::vertexAttrib1f(GC3Duint index, GC3Dfloat v0)
{
    makeContextCurrent();
    ::glVertexAttrib1f(index, v0);
}

void GraphicsContext3D::vertexAttrib1fv(GC3Duint index, const GC3Dfloat* array)
{
    makeContextCurrent();
    ::glVertexAttrib1fv(index, array);
}

void GraphicsContext3D::vertexAttrib2f(GC3Duint index, GC3Dfloat v0, GC3Dfloat v1)
{
    makeContextCurrent();
    ::glVertexAttrib2f(index, v0, v1);
}

void GraphicsContext3D::vertexAttrib2fv(GC3Duint index, const GC3Dfloat* array)
{
    makeContextCurrent();
    ::glVertexAttrib2fv(index, array);
}

void GraphicsContext3D::vertexAttrib3f(GC3Duint index, GC3Dfloat v0, GC3Dfloat v1, GC3Dfloat v2)
{
    makeContextCurrent();
    ::glVertexAttrib3f(index, v0, v1, v2);
}

void GraphicsContext3D::vertexAttrib3fv(GC3Duint index, const GC3Dfloat* array)
{
    makeContextCurrent();
    ::glVertexAttrib3fv(index, array);
}

void GraphicsContext3D::vertexAttrib4f(GC3Duint index, GC3Dfloat v0, GC3Dfloat v1, GC3Dfloat v2, GC3Dfloat v3)
{
    makeContextCurrent();
    ::glVertexAttrib4f(index, v0, v1, v2, v3);
}

void GraphicsContext3D::vertexAttrib4fv(GC3Duint index, const GC3Dfloat* array)
{
    makeContextCurrent();
    ::glVertexAttrib4fv(index, array);
}

void GraphicsContext3D::vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dboolean normalized, GC3Dsizei stride, GC3Dintptr offset)
{
    makeContextCurrent();
    ::glVertexAttribPointer(index, size, type, normalized, stride, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContext3D::viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
    ::glViewport(x, y, width, height);
}

Platform3DObject GraphicsContext3D::createVertexArray()
{
    makeContextCurrent();
    GLuint array = 0;
#if (!USE(OPENGL_ES) && (PLATFORM(GTK) || PLATFORM(WIN))) || PLATFORM(COCOA)
    ::glGenVertexArrays(1, &array);
#endif
    return array;
}

void GraphicsContext3D::deleteVertexArray(Platform3DObject array)
{
    if (!array)
        return;
    
    makeContextCurrent();
#if (!USE(OPENGL_ES) && (PLATFORM(GTK) || PLATFORM(WIN))) || PLATFORM(COCOA)
    ::glDeleteVertexArrays(1, &array);
#endif
}

GC3Dboolean GraphicsContext3D::isVertexArray(Platform3DObject array)
{
    if (!array)
        return GL_FALSE;
    
    makeContextCurrent();
#if (!USE(OPENGL_ES) && (PLATFORM(GTK) || PLATFORM(WIN))) || PLATFORM(COCOA)
    return ::glIsVertexArray(array);
#endif
    return GL_FALSE;
}

void GraphicsContext3D::bindVertexArray(Platform3DObject array)
{
    makeContextCurrent();
#if (!USE(OPENGL_ES) && (PLATFORM(GTK) || PLATFORM(WIN))) || PLATFORM(COCOA)
    ::glBindVertexArray(array);
#else
    UNUSED_PARAM(array);
#endif
}

void GraphicsContext3D::getBooleanv(GC3Denum pname, GC3Dboolean* value)
{
    makeContextCurrent();
    ::glGetBooleanv(pname, value);
}

void GraphicsContext3D::getBufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    ::glGetBufferParameteriv(target, pname, value);
}

void GraphicsContext3D::getFloatv(GC3Denum pname, GC3Dfloat* value)
{
    makeContextCurrent();
    ::glGetFloatv(pname, value);
}
    
void GraphicsContext3D::getInteger64v(GC3Denum pname, GC3Dint64* value)
{
    UNUSED_PARAM(pname);
    makeContextCurrent();
    *value = 0;
    // FIXME 141178: Before enabling this we must first switch over to using gl3.h and creating and initialing the WebGL2 context using OpenGL ES 3.0.
    // ::glGetInteger64v(pname, value);
}

void GraphicsContext3D::getFramebufferAttachmentParameteriv(GC3Denum target, GC3Denum attachment, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    if (attachment == DEPTH_STENCIL_ATTACHMENT)
        attachment = DEPTH_ATTACHMENT; // Or STENCIL_ATTACHMENT, either works.
    ::glGetFramebufferAttachmentParameterivEXT(target, attachment, pname, value);
}

void GraphicsContext3D::getProgramiv(Platform3DObject program, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    ::glGetProgramiv(program, pname, value);
}

void GraphicsContext3D::getNonBuiltInActiveSymbolCount(Platform3DObject program, GC3Denum pname, GC3Dint* value)
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
    GC3Dint attributeCount = 0;
    ::glGetProgramiv(program, ACTIVE_ATTRIBUTES, &attributeCount);
    for (GC3Dint i = 0; i < attributeCount; ++i) {
        ActiveInfo info;
        getActiveAttribImpl(program, i, info);
        if (info.name.startsWith("gl_"))
            continue;

        symbolCounts.filteredToActualAttributeIndexMap.append(i);
    }
    
    // Do the same for uniforms.
    GC3Dint uniformCount = 0;
    ::glGetProgramiv(program, ACTIVE_UNIFORMS, &uniformCount);
    for (GC3Dint i = 0; i < uniformCount; ++i) {
        ActiveInfo info;
        getActiveUniformImpl(program, i, info);
        if (info.name.startsWith("gl_"))
            continue;
        
        symbolCounts.filteredToActualUniformIndexMap.append(i);
    }
    
    *value = symbolCounts.countForType(pname);
}

String GraphicsContext3D::getUnmangledInfoLog(Platform3DObject shaders[2], GC3Dsizei count, const String& log)
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

String GraphicsContext3D::getProgramInfoLog(Platform3DObject program)
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

    GC3Dsizei count;
    Platform3DObject shaders[2];
    getAttachedShaders(program, 2, &count, shaders);

    return getUnmangledInfoLog(shaders, count, String(info.data(), size));
}

void GraphicsContext3D::getRenderbufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    ::glGetRenderbufferParameterivEXT(target, pname, value);
}

void GraphicsContext3D::getShaderiv(Platform3DObject shader, GC3Denum pname, GC3Dint* value)
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

String GraphicsContext3D::getShaderInfoLog(Platform3DObject shader)
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

    Platform3DObject shaders[2] = { shader, 0 };
    return getUnmangledInfoLog(shaders, 1, String(info.data(), size));
}

String GraphicsContext3D::getShaderSource(Platform3DObject shader)
{
    ASSERT(shader);

    makeContextCurrent();

    const auto& result = m_shaderSourceMap.find(shader);
    if (result == m_shaderSourceMap.end())
        return String(); 

    return result->value.source;
}


void GraphicsContext3D::getTexParameterfv(GC3Denum target, GC3Denum pname, GC3Dfloat* value)
{
    makeContextCurrent();
    ::glGetTexParameterfv(target, pname, value);
}

void GraphicsContext3D::getTexParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    ::glGetTexParameteriv(target, pname, value);
}

void GraphicsContext3D::getUniformfv(Platform3DObject program, GC3Dint location, GC3Dfloat* value)
{
    makeContextCurrent();
    ::glGetUniformfv(program, location, value);
}

void GraphicsContext3D::getUniformiv(Platform3DObject program, GC3Dint location, GC3Dint* value)
{
    makeContextCurrent();
    ::glGetUniformiv(program, location, value);
}

GC3Dint GraphicsContext3D::getUniformLocation(Platform3DObject program, const String& name)
{
    ASSERT(program);

    makeContextCurrent();

    String mappedName = mappedSymbolName(program, SHADER_SYMBOL_TYPE_UNIFORM, name);
    LOG(WebGL, "::getUniformLocation is mapping %s to %s", name.utf8().data(), mappedName.utf8().data());
    return ::glGetUniformLocation(program, mappedName.utf8().data());
}

void GraphicsContext3D::getVertexAttribfv(GC3Duint index, GC3Denum pname, GC3Dfloat* value)
{
    makeContextCurrent();
    ::glGetVertexAttribfv(index, pname, value);
}

void GraphicsContext3D::getVertexAttribiv(GC3Duint index, GC3Denum pname, GC3Dint* value)
{
    makeContextCurrent();
    ::glGetVertexAttribiv(index, pname, value);
}

GC3Dsizeiptr GraphicsContext3D::getVertexAttribOffset(GC3Duint index, GC3Denum pname)
{
    makeContextCurrent();

    GLvoid* pointer = 0;
    ::glGetVertexAttribPointerv(index, pname, &pointer);
    return static_cast<GC3Dsizeiptr>(reinterpret_cast<intptr_t>(pointer));
}

void GraphicsContext3D::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoff, GC3Dint yoff, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels)
{
    makeContextCurrent();

#if !USE(OPENGL_ES)
    if (type == HALF_FLOAT_OES)
        type = GL_HALF_FLOAT_ARB;
#endif

    if (m_usingCoreProfile)  {
        // There are some format values used in WebGL that are deprecated when using a core profile, so we need
        // to adapt them, as we do in GraphicsContext3D::texImage2D().
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

void GraphicsContext3D::compressedTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Dsizei imageSize, const void* data)
{
    makeContextCurrent();
    ::glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContext3D::compressedTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Dsizei imageSize, const void* data)
{
    makeContextCurrent();
    ::glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

Platform3DObject GraphicsContext3D::createBuffer()
{
    makeContextCurrent();
    GLuint o = 0;
    glGenBuffers(1, &o);
    return o;
}

Platform3DObject GraphicsContext3D::createFramebuffer()
{
    makeContextCurrent();
    GLuint o = 0;
    glGenFramebuffersEXT(1, &o);
    return o;
}

Platform3DObject GraphicsContext3D::createProgram()
{
    makeContextCurrent();
    return glCreateProgram();
}

Platform3DObject GraphicsContext3D::createRenderbuffer()
{
    makeContextCurrent();
    GLuint o = 0;
    glGenRenderbuffersEXT(1, &o);
    return o;
}

Platform3DObject GraphicsContext3D::createShader(GC3Denum type)
{
    makeContextCurrent();
    return glCreateShader((type == FRAGMENT_SHADER) ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
}

Platform3DObject GraphicsContext3D::createTexture()
{
    makeContextCurrent();
    GLuint o = 0;
    glGenTextures(1, &o);
    m_state.textureSeedCount.add(o);
    return o;
}

void GraphicsContext3D::deleteBuffer(Platform3DObject buffer)
{
    makeContextCurrent();
    glDeleteBuffers(1, &buffer);
}

void GraphicsContext3D::deleteFramebuffer(Platform3DObject framebuffer)
{
    makeContextCurrent();
    if (framebuffer == m_state.boundFBO) {
        // Make sure the framebuffer is not going to be used for drawing
        // operations after it gets deleted.
        bindFramebuffer(FRAMEBUFFER, 0);
    }
    glDeleteFramebuffersEXT(1, &framebuffer);
}

void GraphicsContext3D::deleteProgram(Platform3DObject program)
{
    makeContextCurrent();
    m_shaderProgramSymbolCountMap.remove(program);
    glDeleteProgram(program);
}

void GraphicsContext3D::deleteRenderbuffer(Platform3DObject renderbuffer)
{
    makeContextCurrent();
    glDeleteRenderbuffersEXT(1, &renderbuffer);
}

void GraphicsContext3D::deleteShader(Platform3DObject shader)
{
    makeContextCurrent();
    glDeleteShader(shader);
}

void GraphicsContext3D::deleteTexture(Platform3DObject texture)
{
    makeContextCurrent();
    m_state.boundTextureMap.removeIf([texture] (auto& keyValue) {
        return keyValue.value.first == texture;
    });
    glDeleteTextures(1, &texture);
    m_state.textureSeedCount.removeAll(texture);
}

void GraphicsContext3D::synthesizeGLError(GC3Denum error)
{
    // Need to move the current errors to the synthetic error list to
    // preserve the order of errors, so a caller to getError will get
    // any errors from glError before the error we are synthesizing.
    moveErrorsToSyntheticErrorList();
    m_syntheticErrors.add(error);
}

void GraphicsContext3D::markContextChanged()
{
    m_layerComposited = false;
}

void GraphicsContext3D::markLayerComposited()
{
    m_layerComposited = true;
}

bool GraphicsContext3D::layerComposited() const
{
    return m_layerComposited;
}

void GraphicsContext3D::forceContextLost()
{
#if ENABLE(WEBGL)
    if (m_webglContext)
        m_webglContext->forceLostContext(WebGLRenderingContextBase::RealLostContext);
#endif
}

void GraphicsContext3D::recycleContext()
{
#if ENABLE(WEBGL)
    if (m_webglContext)
        m_webglContext->recycleContext();
#endif
}

void GraphicsContext3D::dispatchContextChangedNotification()
{
#if ENABLE(WEBGL)
    if (m_webglContext)
        m_webglContext->dispatchContextChangedEvent();
#endif
}

void GraphicsContext3D::texImage2DDirect(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels)
{
    makeContextCurrent();
    ::glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
    m_state.textureSeedCount.add(m_state.currentBoundTexture());
}

void GraphicsContext3D::drawArraysInstanced(GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei primcount)
{
    getExtensions().drawArraysInstanced(mode, first, count, primcount);
    checkGPUStatus();
}

void GraphicsContext3D::drawElementsInstanced(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset, GC3Dsizei primcount)
{
    getExtensions().drawElementsInstanced(mode, count, type, offset, primcount);
    checkGPUStatus();
}

void GraphicsContext3D::vertexAttribDivisor(GC3Duint index, GC3Duint divisor)
{
    getExtensions().vertexAttribDivisor(index, divisor);
}

#if USE(OPENGL) && ENABLE(WEBGL2)
void GraphicsContext3D::primitiveRestartIndex(GC3Duint index)
{
    makeContextCurrent();
    ::glPrimitiveRestartIndex(index);
}
#endif

}

#endif // ENABLE(GRAPHICS_CONTEXT_3D)
