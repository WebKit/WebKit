/*
 * Copyright (C) 2015-2023 Apple Inc. All rights reserved.
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

#include "EventLoop.h"
#include "ExceptionOr.h"
#include "GPUBasedCanvasRenderingContext.h"
#include "GraphicsContextGL.h"
#include "ImageBuffer.h"
#include "PredefinedColorSpace.h"
#include "Timer.h"
#include "WebGLAny.h"
#include "WebGLBuffer.h"
#include "WebGLContextAttributes.h"
#include "WebGLExtension.h"
#include "WebGLExtensionAny.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLQuery.h"
#include "WebGLRenderbuffer.h"
#include "WebGLSampler.h"
#include "WebGLTexture.h"
#include "WebGLTimerQueryEXT.h"
#include "WebGLTransformFeedback.h"
#include "WebGLVertexArrayObject.h"
#include "WebGLVertexArrayObjectOES.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <JavaScriptCore/GenericTypedArrayView.h>
#include <JavaScriptCore/TypedArrayAdaptors.h>
#include <array>
#include <limits>
#include <memory>
#include <wtf/CheckedArithmetic.h>
#include <wtf/ListHashSet.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>

#if ENABLE(WEBXR)
#include "JSDOMPromiseDeferredForward.h"
#endif

#include "GCGLSpan.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {
class WebGLRenderingContextBase;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebCore::WebGLRenderingContextBase> : std::true_type { };
}

namespace JSC {
class AbstractSlotVisitor;
}

namespace WTF {
class AbstractLocker;
}

namespace WebCore {

class ANGLEInstancedArrays;
class ByteArrayPixelBuffer;
class EXTBlendMinMax;
class EXTClipControl;
class EXTColorBufferFloat;
class EXTColorBufferHalfFloat;
class EXTConservativeDepth;
class EXTDepthClamp;
class EXTDisjointTimerQuery;
class EXTDisjointTimerQueryWebGL2;
class EXTFloatBlend;
class EXTFragDepth;
class EXTPolygonOffsetClamp;
class EXTRenderSnorm;
class EXTShaderTextureLOD;
class EXTsRGB;
class EXTTextureCompressionBPTC;
class EXTTextureCompressionRGTC;
class EXTTextureFilterAnisotropic;
class EXTTextureMirrorClampToEdge;
class EXTTextureNorm16;
class HTMLImageElement;
class ImageData;
class IntSize;
class KHRParallelShaderCompile;
class NVShaderNoperspectiveInterpolation;
class OESDrawBuffersIndexed;
class OESElementIndexUint;
class OESFBORenderMipmap;
class OESSampleVariables;
class OESShaderMultisampleInterpolation;
class OESStandardDerivatives;
class OESTextureFloat;
class OESTextureFloatLinear;
class OESTextureHalfFloat;
class OESTextureHalfFloatLinear;
class OESVertexArrayObject;
class WebCodecsVideoFrame;
class WebCoreOpaqueRoot;
class WebGLActiveInfo;
class WebGLBlendFuncExtended;
class WebGLClipCullDistance;
class WebGLColorBufferFloat;
class WebGLCompressedTextureASTC;
class WebGLCompressedTextureETC;
class WebGLCompressedTextureETC1;
class WebGLCompressedTexturePVRTC;
class WebGLCompressedTextureS3TC;
class WebGLCompressedTextureS3TCsRGB;
class WebGLDebugRendererInfo;
class WebGLDebugShaders;
class WebGLDefaultFramebuffer;
class WebGLDepthTexture;
class WebGLDrawBuffers;
class WebGLDrawInstancedBaseVertexBaseInstance;
class WebGLLoseContext;
class WebGLMultiDraw;
class WebGLMultiDrawInstancedBaseVertexBaseInstance;
class WebGLObject;
class WebGLPolygonMode;
class WebGLPolygonMode;
class WebGLProvokingVertex;
class WebGLRenderSharedExponent;
class WebGLShader;
class WebGLShaderPrecisionFormat;
class WebGLStencilTexturing;
class WebGLUniformLocation;

#if ENABLE(VIDEO)
class HTMLVideoElement;
#endif

#if ENABLE(OFFSCREEN_CANVAS)
class OffscreenCanvas;
using WebGLCanvas = std::variant<RefPtr<HTMLCanvasElement>, RefPtr<OffscreenCanvas>>;
#else
using WebGLCanvas = std::variant<RefPtr<HTMLCanvasElement>>;
#endif

#if ENABLE(MEDIA_STREAM)
class VideoFrame;
#endif

class WebGLRenderingContextBase : public GraphicsContextGL::Client, public GPUBasedCanvasRenderingContext {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WebGLRenderingContextBase);
public:
    USING_CAN_MAKE_WEAKPTR(GPUBasedCanvasRenderingContext);

    static std::unique_ptr<WebGLRenderingContextBase> create(CanvasBase&, WebGLContextAttributes, WebGLVersion);
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
    std::optional<Vector<Ref<WebGLShader>>> getAttachedShaders(WebGLProgram&);
    GCGLint getAttribLocation(WebGLProgram&, const String& name);
    WebGLAny getBufferParameter(GCGLenum target, GCGLenum pname);
    WEBCORE_EXPORT std::optional<WebGLContextAttributes> getContextAttributes();
    WebGLContextAttributes creationAttributes() const { return m_creationAttributes; }
    GCGLenum getError();
    virtual std::optional<WebGLExtensionAny> getExtension(const String& name) = 0;
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
    bool linkProgramWithoutInvalidatingAttribLocations(WebGLProgram&);
    virtual void pixelStorei(GCGLenum pname, GCGLint param);
#if ENABLE(WEBXR)
    using MakeXRCompatiblePromise = DOMPromiseDeferred<void>;
    void makeXRCompatible(MakeXRCompatiblePromise&&);
    bool isXRCompatible() const { return m_attributes.xrCompatible; }
#endif
    void polygonOffset(GCGLfloat factor, GCGLfloat units);
    // This must be virtual so more validation can be added in WebGL 2.0.
    virtual void readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& pixels);
    void renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height);
    virtual void renderbufferStorageImpl(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, ASCIILiteral functionName);
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
#if ENABLE(OFFSCREEN_CANVAS)
        , RefPtr<OffscreenCanvas>
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

    virtual GCGLint maxDrawBuffers() = 0;
    virtual GCGLint maxColorAttachments() = 0;
    size_t maxVertexAttribs() const { return m_vertexAttribValue.size(); }
    GCGLint maxSamples() const { return m_maxSamples; }

    // WEBKIT_lose_context support
    enum LostContextMode {
        // Lost context occurred at the graphics system level.
        RealLostContext,

        // Lost context provoked by WEBKIT_lose_context.
        SyntheticLostContext
    };
    void forceLostContext(LostContextMode);
    void forceRestoreContext();
    using SimulatedEventForTesting = GraphicsContextGL::SimulatedEventForTesting;
    WEBCORE_EXPORT void simulateEventForTesting(SimulatedEventForTesting);

    GraphicsContextGL* graphicsContextGL() const { return m_context.get(); }
    RefPtr<GraphicsContextGL> protectedGraphicsContextGL() const { return m_context; }

    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() override;

    void reshape() override;

    RefPtr<ImageBuffer> surfaceBufferToImageBuffer(SurfaceBuffer) final;

    RefPtr<ByteArrayPixelBuffer> drawingBufferToPixelBuffer();
#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
    RefPtr<VideoFrame> surfaceBufferToVideoFrame(SurfaceBuffer);
#endif
    RefPtr<ImageBuffer> transferToImageBuffer() final;

    void removeSharedObject(WebGLObject&);
    void removeContextObject(WebGLObject&);

    bool isContextUnrecoverablyLost() const;

    // Instanced Array helper functions.
    void drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount);
    void drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, long long offset, GCGLsizei primcount);
    void vertexAttribDivisor(GCGLuint index, GCGLuint divisor);

    // GraphicsContextGL::Client
    void forceContextLost() final;
    void addDebugMessage(GCGLenum, GCGLenum, GCGLenum, const String&) final;

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

    using PixelStoreParameters = GraphicsContextGL::PixelStoreParameters;
    const PixelStoreParameters& pixelStorePackParameters() const { return m_packParameters; }
    const PixelStoreParameters& unpackPixelStoreParameters() const { return m_unpackParameters; };

    WeakPtr<WebGLRenderingContextBase> createRefForContextObject();

    bool compositingResultsNeedUpdating() const final { return m_compositingResultsNeedUpdating; }
    void prepareForDisplay() final;
protected:
    WebGLRenderingContextBase(CanvasBase&, CanvasRenderingContext::Type, WebGLContextAttributes&&);

    friend class EXTDisjointTimerQuery;
    friend class EXTDisjointTimerQueryWebGL2;
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
    friend class WebGLPolygonMode;

    friend class WebGLFramebuffer;
    friend class WebGLObject;
    friend class WebGLRenderingContextErrorMessageCallback;
    friend class WebGLSync;
    friend class WebGLVertexArrayObject;
    friend class WebGLVertexArrayObjectBase;
    friend class WebGLVertexArrayObjectOES;

    // Implementation helpers.
    friend class ScopedClearColorAndMask;
    friend class ScopedClearDepthAndMask;
    friend class ScopedClearStencilAndMask;
    friend class ScopedDisableRasterizerDiscard;
    friend class ScopedDisableScissorTest;
    friend class ScopedEnableBackbuffer;
    friend class ScopedInspectorShaderProgramHighlight;
    friend class ScopedWebGLRestoreFramebuffer;
    friend class ScopedWebGLRestoreRenderbuffer;
    friend class ScopedWebGLRestoreTexture;

    void initializeNewContext(Ref<GraphicsContextGL>);
    virtual void initializeContextState();
    virtual void initializeDefaultObjects();

    // ActiveDOMObject
    void stop() override;
    void suspend(ReasonForSuspension) override;
    void resume() override;

    void detachAndRemoveAllObjects();

    void destroyGraphicsContextGL();

    enum CallerType {
        // Caller is a user-level draw or clear call.
        CallerTypeDrawOrClear,
        // Caller is anything else, including blits, readbacks or copies.
        CallerTypeOther,
    };

    void markContextChangedAndNotifyCanvasObserver(CallerType = CallerTypeDrawOrClear);

    void addActivityStateChangeObserverIfNecessary();
    void removeActivityStateChangeObserver();

    // Query if depth_stencil buffer is supported.
    bool isDepthStencilSupported() { return m_isDepthStencilSupported; }

    // Helper to return the size in bytes of OpenGL data types
    // like GL_FLOAT, GL_INT, etc.
    unsigned sizeInBytes(GCGLenum type);

    // Validates the incoming WebGL object.
    template<typename T> bool validateWebGLObject(ASCIILiteral, const T&);
    // Helper function for APIs which can legally receive null objects, including
    // the bind* calls (bindBuffer, bindTexture, etc.) and useProgram.
    // This returns true for null WebGLObject arguments!
    template<typename T> bool validateNullableWebGLObject(ASCIILiteral, const T*);
    template<typename T> GCGLboolean validateIsWebGLObject(const T*) const;

    // Validates the incoming WebGL program or shader, which is assumed to be
    // non-null. OpenGL ES's validation rules differ for these types of objects
    // compared to others. Performs a context lost check internally.
    bool validateWebGLObject(ASCIILiteral, WebGLObject*);

    bool validateVertexArrayObject(ASCIILiteral functionName);

    // Adds a compressed texture format.
    void addCompressedTextureFormat(GCGLenum);

    RefPtr<Image> drawImageIntoBuffer(Image&, int width, int height, int deviceScaleFactor, ASCIILiteral functionName);

#if ENABLE(VIDEO)
    RefPtr<Image> videoFrameToImage(HTMLVideoElement&, ASCIILiteral functionName);
#endif

    bool enableSupportedExtension(ASCIILiteral extensionNameLiteral);
    void loseExtensions(LostContextMode);

    virtual void uncacheDeletedBuffer(const AbstractLocker&, WebGLBuffer*);
    bool needsPreparationForDisplay() const final { return true; }
    void updateActiveOrdinal();

    struct ContextLostState {
        ContextLostState(LostContextMode mode)
            : mode(mode)
        {
        }
        GCGLErrorCodeSet errors; // Losing context and WEBGL_lose_context generates errors here.
        LostContextMode mode { LostContextMode::RealLostContext };
        bool restoreRequested { false };
    };

    RefPtr<GraphicsContextGL> m_context;
    Lock m_objectGraphLock;

    EventLoopTimerHandle m_restoreTimer;
    GCGLErrorCodeSet m_errors;

    // List of bound VBO's. Used to maintain info about sizes for ARRAY_BUFFER and stored values for ELEMENT_ARRAY_BUFFER
    WebGLBindingPoint<WebGLBuffer, GraphicsContextGL::ARRAY_BUFFER> m_boundArrayBuffer;

    std::unique_ptr<WebGLDefaultFramebuffer> m_defaultFramebuffer;
    RefPtr<WebGLVertexArrayObjectBase> m_defaultVertexArrayObject;
    WebGLBindingPoint<WebGLVertexArrayObjectBase> m_boundVertexArrayObject;

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

    RefPtr<WebGLProgram> m_currentProgram;
    WebGLBindingPoint<WebGLFramebuffer> m_framebufferBinding;
    WebGLBindingPoint<WebGLRenderbuffer> m_renderbufferBinding;
    struct TextureUnitState {
        WebGLBindingPoint<WebGLTexture, GraphicsContextGL::TEXTURE_2D> texture2DBinding;
        WebGLBindingPoint<WebGLTexture, GraphicsContextGL::TEXTURE_CUBE_MAP> textureCubeMapBinding;
        WebGLBindingPoint<WebGLTexture, GraphicsContextGL::TEXTURE_3D> texture3DBinding;
        WebGLBindingPoint<WebGLTexture, GraphicsContextGL::TEXTURE_2D_ARRAY> texture2DArrayBinding;
    };
    Vector<TextureUnitState> m_textureUnits;
    unsigned long m_activeTextureUnit;

    Vector<GCGLenum> m_compressedTextureFormats;

    // Fixed-size cache of reusable image buffers for video texImage2D calls.
    class LRUImageBufferCache {
    public:
        LRUImageBufferCache(int capacity);
        // Returns pointer to a cleared image buffer that is owned by the cache. The pointer is valid until next call.
        // Using fillOperator == CompositeOperator::Copy can be used to omit the clear of the buffer.
        RefPtr<ImageBuffer> imageBuffer(const IntSize&, DestinationColorSpace, CompositeOperator fillOperator = CompositeOperator::SourceOver);
    private:
        void bubbleToFront(size_t idx);
        Vector<std::optional<std::pair<DestinationColorSpace, Ref<ImageBuffer>>>> m_buffers;
    };
    LRUImageBufferCache m_generatedImageCache { 0 };

    GCGLint m_maxTextureSize;
    GCGLint m_maxCubeMapTextureSize;
    GCGLint m_maxRenderbufferSize;
    std::array<GCGLint, 2> m_maxViewportDims { 0, 0 };
    GCGLint m_maxTextureLevel;
    GCGLint m_maxCubeMapTextureLevel;
    GCGLint m_maxSamples { 0 };

    GCGLint m_maxDrawBuffers;
    GCGLint m_maxColorAttachments;
    GCGLenum m_backDrawBuffer;
    bool m_drawBuffersWebGLRequirementsChecked;
    bool m_drawBuffersSupported;

    PixelStoreParameters m_packParameters;
    PixelStoreParameters m_unpackParameters;
    bool m_unpackFlipY;
    bool m_unpackPremultiplyAlpha;
    GCGLenum m_unpackColorspaceConversion;

    std::optional<ContextLostState>  m_contextLostState;
    WebGLContextAttributes m_attributes; // "actual context parameters" in WebGL 1 spec.
    WebGLContextAttributes m_creationAttributes; // "context creation parameters" in WebGL 1 spec.

    PredefinedColorSpace m_drawingBufferColorSpace { PredefinedColorSpace::SRGB };

    GCGLfloat m_clearColor[4];
    bool m_scissorEnabled;
    GCGLfloat m_clearDepth;
    GCGLint m_clearStencil;
    GCGLboolean m_colorMask[4];
    GCGLuint m_stencilMask;
    GCGLboolean m_depthMask;

    bool m_rasterizerDiscardEnabled { false };

    bool m_isGLES2Compliant;
    bool m_isDepthStencilSupported;

    int m_numGLErrorsToConsoleAllowed;

    bool m_compositingResultsNeedUpdating { false };
    std::optional<SurfaceBuffer> m_canvasBufferContents;

    // Enabled extension objects.
    // FIXME: Move some of these to WebGLRenderingContext, the ones not needed for WebGL2
    RefPtr<ANGLEInstancedArrays> m_angleInstancedArrays;
    RefPtr<EXTBlendMinMax> m_extBlendMinMax;
    RefPtr<EXTClipControl> m_extClipControl;
    RefPtr<EXTColorBufferFloat> m_extColorBufferFloat;
    RefPtr<EXTColorBufferHalfFloat> m_extColorBufferHalfFloat;
    RefPtr<EXTConservativeDepth> m_extConservativeDepth;
    RefPtr<EXTDepthClamp> m_extDepthClamp;
    RefPtr<EXTDisjointTimerQuery> m_extDisjointTimerQuery;
    RefPtr<EXTDisjointTimerQueryWebGL2> m_extDisjointTimerQueryWebGL2;
    RefPtr<EXTFloatBlend> m_extFloatBlend;
    RefPtr<EXTFragDepth> m_extFragDepth;
    RefPtr<EXTPolygonOffsetClamp> m_extPolygonOffsetClamp;
    RefPtr<EXTRenderSnorm> m_extRenderSnorm;
    RefPtr<EXTShaderTextureLOD> m_extShaderTextureLOD;
    RefPtr<EXTTextureCompressionBPTC> m_extTextureCompressionBPTC;
    RefPtr<EXTTextureCompressionRGTC> m_extTextureCompressionRGTC;
    RefPtr<EXTTextureFilterAnisotropic> m_extTextureFilterAnisotropic;
    RefPtr<EXTTextureMirrorClampToEdge> m_extTextureMirrorClampToEdge;
    RefPtr<EXTTextureNorm16> m_extTextureNorm16;
    RefPtr<EXTsRGB> m_extsRGB;
    RefPtr<KHRParallelShaderCompile> m_khrParallelShaderCompile;
    RefPtr<NVShaderNoperspectiveInterpolation> m_nvShaderNoperspectiveInterpolation;
    RefPtr<OESDrawBuffersIndexed> m_oesDrawBuffersIndexed;
    RefPtr<OESElementIndexUint> m_oesElementIndexUint;
    RefPtr<OESFBORenderMipmap> m_oesFBORenderMipmap;
    RefPtr<OESSampleVariables> m_oesSampleVariables;
    RefPtr<OESShaderMultisampleInterpolation> m_oesShaderMultisampleInterpolation;
    RefPtr<OESStandardDerivatives> m_oesStandardDerivatives;
    RefPtr<OESTextureFloat> m_oesTextureFloat;
    RefPtr<OESTextureFloatLinear> m_oesTextureFloatLinear;
    RefPtr<OESTextureHalfFloat> m_oesTextureHalfFloat;
    RefPtr<OESTextureHalfFloatLinear> m_oesTextureHalfFloatLinear;
    RefPtr<OESVertexArrayObject> m_oesVertexArrayObject;
    RefPtr<WebGLBlendFuncExtended> m_webglBlendFuncExtended;
    RefPtr<WebGLClipCullDistance> m_webglClipCullDistance;
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
    RefPtr<WebGLPolygonMode> m_webglPolygonMode;
    RefPtr<WebGLProvokingVertex> m_webglProvokingVertex;
    RefPtr<WebGLRenderSharedExponent> m_webglRenderSharedExponent;
    RefPtr<WebGLStencilTexturing> m_webglStencilTexturing;

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
    virtual long long getInt64Parameter(GCGLenum) = 0;
    RefPtr<Float32Array> getWebGLFloatArrayParameter(GCGLenum);
    RefPtr<Int32Array> getWebGLIntArrayParameter(GCGLenum);

    // Clear the backbuffer if it was composited since the last operation.
    // clearMask is set to the bitfield of any clear that would happen anyway at this time
    // and the function returns true if that clear is now unnecessary.
    bool clearIfComposited(CallerType, GCGLbitfield clearMask = 0);

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
#if ENABLE(OFFSCREEN_CANVAS)
        SourceOffscreenCanvas,
#endif
#if ENABLE(WEB_CODECS)
        SourceWebCodecsVideoFrame,
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

    // Validates size and pack sizes.
    bool validateReadPixelsDimensions(GCGLint width, GCGLint height);
    bool validateTexImageSubRectangle(TexImageFunctionID, const IntRect& imageSize, const IntRect& subRect, GCGLsizei depth, GCGLint unpackImageHeight, bool* selectingSubRectangle);

    IntRect sentinelEmptyRect();
    IntRect getImageDataSize(ImageData*);

    ExceptionOr<void> texImageSourceHelper(TexImageFunctionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& sourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, TexImageSource&&);
    void texImageArrayBufferViewHelper(TexImageFunctionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, RefPtr<ArrayBufferView>&& pixels, NullDisposition, GCGLuint srcOffset);
    void texImageImpl(TexImageFunctionID, GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLenum format, GCGLenum type, Image&, GraphicsContextGL::DOMSource, bool flipY, bool premultiplyAlpha, bool ignoreNativeImageAlphaPremultiplication, const IntRect&, GCGLsizei depth, GCGLint unpackImageHeight);
    void texImage2DBase(GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, std::span<const uint8_t> pixels);
    void texSubImage2DBase(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum internalFormat, GCGLenum format, GCGLenum type, std::span<const uint8_t> pixels);
    static ASCIILiteral texImageFunctionName(TexImageFunctionID);
    static TexImageFunctionType texImageFunctionType(TexImageFunctionID);

    PixelStoreParameters computeUnpackPixelStoreParameters(TexImageDimension) const;

    // Helper function to verify limits on the length of uniform and attribute locations.
    bool validateLocationLength(ASCIILiteral functionName, const String&);

    // Helper function to check if size is non-negative.
    // Generate GL error and return false for negative inputs; otherwise, return true.
    bool validateSize(ASCIILiteral functionName, GCGLint x, GCGLint y, GCGLint z = 0);

    // Helper function to check if all characters in the string belong to the
    // ASCII subset as defined in GLSL ES 1.0 spec section 3.1.
    bool validateString(ASCIILiteral functionName, const String&);

    // Helper function to check target and texture bound to the target.
    // Generate GL errors and return 0 if target is invalid or texture bound is
    // null.  Otherwise, return the texture bound to the target.
    RefPtr<WebGLTexture> validateTextureBinding(ASCIILiteral functionName, GCGLenum target);

    // Wrapper function for validateTexture2D(3D)Binding, used in texImageSourceHelper.
    virtual RefPtr<WebGLTexture> validateTexImageBinding(TexImageFunctionID, GCGLenum);

    // Helper function to check texture 2D target and texture bound to the target.
    // Generate GL errors and return 0 if target is invalid or texture bound is
    // null. Otherwise, return the texture bound to the target.
    RefPtr<WebGLTexture> validateTexture2DBinding(ASCIILiteral, GCGLenum);

    void addExtensionSupportedFormatsAndTypes();
    void addExtensionSupportedFormatsAndTypesWebGL2();

    // Helper function to check input internalformat/format/type for functions
    // Tex{Sub}Image taking TexImageSource source data. Generates GL error and
    // returns false if parameters are invalid.
    bool validateTexImageSourceFormatAndType(TexImageFunctionID, GCGLenum internalformat, GCGLenum format, GCGLenum type);

    // Helper function to check input format/type for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncFormatAndType(ASCIILiteral functionName, GCGLenum internalformat, GCGLenum format, GCGLenum type, GCGLint level);

    // Helper function to check internal formats accepted by ANGLE but not available in WebGL.
    // Generates GL error and returns false if the internal format is invalid.
    bool validateForbiddenInternalFormats(ASCIILiteral functionName, GCGLenum internalformat);

    // Helper function to check input level for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if level is invalid.
    bool validateTexFuncLevel(ASCIILiteral functionName, GCGLenum target, GCGLint level);
    virtual GCGLint maxTextureLevelForTarget(GCGLenum target);

    // Helper function for tex{Sub}Image{2|3}D to check if the input format/type/level/target/width/height/depth/border/xoffset/yoffset/zoffset are valid.
    // Otherwise, it would return quickly without doing other work.
    bool validateTexFunc(TexImageFunctionID, TexFuncValidationSourceType, GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width,
        GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset);

    // Helper function to check input parameters for functions {copy}Tex{Sub}Image.
    // Generates GL error and returns false if parameters are invalid.
    bool validateTexFuncParameters(TexImageFunctionID,
        TexFuncValidationSourceType,
        GCGLenum target, GCGLint level,
        GCGLenum internalformat,
        GCGLsizei width, GCGLsizei height, GCGLsizei depth,
        GCGLint border,
        GCGLenum format, GCGLenum type);

    // Helper function to validate pixel transfer format and type.
    bool validateImageFormatAndType(ASCIILiteral functionName, GCGLenum format, GCGLenum type);

    // Helper function to validate that the given ArrayBufferView
    // is of the correct type and contains enough data for the texImage call.
    // Generates GL error and returns false if parameters are invalid.
    std::optional<std::span<const uint8_t>> validateTexFuncData(ASCIILiteral functionName, TexImageDimension,
        GCGLsizei width, GCGLsizei height, GCGLsizei depth,
        GCGLenum format, GCGLenum type,
        ArrayBufferView* pixels,
        NullDisposition,
        GCGLuint srcOffset);

    // Helper function to validate a given texture format is settable as in
    // you can supply data to texImage2D, or call texImage2D, copyTexImage2D and
    // copyTexSubImage2D.
    // Generates GL error and returns false if the format is not settable.
    bool validateSettableTexInternalFormat(ASCIILiteral functionName, GCGLenum format);

    // Helper function for texParameterf and texParameteri.
    void texParameter(GCGLenum target, GCGLenum pname, GCGLfloat paramf, GCGLint parami, bool isFloat);

    // Helper function to print errors and warnings to console.
    bool shouldPrintToConsole() const;
    void printToConsole(MessageLevel, String&&);

    // Helper function to validate the target for checkFramebufferStatus and
    // validateFramebufferFuncParameters.
    virtual bool validateFramebufferTarget(GCGLenum target);

    // Get the framebuffer bound to the given target.
    virtual WebGLFramebuffer* getFramebufferBinding(GCGLenum target);

    // Helper function to validate input parameters for framebuffer functions.
    // Generate GL error if parameters are illegal.
    bool validateFramebufferFuncParameters(ASCIILiteral functionName, GCGLenum target, GCGLenum attachment);

    // Helper function to validate a GL capability.
    virtual bool validateCapability(ASCIILiteral functionName, GCGLenum);

    // Helper function to validate input parameters for uniform functions.
    bool validateUniformLocation(ASCIILiteral functionName, const WebGLUniformLocation*);
    template<typename T, typename TypedListType>
    std::optional<std::span<const T>> validateUniformParameters(ASCIILiteral functionName, const WebGLUniformLocation* location, const TypedList<TypedListType, T>& values, GCGLsizei requiredMinSize, GCGLuint srcOffset = 0, GCGLuint srcLength = 0)
    {
        return validateUniformMatrixParameters(functionName, location, false, values, requiredMinSize, srcOffset, srcLength);
    }
    template<typename T, typename TypedListType>
    std::optional<std::span<const T>> validateUniformMatrixParameters(ASCIILiteral functionName, const WebGLUniformLocation*, GCGLboolean transpose, const TypedList<TypedListType, T>&, GCGLsizei requiredMinSize, GCGLuint srcOffset = 0, GCGLuint srcLength = 0);

    // Helper function to validate parameters for bufferData.
    // Return the current bound buffer to target, or 0 if parameters are invalid.
    virtual WebGLBuffer* validateBufferDataParameters(ASCIILiteral functionName, GCGLenum target, GCGLenum usage);

    // Helper function for tex{Sub}Image2D to make sure image is ready.
    ExceptionOr<bool> validateHTMLImageElement(ASCIILiteral functionName, HTMLImageElement&);
    ExceptionOr<bool> validateHTMLCanvasElement(HTMLCanvasElement&);
#if ENABLE(VIDEO)
    ExceptionOr<bool> validateHTMLVideoElement(ASCIILiteral functionName, HTMLVideoElement&);
#endif
#if ENABLE(OFFSCREEN_CANVAS)
    ExceptionOr<bool> validateOffscreenCanvas(OffscreenCanvas&);
#endif
    ExceptionOr<bool> validateImageBitmap(ASCIILiteral functionName, ImageBitmap&);

    // Helper functions for vertexAttribNf{v}.
    void vertexAttribfImpl(ASCIILiteral functionName, GCGLuint index, GCGLsizei expectedSize, GCGLfloat, GCGLfloat, GCGLfloat, GCGLfloat);
    void vertexAttribfvImpl(ASCIILiteral functionName, GCGLuint index, Float32List&&, GCGLsizei expectedSize);

    // Helper function for delete* (deleteBuffer, deleteProgram, etc) functions.
    // Return false if caller should return without further processing.
    bool deleteObject(const AbstractLocker&, WebGLObject*);

    // Helper function to validate the target for bufferData and
    // getBufferParameter.
    virtual bool validateBufferTarget(ASCIILiteral functionName, GCGLenum target);

    // Helper function to validate the target for bufferData.
    // Return the current bound buffer to target, or 0 if the target is invalid.
    virtual WebGLBuffer* validateBufferDataTarget(ASCIILiteral functionName, GCGLenum target);

    virtual bool validateAndCacheBufferBinding(const AbstractLocker&, ASCIILiteral functionName, GCGLenum target, WebGLBuffer*);

    // Wrapper for GraphicsContextGLOpenGL::synthesizeGLError that sends a message to the JavaScript console.
    void synthesizeGLError(GCGLenum, ASCIILiteral functionName, ASCIILiteral description);
    void synthesizeLostContextGLError(GCGLenum, ASCIILiteral functionName, ASCIILiteral description);

    // Clamp the width and height to GL_MAX_VIEWPORT_DIMS.
    IntSize clampedCanvasSize();

    void setBackDrawBuffer(GCGLenum);
    void setFramebuffer(const AbstractLocker&, GCGLenum, WebGLFramebuffer*);

    // Check if EXT_draw_buffers extension is supported and if it satisfies the WebGL requirements.
    bool supportsDrawBuffers();

#if ENABLE(OFFSCREEN_CANVAS)
    OffscreenCanvas* offscreenCanvas();
#endif

    bool validateTypeAndArrayBufferType(ASCIILiteral functionName, ArrayBufferViewFunctionType, GCGLenum type, ArrayBufferView* pixels);

    // Fetches all errors from the underlying context and updates local list of errors
    // based on that.
    // Returns true if underlying context had errors.
    bool updateErrors();

private:
    void scheduleTaskToDispatchContextLostEvent();
    // Helper for restoration after context lost.
    void maybeRestoreContextSoon(Seconds timeout = 0_s);
    void maybeRestoreContext();

    ExceptionOr<void> texImageSource(TexImageFunctionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, ImageBitmap& source);
    ExceptionOr<void> texImageSource(TexImageFunctionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, ImageData& source);
    ExceptionOr<void> texImageSource(TexImageFunctionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, HTMLImageElement& source);
    ExceptionOr<void> texImageSource(TexImageFunctionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, HTMLCanvasElement& source);
#if ENABLE(VIDEO)
    ExceptionOr<void> texImageSource(TexImageFunctionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, HTMLVideoElement& source);
#endif
#if ENABLE(OFFSCREEN_CANVAS)
    ExceptionOr<void> texImageSource(TexImageFunctionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, OffscreenCanvas& source);
#endif
#if ENABLE(WEB_CODECS)
    ExceptionOr<void> texImageSource(TexImageFunctionID, GCGLenum target, GCGLint level, GCGLint internalformat, GCGLint border, GCGLenum format, GCGLenum type, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, const IntRect& inputSourceImageRect, GCGLsizei depth, GCGLint unpackImageHeight, WebCodecsVideoFrame& source);
#endif

    bool m_isSuspended { false };
    bool m_packReverseRowOrderSupported { false };
    // The ordinal number of when the context was last active (drew, read pixels).
    uint64_t m_activeOrdinal { 0 };
    WeakPtrFactory<WebGLRenderingContextBase> m_contextObjectWeakPtrFactory;
};

template<typename T>
bool WebGLRenderingContextBase::validateWebGLObject(ASCIILiteral functionName, const T& object)
{
    if (object.context() != this) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "object does not belong to this context"_s);
        return false;
    }
    if (!object.isUsable()) {
        constexpr GCGLenum error = (std::is_same_v<T, WebGLProgram> || std::is_same_v<T, WebGLShader>) ? GraphicsContextGL::INVALID_VALUE : GraphicsContextGL::INVALID_OPERATION;
        synthesizeGLError(error, functionName, "attempt to use a deleted object"_s);
        return false;
    }
    return true;
}

template<typename T>
bool WebGLRenderingContextBase::validateNullableWebGLObject(ASCIILiteral functionName, const T* object)
{
    return !object || validateWebGLObject(functionName, *object);
}

template<typename T>
GCGLboolean WebGLRenderingContextBase::validateIsWebGLObject(const T* object) const
{
    if (!object)
        return false;
    if (object->context() != this)
        return false;
    if (!object->isUsable())
        return false;
    if (!object->isInitialized())
        return false;
    return true;
}

WebCoreOpaqueRoot root(WebGLRenderingContextBase*);

WebCoreOpaqueRoot root(const WebGLExtension<WebGLRenderingContextBase>*);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::WebGLRenderingContextBase, isWebGL())

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif
