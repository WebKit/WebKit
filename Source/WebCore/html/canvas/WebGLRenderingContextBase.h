/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBGL)

#include "ActivityStateChangeObserver.h"
#include "ExceptionOr.h"
#include "GPUBasedCanvasRenderingContext.h"
#include "GraphicsContextGLOpenGL.h"
#include "ImageBuffer.h"
#include "SuspendableTimer.h"
#include "Timer.h"
#include "WebGLAny.h"
#include "WebGLBuffer.h"
#include "WebGLContextAttributes.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLRenderbuffer.h"
#include "WebGLStateTracker.h"
#include "WebGLTexture.h"
#include "WebGLVertexArrayObjectOES.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <limits>
#include <memory>
#include <wtf/CheckedArithmetic.h>

#if ENABLE(WEBGL2)
#include "WebGLVertexArrayObject.h"
#endif

#if ENABLE(WEBXR)
#include "JSDOMPromiseDeferred.h"
#endif

namespace WebCore {

class ANGLEInstancedArrays;
class EXTBlendMinMax;
class EXTColorBufferFloat;
class EXTColorBufferHalfFloat;
class EXTTextureFilterAnisotropic;
class EXTShaderTextureLOD;
class EXTsRGB;
class EXTFragDepth;
class HTMLImageElement;
class ImageData;
class IntSize;
class OESStandardDerivatives;
class OESTextureFloat;
class OESTextureFloatLinear;
class OESTextureHalfFloat;
class OESTextureHalfFloatLinear;
class OESVertexArrayObject;
class OESElementIndexUint;
#if ENABLE(OFFSCREEN_CANVAS)
class OffscreenCanvas;
#endif
class WebGLActiveInfo;
class WebGLColorBufferFloat;
class WebGLCompressedTextureASTC;
class WebGLCompressedTextureATC;
class WebGLCompressedTextureETC;
class WebGLCompressedTextureETC1;
class WebGLCompressedTexturePVRTC;
class WebGLCompressedTextureS3TC;
class WebGLContextGroup;
class WebGLContextObject;
class WebGLDebugRendererInfo;
class WebGLDebugShaders;
class WebGLDepthTexture;
class WebGLDrawBuffers;
class WebGLExtension;
class WebGLLoseContext;
class WebGLObject;
class WebGLShader;
class WebGLSharedObject;
class WebGLShaderPrecisionFormat;
class WebGLUniformLocation;

#if ENABLE(VIDEO)
class HTMLVideoElement;
#endif

#if ENABLE(OFFSCREEN_CANVAS)
using WebGLCanvas = WTF::Variant<RefPtr<HTMLCanvasElement>, RefPtr<OffscreenCanvas>>;
#else
using WebGLCanvas = WTF::Variant<RefPtr<HTMLCanvasElement>>;
#endif

class WebGLRenderingContextBase : public GraphicsContextGLOpenGL::Client, public GPUBasedCanvasRenderingContext, private ActivityStateChangeObserver {
    WTF_MAKE_ISO_ALLOCATED(WebGLRenderingContextBase);
public:
    static std::unique_ptr<WebGLRenderingContextBase> create(CanvasBase&, WebGLContextAttributes&, const String&);
    virtual ~WebGLRenderingContextBase();

    WebGLCanvas canvas();

    int drawingBufferWidth() const;
    int drawingBufferHeight() const;

    void activeTexture(GCGLenum texture);
    void attachShader(WebGLProgram*, WebGLShader*);
    void bindAttribLocation(WebGLProgram*, GCGLuint index, const String& name);
    void bindBuffer(GCGLenum target, WebGLBuffer*);
    virtual void bindFramebuffer(GCGLenum target, WebGLFramebuffer*);
    void bindRenderbuffer(GCGLenum target, WebGLRenderbuffer*);
    void bindTexture(GCGLenum target, WebGLTexture*);
    void blendColor(GCGLfloat red, GCGLfloat green, GCGLfloat blue, GCGLfloat alpha);
    void blendEquation(GCGLenum mode);
    void blendEquationSeparate(GCGLenum modeRGB, GCGLenum modeAlpha);
    void blendFunc(GCGLenum sfactor, GCGLenum dfactor);
    void blendFuncSeparate(GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha);

    using BufferDataSource = WTF::Variant<RefPtr<ArrayBuffer>, RefPtr<ArrayBufferView>>;
    void bufferData(GCGLenum target, long long size, GCGLenum usage);
    void bufferData(GCGLenum target, Optional<BufferDataSource>&&, GCGLenum usage);
    void bufferSubData(GCGLenum target, long long offset, Optional<BufferDataSource>&&);

    GCGLenum checkFramebufferStatus(GCGLenum target);
    virtual void clear(GCGLbitfield mask) = 0;
    void clearColor(GCGLfloat red, GCGLfloat green, GCGLfloat blue, GCGLfloat alpha);
    void clearDepth(GCGLfloat);
    void clearStencil(GCGLint);
    void colorMask(GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha);
    void compileShader(WebGLShader*);

    void compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, ArrayBufferView& data);
    void compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, ArrayBufferView& data);

    void copyTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLint border);
    void copyTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height);

    RefPtr<WebGLBuffer> createBuffer();
    RefPtr<WebGLFramebuffer> createFramebuffer();
    RefPtr<WebGLProgram> createProgram();
    RefPtr<WebGLRenderbuffer> createRenderbuffer();
    RefPtr<WebGLShader> createShader(GCGLenum type);
    RefPtr<WebGLTexture> createTexture();

    void cullFace(GCGLenum mode);

    void deleteBuffer(WebGLBuffer*);
    virtual void deleteFramebuffer(WebGLFramebuffer*);
    void deleteProgram(WebGLProgram*);
    void deleteRenderbuffer(WebGLRenderbuffer*);
    void deleteShader(WebGLShader*);
    void deleteTexture(WebGLTexture*);

    void depthFunc(GCGLenum);
    void depthMask(GCGLboolean);
    void depthRange(GCGLfloat zNear, GCGLfloat zFar);
    void detachShader(WebGLProgram*, WebGLShader*);
    void disable(GCGLenum cap);
    void disableVertexAttribArray(GCGLuint index);
    void drawArrays(GCGLenum mode, GCGLint first, GCGLsizei count);
    void drawElements(GCGLenum mode, GCGLsizei count, GCGLenum type, long long offset);

    void enable(GCGLenum cap);
    void enableVertexAttribArray(GCGLuint index);
    void finish();
    void flush();
    void framebufferRenderbuffer(GCGLenum target, GCGLenum attachment, GCGLenum renderbuffertarget, WebGLRenderbuffer*);
    void framebufferTexture2D(GCGLenum target, GCGLenum attachment, GCGLenum textarget, WebGLTexture*, GCGLint level);
    void frontFace(GCGLenum mode);
    void generateMipmap(GCGLenum target);

    RefPtr<WebGLActiveInfo> getActiveAttrib(WebGLProgram*, GCGLuint index);
    RefPtr<WebGLActiveInfo> getActiveUniform(WebGLProgram*, GCGLuint index);
    Optional<Vector<RefPtr<WebGLShader>>> getAttachedShaders(WebGLProgram*);
    GCGLint getAttribLocation(WebGLProgram*, const String& name);
    WebGLAny getBufferParameter(GCGLenum target, GCGLenum pname);
    Optional<WebGLContextAttributes> getContextAttributes();
    GCGLenum getError();
    virtual WebGLExtension* getExtension(const String& name) = 0;
    virtual WebGLAny getFramebufferAttachmentParameter(GCGLenum target, GCGLenum attachment, GCGLenum pname) = 0;
    virtual WebGLAny getParameter(GCGLenum pname);
    WebGLAny getProgramParameter(WebGLProgram*, GCGLenum pname);
    String getProgramInfoLog(WebGLProgram*);
    WebGLAny getRenderbufferParameter(GCGLenum target, GCGLenum pname);
    WebGLAny getShaderParameter(WebGLShader*, GCGLenum pname);
    String getShaderInfoLog(WebGLShader*);
    RefPtr<WebGLShaderPrecisionFormat> getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType);
    String getShaderSource(WebGLShader*);
    virtual Optional<Vector<String>> getSupportedExtensions() = 0;
    virtual WebGLAny getTexParameter(GCGLenum target, GCGLenum pname);
    WebGLAny getUniform(WebGLProgram*, const WebGLUniformLocation*);
    RefPtr<WebGLUniformLocation> getUniformLocation(WebGLProgram*, const String&);
    WebGLAny getVertexAttrib(GCGLuint index, GCGLenum pname);
    long long getVertexAttribOffset(GCGLuint index, GCGLenum pname);

    bool extensionIsEnabled(const String&);

    bool isPreservingDrawingBuffer() const { return m_attributes.preserveDrawingBuffer; }
    // Concession to canvas capture API, which must dynamically enable
    // preserveDrawingBuffer. This can only be called once, when
    // isPreservingDrawingBuffer() returns false.
    void enablePreserveDrawingBuffer();

    bool preventBufferClearForInspector() const { return m_preventBufferClearForInspector; }
    void setPreventBufferClearForInspector(bool value) { m_preventBufferClearForInspector = value; }

    virtual void hint(GCGLenum target, GCGLenum mode) = 0;
    GCGLboolean isBuffer(WebGLBuffer*);
    bool isContextLost() const;
    GCGLboolean isEnabled(GCGLenum cap);
    GCGLboolean isFramebuffer(WebGLFramebuffer*);
    GCGLboolean isProgram(WebGLProgram*);
    GCGLboolean isRenderbuffer(WebGLRenderbuffer*);
    GCGLboolean isShader(WebGLShader*);
    GCGLboolean isTexture(WebGLTexture*);

    void lineWidth(GCGLfloat);
    void linkProgram(WebGLProgram*);
    bool linkProgramWithoutInvalidatingAttribLocations(WebGLProgram*);
    virtual void pixelStorei(GCGLenum pname, GCGLint param);
#if ENABLE(WEBXR)
    using MakeXRCompatiblePromise = DOMPromiseDeferred<void>;
    void makeXRCompatible(MakeXRCompatiblePromise&&);
    bool isXRCompatible() const { return m_isXRCompatible; }
#endif
    void polygonOffset(GCGLfloat factor, GCGLfloat units);
    // This must be virtual so more validation can be added in WebGL 2.0.
    virtual void readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, ArrayBufferView& pixels);
    void releaseShaderCompiler();
    virtual void renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height) = 0;
    void sampleCoverage(GCGLfloat value, GCGLboolean invert);
    void scissor(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height);
    void shaderSource(WebGLShader*, const String&);
    void stencilFunc(GCGLenum func, GCGLint ref, GCGLuint mask);
    void stencilFuncSeparate(GCGLenum face, GCGLenum func, GCGLint ref, GCGLuint mask);
    void stencilMask(GCGLuint);
    void stencilMaskSeparate(GCGLenum face, GCGLuint mask);
    void stencilOp(GCGLenum fail, GCGLenum zfail, GCGLenum zpass);
    void stencilOpSeparate(GCGLenum face, GCGLenum fail, GCGLenum zfail, GCGLenum zpass);

    // These must be virtual so more validation can be added in WebGL 2.0.
    virtual void texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&&);

#if ENABLE(VIDEO)
    using TexImageSource = WTF::Variant<RefPtr<ImageBitmap>, RefPtr<ImageData>, RefPtr<HTMLImageElement>, RefPtr<HTMLCanvasElement>, RefPtr<HTMLVideoElement>>;
#else
    using TexImageSource = WTF::Variant<RefPtr<ImageBitmap>, RefPtr<ImageData>, RefPtr<HTMLImageElement>, RefPtr<HTMLCanvasElement>>;
