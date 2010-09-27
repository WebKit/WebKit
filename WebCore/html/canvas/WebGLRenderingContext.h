/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef WebGLRenderingContext_h
#define WebGLRenderingContext_h

#include "CanvasRenderingContext.h"
#include "ExceptionCode.h"
#include "Float32Array.h"
#include "GraphicsContext3D.h"
#include "Int32Array.h"
#include "PlatformString.h"
#include "Uint8Array.h"
#include "WebGLGetInfo.h"

#include <wtf/OwnArrayPtr.h>

namespace WebCore {

class WebGLActiveInfo;
class WebGLBuffer;
class WebGLContextAttributes;
class WebGLFramebuffer;
class WebGLObject;
class WebGLProgram;
class WebGLRenderbuffer;
class WebGLShader;
class WebGLTexture;
class WebGLUniformLocation;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBuffer;
class ImageData;
class IntSize;
class WebKitCSSMatrix;

class WebGLRenderingContext : public CanvasRenderingContext {
public:
    static PassOwnPtr<WebGLRenderingContext> create(HTMLCanvasElement*, WebGLContextAttributes*);
    virtual ~WebGLRenderingContext();

    virtual bool is3d() const { return true; }
    virtual bool isAccelerated() const { return true; }
    virtual bool paintsIntoCanvasBuffer() const;

    void activeTexture(unsigned long texture, ExceptionCode& ec);
    void attachShader(WebGLProgram*, WebGLShader*, ExceptionCode& ec);
    void bindAttribLocation(WebGLProgram*, unsigned long index, const String& name, ExceptionCode& ec);
    void bindBuffer(unsigned long target, WebGLBuffer*, ExceptionCode& ec);
    void bindFramebuffer(unsigned long target, WebGLFramebuffer*, ExceptionCode& ec);
    void bindRenderbuffer(unsigned long target, WebGLRenderbuffer*, ExceptionCode& ec);
    void bindTexture(unsigned long target, WebGLTexture*, ExceptionCode& ec);
    void blendColor(double red, double green, double blue, double alpha);
    void blendEquation(unsigned long mode);
    void blendEquationSeparate(unsigned long modeRGB, unsigned long modeAlpha);
    void blendFunc(unsigned long sfactor, unsigned long dfactor);
    void blendFuncSeparate(unsigned long srcRGB, unsigned long dstRGB, unsigned long srcAlpha, unsigned long dstAlpha);

    void bufferData(unsigned long target, int size, unsigned long usage, ExceptionCode&);
    void bufferData(unsigned long target, ArrayBuffer* data, unsigned long usage, ExceptionCode&);
    void bufferData(unsigned long target, ArrayBufferView* data, unsigned long usage, ExceptionCode&);
    void bufferSubData(unsigned long target, long offset, ArrayBuffer* data, ExceptionCode&);
    void bufferSubData(unsigned long target, long offset, ArrayBufferView* data, ExceptionCode&);

    unsigned long checkFramebufferStatus(unsigned long target);
    void clear(unsigned long mask);
    void clearColor(double red, double green, double blue, double alpha);
    void clearDepth(double);
    void clearStencil(long);
    void colorMask(bool red, bool green, bool blue, bool alpha);
    void compileShader(WebGLShader*, ExceptionCode& ec);

    // void compressedTexImage2D(unsigned long target, long level, unsigned long internalformat, unsigned long width, unsigned long height, long border, unsigned long imageSize, const void* data);
    // void compressedTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, unsigned long width, unsigned long height, unsigned long format, unsigned long imageSize, const void* data);

    void copyTexImage2D(unsigned long target, long level, unsigned long internalformat, long x, long y, unsigned long width, unsigned long height, long border);
    void copyTexSubImage2D(unsigned long target, long level, long xoffset, long yoffset, long x, long y, unsigned long width, unsigned long height);

    PassRefPtr<WebGLBuffer> createBuffer();
    PassRefPtr<WebGLFramebuffer> createFramebuffer();
    PassRefPtr<WebGLProgram> createProgram();
    PassRefPtr<WebGLRenderbuffer> createRenderbuffer();
    PassRefPtr<WebGLShader> createShader(unsigned long type, ExceptionCode&);
    PassRefPtr<WebGLTexture> createTexture();

