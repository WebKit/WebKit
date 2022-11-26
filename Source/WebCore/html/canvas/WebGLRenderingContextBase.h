/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include "GraphicsContextGL.h"
#include "ImageBuffer.h"
#include "PredefinedColorSpace.h"
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
#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/ConsoleTypes.h>
#include <limits>
#include <memory>
#include <wtf/CheckedArithmetic.h>
#include <wtf/ListHashSet.h>
#include <wtf/Lock.h>

#if ENABLE(WEB_CODECS)
#include "WebCodecsVideoFrame.h"
#endif

#if ENABLE(WEBGL2)
#include "WebGLSampler.h"
#include "WebGLTransformFeedback.h"
#include "WebGLVertexArrayObject.h"
#endif

#if ENABLE(WEBXR)
#include "JSDOMPromiseDeferred.h"
#endif

namespace JSC {
class AbstractSlotVisitor;
}

namespace WTF {
class AbstractLocker;
}

namespace WebCore {

class ANGLEInstancedArrays;
class EXTBlendMinMax;
class EXTColorBufferFloat;
class EXTColorBufferHalfFloat;
class EXTFloatBlend;
class EXTFragDepth;
class EXTShaderTextureLOD;
class EXTTextureCompressionBPTC;
class EXTTextureCompressionRGTC;
class EXTTextureFilterAnisotropic;
class EXTTextureNorm16;
class EXTsRGB;
class HTMLImageElement;
class ImageData;
class IntSize;
class KHRParallelShaderCompile;
class OESDrawBuffersIndexed;
class OESElementIndexUint;
class OESFBORenderMipmap;
class OESStandardDerivatives;
class OESTextureFloat;
class OESTextureFloatLinear;
class OESTextureHalfFloat;
class OESTextureHalfFloatLinear;
class OESVertexArrayObject;
#if ENABLE(OFFSCREEN_CANVAS)
class OffscreenCanvas;
#endif
class WebCoreOpaqueRoot;
class WebGLActiveInfo;
class WebGLColorBufferFloat;
class WebGLCompressedTextureASTC;
class WebGLCompressedTextureETC;
class WebGLCompressedTextureETC1;
class WebGLCompressedTexturePVRTC;
class WebGLCompressedTextureS3TC;
class WebGLCompressedTextureS3TCsRGB;
class WebGLContextGroup;
class WebGLContextObject;
class WebGLDebugRendererInfo;
class WebGLDebugShaders;
class WebGLDepthTexture;
class WebGLDrawBuffers;
class WebGLDrawInstancedBaseVertexBaseInstance;
class WebGLExtension;
class WebGLLoseContext;
class WebGLMultiDraw;
class WebGLMultiDrawInstancedBaseVertexBaseInstance;
class WebGLObject;
class WebGLProvokingVertex;
class WebGLShader;
class WebGLShaderPrecisionFormat;
class WebGLSharedObject;
class WebGLUniformLocation;

#if ENABLE(VIDEO)
class HTMLVideoElement;
#endif

#if ENABLE(OFFSCREEN_CANVAS)
using WebGLCanvas = std::variant<RefPtr<HTMLCanvasElement>, RefPtr<OffscreenCanvas>>;
#else
using WebGLCanvas = std::variant<RefPtr<HTMLCanvasElement>>;
#endif

#if ENABLE(MEDIA_STREAM)
class VideoFrame;
#endif

class InspectorScopedShaderProgramHighlight {
public:
    InspectorScopedShaderProgramHighlight(WebGLRenderingContextBase&, WebGLProgram*);

    ~InspectorScopedShaderProgramHighlight();

private:
    void showHighlight();
    void hideHighlight();

    struct {
        GCGLfloat color[4];
        GCGLenum equationRGB;
        GCGLenum equationAlpha;
        GCGLenum srcRGB;
        GCGLenum dstRGB;
        GCGLenum srcAlpha;
        GCGLenum dstAlpha;
        GCGLboolean enabled;
    } m_savedBlend;

