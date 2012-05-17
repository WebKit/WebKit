/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef GraphicsContext3DPrivate_h
#define GraphicsContext3DPrivate_h

#include "Extensions3DChromium.h"
#include "GraphicsContext3D.h"
#include "SkBitmap.h"
#include <wtf/HashSet.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/OwnPtr.h>

class GrContext;

namespace WebKit {
class WebGraphicsContext3D;
}

namespace WebCore {

class DrawingBuffer;
class Extensions3DChromium;
class GraphicsContextLostCallbackAdapter;
class GraphicsContext3DSwapBuffersCompleteCallbackAdapter;
class GraphicsErrorMessageCallbackAdapter;
class GraphicsContext3DMemoryAllocationChangedCallbackAdapter;

class GraphicsContext3DPrivate {
public:
    // Callers must make the context current before using it AND check that the context was created successfully
    // via ContextLost before using the context in any way. Once made current on a thread, the context cannot
    // be used on any other thread.
    static PassRefPtr<GraphicsContext3D> createGraphicsContextFromWebContext(PassOwnPtr<WebKit::WebGraphicsContext3D>, GraphicsContext3D::RenderStyle, bool preserveDrawingBuffer = false);

    virtual ~GraphicsContext3DPrivate();

    // Helper function to provide access to the lower-level WebGraphicsContext3D,
    // which is needed for subordinate contexts like WebGL's to share resources
    // with the compositor's context.
    static WebKit::WebGraphicsContext3D* extractWebGraphicsContext3D(GraphicsContext3D*);

    PlatformGraphicsContext3D platformGraphicsContext3D() const;
    Platform3DObject platformTexture() const;
    GrContext* grContext();

    bool makeContextCurrent();

    void reshape(int width, int height);
    IntSize getInternalFramebufferSize() const;
    bool isResourceSafe();

    void markContextChanged();
    bool layerComposited() const;
    void markLayerComposited();

    void paintRenderingResultsToCanvas(ImageBuffer*, DrawingBuffer*);
    void paintFramebufferToCanvas(int framebuffer, int width, int height, bool premultiplyAlpha, ImageBuffer*);
    PassRefPtr<ImageData> paintRenderingResultsToImageData(DrawingBuffer*);
    bool paintCompositedResultsToCanvas(ImageBuffer*);

    void prepareTexture();

    // CHROMIUM_post_sub_buffer
    void postSubBufferCHROMIUM(int x, int y, int width, int height);

    bool isGLES2Compliant() const;

    void releaseShaderCompiler();
    bool isContextLost();