#endif

    virtual ExceptionOr<void> texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLenum format, GCGLenum type, Optional<TexImageSource>);

    void texParameterf(GCGLenum target, GCGLenum pname, GCGLfloat param);
    void texParameteri(GCGLenum target, GCGLenum pname, GCGLint param);

    // These must be virtual so more validation can be added in WebGL 2.0.
    virtual void texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&&);
    virtual ExceptionOr<void> texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLenum format, GCGLenum type, Optional<TexImageSource>&&);

    template <class TypedArray, class DataType>
    class TypedList {
    public:
        using VariantType = Variant<RefPtr<TypedArray>, Vector<DataType>>;

        TypedList(VariantType&& variant)
            : m_variant(WTFMove(variant))
        {
        }

        const DataType* data() const
        {
            return WTF::switchOn(m_variant,
                [] (const RefPtr<TypedArray>& typedArray) -> const DataType* { return typedArray->data(); },
                [] (const Vector<DataType>& vector) -> const DataType* { return vector.data(); }
            );
        }

        GCGLsizei length() const
        {
            return WTF::switchOn(m_variant,
                [] (const RefPtr<TypedArray>& typedArray) -> GCGLsizei { return typedArray->length(); },
                [] (const Vector<DataType>& vector) -> GCGLsizei { return vector.size(); }
            );
        }

    private:
        VariantType m_variant;
    };

    using Float32List = TypedList<Float32Array, float>;
    using Int32List = TypedList<Int32Array, int>;
    using Uint32List = TypedList<Uint32Array, uint32_t>;

    void uniform1f(const WebGLUniformLocation*, GCGLfloat x);
    void uniform2f(const WebGLUniformLocation*, GCGLfloat x, GCGLfloat y);
    void uniform3f(const WebGLUniformLocation*, GCGLfloat x, GCGLfloat y, GCGLfloat z);
    void uniform4f(const WebGLUniformLocation*, GCGLfloat x, GCGLfloat y, GCGLfloat z, GCGLfloat w);

    void uniform1i(const WebGLUniformLocation*, GCGLint x);
    void uniform2i(const WebGLUniformLocation*, GCGLint x, GCGLint y);
    void uniform3i(const WebGLUniformLocation*, GCGLint x, GCGLint y, GCGLint z);
    void uniform4i(const WebGLUniformLocation*, GCGLint x, GCGLint y, GCGLint z, GCGLint w);

    void uniform1fv(const WebGLUniformLocation*, Float32List&&);
    void uniform2fv(const WebGLUniformLocation*, Float32List&&);
    void uniform3fv(const WebGLUniformLocation*, Float32List&&);
    void uniform4fv(const WebGLUniformLocation*, Float32List&&);

    void uniform1iv(const WebGLUniformLocation*, Int32List&&);
    void uniform2iv(const WebGLUniformLocation*, Int32List&&);
    void uniform3iv(const WebGLUniformLocation*, Int32List&&);
    void uniform4iv(const WebGLUniformLocation*, Int32List&&);

    void uniformMatrix2fv(const WebGLUniformLocation*, GCGLboolean transpose, Float32List&&);
    void uniformMatrix3fv(const WebGLUniformLocation*, GCGLboolean transpose, Float32List&&);
    void uniformMatrix4fv(const WebGLUniformLocation*, GCGLboolean transpose, Float32List&&);

    void useProgram(WebGLProgram*);
    void validateProgram(WebGLProgram*);

    void vertexAttrib1f(GCGLuint index, GCGLfloat x);
    void vertexAttrib2f(GCGLuint index, GCGLfloat x, GCGLfloat y);
    void vertexAttrib3f(GCGLuint index, GCGLfloat x, GCGLfloat y, GCGLfloat z);
    void vertexAttrib4f(GCGLuint index, GCGLfloat x, GCGLfloat y, GCGLfloat z, GCGLfloat w);

    void vertexAttrib1fv(GCGLuint index, Float32List&&);
    void vertexAttrib2fv(GCGLuint index, Float32List&&);
    void vertexAttrib3fv(GCGLuint index, Float32List&&);
    void vertexAttrib4fv(GCGLuint index, Float32List&&);

    void vertexAttribPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLboolean normalized,
        GCGLsizei stride, long long offset);

    void viewport(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height);

    // WEBKIT_lose_context support
    enum LostContextMode {
        // Lost context occurred at the graphics system level.
        RealLostContext,

        // Lost context provoked by WEBKIT_lose_context.
        SyntheticLostContext
    };
    void forceLostContext(LostContextMode);
    void forceRestoreContext();
    void loseContextImpl(LostContextMode);
    WEBCORE_EXPORT void simulateContextChanged();

    GraphicsContextGLOpenGL* graphicsContextGL() const { return m_context.get(); }
    WebGLContextGroup* contextGroup() const { return m_contextGroup.get(); }
    PlatformLayer* platformLayer() const override;

    void reshape(int width, int height) override;

    void markLayerComposited() final;
    void paintRenderingResultsToCanvas() override;
    RefPtr<ImageData> paintRenderingResultsToImageData();

    void removeSharedObject(WebGLSharedObject&);
    void removeContextObject(WebGLContextObject&);
    
    unsigned getMaxVertexAttribs() const { return m_maxVertexAttribs; }

    bool isContextUnrecoverablyLost() const;

    // Instanced Array helper functions.
    void drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount);
    void drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, long long offset, GCGLsizei primcount);
    void vertexAttribDivisor(GCGLuint index, GCGLuint divisor);

    // Used for testing only, from Internals.
    WEBCORE_EXPORT void setFailNextGPUStatusCheck();

    void prepareForDisplay();

    // GraphicsContextGL::Client
    void didComposite() override;
    void forceContextLost() override;
    void recycleContext() override;
    void dispatchContextChangedNotification() override;