    WebGLRenderingContextBase& m_context;
    WebGLProgram* m_program { nullptr };
    bool m_didApply { false };
};

class WebGLRenderingContextBase : public GraphicsContextGL::Client, public GPUBasedCanvasRenderingContext, private ActivityStateChangeObserver {
    WTF_MAKE_ISO_ALLOCATED(WebGLRenderingContextBase);
public:
    using WebGLVersion = GraphicsContextGLWebGLVersion;
    static std::unique_ptr<WebGLRenderingContextBase> create(CanvasBase&, WebGLContextAttributes&, WebGLVersion);
    virtual ~WebGLRenderingContextBase();

    WebGLCanvas canvas();

    int drawingBufferWidth() const;
    int drawingBufferHeight() const;

    PredefinedColorSpace drawingBufferColorSpace() const { return m_drawingBufferColorSpace; }
    void setDrawingBufferColorSpace(PredefinedColorSpace);

    void activeTexture(GCGLenum texture);
    void attachShader(WebGLProgram&, WebGLShader&);
    void bindAttribLocation(WebGLProgram&, GCGLuint index, const String& name);
    void bindBuffer(GCGLenum target, WebGLBuffer*);
    virtual void bindFramebuffer(GCGLenum target, WebGLFramebuffer*);
    void bindRenderbuffer(GCGLenum target, WebGLRenderbuffer*);
    void bindTexture(GCGLenum target, WebGLTexture*);
    void blendColor(GCGLfloat red, GCGLfloat green, GCGLfloat blue, GCGLfloat alpha);
    void blendEquation(GCGLenum mode);
    void blendEquationSeparate(GCGLenum modeRGB, GCGLenum modeAlpha);
    void blendFunc(GCGLenum sfactor, GCGLenum dfactor);
    void blendFuncSeparate(GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha);

    using BufferDataSource = std::variant<RefPtr<ArrayBuffer>, RefPtr<ArrayBufferView>>;
    void bufferData(GCGLenum target, long long size, GCGLenum usage);
    void bufferData(GCGLenum target, std::optional<BufferDataSource>&&, GCGLenum usage);
    void bufferSubData(GCGLenum target, long long offset, BufferDataSource&&);

    GCGLenum checkFramebufferStatus(GCGLenum target);
    void clear(GCGLbitfield mask);
    void clearColor(GCGLfloat red, GCGLfloat green, GCGLfloat blue, GCGLfloat alpha);
    void clearDepth(GCGLfloat);
    void clearStencil(GCGLint);
    void colorMask(GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha);
    void compileShader(WebGLShader&);

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
    void detachShader(WebGLProgram&, WebGLShader&);
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

    RefPtr<WebGLActiveInfo> getActiveAttrib(WebGLProgram&, GCGLuint index);
    RefPtr<WebGLActiveInfo> getActiveUniform(WebGLProgram&, GCGLuint index);
    std::optional<Vector<RefPtr<WebGLShader>>> getAttachedShaders(WebGLProgram&);
    GCGLint getAttribLocation(WebGLProgram&, const String& name);
    WebGLAny getBufferParameter(GCGLenum target, GCGLenum pname);
    WEBCORE_EXPORT std::optional<WebGLContextAttributes> getContextAttributes();
    GCGLenum getError();
    virtual WebGLExtension* getExtension(const String& name) = 0;
    virtual WebGLAny getFramebufferAttachmentParameter(GCGLenum target, GCGLenum attachment, GCGLenum pname) = 0;
    virtual WebGLAny getParameter(GCGLenum pname);
    WebGLAny getProgramParameter(WebGLProgram&, GCGLenum pname);
    String getProgramInfoLog(WebGLProgram&);
    WebGLAny getRenderbufferParameter(GCGLenum target, GCGLenum pname);
    WebGLAny getShaderParameter(WebGLShader&, GCGLenum pname);
    String getShaderInfoLog(WebGLShader&);
    RefPtr<WebGLShaderPrecisionFormat> getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType);
    String getShaderSource(WebGLShader&);
    virtual std::optional<Vector<String>> getSupportedExtensions() = 0;
    virtual WebGLAny getTexParameter(GCGLenum target, GCGLenum pname);
    WebGLAny getUniform(WebGLProgram&, const WebGLUniformLocation&);
    RefPtr<WebGLUniformLocation> getUniformLocation(WebGLProgram&, const String&);
    WebGLAny getVertexAttrib(GCGLuint index, GCGLenum pname);
    long long getVertexAttribOffset(GCGLuint index, GCGLenum pname);