    void cullFace(unsigned long mode);

    void deleteBuffer(WebGLBuffer*);
    void deleteFramebuffer(WebGLFramebuffer*);
    void deleteProgram(WebGLProgram*);
    void deleteRenderbuffer(WebGLRenderbuffer*);
    void deleteShader(WebGLShader*);
    void deleteTexture(WebGLTexture*);

    void depthFunc(unsigned long);
    void depthMask(bool);
    void depthRange(double zNear, double zFar);
    void detachShader(WebGLProgram*, WebGLShader*, ExceptionCode&);
    void disable(unsigned long cap);
    void disableVertexAttribArray(unsigned long index, ExceptionCode&);
    void drawArrays(unsigned long mode, long first, long count, ExceptionCode&);
    void drawElements(unsigned long mode, long count, unsigned long type, long offset, ExceptionCode&);

    void enable(unsigned long cap);
    void enableVertexAttribArray(unsigned long index, ExceptionCode&);
    void finish();
    void flush();
    void framebufferRenderbuffer(unsigned long target, unsigned long attachment, unsigned long renderbuffertarget, WebGLRenderbuffer*, ExceptionCode& ec);
    void framebufferTexture2D(unsigned long target, unsigned long attachment, unsigned long textarget, WebGLTexture*, long level, ExceptionCode& ec);
    void frontFace(unsigned long mode);
    void generateMipmap(unsigned long target);

    PassRefPtr<WebGLActiveInfo> getActiveAttrib(WebGLProgram*, unsigned long index, ExceptionCode&);
    PassRefPtr<WebGLActiveInfo> getActiveUniform(WebGLProgram*, unsigned long index, ExceptionCode&);

    bool getAttachedShaders(WebGLProgram*, Vector<WebGLShader*>&, ExceptionCode&);

    int getAttribLocation(WebGLProgram*, const String& name);

    WebGLGetInfo getBufferParameter(unsigned long target, unsigned long pname, ExceptionCode&);

    PassRefPtr<WebGLContextAttributes> getContextAttributes();

    unsigned long getError();

    WebGLGetInfo getFramebufferAttachmentParameter(unsigned long target, unsigned long attachment, unsigned long pname, ExceptionCode&);

    WebGLGetInfo getParameter(unsigned long pname, ExceptionCode&);

    WebGLGetInfo getProgramParameter(WebGLProgram*, unsigned long pname, ExceptionCode&);

    String getProgramInfoLog(WebGLProgram*, ExceptionCode& ec);

    WebGLGetInfo getRenderbufferParameter(unsigned long target, unsigned long pname, ExceptionCode&);

    WebGLGetInfo getShaderParameter(WebGLShader*, unsigned long pname, ExceptionCode& ec);

    String getShaderInfoLog(WebGLShader*, ExceptionCode& ec);

    // TBD
    // void glGetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision);

    String getShaderSource(WebGLShader*, ExceptionCode&);

    WebGLGetInfo getTexParameter(unsigned long target, unsigned long pname, ExceptionCode&);

    WebGLGetInfo getUniform(WebGLProgram*, const WebGLUniformLocation*, ExceptionCode&);

    PassRefPtr<WebGLUniformLocation> getUniformLocation(WebGLProgram*, const String&, ExceptionCode&);

    WebGLGetInfo getVertexAttrib(unsigned long index, unsigned long pname, ExceptionCode&);

    long getVertexAttribOffset(unsigned long index, unsigned long pname);