    //----------------------------------------------------------------------
    // Entry points for WebGL.
    //
    void activeTexture(GC3Denum texture);
    void attachShader(Platform3DObject program, Platform3DObject shader);
    void bindAttribLocation(Platform3DObject, GC3Duint index, const String& name);
    void bindBuffer(GC3Denum target, Platform3DObject);
    void bindFramebuffer(GC3Denum target, Platform3DObject);
    void bindRenderbuffer(GC3Denum target, Platform3DObject);
    void bindTexture(GC3Denum target, Platform3DObject);
    void blendColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha);
    void blendEquation(GC3Denum mode);
    void blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha);
    void blendFunc(GC3Denum sfactor, GC3Denum dfactor);
    void blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB, GC3Denum srcAlpha, GC3Denum dstAlpha);

    void bufferData(GC3Denum target, GC3Dsizeiptr, GC3Denum usage);
    void bufferData(GC3Denum target, GC3Dsizeiptr, const void* data, GC3Denum usage);
    void bufferSubData(GC3Denum target, GC3Dintptr offset, GC3Dsizeiptr, const void* data);

    GC3Denum checkFramebufferStatus(GC3Denum target);
    void clear(GC3Dbitfield mask);
    void clearColor(GC3Dclampf red, GC3Dclampf green, GC3Dclampf blue, GC3Dclampf alpha);
    void clearDepth(GC3Dclampf depth);
    void clearStencil(GC3Dint s);
    void colorMask(GC3Dboolean red, GC3Dboolean green, GC3Dboolean blue, GC3Dboolean alpha);
    void compileShader(Platform3DObject);

    void compressedTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Dsizei imageSize, const void* data);
    void compressedTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Dsizei imageSize, const void* data);
    void copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border);
    void copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);
    void cullFace(GC3Denum mode);
    void depthFunc(GC3Denum func);
    void depthMask(GC3Dboolean flag);
    void depthRange(GC3Dclampf zNear, GC3Dclampf zFar);
    void detachShader(Platform3DObject, Platform3DObject);
    void disable(GC3Denum cap);
    void disableVertexAttribArray(GC3Duint index);
    void drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count);
    void drawElements(GC3Denum mode, GC3Dsizei count, GC3Denum type, GC3Dintptr offset);

    void enable(GC3Denum cap);
    void enableVertexAttribArray(GC3Duint index);
    void finish();
    void flush();
    void framebufferRenderbuffer(GC3Denum target, GC3Denum attachment, GC3Denum renderbuffertarget, Platform3DObject);
    void framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum textarget, Platform3DObject, GC3Dint level);
    void frontFace(GC3Denum mode);
    void generateMipmap(GC3Denum target);

    bool getActiveAttrib(Platform3DObject program, GC3Duint index, ActiveInfo&);
    bool getActiveUniform(Platform3DObject program, GC3Duint index, ActiveInfo&);
    void getAttachedShaders(Platform3DObject program, GC3Dsizei maxCount, GC3Dsizei* count, Platform3DObject* shaders);
    GC3Dint getAttribLocation(Platform3DObject, const String& name);
    void getBooleanv(GC3Denum pname, GC3Dboolean* value);
    void getBufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value);
    GraphicsContext3D::Attributes getContextAttributes();
    GC3Denum getError();
    void getFloatv(GC3Denum pname, GC3Dfloat* value);
    void getFramebufferAttachmentParameteriv(GC3Denum target, GC3Denum attachment, GC3Denum pname, GC3Dint* value);
    void getIntegerv(GC3Denum pname, GC3Dint* value);
    void getProgramiv(Platform3DObject program, GC3Denum pname, GC3Dint* value);
    String getProgramInfoLog(Platform3DObject);
    void getRenderbufferParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value);
    void getShaderiv(Platform3DObject, GC3Denum pname, GC3Dint* value);
    String getShaderInfoLog(Platform3DObject);
    void getShaderPrecisionFormat(GC3Denum shaderType, GC3Denum precisionType, GC3Dint* range, GC3Dint* precision);

    String getShaderSource(Platform3DObject);
    String getString(GC3Denum name);
    void getTexParameterfv(GC3Denum target, GC3Denum pname, GC3Dfloat* value);
    void getTexParameteriv(GC3Denum target, GC3Denum pname, GC3Dint* value);
    void getUniformfv(Platform3DObject program, GC3Dint location, GC3Dfloat* value);
    void getUniformiv(Platform3DObject program, GC3Dint location, GC3Dint* value);
    GC3Dint getUniformLocation(Platform3DObject, const String& name);
    void getVertexAttribfv(GC3Duint index, GC3Denum pname, GC3Dfloat* value);
    void getVertexAttribiv(GC3Duint index, GC3Denum pname, GC3Dint* value);
    GC3Dsizeiptr getVertexAttribOffset(GC3Duint index, GC3Denum pname);

    void hint(GC3Denum target, GC3Denum mode);
    GC3Dboolean isBuffer(Platform3DObject);
    GC3Dboolean isEnabled(GC3Denum cap);
    GC3Dboolean isFramebuffer(Platform3DObject);
    GC3Dboolean isProgram(Platform3DObject);
    GC3Dboolean isRenderbuffer(Platform3DObject);
    GC3Dboolean isShader(Platform3DObject);
    GC3Dboolean isTexture(Platform3DObject);
    void lineWidth(GC3Dfloat);
    void linkProgram(Platform3DObject);
    void pixelStorei(GC3Denum pname, GC3Dint param);
    void polygonOffset(GC3Dfloat factor, GC3Dfloat units);

    void readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data);

    void renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height);
    void sampleCoverage(GC3Dclampf value, GC3Dboolean invert);
    void scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);
    void shaderSource(Platform3DObject, const String&);
    void stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask);
    void stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask);
    void stencilMask(GC3Duint mask);
    void stencilMaskSeparate(GC3Denum face, GC3Duint mask);
    void stencilOp(GC3Denum fail, GC3Denum zfail, GC3Denum zpass);
    void stencilOpSeparate(GC3Denum face, GC3Denum fail, GC3Denum zfail, GC3Denum zpass);

    // texImage2D return false on any error rather than using an ExceptionCode.
    bool texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels);
    void texParameterf(GC3Denum target, GC3Denum pname, GC3Dfloat param);
    void texParameteri(GC3Denum target, GC3Denum pname, GC3Dint param);
    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, const void* pixels);

    void uniform1f(GC3Dint location, GC3Dfloat x);
    void uniform1fv(GC3Dint location, GC3Dsizei, GC3Dfloat* v);
    void uniform1i(GC3Dint location, GC3Dint x);
    void uniform1iv(GC3Dint location, GC3Dsizei, GC3Dint* v);
    void uniform2f(GC3Dint location, GC3Dfloat x, float y);
    void uniform2fv(GC3Dint location, GC3Dsizei, GC3Dfloat* v);
    void uniform2i(GC3Dint location, GC3Dint x, GC3Dint y);
    void uniform2iv(GC3Dint location, GC3Dsizei, GC3Dint* v);
    void uniform3f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z);
    void uniform3fv(GC3Dint location, GC3Dsizei, GC3Dfloat* v);
    void uniform3i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z);
    void uniform3iv(GC3Dint location, GC3Dsizei, GC3Dint* v);
    void uniform4f(GC3Dint location, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w);
    void uniform4fv(GC3Dint location, GC3Dsizei, GC3Dfloat* v);
    void uniform4i(GC3Dint location, GC3Dint x, GC3Dint y, GC3Dint z, GC3Dint w);
    void uniform4iv(GC3Dint location, GC3Dsizei, GC3Dint* v);
    void uniformMatrix2fv(GC3Dint location, GC3Dsizei, GC3Dboolean transpose, GC3Dfloat* value);
    void uniformMatrix3fv(GC3Dint location, GC3Dsizei, GC3Dboolean transpose, GC3Dfloat* value);
    void uniformMatrix4fv(GC3Dint location, GC3Dsizei, GC3Dboolean transpose, GC3Dfloat* value);

    void useProgram(Platform3DObject);
    void validateProgram(Platform3DObject);

    void vertexAttrib1f(GC3Duint index, GC3Dfloat x);
    void vertexAttrib1fv(GC3Duint index, GC3Dfloat* values);
    void vertexAttrib2f(GC3Duint index, GC3Dfloat x, GC3Dfloat y);
    void vertexAttrib2fv(GC3Duint index, GC3Dfloat* values);
    void vertexAttrib3f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z);
    void vertexAttrib3fv(GC3Duint index, GC3Dfloat* values);
    void vertexAttrib4f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w);
    void vertexAttrib4fv(GC3Duint index, GC3Dfloat* values);
    void vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dboolean normalized,
                             GC3Dsizei stride, GC3Dintptr offset);

    void viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);

    Platform3DObject createBuffer();
    Platform3DObject createFramebuffer();
    Platform3DObject createProgram();
    Platform3DObject createRenderbuffer();
    Platform3DObject createShader(GC3Denum);
    Platform3DObject createTexture();

    void deleteBuffer(Platform3DObject);
    void deleteFramebuffer(Platform3DObject);
    void deleteProgram(Platform3DObject);
    void deleteRenderbuffer(Platform3DObject);
    void deleteShader(Platform3DObject);
    void deleteTexture(Platform3DObject);

    void synthesizeGLError(GC3Denum error);

    void setContextLostCallback(PassOwnPtr<GraphicsContext3D::ContextLostCallback>);
    void setErrorMessageCallback(PassOwnPtr<GraphicsContext3D::ErrorMessageCallback>);

    // Extensions3D support.
    Extensions3D* getExtensions();
    bool supportsExtension(const String& name);
    bool ensureExtensionEnabled(const String& name);
    bool isExtensionEnabled(const String& name);

    // EXT_texture_format_BGRA8888
    bool supportsBGRA();

    // GL_CHROMIUM_map_sub
    bool supportsMapSubCHROMIUM();
    void* mapBufferSubDataCHROMIUM(GC3Denum target, GC3Dsizeiptr offset, GC3Dsizei, GC3Denum access);
    void unmapBufferSubDataCHROMIUM(const void*);
    void* mapTexSubImage2DCHROMIUM(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, GC3Denum access);
    void unmapTexSubImage2DCHROMIUM(const void*);

    // GL_CHROMIUM_set_visibility
    void setVisibilityCHROMIUM(bool);

    // GL_EXT_discard_framebuffer
    virtual void discardFramebufferEXT(GC3Denum target, GC3Dsizei numAttachments, const GC3Denum* attachments);
    virtual void ensureFramebufferCHROMIUM();

    // GL_CHROMIUM_gpu_memory_manager
    void setGpuMemoryAllocationChangedCallbackCHROMIUM(PassOwnPtr<Extensions3DChromium::GpuMemoryAllocationChangedCallbackCHROMIUM>);

    // GL_CHROMIUM_framebuffer_multisample
    void blitFramebufferCHROMIUM(GC3Dint srcX0, GC3Dint srcY0, GC3Dint srcX1, GC3Dint srcY1, GC3Dint dstX0, GC3Dint dstY0, GC3Dint dstX1, GC3Dint dstY1, GC3Dbitfield mask, GC3Denum filter);
    void renderbufferStorageMultisampleCHROMIUM(GC3Denum target, GC3Dsizei samples, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height);

    // GL_CHROMIUM_swapbuffers_complete_callback
    void setSwapBuffersCompleteCallbackCHROMIUM(PassOwnPtr<Extensions3DChromium::SwapBuffersCompleteCallbackCHROMIUM>);

    // GL_CHROMIUM_rate_limit_offscreen_context
    void rateLimitOffscreenContextCHROMIUM();

    // GL_ARB_robustness
    GC3Denum getGraphicsResetStatusARB();

    // GL_ANGLE_translated_shader_source
    String getTranslatedShaderSourceANGLE(Platform3DObject shader);

    // GL_CHROMIUM_iosurface
    void texImageIOSurface2DCHROMIUM(GC3Denum target, GC3Dint width, GC3Dint height, GC3Duint ioSurfaceId, GC3Duint plane);

    // GL_EXT_texture_storage
    void texStorage2DEXT(GC3Denum target, GC3Dint levels, GC3Duint internalformat, GC3Dint width, GC3Dint height);

    // GL_EXT_occlusion_query
    Platform3DObject createQueryEXT();
    void deleteQueryEXT(Platform3DObject);
    GC3Dboolean isQueryEXT(Platform3DObject);
    void beginQueryEXT(GC3Denum, Platform3DObject);
    void endQueryEXT(GC3Denum);
    void getQueryivEXT(GC3Denum, GC3Denum, GC3Dint*);
    void getQueryObjectuivEXT(Platform3DObject, GC3Denum, GC3Duint*);