    bool extensionIsEnabled(const String&);

    bool isPreservingDrawingBuffer() const { return m_attributes.preserveDrawingBuffer; }

    bool preventBufferClearForInspector() const { return m_preventBufferClearForInspector; }
    void setPreventBufferClearForInspector(bool value) { m_preventBufferClearForInspector = value; }

    void hint(GCGLenum target, GCGLenum mode);
    GCGLboolean isBuffer(WebGLBuffer*);
    bool isContextLost() const;
    GCGLboolean isEnabled(GCGLenum cap);
    GCGLboolean isFramebuffer(WebGLFramebuffer*);
    GCGLboolean isProgram(WebGLProgram*);
    GCGLboolean isRenderbuffer(WebGLRenderbuffer*);
    GCGLboolean isShader(WebGLShader*);
    GCGLboolean isTexture(WebGLTexture*);

    void lineWidth(GCGLfloat);
    void linkProgram(WebGLProgram&);
    bool linkProgramWithoutInvalidatingAttribLocations(WebGLProgram*);
    virtual void pixelStorei(GCGLenum pname, GCGLint param);
#if ENABLE(WEBXR)
    using MakeXRCompatiblePromise = DOMPromiseDeferred<void>;
    void makeXRCompatible(MakeXRCompatiblePromise&&);
    bool isXRCompatible() const { return m_isXRCompatible; }
#endif
    void polygonOffset(GCGLfloat factor, GCGLfloat units);
    // This must be virtual so more validation can be added in WebGL 2.0.
    virtual void readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& pixels);
    void renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height);
    virtual void renderbufferStorageImpl(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, const char* functionName);
    void sampleCoverage(GCGLfloat value, GCGLboolean invert);
    void scissor(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height);
    void shaderSource(WebGLShader&, const String&);
    void stencilFunc(GCGLenum func, GCGLint ref, GCGLuint mask);
    void stencilFuncSeparate(GCGLenum face, GCGLenum func, GCGLint ref, GCGLuint mask);
    void stencilMask(GCGLuint);
    void stencilMaskSeparate(GCGLenum face, GCGLuint mask);
    void stencilOp(GCGLenum fail, GCGLenum zfail, GCGLenum zpass);
    void stencilOpSeparate(GCGLenum face, GCGLenum fail, GCGLenum zfail, GCGLenum zpass);

    // These must be virtual so more validation can be added in WebGL 2.0.
    virtual void texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&&);

    using TexImageSource = std::variant<RefPtr<ImageBitmap>, RefPtr<ImageData>, RefPtr<HTMLImageElement>, RefPtr<HTMLCanvasElement>
#if ENABLE(VIDEO)
        , RefPtr<HTMLVideoElement>
#endif
#if ENABLE(WEB_CODECS)
        , RefPtr<WebCodecsVideoFrame>
