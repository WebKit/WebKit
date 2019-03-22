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
#include "GraphicsContext3D.h"
#include "ImageBuffer.h"
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

#if ENABLE(WEBGL2)
#include "WebGLVertexArrayObject.h"
#endif

namespace WebCore {

class ANGLEInstancedArrays;
class EXTBlendMinMax;
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
class OffscreenCanvas;
class WebGLActiveInfo;
class WebGLContextGroup;
class WebGLContextObject;
class WebGLCompressedTextureASTC;
class WebGLCompressedTextureATC;
class WebGLCompressedTexturePVRTC;
class WebGLCompressedTextureS3TC;
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

using WebGLCanvas = WTF::Variant<RefPtr<HTMLCanvasElement>, RefPtr<OffscreenCanvas>>;

class WebGLRenderingContextBase : public GPUBasedCanvasRenderingContext, private ActivityStateChangeObserver {
public:
    static std::unique_ptr<WebGLRenderingContextBase> create(CanvasBase&, WebGLContextAttributes&, const String&);
    virtual ~WebGLRenderingContextBase();

    WebGLCanvas canvas();

    int drawingBufferWidth() const;
    int drawingBufferHeight() const;

    void activeTexture(GC3Denum texture);
    void attachShader(WebGLProgram*, WebGLShader*);
    void bindAttribLocation(WebGLProgram*, GC3Duint index, const String& name);
    void bindBuffer(GC3Denum target, WebGLBuffer*);
    void bindFramebuffer(GC3Denum target, WebGLFramebuffer*);
    void bindRenderbuffer(GC3Denum target, WebGLRenderbuffer*);
    void bindTexture(GC3Denum target, WebGLTexture*);
    void blendColor(GC3Dfloat red, GC3Dfloat green, GC3Dfloat blue, GC3Dfloat alpha);
    void blendEquation(GC3Denum mode);
    void blendEquationSeparate(GC3Denum modeRGB, GC3Denum modeAlpha);
    void blendFunc(GC3Denum sfactor, GC3Denum dfactor);
    void blendFuncSeparate(GC3Denum srcRGB, GC3Denum dstRGB, GC3Denum srcAlpha, GC3Denum dstAlpha);

    using BufferDataSource = WTF::Variant<RefPtr<ArrayBuffer>, RefPtr<ArrayBufferView>>;
    void bufferData(GC3Denum target, long long size, GC3Denum usage);
    void bufferData(GC3Denum target, Optional<BufferDataSource>&&, GC3Denum usage);
    void bufferSubData(GC3Denum target, long long offset, Optional<BufferDataSource>&&);

    GC3Denum checkFramebufferStatus(GC3Denum target);
    virtual void clear(GC3Dbitfield mask) = 0;
    void clearColor(GC3Dfloat red, GC3Dfloat green, GC3Dfloat blue, GC3Dfloat alpha);
    void clearDepth(GC3Dfloat);
    void clearStencil(GC3Dint);
    void colorMask(GC3Dboolean red, GC3Dboolean green, GC3Dboolean blue, GC3Dboolean alpha);
    void compileShader(WebGLShader*);

    void compressedTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, ArrayBufferView& data);
    void compressedTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, ArrayBufferView& data);

    void copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border);
    void copyTexSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);

    RefPtr<WebGLBuffer> createBuffer();
    RefPtr<WebGLFramebuffer> createFramebuffer();
    RefPtr<WebGLProgram> createProgram();
    RefPtr<WebGLRenderbuffer> createRenderbuffer();
    RefPtr<WebGLShader> createShader(GC3Denum type);
    RefPtr<WebGLTexture> createTexture();

    void cullFace(GC3Denum mode);

    void deleteBuffer(WebGLBuffer*);
    void deleteFramebuffer(WebGLFramebuffer*);
    void deleteProgram(WebGLProgram*);
    void deleteRenderbuffer(WebGLRenderbuffer*);
    void deleteShader(WebGLShader*);
    void deleteTexture(WebGLTexture*);

    void depthFunc(GC3Denum);
    void depthMask(GC3Dboolean);
    void depthRange(GC3Dfloat zNear, GC3Dfloat zFar);
    void detachShader(WebGLProgram*, WebGLShader*);
    void disable(GC3Denum cap);
    void disableVertexAttribArray(GC3Duint index);
    void drawArrays(GC3Denum mode, GC3Dint first, GC3Dsizei count);
    void drawElements(GC3Denum mode, GC3Dsizei count, GC3Denum type, long long offset);

    void enable(GC3Denum cap);
    void enableVertexAttribArray(GC3Duint index);
    void finish();
    void flush();
    void framebufferRenderbuffer(GC3Denum target, GC3Denum attachment, GC3Denum renderbuffertarget, WebGLRenderbuffer*);
    void framebufferTexture2D(GC3Denum target, GC3Denum attachment, GC3Denum textarget, WebGLTexture*, GC3Dint level);
    void frontFace(GC3Denum mode);
    void generateMipmap(GC3Denum target);

    RefPtr<WebGLActiveInfo> getActiveAttrib(WebGLProgram*, GC3Duint index);
    RefPtr<WebGLActiveInfo> getActiveUniform(WebGLProgram*, GC3Duint index);
    Optional<Vector<RefPtr<WebGLShader>>> getAttachedShaders(WebGLProgram*);
    GC3Dint getAttribLocation(WebGLProgram*, const String& name);
    WebGLAny getBufferParameter(GC3Denum target, GC3Denum pname);
    Optional<WebGLContextAttributes> getContextAttributes();
    GC3Denum getError();
    virtual WebGLExtension* getExtension(const String& name) = 0;
    virtual WebGLAny getFramebufferAttachmentParameter(GC3Denum target, GC3Denum attachment, GC3Denum pname) = 0;
    virtual WebGLAny getParameter(GC3Denum pname) = 0;
    WebGLAny getProgramParameter(WebGLProgram*, GC3Denum pname);
    String getProgramInfoLog(WebGLProgram*);
    WebGLAny getRenderbufferParameter(GC3Denum target, GC3Denum pname);
    WebGLAny getShaderParameter(WebGLShader*, GC3Denum pname);
    String getShaderInfoLog(WebGLShader*);
    RefPtr<WebGLShaderPrecisionFormat> getShaderPrecisionFormat(GC3Denum shaderType, GC3Denum precisionType);
    String getShaderSource(WebGLShader*);
    virtual Optional<Vector<String>> getSupportedExtensions() = 0;
    WebGLAny getTexParameter(GC3Denum target, GC3Denum pname);
    WebGLAny getUniform(WebGLProgram*, const WebGLUniformLocation*);
    RefPtr<WebGLUniformLocation> getUniformLocation(WebGLProgram*, const String&);
    WebGLAny getVertexAttrib(GC3Duint index, GC3Denum pname);
    long long getVertexAttribOffset(GC3Duint index, GC3Denum pname);

    bool extensionIsEnabled(const String&);

    bool isPreservingDrawingBuffer() const { return m_attributes.preserveDrawingBuffer; }
    void setPreserveDrawingBuffer(bool value) { m_attributes.preserveDrawingBuffer = value; }

    bool preventBufferClearForInspector() const { return m_preventBufferClearForInspector; }
    void setPreventBufferClearForInspector(bool value) { m_preventBufferClearForInspector = value; }

    virtual void hint(GC3Denum target, GC3Denum mode) = 0;
    GC3Dboolean isBuffer(WebGLBuffer*);
    bool isContextLost() const;
    GC3Dboolean isEnabled(GC3Denum cap);
    GC3Dboolean isFramebuffer(WebGLFramebuffer*);
    GC3Dboolean isProgram(WebGLProgram*);
    GC3Dboolean isRenderbuffer(WebGLRenderbuffer*);
    GC3Dboolean isShader(WebGLShader*);
    GC3Dboolean isTexture(WebGLTexture*);

    void lineWidth(GC3Dfloat);
    void linkProgram(WebGLProgram*);
    bool linkProgramWithoutInvalidatingAttribLocations(WebGLProgram*);
    void pixelStorei(GC3Denum pname, GC3Dint param);
    void polygonOffset(GC3Dfloat factor, GC3Dfloat units);
    void readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, ArrayBufferView& pixels);
    void releaseShaderCompiler();
    virtual void renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height) = 0;
    void sampleCoverage(GC3Dfloat value, GC3Dboolean invert);
    void scissor(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);
    void shaderSource(WebGLShader*, const String&);
    void stencilFunc(GC3Denum func, GC3Dint ref, GC3Duint mask);
    void stencilFuncSeparate(GC3Denum face, GC3Denum func, GC3Dint ref, GC3Duint mask);
    void stencilMask(GC3Duint);
    void stencilMaskSeparate(GC3Denum face, GC3Duint mask);
    void stencilOp(GC3Denum fail, GC3Denum zfail, GC3Denum zpass);
    void stencilOpSeparate(GC3Denum face, GC3Denum fail, GC3Denum zfail, GC3Denum zpass);

    void texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, RefPtr<ArrayBufferView>&&);

#if ENABLE(VIDEO)
    using TexImageSource = WTF::Variant<RefPtr<ImageBitmap>, RefPtr<ImageData>, RefPtr<HTMLImageElement>, RefPtr<HTMLCanvasElement>, RefPtr<HTMLVideoElement>>;
#else
    using TexImageSource = WTF::Variant<RefPtr<ImageBitmap>, RefPtr<ImageData>, RefPtr<HTMLImageElement>, RefPtr<HTMLCanvasElement>>;
#endif

    ExceptionOr<void> texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Denum format, GC3Denum type, Optional<TexImageSource>);

    void texParameterf(GC3Denum target, GC3Denum pname, GC3Dfloat param);
    void texParameteri(GC3Denum target, GC3Denum pname, GC3Dint param);

    void texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, RefPtr<ArrayBufferView>&&);
    ExceptionOr<void> texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, Optional<TexImageSource>&&);

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

        GC3Dsizei length() const
        {
            return WTF::switchOn(m_variant,
                [] (const RefPtr<TypedArray>& typedArray) -> GC3Dsizei { return typedArray->length(); },
                [] (const Vector<DataType>& vector) -> GC3Dsizei { return vector.size(); }
            );
        }

    private:
        VariantType m_variant;
    };

    using Float32List = TypedList<Float32Array, float>;
    using Int32List = TypedList<Int32Array, int>;

    void uniform1f(const WebGLUniformLocation*, GC3Dfloat x);
    void uniform2f(const WebGLUniformLocation*, GC3Dfloat x, GC3Dfloat y);
    void uniform3f(const WebGLUniformLocation*, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z);
    void uniform4f(const WebGLUniformLocation*, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w);

    void uniform1i(const WebGLUniformLocation*, GC3Dint x);
    void uniform2i(const WebGLUniformLocation*, GC3Dint x, GC3Dint y);
    void uniform3i(const WebGLUniformLocation*, GC3Dint x, GC3Dint y, GC3Dint z);
    void uniform4i(const WebGLUniformLocation*, GC3Dint x, GC3Dint y, GC3Dint z, GC3Dint w);

    void uniform1fv(const WebGLUniformLocation*, Float32List&&);
    void uniform2fv(const WebGLUniformLocation*, Float32List&&);
    void uniform3fv(const WebGLUniformLocation*, Float32List&&);
    void uniform4fv(const WebGLUniformLocation*, Float32List&&);

    void uniform1iv(const WebGLUniformLocation*, Int32List&&);
    void uniform2iv(const WebGLUniformLocation*, Int32List&&);
    void uniform3iv(const WebGLUniformLocation*, Int32List&&);
    void uniform4iv(const WebGLUniformLocation*, Int32List&&);

    void uniformMatrix2fv(const WebGLUniformLocation*, GC3Dboolean transpose, Float32List&&);
    void uniformMatrix3fv(const WebGLUniformLocation*, GC3Dboolean transpose, Float32List&&);
    void uniformMatrix4fv(const WebGLUniformLocation*, GC3Dboolean transpose, Float32List&&);

    void useProgram(WebGLProgram*);
    void validateProgram(WebGLProgram*);

    void vertexAttrib1f(GC3Duint index, GC3Dfloat x);
    void vertexAttrib2f(GC3Duint index, GC3Dfloat x, GC3Dfloat y);
    void vertexAttrib3f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z);
    void vertexAttrib4f(GC3Duint index, GC3Dfloat x, GC3Dfloat y, GC3Dfloat z, GC3Dfloat w);

    void vertexAttrib1fv(GC3Duint index, Float32List&&);
    void vertexAttrib2fv(GC3Duint index, Float32List&&);
    void vertexAttrib3fv(GC3Duint index, Float32List&&);
    void vertexAttrib4fv(GC3Duint index, Float32List&&);

    void vertexAttribPointer(GC3Duint index, GC3Dint size, GC3Denum type, GC3Dboolean normalized,
        GC3Dsizei stride, long long offset);

    void viewport(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height);

    // WEBKIT_lose_context support
    enum LostContextMode {
        // Lost context occurred at the graphics system level.
        RealLostContext,

        // Lost context provoked by WEBKIT_lose_context.
        SyntheticLostContext
    };
    void forceLostContext(LostContextMode);
    void recycleContext();
    void forceRestoreContext();
    void loseContextImpl(LostContextMode);
    void dispatchContextChangedEvent();
    WEBCORE_EXPORT void simulateContextChanged();

    GraphicsContext3D* graphicsContext3D() const { return m_context.get(); }
    WebGLContextGroup* contextGroup() const { return m_contextGroup.get(); }
    PlatformLayer* platformLayer() const override;

    void reshape(int width, int height) override;

    void markLayerComposited() final;
    void paintRenderingResultsToCanvas() override;
    RefPtr<ImageData> paintRenderingResultsToImageData();

    void removeSharedObject(WebGLSharedObject&);
    void removeContextObject(WebGLContextObject&);
    
    unsigned getMaxVertexAttribs() const { return m_maxVertexAttribs; }

    // Instanced Array helper functions.
    void drawArraysInstanced(GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei primcount);
    void drawElementsInstanced(GC3Denum mode, GC3Dsizei count, GC3Denum type, long long offset, GC3Dsizei primcount);
    void vertexAttribDivisor(GC3Duint index, GC3Duint divisor);

    // Used for testing only, from Internals.
    WEBCORE_EXPORT void setFailNextGPUStatusCheck();