private:
    GraphicsContext3DPrivate(PassOwnPtr<WebKit::WebGraphicsContext3D>, bool preserveDrawingBuffer);

    void initializeExtensions();

    OwnPtr<WebKit::WebGraphicsContext3D> m_impl;
    OwnPtr<Extensions3DChromium> m_extensions;
    OwnPtr<GraphicsContextLostCallbackAdapter> m_contextLostCallbackAdapter;
    OwnPtr<GraphicsErrorMessageCallbackAdapter> m_errorMessageCallbackAdapter;
    OwnPtr<GraphicsContext3DSwapBuffersCompleteCallbackAdapter> m_swapBuffersCompleteCallbackAdapter;
    OwnPtr<GraphicsContext3DMemoryAllocationChangedCallbackAdapter> m_memoryAllocationChangedCallbackAdapter;
    bool m_initializedAvailableExtensions;
    HashSet<String> m_enabledExtensions;
    HashSet<String> m_requestableExtensions;
    bool m_layerComposited;
    bool m_preserveDrawingBuffer;

    enum ResourceSafety {
        ResourceSafetyUnknown,
        ResourceSafe,
        ResourceUnsafe
    };
    ResourceSafety m_resourceSafety;

    // If the width and height of the Canvas's backing store don't
    // match those that we were given in the most recent call to
    // reshape(), then we need an intermediate bitmap to read back the
    // frame buffer into. This seems to happen when CSS styles are
    // used to resize the Canvas.
    SkBitmap m_resizingBitmap;

    GrContext* m_grContext;
};

} // namespace WebCore

#endif // GraphicsContext3DPrivate_h