protected:
    WebGLRenderingContextBase(CanvasBase&, WebGLContextAttributes);
    WebGLRenderingContextBase(CanvasBase&, Ref<GraphicsContextGLOpenGL>&&, WebGLContextAttributes);

    friend class WebGLDrawBuffers;
    friend class WebGLFramebuffer;
    friend class WebGLObject;
    friend class OESVertexArrayObject;
    friend class WebGLDebugShaders;
    friend class WebGLCompressedTextureASTC;
    friend class WebGLCompressedTextureATC;
    friend class WebGLCompressedTextureETC;
    friend class WebGLCompressedTextureETC1;
    friend class WebGLCompressedTexturePVRTC;
    friend class WebGLCompressedTextureS3TC;
    friend class WebGLRenderingContextErrorMessageCallback;
    friend class WebGLVertexArrayObjectOES;
    friend class WebGLVertexArrayObject;
    friend class WebGLVertexArrayObjectBase;
    friend class WebGLSync;

    // Implementation helpers.
    friend class ScopedUnpackParametersResetRestore;

    virtual void initializeNewContext();
    virtual void initializeVertexArrayObjects() = 0;
    void setupFlags();

    // ActiveDOMObject
    void stop() override;
    const char* activeDOMObjectName() const override;
    void suspend(ReasonForSuspension) override;
    void resume() override;

    void addSharedObject(WebGLSharedObject&);
    void addContextObject(WebGLContextObject&);
    void detachAndRemoveAllObjects();

    void destroyGraphicsContextGL();
    void markContextChanged();
    void markContextChangedAndNotifyCanvasObserver();

    void addActivityStateChangeObserverIfNecessary();
    void removeActivityStateChangeObserver();

    // Query whether it is built on top of compliant GLES2 implementation.
    bool isGLES2Compliant() { return m_isGLES2Compliant; }
    // Query if the GL implementation is NPOT strict.
    bool isGLES2NPOTStrict() { return m_isGLES2NPOTStrict; }
    // Query if depth_stencil buffer is supported.
    bool isDepthStencilSupported() { return m_isDepthStencilSupported; }

    // Helper to return the size in bytes of OpenGL data types
    // like GL_FLOAT, GL_INT, etc.
    unsigned sizeInBytes(GCGLenum type);

    // Basic validation of count and offset against number of elements in element array buffer
    bool validateElementArraySize(GCGLsizei count, GCGLenum type, GCGLintptr offset);

    // Conservative but quick index validation
    virtual bool validateIndexArrayConservative(GCGLenum type, unsigned& numElementsRequired) = 0;

    // Precise but slow index validation -- only done if conservative checks fail
    bool validateIndexArrayPrecise(GCGLsizei count, GCGLenum type, GCGLintptr offset, unsigned& numElementsRequired);
    bool validateVertexAttributes(unsigned elementCount, unsigned primitiveCount = 0);

    bool validateWebGLObject(const char*, WebGLObject*);
    bool validateWebGLProgramOrShader(const char*, WebGLObject*);

#if !USE(ANGLE)
    bool validateDrawArrays(const char* functionName, GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount);
    bool validateDrawElements(const char* functionName, GCGLenum mode, GCGLsizei count, GCGLenum type, long long offset, unsigned& numElements, GCGLsizei primcount);
    bool validateNPOTTextureLevel(GCGLsizei width, GCGLsizei height, GCGLint level, const char* functionName);
#endif

    // Adds a compressed texture format.
    void addCompressedTextureFormat(GCGLenum);

    // Set UNPACK_ALIGNMENT to 1, all other parameters to 0.
    virtual void resetUnpackParameters();
    // Restore the client unpack parameters.
    virtual void restoreUnpackParameters();

    RefPtr<Image> drawImageIntoBuffer(Image&, int width, int height, int deviceScaleFactor, const char* functionName);

#if ENABLE(VIDEO)
    RefPtr<Image> videoFrameToImage(HTMLVideoElement*, BackingStoreCopy);
#endif

    WebGLTexture::TextureExtensionFlag textureExtensionFlags() const;

    bool enableSupportedExtension(ASCIILiteral extensionNameLiteral);

    virtual void uncacheDeletedBuffer(WebGLBuffer*);

    RefPtr<GraphicsContextGLOpenGL> m_context;
    RefPtr<WebGLContextGroup> m_contextGroup;

    bool m_restoreAllowed { false };
    SuspendableTimer m_restoreTimer;

    bool m_needsUpdate;
    bool m_markedCanvasDirty;
    HashSet<WebGLContextObject*> m_contextObjects;

    // List of bound VBO's. Used to maintain info about sizes for ARRAY_BUFFER and stored values for ELEMENT_ARRAY_BUFFER
    RefPtr<WebGLBuffer> m_boundArrayBuffer;

    RefPtr<WebGLVertexArrayObjectBase> m_defaultVertexArrayObject;
    RefPtr<WebGLVertexArrayObjectBase> m_boundVertexArrayObject;

    void setBoundVertexArrayObject(WebGLVertexArrayObjectBase* arrayObject)
    {
        m_boundVertexArrayObject = arrayObject ? arrayObject : m_defaultVertexArrayObject;
    }
    
    class VertexAttribValue {
    public:
        VertexAttribValue()
        {
            initValue();
        }
        
        void initValue()
        {
            type = GraphicsContextGL::FLOAT;
            fValue[0] = 0.0f;
            fValue[1] = 0.0f;
            fValue[2] = 0.0f;
            fValue[3] = 1.0f;
        }
        
        GCGLenum type;
        union {
            GCGLfloat fValue[4];
            GCGLint iValue[4];
            GCGLuint uiValue[4];
        };
    };
    Vector<VertexAttribValue> m_vertexAttribValue;
    unsigned m_maxVertexAttribs;
#if !USE(ANGLE)
    RefPtr<WebGLBuffer> m_vertexAttrib0Buffer;
    long m_vertexAttrib0BufferSize { 0 };
    GCGLfloat m_vertexAttrib0BufferValue[4];
    bool m_forceAttrib0BufferRefill { true };
    bool m_vertexAttrib0UsedBefore { false };
#endif

    RefPtr<WebGLProgram> m_currentProgram;
    RefPtr<WebGLFramebuffer> m_framebufferBinding;
    RefPtr<WebGLRenderbuffer> m_renderbufferBinding;
    struct TextureUnitState {
        RefPtr<WebGLTexture> texture2DBinding;
        RefPtr<WebGLTexture> textureCubeMapBinding;
        RefPtr<WebGLTexture> texture3DBinding;
        RefPtr<WebGLTexture> texture2DArrayBinding;
    };
    Vector<TextureUnitState> m_textureUnits;
#if !USE(ANGLE)
    HashSet<unsigned, DefaultHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> m_unrenderableTextureUnits;