protected:
    WebGLRenderingContextBase(CanvasBase&, WebGLContextAttributes);
    WebGLRenderingContextBase(CanvasBase&, Ref<GraphicsContext3D>&&, WebGLContextAttributes);

    friend class WebGLDrawBuffers;
    friend class WebGLFramebuffer;
    friend class WebGLObject;
    friend class OESVertexArrayObject;
    friend class WebGLDebugShaders;
    friend class WebGLCompressedTextureASTC;
    friend class WebGLCompressedTextureATC;
    friend class WebGLCompressedTexturePVRTC;
    friend class WebGLCompressedTextureS3TC;
    friend class WebGLRenderingContextErrorMessageCallback;
    friend class WebGLVertexArrayObjectOES;
    friend class WebGLVertexArrayObject;
    friend class WebGLVertexArrayObjectBase;

    virtual void initializeNewContext();
    virtual void initializeVertexArrayObjects() = 0;
    void setupFlags();

    // ActiveDOMObject
    bool hasPendingActivity() const override;
    void stop() override;
    const char* activeDOMObjectName() const override;
    bool canSuspendForDocumentSuspension() const override;

    void addSharedObject(WebGLSharedObject&);
    void addContextObject(WebGLContextObject&);
    void detachAndRemoveAllObjects();

    void destroyGraphicsContext3D();
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
    unsigned int sizeInBytes(GC3Denum type);

    // Basic validation of count and offset against number of elements in element array buffer
    bool validateElementArraySize(GC3Dsizei count, GC3Denum type, GC3Dintptr offset);

    // Conservative but quick index validation
    virtual bool validateIndexArrayConservative(GC3Denum type, unsigned& numElementsRequired) = 0;

    // Precise but slow index validation -- only done if conservative checks fail
    bool validateIndexArrayPrecise(GC3Dsizei count, GC3Denum type, GC3Dintptr offset, unsigned& numElementsRequired);
    bool validateVertexAttributes(unsigned elementCount, unsigned primitiveCount = 0);

    bool validateWebGLObject(const char*, WebGLObject*);

    bool validateDrawArrays(const char* functionName, GC3Denum mode, GC3Dint first, GC3Dsizei count, GC3Dsizei primcount);
    bool validateDrawElements(const char* functionName, GC3Denum mode, GC3Dsizei count, GC3Denum type, long long offset, unsigned& numElements, GC3Dsizei primcount);
    bool validateNPOTTextureLevel(GC3Dsizei width, GC3Dsizei height, GC3Dint level, const char* functionName);

    // Adds a compressed texture format.
    void addCompressedTextureFormat(GC3Denum);

    RefPtr<Image> drawImageIntoBuffer(Image&, int width, int height, int deviceScaleFactor);