#endif
    >;

    virtual ExceptionOr<void> texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLenum format, GCGLenum type, std::optional<TexImageSource>);

    void texParameterf(GCGLenum target, GCGLenum pname, GCGLfloat param);
    void texParameteri(GCGLenum target, GCGLenum pname, GCGLint param);

    // These must be virtual so more validation can be added in WebGL 2.0.
    virtual void texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&&);
    virtual ExceptionOr<void> texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLenum format, GCGLenum type, std::optional<TexImageSource>&&);

    template <class TypedArray, class DataType>
    class TypedList {
    public:
        using VariantType = std::variant<RefPtr<TypedArray>, Vector<DataType>>;

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
    void validateProgram(WebGLProgram&);

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
    using SimulatedEventForTesting = GraphicsContextGL::SimulatedEventForTesting;
    WEBCORE_EXPORT void simulateEventForTesting(SimulatedEventForTesting);

    GraphicsContextGL* graphicsContextGL() const { return m_context.get(); }
    WebGLContextGroup* contextGroup() const { return m_contextGroup.get(); }
    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() override;

    void reshape(int width, int height) override;

    void prepareForDisplayWithPaint() final;
    void paintRenderingResultsToCanvas() final;
    RefPtr<PixelBuffer> paintRenderingResultsToPixelBuffer();
#if ENABLE(MEDIA_STREAM)
    RefPtr<VideoFrame> paintCompositedResultsToVideoFrame();
#endif

    void removeSharedObject(WebGLSharedObject&);
    void removeContextObject(WebGLContextObject&);
    
    unsigned getMaxVertexAttribs() const { return m_maxVertexAttribs; }

    bool isContextUnrecoverablyLost() const;

    // Instanced Array helper functions.
    void drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount);
    void drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, long long offset, GCGLsizei primcount);
    void vertexAttribDivisor(GCGLuint index, GCGLuint divisor);

    // GraphicsContextGL::Client
    void didComposite() override;
    void forceContextLost() override;
    void dispatchContextChangedNotification() override;

    void recycleContext();

    virtual void addMembersToOpaqueRoots(JSC::AbstractSlotVisitor&);
    // This lock must be held across all mutations of containers like
    // Vectors, HashSets, etc. which contain RefPtr<WebGLObject>, and
    // which are traversed by addMembersToOpaqueRoots() or any of the
    // similarly-named methods in WebGLObject subclasses.
    //
    // FIXME: consider changing this mechanism to instead record when
    // individual WebGLObjects are latched / unlatched in the
    // context's state, either directly, or indirectly through
    // container objects. If that were done, then the
    // "GenerateIsReachable=Impl" in various WebGL objects' IDL files
    // would need to be changed to instead query whether the object is
    // currently latched into the context - without traversing all of
    // the latched objects to find the current one, which would be
    // prohibitively expensive.
    Lock& objectGraphLock() WTF_RETURNS_LOCK(m_objectGraphLock);

    // Returns the ordinal number of when the context was last active (drew, read pixels).
    uint64_t activeOrdinal() const { return m_activeOrdinal; }
protected:
    WebGLRenderingContextBase(CanvasBase&, WebGLContextAttributes);
    WebGLRenderingContextBase(CanvasBase&, Ref<GraphicsContextGL>&&, WebGLContextAttributes);

    friend class EXTTextureCompressionBPTC;
    friend class EXTTextureCompressionRGTC;
    friend class OESDrawBuffersIndexed;
    friend class OESVertexArrayObject;
    friend class WebGLCompressedTextureASTC;
    friend class WebGLCompressedTextureETC;
    friend class WebGLCompressedTextureETC1;
    friend class WebGLCompressedTexturePVRTC;
    friend class WebGLCompressedTextureS3TC;
    friend class WebGLCompressedTextureS3TCsRGB;
    friend class WebGLDebugShaders;
    friend class WebGLDrawBuffers;
    friend class WebGLDrawInstancedBaseVertexBaseInstance;
    friend class WebGLMultiDraw;
    friend class WebGLMultiDrawInstancedBaseVertexBaseInstance;

    friend class WebGLFramebuffer;
    friend class WebGLObject;
    friend class WebGLRenderingContextErrorMessageCallback;
    friend class WebGLSync;
    friend class WebGLVertexArrayObject;
    friend class WebGLVertexArrayObjectBase;
    friend class WebGLVertexArrayObjectOES;

    // Implementation helpers.
    friend class InspectorScopedShaderProgramHighlight;
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

    void setGraphicsContextGL(Ref<GraphicsContextGL>&&);
    void destroyGraphicsContextGL();

    enum CallerType {
        // Caller is a user-level draw or clear call.
        CallerTypeDrawOrClear,
        // Caller is anything else, including blits, readbacks or copies.
        CallerTypeOther,
    };

    void markContextChanged();
    void markContextChangedAndNotifyCanvasObserver(CallerType = CallerTypeDrawOrClear);

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

    // Validates the incoming WebGL object, which is assumed to be non-null.
    // Checks that the object belongs to this context and that it's not marked for
    // deletion. Performs a context lost check internally.
    bool validateWebGLObject(const char*, WebGLObject*);

    // Validates the incoming WebGL program or shader, which is assumed to be
    // non-null. OpenGL ES's validation rules differ for these types of objects
    // compared to others. Performs a context lost check internally.
    bool validateWebGLProgramOrShader(const char*, WebGLObject*);

    bool validateVertexArrayObject(const char* functionName);

    // Adds a compressed texture format.
    void addCompressedTextureFormat(GCGLenum);

    // Set UNPACK_ALIGNMENT to 1, all other parameters to 0.
    virtual void resetUnpackParameters();
    // Restore the client unpack parameters.
    virtual void restoreUnpackParameters();

    RefPtr<Image> drawImageIntoBuffer(Image&, int width, int height, int deviceScaleFactor, const char* functionName);