#endif
    unsigned long m_activeTextureUnit;

    RefPtr<WebGLTexture> m_blackTexture2D;
    RefPtr<WebGLTexture> m_blackTextureCubeMap;

    Vector<GCGLenum> m_compressedTextureFormats;

    // Fixed-size cache of reusable image buffers for video texImage2D calls.
    class LRUImageBufferCache {
    public:
        LRUImageBufferCache(int capacity);
        // The pointer returned is owned by the image buffer map.
        ImageBuffer* imageBuffer(const IntSize& size);
    private:
        void bubbleToFront(size_t idx);
        Vector<std::unique_ptr<ImageBuffer>> m_buffers;
    };
    LRUImageBufferCache m_generatedImageCache { 0 };

    GCGLint m_maxTextureSize;
    GCGLint m_maxCubeMapTextureSize;
    GCGLint m_maxRenderbufferSize;
    GCGLint m_maxViewportDims[2] { 0, 0 };
    GCGLint m_maxTextureLevel;
    GCGLint m_maxCubeMapTextureLevel;

    GCGLint m_maxDrawBuffers;
    GCGLint m_maxColorAttachments;
    GCGLenum m_backDrawBuffer;
    bool m_drawBuffersWebGLRequirementsChecked;
    bool m_drawBuffersSupported;

    GCGLint m_packAlignment;
    GCGLint m_unpackAlignment;
    bool m_unpackFlipY;
    bool m_unpackPremultiplyAlpha;
    GCGLenum m_unpackColorspaceConversion;

    bool m_contextLost { false };
    LostContextMode m_contextLostMode { SyntheticLostContext };
    WebGLContextAttributes m_attributes;

    bool m_layerCleared;
    GCGLfloat m_clearColor[4];
    bool m_scissorEnabled;
    GCGLfloat m_clearDepth;
    GCGLint m_clearStencil;
    GCGLboolean m_colorMask[4];
    GCGLboolean m_depthMask;

    bool m_stencilEnabled;
    GCGLuint m_stencilMask, m_stencilMaskBack;
    GCGLint m_stencilFuncRef, m_stencilFuncRefBack; // Note that these are the user specified values, not the internal clamped value.
    GCGLuint m_stencilFuncMask, m_stencilFuncMaskBack;

    bool m_isGLES2Compliant;
    bool m_isGLES2NPOTStrict;
    bool m_isDepthStencilSupported;
    bool m_isRobustnessEXTSupported;

    bool m_synthesizedErrorsToConsole { true };
    int m_numGLErrorsToConsoleAllowed;

    bool m_preventBufferClearForInspector { false };

    // A WebGLRenderingContext can be created in a state where it appears as
    // a valid and active context, but will not execute any important operations
    // until its load policy is completely resolved.
    bool m_isPendingPolicyResolution { false };
    bool m_hasRequestedPolicyResolution { false };
    bool isContextLostOrPending();

    // Enabled extension objects.
    // FIXME: Move some of these to WebGLRenderingContext, the ones not needed for WebGL2
    std::unique_ptr<EXTFragDepth> m_extFragDepth;
    std::unique_ptr<EXTBlendMinMax> m_extBlendMinMax;
    std::unique_ptr<EXTsRGB> m_extsRGB;
    std::unique_ptr<EXTTextureFilterAnisotropic> m_extTextureFilterAnisotropic;
    std::unique_ptr<EXTShaderTextureLOD> m_extShaderTextureLOD;
    std::unique_ptr<OESTextureFloat> m_oesTextureFloat;
    std::unique_ptr<OESTextureFloatLinear> m_oesTextureFloatLinear;
    std::unique_ptr<OESTextureHalfFloat> m_oesTextureHalfFloat;
    std::unique_ptr<OESTextureHalfFloatLinear> m_oesTextureHalfFloatLinear;
    std::unique_ptr<OESStandardDerivatives> m_oesStandardDerivatives;
    std::unique_ptr<OESVertexArrayObject> m_oesVertexArrayObject;
    std::unique_ptr<OESElementIndexUint> m_oesElementIndexUint;
    std::unique_ptr<WebGLLoseContext> m_webglLoseContext;
    std::unique_ptr<WebGLDebugRendererInfo> m_webglDebugRendererInfo;
    std::unique_ptr<WebGLDebugShaders> m_webglDebugShaders;
    std::unique_ptr<WebGLCompressedTextureASTC> m_webglCompressedTextureASTC;
    std::unique_ptr<WebGLCompressedTextureATC> m_webglCompressedTextureATC;
    std::unique_ptr<WebGLCompressedTextureETC> m_webglCompressedTextureETC;
    std::unique_ptr<WebGLCompressedTextureETC1> m_webglCompressedTextureETC1;
    std::unique_ptr<WebGLCompressedTexturePVRTC> m_webglCompressedTexturePVRTC;
    std::unique_ptr<WebGLCompressedTextureS3TC> m_webglCompressedTextureS3TC;
    std::unique_ptr<WebGLDepthTexture> m_webglDepthTexture;
    std::unique_ptr<WebGLDrawBuffers> m_webglDrawBuffers;
    std::unique_ptr<ANGLEInstancedArrays> m_angleInstancedArrays;
    std::unique_ptr<EXTColorBufferHalfFloat> m_extColorBufferHalfFloat;
    std::unique_ptr<WebGLColorBufferFloat> m_webglColorBufferFloat;
    std::unique_ptr<EXTColorBufferFloat> m_extColorBufferFloat;

    bool m_areWebGL2TexImageSourceFormatsAndTypesAdded { false };
    bool m_areOESTextureFloatFormatsAndTypesAdded { false };
    bool m_areOESTextureHalfFloatFormatsAndTypesAdded { false };
    bool m_areEXTsRGBFormatsAndTypesAdded { false };

    HashSet<GCGLenum> m_supportedTexImageSourceInternalFormats;
    HashSet<GCGLenum> m_supportedTexImageSourceFormats;
    HashSet<GCGLenum> m_supportedTexImageSourceTypes;

    // Helpers for getParameter and other similar functions.
    bool getBooleanParameter(GCGLenum);
    Vector<bool> getBooleanArrayParameter(GCGLenum);
    float getFloatParameter(GCGLenum);
    int getIntParameter(GCGLenum);
    unsigned getUnsignedIntParameter(GCGLenum);
    RefPtr<Float32Array> getWebGLFloatArrayParameter(GCGLenum);
    RefPtr<Int32Array> getWebGLIntArrayParameter(GCGLenum);

    // Clear the backbuffer if it was composited since the last operation.
    // clearMask is set to the bitfield of any clear that would happen anyway at this time
    // and the function returns true if that clear is now unnecessary.
    bool clearIfComposited(GCGLbitfield clearMask = 0);

    // Helper to restore state that clearing the framebuffer may destroy.
    void restoreStateAfterClear();

    enum class TexImageFunctionType {
        TexImage,
        TexSubImage,
        CopyTexImage,
        CompressedTexImage
    };

    enum class TexImageFunctionID {
        TexImage2D,
        TexSubImage2D,
        TexImage3D,
        TexSubImage3D
    };

    enum class TexImageDimension {
        Tex2D,
        Tex3D
    };

    enum TexFuncValidationSourceType {
        SourceArrayBufferView,
        SourceImageBitmap,
        SourceImageData,
        SourceHTMLImageElement,
        SourceHTMLCanvasElement,
#if ENABLE(VIDEO)
        SourceHTMLVideoElement,
#endif
        // WebGL 2.0.
        SourceUnpackBuffer,
    };

    enum NullDisposition {
        NullAllowed,
        NullNotAllowed,
        NullNotReachable
    };

    template <typename T> IntRect getTextureSourceSize(T* textureSource)
    {
        return IntRect(0, 0, textureSource->width(), textureSource->height());
    }
    template <typename T> bool validateTexImageSubRectangle(const char* functionName,
        TexImageFunctionID functionID,
        T* image,
        const IntRect& subRect,
        GCGLsizei depth,
        GCGLint unpackImageHeight,
        bool* selectingSubRectangle)
    {
        ASSERT(functionName);
        ASSERT(selectingSubRectangle);
        if (!image) {
            // Probably indicates a failure to allocate the image.
            synthesizeGLError(GraphicsContextGL::OUT_OF_MEMORY, functionName, "out of memory");
            return false;
        }

        int imageWidth = static_cast<int>(image->width());
        int imageHeight = static_cast<int>(image->height());
        *selectingSubRectangle = !(!subRect.x() && !subRect.y() && subRect.width() == imageWidth && subRect.height() == imageHeight);
        // If the source image rect selects anything except the entire
        // contents of the image, assert that we're running WebGL 2.0,
        // since this should never happen for WebGL 1.0 (even though
        // the code could support it). If the image is null, that will
        // be signaled as an error later.
        ASSERT(!*selectingSubRectangle || isWebGL2());

        if (!subRect.isValid() || subRect.x() < 0 || subRect.y() < 0
            || subRect.maxX() > imageWidth || subRect.maxY() > imageHeight
            || subRect.width() < 0 || subRect.height() < 0) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName,
                "source sub-rectangle specified via pixel unpack parameters is invalid");
            return false;
        }

        if (functionID == TexImageFunctionID::TexImage3D || functionID == TexImageFunctionID::TexSubImage3D) {
            ASSERT(unpackImageHeight >= 0);

            if (depth < 1) {
                synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName,
                    "Can't define a 3D texture with depth < 1");
                return false;
            }

            // According to the WebGL 2.0 spec, specifying depth > 1 means
            // to select multiple rectangles stacked vertically.
            Checked<GCGLint, RecordOverflow> maxYAccessed;
            if (unpackImageHeight)
                maxYAccessed = unpackImageHeight;
            else
                maxYAccessed = subRect.height();
            maxYAccessed *= depth - 1;
            maxYAccessed += subRect.height();
            maxYAccessed += subRect.y();

            if (maxYAccessed.hasOverflowed()) {
                synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName,
                    "Out-of-range parameters passed for 3D texture upload");
                return false;
            }

            if (maxYAccessed.unsafeGet() > imageHeight) {
                synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName,
                    "Not enough data supplied to upload to a 3D texture with depth > 1");
                return false;
            }
        } else {
            ASSERT(depth >= 1);
            ASSERT(!unpackImageHeight);
        }
        return true;
    }
    IntRect sentinelEmptyRect();
    IntRect safeGetImageSize(Image*);
    IntRect getImageDataSize(ImageData*);
    IntRect getTexImageSourceSize(TexImageSource&);

    ExceptionOr<void> texImageSourceHelper(TexImageFunctionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& sourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, TexImageSource&&);
    // Helper function for tex(Sub)Image2D && texSubImage3D.
    void texImageArrayBufferViewHelper(TexImageFunctionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, RefPtr<ArrayBufferView>&& pixels, NullDisposition, GCGLuint srcOffset);
    void texImageImpl(TexImageFunctionID, GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLenum format, GCGLenum type, Image*, GraphicsContextGL::DOMSource, bool flipY, bool premultiplyAlpha, const IntRect&, GCGLsizei depth, GCGLint unpackImageHeight);
    void texImage2DBase(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLsizei byteLength, const void* pixels);
    void texSubImage2DBase(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum internalformat, GCGLenum format, GCGLenum type, GCGLsizei byteLength, const void* pixels);
    static const char* getTexImageFunctionName(TexImageFunctionID);

    virtual GraphicsContextGLOpenGL::PixelStoreParams getPackPixelStoreParams() const;
    virtual GraphicsContextGLOpenGL::PixelStoreParams getUnpackPixelStoreParams(TexImageDimension) const;