    void hint(unsigned long target, unsigned long mode);
    bool isBuffer(WebGLBuffer*);
    bool isEnabled(unsigned long cap);
    bool isFramebuffer(WebGLFramebuffer*);
    bool isProgram(WebGLProgram*);
    bool isRenderbuffer(WebGLRenderbuffer*);
    bool isShader(WebGLShader*);
    bool isTexture(WebGLTexture*);
    void lineWidth(double);
    void linkProgram(WebGLProgram*, ExceptionCode&);
    void pixelStorei(unsigned long pname, long param);
    void polygonOffset(double factor, double units);
    void readPixels(long x, long y, long width, long height, unsigned long format, unsigned long type, ArrayBufferView* pixels);
    void releaseShaderCompiler();
    void renderbufferStorage(unsigned long target, unsigned long internalformat, unsigned long width, unsigned long height);
    void sampleCoverage(double value, bool invert);
    void scissor(long x, long y, unsigned long width, unsigned long height);
    void shaderSource(WebGLShader*, const String&, ExceptionCode&);
    void stencilFunc(unsigned long func, long ref, unsigned long mask);
    void stencilFuncSeparate(unsigned long face, unsigned long func, long ref, unsigned long mask);
    void stencilMask(unsigned long);
    void stencilMaskSeparate(unsigned long face, unsigned long mask);
    void stencilOp(unsigned long fail, unsigned long zfail, unsigned long zpass);
    void stencilOpSeparate(unsigned long face, unsigned long fail, unsigned long zfail, unsigned long zpass);

    void texImage2D(unsigned target, unsigned level, unsigned internalformat,
                    unsigned width, unsigned height, unsigned border,
                    unsigned format, unsigned type, ArrayBufferView* pixels, ExceptionCode&);
    void texImage2D(unsigned target, unsigned level, unsigned internalformat,
                    unsigned format, unsigned type, ImageData* pixels, ExceptionCode&);
    void texImage2D(unsigned target, unsigned level, unsigned internalformat,
                    unsigned format, unsigned type, HTMLImageElement* image, ExceptionCode&);
    void texImage2D(unsigned target, unsigned level, unsigned internalformat,
                    unsigned format, unsigned type, HTMLCanvasElement* canvas, ExceptionCode&);
    void texImage2D(unsigned target, unsigned level, unsigned internalformat,
                    unsigned format, unsigned type, HTMLVideoElement* video, ExceptionCode&);

    void texParameterf(unsigned target, unsigned pname, float param);
    void texParameteri(unsigned target, unsigned pname, int param);