#if ENABLE(VIDEO)
    RefPtr<Image> videoFrameToImage(HTMLVideoElement*, BackingStoreCopy, const char* functionName);
#endif

    WebGLTexture::TextureExtensionFlag textureExtensionFlags() const;

    bool enableSupportedExtension(ASCIILiteral extensionNameLiteral);
    void loseExtensions(LostContextMode);

    virtual void uncacheDeletedBuffer(const AbstractLocker&, WebGLBuffer*);

    bool compositingResultsNeedUpdating() const final { return m_compositingResultsNeedUpdating; }
    bool needsPreparationForDisplay() const final { return true; }
    void prepareForDisplay() final;
    void updateActiveOrdinal();

    struct ContextLostState {
        ContextLostState(LostContextMode mode)
            : mode(mode)
        {
        }
        ListHashSet<GCGLint> errors; // Losing context and WEBGL_lose_context generates errors here.
        LostContextMode mode { LostContextMode::RealLostContext };
        bool restoreRequested { false };
    };

    RefPtr<GraphicsContextGL> m_context;
    RefPtr<WebGLContextGroup> m_contextGroup;
    Lock m_objectGraphLock;

    SuspendableTimer m_restoreTimer;

    bool m_needsUpdate;
    bool m_markedCanvasDirty;
    HashSet<WebGLContextObject*> m_contextObjects;

    // List of bound VBO's. Used to maintain info about sizes for ARRAY_BUFFER and stored values for ELEMENT_ARRAY_BUFFER
    RefPtr<WebGLBuffer> m_boundArrayBuffer;

    RefPtr<WebGLVertexArrayObjectBase> m_defaultVertexArrayObject;
    RefPtr<WebGLVertexArrayObjectBase> m_boundVertexArrayObject;

    void setBoundVertexArrayObject(const AbstractLocker&, WebGLVertexArrayObjectBase*);
    
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
    unsigned long m_activeTextureUnit;

    RefPtr<WebGLTexture> m_blackTexture2D;
    RefPtr<WebGLTexture> m_blackTextureCubeMap;

    Vector<GCGLenum> m_compressedTextureFormats;

    // Fixed-size cache of reusable image buffers for video texImage2D calls.
    class LRUImageBufferCache {
    public:
        LRUImageBufferCache(int capacity);
        // Returns pointer to a cleared image buffer that is owned by the cache. The pointer is valid until next call.
        // Using fillOperator == CompositeOperator::Copy can be used to omit the clear of the buffer.
        ImageBuffer* imageBuffer(const IntSize&, DestinationColorSpace, CompositeOperator fillOperator = CompositeOperator::SourceOver);
    private:
        void bubbleToFront(size_t idx);
        Vector<std::optional<std::pair<DestinationColorSpace, Ref<ImageBuffer>>>> m_buffers;
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

    std::optional<ContextLostState>  m_contextLostState;
    WebGLContextAttributes m_attributes;

    PredefinedColorSpace m_drawingBufferColorSpace { PredefinedColorSpace::SRGB };

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

    bool m_rasterizerDiscardEnabled { false };

    bool m_isGLES2Compliant;
    bool m_isGLES2NPOTStrict;
    bool m_isDepthStencilSupported;
    bool m_isRobustnessEXTSupported;

    bool m_synthesizedErrorsToConsole { true };
    int m_numGLErrorsToConsoleAllowed;

    bool m_preventBufferClearForInspector { false };

    bool m_compositingResultsNeedUpdating { false };
    bool m_isDisplayingWithPaint { false };

    // Enabled extension objects.
    // FIXME: Move some of these to WebGLRenderingContext, the ones not needed for WebGL2
    RefPtr<ANGLEInstancedArrays> m_angleInstancedArrays;
    RefPtr<EXTBlendMinMax> m_extBlendMinMax;
    RefPtr<EXTColorBufferFloat> m_extColorBufferFloat;
    RefPtr<EXTColorBufferHalfFloat> m_extColorBufferHalfFloat;
    RefPtr<EXTFloatBlend> m_extFloatBlend;
    RefPtr<EXTFragDepth> m_extFragDepth;
    RefPtr<EXTShaderTextureLOD> m_extShaderTextureLOD;
    RefPtr<EXTTextureCompressionBPTC> m_extTextureCompressionBPTC;
    RefPtr<EXTTextureCompressionRGTC> m_extTextureCompressionRGTC;
    RefPtr<EXTTextureFilterAnisotropic> m_extTextureFilterAnisotropic;
    RefPtr<EXTTextureNorm16> m_extTextureNorm16;
    RefPtr<EXTsRGB> m_extsRGB;
    RefPtr<KHRParallelShaderCompile> m_khrParallelShaderCompile;
    RefPtr<OESDrawBuffersIndexed> m_oesDrawBuffersIndexed;
    RefPtr<OESElementIndexUint> m_oesElementIndexUint;
    RefPtr<OESFBORenderMipmap> m_oesFBORenderMipmap;
    RefPtr<OESStandardDerivatives> m_oesStandardDerivatives;
    RefPtr<OESTextureFloat> m_oesTextureFloat;
    RefPtr<OESTextureFloatLinear> m_oesTextureFloatLinear;
    RefPtr<OESTextureHalfFloat> m_oesTextureHalfFloat;
    RefPtr<OESTextureHalfFloatLinear> m_oesTextureHalfFloatLinear;
    RefPtr<OESVertexArrayObject> m_oesVertexArrayObject;
    RefPtr<WebGLColorBufferFloat> m_webglColorBufferFloat;
    RefPtr<WebGLCompressedTextureASTC> m_webglCompressedTextureASTC;
    RefPtr<WebGLCompressedTextureETC> m_webglCompressedTextureETC;
    RefPtr<WebGLCompressedTextureETC1> m_webglCompressedTextureETC1;
    RefPtr<WebGLCompressedTexturePVRTC> m_webglCompressedTexturePVRTC;
    RefPtr<WebGLCompressedTextureS3TC> m_webglCompressedTextureS3TC;
    RefPtr<WebGLCompressedTextureS3TCsRGB> m_webglCompressedTextureS3TCsRGB;
    RefPtr<WebGLDebugRendererInfo> m_webglDebugRendererInfo;
    RefPtr<WebGLDebugShaders> m_webglDebugShaders;
    RefPtr<WebGLDepthTexture> m_webglDepthTexture;
    RefPtr<WebGLDrawBuffers> m_webglDrawBuffers;
    RefPtr<WebGLDrawInstancedBaseVertexBaseInstance> m_webglDrawInstancedBaseVertexBaseInstance;
    RefPtr<WebGLLoseContext> m_webglLoseContext;
    RefPtr<WebGLMultiDraw> m_webglMultiDraw;
    RefPtr<WebGLMultiDrawInstancedBaseVertexBaseInstance> m_webglMultiDrawInstancedBaseVertexBaseInstance;
    RefPtr<WebGLProvokingVertex> m_webglProvokingVertex;

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
    bool clearIfComposited(CallerType, GCGLbitfield clearMask = 0);

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
    enum class ArrayBufferViewFunctionType {
        TexImage,
        ReadPixels
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

            if (maxYAccessed > imageHeight) {
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
    void texImageImpl(TexImageFunctionID, GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLenum format, GCGLenum type, Image*, GraphicsContextGL::DOMSource, bool flipY, bool premultiplyAlpha, bool ignoreNativeImageAlphaPremultiplication, const IntRect&, GCGLsizei depth, GCGLint unpackImageHeight);
    void texImage2DBase(GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels);
    void texSubImage2DBase(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum internalFormat, GCGLenum format, GCGLenum type, GCGLSpan<const GCGLvoid> pixels);
    static const char* getTexImageFunctionName(TexImageFunctionID);
    using PixelStoreParams = GraphicsContextGL::PixelStoreParams;
    virtual PixelStoreParams getPackPixelStoreParams() const;
    virtual PixelStoreParams getUnpackPixelStoreParams(TexImageDimension) const;

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

    // Helper function to check internal formats accepted by ANGLE but not available in WebGL.
    // Generates GL error and returns false if the internal format is invalid.
    bool validateForbiddenInternalFormats(const char* functionName, GCGLenum internalformat);

    // Helper function to check input level for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if level is invalid.
    bool validateTexFuncLevel(const char* functionName, GCGLenum target, GCGLint level);
    virtual GCGLint maxTextureLevelForTarget(GCGLenum target);

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
    std::optional<GCGLSpan<const GCGLvoid>> validateTexFuncData(const char* functionName, TexImageDimension,
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

    // Helper function for validating compressed texture formats.
    bool validateCompressedTexFormat(const char* functionName, GCGLenum format);

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
    virtual bool validateCapability(const char* functionName, GCGLenum);

    // Helper function to validate input parameters for uniform functions.
    bool validateUniformLocation(const char* functionName, const WebGLUniformLocation*);
    template<typename T, typename TypedListType>
    std::optional<GCGLSpan<const T>> validateUniformParameters(const char* functionName, const WebGLUniformLocation* location, const TypedList<TypedListType, T>& values, GCGLsizei requiredMinSize, GCGLuint srcOffset = 0, GCGLuint srcLength = 0)
    {
        return validateUniformMatrixParameters(functionName, location, false, values, requiredMinSize, srcOffset, srcLength);
    }
    template<typename T, typename TypedListType>
    std::optional<GCGLSpan<const T>> validateUniformMatrixParameters(const char* functionName, const WebGLUniformLocation*, GCGLboolean transpose, const TypedList<TypedListType, T>&, GCGLsizei requiredMinSize, GCGLuint srcOffset = 0, GCGLuint srcLength = 0);

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
    bool deleteObject(const AbstractLocker&, WebGLObject*);

    // Helper function for APIs which can legally receive null objects, including
    // the bind* calls (bindBuffer, bindTexture, etc.) and useProgram. Checks that
    // the object belongs to this context and that it's not marked for deletion.
    // Returns false if the caller should return without further processing.
    // Performs a context lost check internally.
    // This returns true for null WebGLObject arguments!
    bool validateNullableWebGLObject(const char* functionName, WebGLObject*);

    // Helper function to validate the target for bufferData and
    // getBufferParameter.
    virtual bool validateBufferTarget(const char* functionName, GCGLenum target);

    // Helper function to validate the target for bufferData.
    // Return the current bound buffer to target, or 0 if the target is invalid.
    virtual WebGLBuffer* validateBufferDataTarget(const char* functionName, GCGLenum target);

    virtual bool validateAndCacheBufferBinding(const AbstractLocker&, const char* functionName, GCGLenum target, WebGLBuffer*);

    // Wrapper for GraphicsContextGLOpenGL::synthesizeGLError that sends a message to the JavaScript console.
    void synthesizeGLError(GCGLenum, const char* functionName, const char* description);
    void synthesizeLostContextGLError(GCGLenum, const char* functionName, const char* description);

    String ensureNotNull(const String&) const;

    // Helper for enabling or disabling a capability.
    void enableOrDisable(GCGLenum capability, bool enable);

    // Clamp the width and height to GL_MAX_VIEWPORT_DIMS.
    IntSize clampedCanvasSize();

    virtual GCGLint getMaxDrawBuffers() = 0;
    virtual GCGLint getMaxColorAttachments() = 0;

    void setBackDrawBuffer(GCGLenum);
    void setFramebuffer(const AbstractLocker&, GCGLenum, WebGLFramebuffer*);

    virtual void restoreCurrentFramebuffer();
    void restoreCurrentTexture2D();

    // Check if EXT_draw_buffers extension is supported and if it satisfies the WebGL requirements.
    bool supportsDrawBuffers();

#if ENABLE(OFFSCREEN_CANVAS)
    OffscreenCanvas* offscreenCanvas();
#endif

    bool validateTypeAndArrayBufferType(const char* functionName, ArrayBufferViewFunctionType, GCGLenum type, ArrayBufferView* pixels);

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
    // The ordinal number of when the context was last active (drew, read pixels).
    uint64_t m_activeOrdinal { 0 };
};

WebCoreOpaqueRoot root(WebGLRenderingContextBase*);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::WebGLRenderingContextBase, isWebGL())

#endif