#if !USE(ANGLE)
    bool checkTextureCompleteness(const char*, bool);

    void createFallbackBlackTextures1x1();

    // Helper function for copyTex{Sub}Image, check whether the internalformat
    // and the color buffer format of the current bound framebuffer combination
    // is valid.
    bool isTexInternalFormatColorBufferCombinationValid(GCGLenum texInternalFormat, GCGLenum colorBufferFormat);

    // Helper function to get the bound read framebuffer's color buffer format.
    GCGLenum getBoundReadFramebufferColorFormat();

    // Helper function to get the bound read framebuffer's width.
    int getBoundReadFramebufferWidth();

    // Helper function to get the bound read framebuffer's height.
    int getBoundReadFramebufferHeight();
#endif

    // Helper function to verify limits on the length of uniform and attribute locations.
    bool validateLocationLength(const char* functionName, const String&);

    // Helper function to check if size is non-negative.
    // Generate GL error and return false for negative inputs; otherwise, return true.
    bool validateSize(const char* functionName, GCGLint x, GCGLint y, GCGLint z = 0);

    // Helper function to check if all characters in the string belong to the
    // ASCII subset as defined in GLSL ES 1.0 spec section 3.1.
    bool validateString(const char* functionName, const String&);

    // Helper function to check target and texture bound to the target.
    // Generate GL errors and return 0 if target is invalid or texture bound is
    // null.  Otherwise, return the texture bound to the target.
    RefPtr<WebGLTexture> validateTextureBinding(const char* functionName, GCGLenum target);

    // Wrapper function for validateTexture2D(3D)Binding, used in texImageSourceHelper.
    virtual RefPtr<WebGLTexture> validateTexImageBinding(const char*, TexImageFunctionID, GCGLenum);

    // Helper function to check texture 2D target and texture bound to the target.
    // Generate GL errors and return 0 if target is invalid or texture bound is
    // null. Otherwise, return the texture bound to the target.
    RefPtr<WebGLTexture> validateTexture2DBinding(const char*, GCGLenum);

    void addExtensionSupportedFormatsAndTypes();
    void addExtensionSupportedFormatsAndTypesWebGL2();

    // Helper function to check input internalformat/format/type for functions
    // Tex{Sub}Image taking TexImageSource source data. Generates GL error and
    // returns false if parameters are invalid.
    bool validateTexImageSourceFormatAndType(const char* functionName, TexImageFunctionType, GCGLenum internalformat, GCGLenum format, GCGLenum type);

    // Helper function to check input format/type for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncFormatAndType(const char* functionName, GCGLenum internalformat, GCGLenum format, GCGLenum type, GCGLint level);

    // Helper function to check input level for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if level is invalid.
    bool validateTexFuncLevel(const char* functionName, GCGLenum target, GCGLint level);

    // Helper function for tex{Sub}Image{2|3}D to check if the input format/type/level/target/width/height/depth/border/xoffset/yoffset/zoffset are valid.
    // Otherwise, it would return quickly without doing other work.
    bool validateTexFunc(const char* functionName, TexImageFunctionType, TexFuncValidationSourceType, GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width,
        GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset);

    // Helper function to check input parameters for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncParameters(const char* functionName,
        TexImageFunctionType,
        TexFuncValidationSourceType,
        GCGLenum target, GCGLint level,
        GCGLenum internalformat,
        GCGLsizei width, GCGLsizei height, GCGLsizei depth,
        GCGLint border,
        GCGLenum format, GCGLenum type);

    // Helper function to validate that the given ArrayBufferView
    // is of the correct type and contains enough data for the texImage call.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncData(const char* functionName, TexImageDimension,
        GCGLsizei width, GCGLsizei height, GCGLsizei depth,
        GCGLenum format, GCGLenum type,
        ArrayBufferView* pixels,
        NullDisposition,
        GCGLuint srcOffset);

    // Helper function to validate a given texture format is settable as in
    // you can supply data to texImage2D, or call texImage2D, copyTexImage2D and
    // copyTexSubImage2D.
    // Generates GL error and returns false if the format is not settable.
    bool validateSettableTexInternalFormat(const char* functionName, GCGLenum format);