    void texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                       unsigned width, unsigned height,
                       unsigned format, unsigned type, ArrayBufferView* pixels, ExceptionCode&);
    void texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                       unsigned format, unsigned type, ImageData* pixels, ExceptionCode&);
    void texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                       unsigned format, unsigned type, HTMLImageElement* image, ExceptionCode&);
    void texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                       unsigned format, unsigned type, HTMLCanvasElement* canvas, ExceptionCode&);
    void texSubImage2D(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                       unsigned format, unsigned type, HTMLVideoElement* video, ExceptionCode&);

    void uniform1f(const WebGLUniformLocation* location, float x, ExceptionCode&);
    void uniform1fv(const WebGLUniformLocation* location, Float32Array* v, ExceptionCode&);
    void uniform1fv(const WebGLUniformLocation* location, float* v, int size, ExceptionCode&);
    void uniform1i(const WebGLUniformLocation* location, int x, ExceptionCode&);
    void uniform1iv(const WebGLUniformLocation* location, Int32Array* v, ExceptionCode&);
    void uniform1iv(const WebGLUniformLocation* location, int* v, int size, ExceptionCode&);
    void uniform2f(const WebGLUniformLocation* location, float x, float y, ExceptionCode&);
    void uniform2fv(const WebGLUniformLocation* location, Float32Array* v, ExceptionCode&);
    void uniform2fv(const WebGLUniformLocation* location, float* v, int size, ExceptionCode&);
    void uniform2i(const WebGLUniformLocation* location, int x, int y, ExceptionCode&);
    void uniform2iv(const WebGLUniformLocation* location, Int32Array* v, ExceptionCode&);
    void uniform2iv(const WebGLUniformLocation* location, int* v, int size, ExceptionCode&);
    void uniform3f(const WebGLUniformLocation* location, float x, float y, float z, ExceptionCode&);
    void uniform3fv(const WebGLUniformLocation* location, Float32Array* v, ExceptionCode&);
    void uniform3fv(const WebGLUniformLocation* location, float* v, int size, ExceptionCode&);
    void uniform3i(const WebGLUniformLocation* location, int x, int y, int z, ExceptionCode&);
    void uniform3iv(const WebGLUniformLocation* location, Int32Array* v, ExceptionCode&);
    void uniform3iv(const WebGLUniformLocation* location, int* v, int size, ExceptionCode&);
    void uniform4f(const WebGLUniformLocation* location, float x, float y, float z, float w, ExceptionCode&);
    void uniform4fv(const WebGLUniformLocation* location, Float32Array* v, ExceptionCode&);
    void uniform4fv(const WebGLUniformLocation* location, float* v, int size, ExceptionCode&);
    void uniform4i(const WebGLUniformLocation* location, int x, int y, int z, int w, ExceptionCode&);
    void uniform4iv(const WebGLUniformLocation* location, Int32Array* v, ExceptionCode&);
    void uniform4iv(const WebGLUniformLocation* location, int* v, int size, ExceptionCode&);
    void uniformMatrix2fv(const WebGLUniformLocation* location, bool transpose, Float32Array* value, ExceptionCode&);
    void uniformMatrix2fv(const WebGLUniformLocation* location, bool transpose, float* value, int size, ExceptionCode&);
    void uniformMatrix3fv(const WebGLUniformLocation* location, bool transpose, Float32Array* value, ExceptionCode&);
    void uniformMatrix3fv(const WebGLUniformLocation* location, bool transpose, float* value, int size, ExceptionCode&);
    void uniformMatrix4fv(const WebGLUniformLocation* location, bool transpose, Float32Array* value, ExceptionCode&);
    void uniformMatrix4fv(const WebGLUniformLocation* location, bool transpose, float* value, int size, ExceptionCode&);

    void useProgram(WebGLProgram*, ExceptionCode&);
    void validateProgram(WebGLProgram*, ExceptionCode&);

    void vertexAttrib1f(unsigned long index, float x);
    void vertexAttrib1fv(unsigned long index, Float32Array* values);
    void vertexAttrib1fv(unsigned long index, float* values, int size);
    void vertexAttrib2f(unsigned long index, float x, float y);
    void vertexAttrib2fv(unsigned long index, Float32Array* values);
    void vertexAttrib2fv(unsigned long index, float* values, int size);
    void vertexAttrib3f(unsigned long index, float x, float y, float z);
    void vertexAttrib3fv(unsigned long index, Float32Array* values);
    void vertexAttrib3fv(unsigned long index, float* values, int size);
    void vertexAttrib4f(unsigned long index, float x, float y, float z, float w);
    void vertexAttrib4fv(unsigned long index, Float32Array* values);
    void vertexAttrib4fv(unsigned long index, float* values, int size);
    void vertexAttribPointer(unsigned long index, long size, unsigned long type, bool normalized,
                             long stride, long offset, ExceptionCode&);

    void viewport(long x, long y, unsigned long width, unsigned long height);

    GraphicsContext3D* graphicsContext3D() const { return m_context.get(); }
#if USE(ACCELERATED_COMPOSITING)
    virtual PlatformLayer* platformLayer() const { return m_context->platformLayer(); }