#if ENABLE(VIDEO)
    RefPtr<Image> videoFrameToImage(HTMLVideoElement*, BackingStoreCopy);
#endif

    WebGLTexture::TextureExtensionFlag textureExtensionFlags() const;

    bool enableSupportedExtension(ASCIILiteral extensionNameLiteral);

    RefPtr<GraphicsContext3D> m_context;
    RefPtr<WebGLContextGroup> m_contextGroup;

    // Dispatches a context lost event once it is determined that one is needed.
    // This is used both for synthetic and real context losses. For real ones, it's
    // likely that there's no JavaScript on the stack, but that might be dependent
    // on how exactly the platform discovers that the context was lost. For better
    // portability we always defer the dispatch of the event.
    Timer m_dispatchContextLostEventTimer;
    bool m_restoreAllowed { false };
    Timer m_restoreTimer;

    bool m_needsUpdate;
    bool m_markedCanvasDirty;
    HashSet<WebGLContextObject*> m_contextObjects;

    // List of bound VBO's. Used to maintain info about sizes for ARRAY_BUFFER and stored values for ELEMENT_ARRAY_BUFFER
    RefPtr<WebGLBuffer> m_boundArrayBuffer;
    RefPtr<WebGLBuffer> m_boundCopyReadBuffer;
    RefPtr<WebGLBuffer> m_boundCopyWriteBuffer;
    RefPtr<WebGLBuffer> m_boundPixelPackBuffer;
    RefPtr<WebGLBuffer> m_boundPixelUnpackBuffer;
    RefPtr<WebGLBuffer> m_boundTransformFeedbackBuffer;
    RefPtr<WebGLBuffer> m_boundUniformBuffer;

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
            value[0] = 0.0f;
            value[1] = 0.0f;
            value[2] = 0.0f;
            value[3] = 1.0f;
        }
        
        GC3Dfloat value[4];
    };
    Vector<VertexAttribValue> m_vertexAttribValue;
    unsigned m_maxVertexAttribs;
    RefPtr<WebGLBuffer> m_vertexAttrib0Buffer;
    long m_vertexAttrib0BufferSize { 0 };
    GC3Dfloat m_vertexAttrib0BufferValue[4];
    bool m_forceAttrib0BufferRefill { true };
    bool m_vertexAttrib0UsedBefore { false };

    RefPtr<WebGLProgram> m_currentProgram;
    RefPtr<WebGLFramebuffer> m_framebufferBinding;
    RefPtr<WebGLFramebuffer> m_readFramebufferBinding;
    RefPtr<WebGLRenderbuffer> m_renderbufferBinding;
    struct TextureUnitState {
        RefPtr<WebGLTexture> texture2DBinding;
        RefPtr<WebGLTexture> textureCubeMapBinding;
    };
    Vector<TextureUnitState> m_textureUnits;
    HashSet<unsigned, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> m_unrenderableTextureUnits;

    unsigned long m_activeTextureUnit;

    RefPtr<WebGLTexture> m_blackTexture2D;
    RefPtr<WebGLTexture> m_blackTextureCubeMap;

    Vector<GC3Denum> m_compressedTextureFormats;

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

    GC3Dint m_maxTextureSize;
    GC3Dint m_maxCubeMapTextureSize;
    GC3Dint m_maxRenderbufferSize;
    GC3Dint m_maxViewportDims[2] { 0, 0 };
    GC3Dint m_maxTextureLevel;
    GC3Dint m_maxCubeMapTextureLevel;

    GC3Dint m_maxDrawBuffers;
    GC3Dint m_maxColorAttachments;
    GC3Denum m_backDrawBuffer;
    bool m_drawBuffersWebGLRequirementsChecked;
    bool m_drawBuffersSupported;

    GC3Dint m_packAlignment;
    GC3Dint m_unpackAlignment;
    bool m_unpackFlipY;
    bool m_unpackPremultiplyAlpha;
    GC3Denum m_unpackColorspaceConversion;
    bool m_contextLost { false };
    LostContextMode m_contextLostMode { SyntheticLostContext };
    WebGLContextAttributes m_attributes;

    bool m_layerCleared;
    GC3Dfloat m_clearColor[4];
    bool m_scissorEnabled;
    GC3Dfloat m_clearDepth;
    GC3Dint m_clearStencil;
    GC3Dboolean m_colorMask[4];
    GC3Dboolean m_depthMask;

    bool m_stencilEnabled;
    GC3Duint m_stencilMask, m_stencilMaskBack;
    GC3Dint m_stencilFuncRef, m_stencilFuncRefBack; // Note that these are the user specified values, not the internal clamped value.
    GC3Duint m_stencilFuncMask, m_stencilFuncMaskBack;

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
    std::unique_ptr<WebGLCompressedTexturePVRTC> m_webglCompressedTexturePVRTC;
    std::unique_ptr<WebGLCompressedTextureS3TC> m_webglCompressedTextureS3TC;
    std::unique_ptr<WebGLDepthTexture> m_webglDepthTexture;
    std::unique_ptr<WebGLDrawBuffers> m_webglDrawBuffers;
    std::unique_ptr<ANGLEInstancedArrays> m_angleInstancedArrays;

    // Helpers for getParameter and other similar functions.
    bool getBooleanParameter(GC3Denum);
    Vector<bool> getBooleanArrayParameter(GC3Denum);
    float getFloatParameter(GC3Denum);
    int getIntParameter(GC3Denum);
    unsigned getUnsignedIntParameter(GC3Denum);
    long long getInt64Parameter(GC3Denum);
    RefPtr<Float32Array> getWebGLFloatArrayParameter(GC3Denum);
    RefPtr<Int32Array> getWebGLIntArrayParameter(GC3Denum);

    // Clear the backbuffer if it was composited since the last operation.
    // clearMask is set to the bitfield of any clear that would happen anyway at this time
    // and the function returns true if that clear is now unnecessary.
    bool clearIfComposited(GC3Dbitfield clearMask = 0);

    // Helper to restore state that clearing the framebuffer may destroy.
    void restoreStateAfterClear();

    void texImage2DBase(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels);
    void texImage2DImpl(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Denum format, GC3Denum type, Image*, GraphicsContext3D::ImageHtmlDomSource, bool flipY, bool premultiplyAlpha);
    void texSubImage2DBase(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum internalformat, GC3Denum format, GC3Denum type, const void* pixels);
    void texSubImage2DImpl(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, Image*, GraphicsContext3D::ImageHtmlDomSource, bool flipY, bool premultiplyAlpha);

    bool checkTextureCompleteness(const char*, bool);

    void createFallbackBlackTextures1x1();

    // Helper function for copyTex{Sub}Image, check whether the internalformat
    // and the color buffer format of the current bound framebuffer combination
    // is valid.
    bool isTexInternalFormatColorBufferCombinationValid(GC3Denum texInternalFormat,
                                                        GC3Denum colorBufferFormat);

    // Helper function to get the bound framebuffer's color buffer format.
    GC3Denum getBoundFramebufferColorFormat();

    // Helper function to get the bound framebuffer's width.
    int getBoundFramebufferWidth();

    // Helper function to get the bound framebuffer's height.
    int getBoundFramebufferHeight();

    // Helper function to verify limits on the length of uniform and attribute locations.
    bool validateLocationLength(const char* functionName, const String&);

    // Helper function to check if size is non-negative.
    // Generate GL error and return false for negative inputs; otherwise, return true.
    bool validateSize(const char* functionName, GC3Dint x, GC3Dint y);

    // Helper function to check if all characters in the string belong to the
    // ASCII subset as defined in GLSL ES 1.0 spec section 3.1.
    bool validateString(const char* functionName, const String&);

    // Helper function to check target and texture bound to the target.
    // Generate GL errors and return 0 if target is invalid or texture bound is
    // null.  Otherwise, return the texture bound to the target.
    RefPtr<WebGLTexture> validateTextureBinding(const char* functionName, GC3Denum target, bool useSixEnumsForCubeMap);

    // Helper function to check input format/type for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncFormatAndType(const char* functionName, GC3Denum internalformat, GC3Denum format, GC3Denum type, GC3Dint level);

    // Helper function to check input level for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if level is invalid.
    bool validateTexFuncLevel(const char* functionName, GC3Denum target, GC3Dint level);

    enum TexFuncValidationFunctionType {
        TexImage,
        TexSubImage,
        CopyTexImage
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
    };

    // Helper function for tex{Sub}Image2D to check if the input format/type/level/target/width/height/border/xoffset/yoffset are valid.
    // Otherwise, it would return quickly without doing other work.
    bool validateTexFunc(const char* functionName, TexFuncValidationFunctionType, TexFuncValidationSourceType, GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width,
        GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, GC3Dint xoffset, GC3Dint yoffset);

    // Helper function to check input parameters for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncParameters(const char* functionName,
        TexFuncValidationFunctionType,
        GC3Denum target, GC3Dint level,
        GC3Denum internalformat,
        GC3Dsizei width, GC3Dsizei height, GC3Dint border,
        GC3Denum format, GC3Denum type);

    enum NullDisposition {
        NullAllowed,
        NullNotAllowed
    };

    // Helper function to validate that the given ArrayBufferView
    // is of the correct type and contains enough data for the texImage call.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncData(const char* functionName, GC3Dint level,
        GC3Dsizei width, GC3Dsizei height,
        GC3Denum internalformat, GC3Denum format, GC3Denum type,
        ArrayBufferView* pixels,
        NullDisposition);

    // Helper function to validate a given texture format is settable as in
    // you can supply data to texImage2D, or call texImage2D, copyTexImage2D and
    // copyTexSubImage2D.
    // Generates GL error and returns false if the format is not settable.
    bool validateSettableTexInternalFormat(const char* functionName, GC3Denum format);

    // Helper function to validate compressed texture data is correct size
    // for the given format and dimensions.
    bool validateCompressedTexFuncData(const char* functionName, GC3Dsizei width, GC3Dsizei height, GC3Denum format, ArrayBufferView& pixels);

    // Helper function for validating compressed texture formats.
    bool validateCompressedTexFormat(GC3Denum format);

    // Helper function to validate compressed texture dimensions are valid for
    // the given format.
    bool validateCompressedTexDimensions(const char* functionName, GC3Denum target, GC3Dint level, GC3Dsizei width, GC3Dsizei height, GC3Denum format);

    // Helper function to validate compressed texture dimensions are valid for
    // the given format.
    bool validateCompressedTexSubDimensions(const char* functionName, GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset,
                                            GC3Dsizei width, GC3Dsizei height, GC3Denum format, WebGLTexture*);

    // Helper function to validate mode for draw{Arrays/Elements}.
    bool validateDrawMode(const char* functionName, GC3Denum);

    // Helper function to validate if front/back stencilMask and stencilFunc settings are the same.
    bool validateStencilSettings(const char* functionName);

    // Helper function to validate stencil func.
    bool validateStencilFunc(const char* functionName, GC3Denum);

    // Helper function for texParameterf and texParameteri.
    void texParameter(GC3Denum target, GC3Denum pname, GC3Dfloat parami, GC3Dint paramf, bool isFloat);

    // Helper function to print errors and warnings to console.
    void printToConsole(MessageLevel, const String&);

    // Helper function to validate input parameters for framebuffer functions.
    // Generate GL error if parameters are illegal.
    virtual bool validateFramebufferFuncParameters(const char* functionName, GC3Denum target, GC3Denum attachment) = 0;

    // Helper function to validate blend equation mode.
    virtual bool validateBlendEquation(const char* functionName, GC3Denum) = 0;

    // Helper function to validate blend func factors.
    bool validateBlendFuncFactors(const char* functionName, GC3Denum src, GC3Denum dst);

    // Helper function to validate a GL capability.
    virtual bool validateCapability(const char* functionName, GC3Denum) = 0;

    // Helper function to validate input parameters for uniform functions.
    bool validateUniformParameters(const char* functionName, const WebGLUniformLocation*, const Float32List&, GC3Dsizei mod);
    bool validateUniformParameters(const char* functionName, const WebGLUniformLocation*, const Int32List&, GC3Dsizei mod);
    bool validateUniformParameters(const char* functionName, const WebGLUniformLocation*, void*, GC3Dsizei, GC3Dsizei mod);
    bool validateUniformMatrixParameters(const char* functionName, const WebGLUniformLocation*, GC3Dboolean transpose, const Float32List&, GC3Dsizei mod);
    bool validateUniformMatrixParameters(const char* functionName, const WebGLUniformLocation*, GC3Dboolean transpose, const void*, GC3Dsizei, GC3Dsizei mod);

    // Helper function to validate parameters for bufferData.
    // Return the current bound buffer to target, or 0 if parameters are invalid.
    WebGLBuffer* validateBufferDataParameters(const char* functionName, GC3Denum target, GC3Denum usage);

    // Helper function for tex{Sub}Image2D to make sure image is ready.
    ExceptionOr<bool> validateHTMLImageElement(const char* functionName, HTMLImageElement*);
    ExceptionOr<bool> validateHTMLCanvasElement(const char* functionName, HTMLCanvasElement*);