#if !USE(ANGLE)
    // Helper function to validate compressed texture data is correct size
    // for the given format and dimensions.
    bool validateCompressedTexFuncData(const char* functionName, GCGLsizei width, GCGLsizei height, GCGLenum format, ArrayBufferView& pixels);

    // Helper function for validating compressed texture formats.
    bool validateCompressedTexFormat(GCGLenum format);

    // Helper function to validate compressed texture dimensions are valid for
    // the given format.
    bool validateCompressedTexDimensions(const char* functionName, GCGLenum target, GCGLint level, GCGLsizei width, GCGLsizei height, GCGLenum format);

    // Helper function to validate compressed texture dimensions are valid for
    // the given format.
    bool validateCompressedTexSubDimensions(const char* functionName, GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset,
        GCGLsizei width, GCGLsizei height, GCGLenum format, WebGLTexture*);
#endif

    // Helper function to validate mode for draw{Arrays/Elements}.
    bool validateDrawMode(const char* functionName, GCGLenum);

    // Helper function to validate if front/back stencilMask and stencilFunc settings are the same.
    bool validateStencilSettings(const char* functionName);

    // Helper function to validate stencil func.
    bool validateStencilFunc(const char* functionName, GCGLenum);

    // Helper function for texParameterf and texParameteri.
    void texParameter(GCGLenum target, GCGLenum pname, GCGLfloat parami, GCGLint paramf, bool isFloat);

    // Helper function to print errors and warnings to console.
    void printToConsole(MessageLevel, const String&);

    // Helper function to validate the target for checkFramebufferStatus and
    // validateFramebufferFuncParameters.
    virtual bool validateFramebufferTarget(GCGLenum target);

    // Get the framebuffer bound to the given target.
    virtual WebGLFramebuffer* getFramebufferBinding(GCGLenum target);

    virtual WebGLFramebuffer* getReadFramebufferBinding();

    // Helper function to validate input parameters for framebuffer functions.
    // Generate GL error if parameters are illegal.
    bool validateFramebufferFuncParameters(const char* functionName, GCGLenum target, GCGLenum attachment);

    // Helper function to validate blend equation mode.
    virtual bool validateBlendEquation(const char* functionName, GCGLenum) = 0;

    // Helper function to validate blend func factors.
    bool validateBlendFuncFactors(const char* functionName, GCGLenum src, GCGLenum dst);

    // Helper function to validate a GL capability.
    virtual bool validateCapability(const char* functionName, GCGLenum) = 0;

    // Helper function to validate input parameters for uniform functions.
    bool validateUniformLocation(const char* functionName, const WebGLUniformLocation*);
    bool validateUniformParameters(const char* functionName, const WebGLUniformLocation*, const Float32List&, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength);
    bool validateUniformParameters(const char* functionName, const WebGLUniformLocation*, const Int32List&, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength);
    bool validateUniformParameters(const char* functionName, const WebGLUniformLocation*, const Uint32List&, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength);
    bool validateUniformParameters(const char* functionName, const WebGLUniformLocation*, void*, GCGLsizei, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength);
    bool validateUniformMatrixParameters(const char* functionName, const WebGLUniformLocation*, GCGLboolean transpose, const Float32List&, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength);
    bool validateUniformMatrixParameters(const char* functionName, const WebGLUniformLocation*, GCGLboolean transpose, const void*, GCGLsizei, GCGLsizei requiredMinSize, GCGLuint srcOffset, GCGLuint srcLength);

    // Helper function to validate parameters for bufferData.
    // Return the current bound buffer to target, or 0 if parameters are invalid.
    virtual WebGLBuffer* validateBufferDataParameters(const char* functionName, GCGLenum target, GCGLenum usage);

    // Helper function for tex{Sub}Image2D to make sure image is ready.
    ExceptionOr<bool> validateHTMLImageElement(const char* functionName, HTMLImageElement*);
    ExceptionOr<bool> validateHTMLCanvasElement(const char* functionName, HTMLCanvasElement*);
#if ENABLE(VIDEO)
    ExceptionOr<bool> validateHTMLVideoElement(const char* functionName, HTMLVideoElement*);
#endif
    ExceptionOr<bool> validateImageBitmap(const char* functionName, ImageBitmap*);

    // Helper functions for vertexAttribNf{v}.
    void vertexAttribfImpl(const char* functionName, GCGLuint index, GCGLsizei expectedSize, GCGLfloat, GCGLfloat, GCGLfloat, GCGLfloat);
    void vertexAttribfvImpl(const char* functionName, GCGLuint index, Float32List&&, GCGLsizei expectedSize);

    // Helper function for delete* (deleteBuffer, deleteProgram, etc) functions.
    // Return false if caller should return without further processing.
    bool deleteObject(WebGLObject*);

    // Helper function for bind* (bindBuffer, bindTexture, etc) and useProgram.
    // Return false if caller should return without further processing.
    bool checkObjectToBeBound(const char* functionName, WebGLObject*);

    // Helper function to validate the target for bufferData and
    // getBufferParameter.
    virtual bool validateBufferTarget(const char* functionName, GCGLenum target);

    // Helper function to validate the target for bufferData.
    // Return the current bound buffer to target, or 0 if the target is invalid.
    virtual WebGLBuffer* validateBufferDataTarget(const char* functionName, GCGLenum target);

    virtual bool validateAndCacheBufferBinding(const char* functionName, GCGLenum target, WebGLBuffer*);

#if !USE(ANGLE)
    // Helpers for simulating vertexAttrib0.
    void initVertexAttrib0();
    Optional<bool> simulateVertexAttrib0(GCGLuint numVertex);
    bool validateSimulatedVertexAttrib0(GCGLuint numVertex);
    void restoreStatesAfterVertexAttrib0Simulation();
#endif

    // Wrapper for GraphicsContextGLOpenGL::synthesizeGLError that sends a message to the JavaScript console.
    enum ConsoleDisplayPreference { DisplayInConsole, DontDisplayInConsole };
    void synthesizeGLError(GCGLenum, const char* functionName, const char* description, ConsoleDisplayPreference = DisplayInConsole);

    String ensureNotNull(const String&) const;

    // Enable or disable stencil test based on user setting and whether the current FBO has a stencil buffer.
    void applyStencilTest();

    // Helper for enabling or disabling a capability.
    void enableOrDisable(GCGLenum capability, bool enable);

    // Clamp the width and height to GL_MAX_VIEWPORT_DIMS.
    IntSize clampedCanvasSize();

    virtual GCGLint getMaxDrawBuffers() = 0;
    virtual GCGLint getMaxColorAttachments() = 0;

    void setBackDrawBuffer(GCGLenum);
    void setFramebuffer(GCGLenum, WebGLFramebuffer*);

    virtual void restoreCurrentFramebuffer();
    void restoreCurrentTexture2D();

    // Check if EXT_draw_buffers extension is supported and if it satisfies the WebGL requirements.
    bool supportsDrawBuffers();

#if ENABLE(OFFSCREEN_CANVAS)
    OffscreenCanvas* offscreenCanvas();
#endif

    template <typename T> inline Optional<T> checkedAddAndMultiply(T value, T add, T multiply);
    template <typename T> unsigned getMaxIndex(const RefPtr<JSC::ArrayBuffer> elementArrayBuffer, GCGLintptr uoffset, GCGLsizei n);

    bool validateArrayBufferType(const char* functionName, GCGLenum type, Optional<JSC::TypedArrayType>);

private:
    void scheduleTaskToDispatchContextLostEvent();
    // Helper for restoration after context lost.
    void maybeRestoreContext();

    void registerWithWebGLStateTracker();
    void checkForContextLossHandling();

    void activityStateDidChange(OptionSet<ActivityState::Flag> oldActivityState, OptionSet<ActivityState::Flag> newActivityState) override;

    WebGLStateTracker::Token m_trackerToken;
    Timer m_checkForContextLossHandlingTimer;
    bool m_isSuspended { false };

#if ENABLE(WEBXR)
    bool m_isXRCompatible { false };
#endif
};

template <typename T>
inline Optional<T> WebGLRenderingContextBase::checkedAddAndMultiply(T value, T add, T multiply)
{
    Checked<T, RecordOverflow> checkedResult = Checked<T>(value);
    checkedResult += Checked<T>(add);
    checkedResult *= Checked<T>(multiply);
    if (checkedResult.hasOverflowed())
        return WTF::nullopt;

    return checkedResult.unsafeGet();
}

template<typename T>
inline unsigned WebGLRenderingContextBase::getMaxIndex(const RefPtr<JSC::ArrayBuffer> elementArrayBuffer, GCGLintptr uoffset, GCGLsizei n)
{
    unsigned maxIndex = 0;
    T restartIndex = 0;

#if ENABLE(WEBGL2)
    // WebGL 2 spec enforces that GL_PRIMITIVE_RESTART_FIXED_INDEX is always enabled, so ignore the restart index.
    if (isWebGL2())
        restartIndex = std::numeric_limits<T>::max();
#endif

    // Make uoffset an element offset.
    uoffset /= sizeof(T);
    const T* p = static_cast<const T*>(elementArrayBuffer->data()) + uoffset;
    while (n-- > 0) {
        if (*p != restartIndex && *p > maxIndex)
            maxIndex = *p;
        ++p;
    }

    return maxIndex;
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::WebGLRenderingContextBase, isWebGL())

#endif