#endif

    void reshape(int width, int height);

    virtual void paintRenderingResultsToCanvas();

    void removeObject(WebGLObject*);

  private:
    friend class WebGLObject;

    WebGLRenderingContext(HTMLCanvasElement*, PassOwnPtr<GraphicsContext3D>);

    void addObject(WebGLObject*);
    void detachAndRemoveAllObjects();
    WebGLTexture* findTexture(Platform3DObject);
    WebGLRenderbuffer* findRenderbuffer(Platform3DObject);
    WebGLBuffer* findBuffer(Platform3DObject);
    WebGLShader* findShader(Platform3DObject);

    void markContextChanged();
    void cleanupAfterGraphicsCall(bool changed)
    {
        if (changed)
            markContextChanged();
    }

    bool isGLES2Compliant();
    bool isGLES2NPOTStrict();
    bool isErrorGeneratedOnOutOfBoundsAccesses();

    // Helper to return the size in bytes of OpenGL data types
    // like GL_FLOAT, GL_INT, etc.
    int sizeInBytes(int type);

    // Basic validation of count and offset against number of elements in element array buffer
    bool validateElementArraySize(unsigned long count, unsigned long type, long offset);

    // Conservative but quick index validation
    bool validateIndexArrayConservative(unsigned long type, long& numElementsRequired);

    // Precise but slow index validation -- only done if conservative checks fail
    bool validateIndexArrayPrecise(unsigned long count, unsigned long type, long offset, long& numElementsRequired);
    bool validateRenderingState(long numElements);

    bool validateWebGLObject(WebGLObject* object);

    PassRefPtr<Image> videoFrameToImage(HTMLVideoElement* video);

    OwnPtr<GraphicsContext3D> m_context;
    bool m_needsUpdate;
    bool m_markedCanvasDirty;
    // FIXME: I think this is broken -- it does not increment any
    // reference counts, so may refer to destroyed objects.
    HashSet<RefPtr<WebGLObject> > m_canvasObjects;

    // List of bound VBO's. Used to maintain info about sizes for ARRAY_BUFFER and stored values for ELEMENT_ARRAY_BUFFER
    RefPtr<WebGLBuffer> m_boundArrayBuffer;
    RefPtr<WebGLBuffer> m_boundElementArrayBuffer;

    // Cached values for vertex attrib range checks
    class VertexAttribState {
    public:
        VertexAttribState()
            : enabled(false)
            , bytesPerElement(0)
            , size(4)
            , type(GraphicsContext3D::FLOAT)
            , normalized(false)
            , stride(16)
            , originalStride(0)
            , offset(0)
        {
            initValue();
        }

        void initValue()
        {
            value[0] = 0.0f;
            value[1] = 0.0f;
            value[2] = 0.0f;
            value[3] = 1.0f;
        }

        bool enabled;
        RefPtr<WebGLBuffer> bufferBinding;
        long bytesPerElement;
        long size;
        unsigned long type;
        bool normalized;
        long stride;
        long originalStride;
        long offset;
        float value[4];
    };

    Vector<VertexAttribState> m_vertexAttribState;
    unsigned m_maxVertexAttribs;
    RefPtr<WebGLBuffer> m_vertexAttrib0Buffer;
    long m_vertexAttrib0BufferSize;
    float m_vertexAttrib0BufferValue[4];

    RefPtr<WebGLProgram> m_currentProgram;
    RefPtr<WebGLFramebuffer> m_framebufferBinding;
    RefPtr<WebGLRenderbuffer> m_renderbufferBinding;
    class TextureUnitState {
    public:
        RefPtr<WebGLTexture> m_texture2DBinding;
        RefPtr<WebGLTexture> m_textureCubeMapBinding;
    };
    Vector<TextureUnitState> m_textureUnits;
    unsigned long m_activeTextureUnit;

    RefPtr<WebGLTexture> m_blackTexture2D;
    RefPtr<WebGLTexture> m_blackTextureCubeMap;

    // Fixed-size cache of reusable image buffers for video texImage2D calls.
    class LRUImageBufferCache {
    public:
        LRUImageBufferCache(int capacity);
        // The pointer returned is owned by the image buffer map.
        ImageBuffer* imageBuffer(const IntSize& size);
    private:
        void bubbleToFront(int idx);
        OwnArrayPtr<OwnPtr<ImageBuffer> > m_buffers;
        int m_capacity;
    };
    LRUImageBufferCache m_videoCache;

    int m_maxTextureSize;
    int m_maxCubeMapTextureSize;
    int m_maxTextureLevel;
    int m_maxCubeMapTextureLevel;

    int m_packAlignment;
    int m_unpackAlignment;
    unsigned long m_implementationColorReadFormat;
    unsigned long m_implementationColorReadType;
    bool m_unpackFlipY;
    bool m_unpackPremultiplyAlpha;

    // Helpers for getParameter and others
    WebGLGetInfo getBooleanParameter(unsigned long pname);
    WebGLGetInfo getBooleanArrayParameter(unsigned long pname);
    WebGLGetInfo getFloatParameter(unsigned long pname);
    WebGLGetInfo getIntParameter(unsigned long pname);
    WebGLGetInfo getLongParameter(unsigned long pname);
    WebGLGetInfo getUnsignedLongParameter(unsigned long pname);
    WebGLGetInfo getWebGLFloatArrayParameter(unsigned long pname);
    WebGLGetInfo getWebGLIntArrayParameter(unsigned long pname);

    void texImage2DBase(unsigned target, unsigned level, unsigned internalformat,
                        unsigned width, unsigned height, unsigned border,
                        unsigned format, unsigned type, void* pixels, ExceptionCode&);
    void texImage2DImpl(unsigned target, unsigned level, unsigned internalformat,
                        unsigned format, unsigned type, Image* image,
                        bool flipY, bool premultiplyAlpha, ExceptionCode&);
    void texSubImage2DBase(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                           unsigned width, unsigned height,
                           unsigned format, unsigned type, void* pixels, ExceptionCode&);
    void texSubImage2DImpl(unsigned target, unsigned level, unsigned xoffset, unsigned yoffset,
                           unsigned format, unsigned type,
                           Image* image, bool flipY, bool premultiplyAlpha, ExceptionCode&);

    void handleNPOTTextures(bool prepareToDraw);

    void createFallbackBlackTextures1x1();

    // Helper function for copyTex{Sub}Image, check whether the internalformat
    // and the color buffer format of the current bound framebuffer combination
    // is valid.
    bool isTexInternalFormatColorBufferCombinationValid(unsigned long texInternalFormat,
                                                        unsigned long colorBufferFormat);

    // Helper function to check target and texture bound to the target.
    // Generate GL errors and return 0 if target is invalid or texture bound is
    // null.  Otherwise, return the texture bound to the target.
    WebGLTexture* validateTextureBinding(unsigned long target, bool useSixEnumsForCubeMap);

    // Helper function to check input format/type for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncFormatAndType(unsigned long format, unsigned long type);

    // Helper function to check input parameters for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncParameters(unsigned long target, long level,
                                   unsigned long internalformat,
                                   long width, long height, long border,
                                   unsigned long format, unsigned long type);

    // Helper function to validate that the given ArrayBufferView
    // is of the correct type and contains enough data for the texImage call.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncData(long width, long height,
                             unsigned long format, unsigned long type,
                             ArrayBufferView* pixels);

    // Helper function to validate mode for draw{Arrays/Elements}.
    bool validateDrawMode(unsigned long);

    // Helper function for texParameterf and texParameteri.
    void texParameter(unsigned long target, unsigned long pname, float parami, int paramf, bool isFloat);

    // Helper function to print warnings to console. Currently
    // used only to warn about use of obsolete functions.
    void printWarningToConsole(const String& message);

    // Helper function to validate input parameters for framebuffer functions.
    // Generate GL error if parameters are illegal.
    bool validateFramebufferFuncParameters(unsigned long target, unsigned long attachment);

    // Helper function to validate blend equation mode.
    bool validateBlendEquation(unsigned long);

    // Helper function to validate a GL capability.
    bool validateCapability(unsigned long);

    // Helper function to validate input parameters for uniform functions.
    bool validateUniformParameters(const WebGLUniformLocation* location, Float32Array* v, int mod);
    bool validateUniformParameters(const WebGLUniformLocation* location, Int32Array* v, int mod);
    bool validateUniformParameters(const WebGLUniformLocation* location, void* v, int size, int mod);
    bool validateUniformMatrixParameters(const WebGLUniformLocation* location, bool transpose, Float32Array* v, int mod);
    bool validateUniformMatrixParameters(const WebGLUniformLocation* location, bool transpose, void* v, int size, int mod);

    // Helper function to validate parameters for bufferData.
    // Return the current bound buffer to target, or 0 if parameters are invalid.
    WebGLBuffer* validateBufferDataParameters(unsigned long target, unsigned long usage);

    // Helper functions for vertexAttribNf{v}.
    void vertexAttribfImpl(unsigned long index, int expectedSize, float v0, float v1, float v2, float v3);
    void vertexAttribfvImpl(unsigned long index, Float32Array* v, int expectedSize);
    void vertexAttribfvImpl(unsigned long index, float* v, int size, int expectedSize);

    // Helpers for simulating vertexAttrib0
    void initVertexAttrib0();
    bool simulateVertexAttrib0(long numVertex);
    void restoreStatesAfterVertexAttrib0Simulation();

    friend class WebGLStateRestorer;
};

} // namespace WebCore

#endif