#if ENABLE(VIDEO)
    ExceptionOr<bool> validateHTMLVideoElement(const char* functionName, HTMLVideoElement*);
#endif

    // Helper functions for vertexAttribNf{v}.
    void vertexAttribfImpl(const char* functionName, GC3Duint index, GC3Dsizei expectedSize, GC3Dfloat, GC3Dfloat, GC3Dfloat, GC3Dfloat);
    void vertexAttribfvImpl(const char* functionName, GC3Duint index, Float32List&&, GC3Dsizei expectedSize);

    // Helper function for delete* (deleteBuffer, deleteProgram, etc) functions.
    // Return false if caller should return without further processing.
    bool deleteObject(WebGLObject*);

    // Helper function for bind* (bindBuffer, bindTexture, etc) and useProgram.
    // If the object has already been deleted, set deleted to true upon return.
    // Return false if caller should return without further processing.
    bool checkObjectToBeBound(const char* functionName, WebGLObject*, bool& deleted);

    // Helpers for simulating vertexAttrib0.
    void initVertexAttrib0();
    Optional<bool> simulateVertexAttrib0(GC3Duint numVertex);
    bool validateSimulatedVertexAttrib0(GC3Duint numVertex);
    void restoreStatesAfterVertexAttrib0Simulation();

    void dispatchContextLostEvent();
    // Helper for restoration after context lost.
    void maybeRestoreContext();

    // Wrapper for GraphicsContext3D::synthesizeGLError that sends a message to the JavaScript console.
    enum ConsoleDisplayPreference { DisplayInConsole, DontDisplayInConsole };
    void synthesizeGLError(GC3Denum, const char* functionName, const char* description, ConsoleDisplayPreference = DisplayInConsole);

    String ensureNotNull(const String&) const;

    // Enable or disable stencil test based on user setting and whether the current FBO has a stencil buffer.
    void applyStencilTest();

    // Helper for enabling or disabling a capability.
    void enableOrDisable(GC3Denum capability, bool enable);

    // Clamp the width and height to GL_MAX_VIEWPORT_DIMS.
    IntSize clampedCanvasSize();

    virtual GC3Dint getMaxDrawBuffers() = 0;
    virtual GC3Dint getMaxColorAttachments() = 0;

    void setBackDrawBuffer(GC3Denum);

    void restoreCurrentFramebuffer();
    void restoreCurrentTexture2D();

    // Check if EXT_draw_buffers extension is supported and if it satisfies the WebGL requirements.
    bool supportsDrawBuffers();

    HTMLCanvasElement* htmlCanvas();
    OffscreenCanvas* offscreenCanvas();

    template <typename T> inline Optional<T> checkedAddAndMultiply(T value, T add, T multiply);
    template <typename T> unsigned getMaxIndex(const RefPtr<JSC::ArrayBuffer> elementArrayBuffer, GC3Dintptr uoffset, GC3Dsizei n);

private:
    bool validateArrayBufferType(const char* functionName, GC3Denum type, Optional<JSC::TypedArrayType>);
    void registerWithWebGLStateTracker();
    void checkForContextLossHandling();

    void activityStateDidChange(OptionSet<ActivityState::Flag> oldActivityState, OptionSet<ActivityState::Flag> newActivityState) override;

    WebGLStateTracker::Token m_trackerToken;
    Timer m_checkForContextLossHandlingTimer;
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
inline unsigned WebGLRenderingContextBase::getMaxIndex(const RefPtr<JSC::ArrayBuffer> elementArrayBuffer, GC3Dintptr uoffset, GC3Dsizei n)
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
