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

#include "config.h"
#include "WebGL2RenderingContext.h"

#if ENABLE(WEBGL2)

#include "CachedImage.h"
#include "EXTTextureFilterAnisotropic.h"
#include "EventLoop.h"
#include "ExtensionsGL.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageData.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "OESTextureFloat.h"
#include "OESTextureFloatLinear.h"
#include "OESTextureHalfFloat.h"
#include "OESTextureHalfFloatLinear.h"
#include "RenderBox.h"
#include "WebGLActiveInfo.h"
#include "WebGLCompressedTextureASTC.h"
#include "WebGLCompressedTextureATC.h"
#include "WebGLCompressedTextureETC.h"
#include "WebGLCompressedTextureETC1.h"
#include "WebGLCompressedTexturePVRTC.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLDepthTexture.h"
#include "WebGLLoseContext.h"
#include "WebGLQuery.h"
#include "WebGLSampler.h"
#include "WebGLSync.h"
#include "WebGLTransformFeedback.h"
#include "WebGLVertexArrayObject.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <JavaScriptCore/TypedArrayType.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebGL2RenderingContext);

std::unique_ptr<WebGL2RenderingContext> WebGL2RenderingContext::create(CanvasBase& canvas, GraphicsContextGLAttributes attributes)
{
    auto renderingContext = std::unique_ptr<WebGL2RenderingContext>(new WebGL2RenderingContext(canvas, attributes));

    InspectorInstrumentation::didCreateCanvasRenderingContext(*renderingContext);

    return renderingContext;
}

std::unique_ptr<WebGL2RenderingContext> WebGL2RenderingContext::create(CanvasBase& canvas, Ref<GraphicsContextGLOpenGL>&& context, GraphicsContextGLAttributes attributes)
{
    auto renderingContext = std::unique_ptr<WebGL2RenderingContext>(new WebGL2RenderingContext(canvas, WTFMove(context), attributes));

    InspectorInstrumentation::didCreateCanvasRenderingContext(*renderingContext);

    return renderingContext;
}

WebGL2RenderingContext::WebGL2RenderingContext(CanvasBase& canvas, GraphicsContextGLAttributes attributes)
    : WebGLRenderingContextBase(canvas, attributes)
{
}

WebGL2RenderingContext::WebGL2RenderingContext(CanvasBase& canvas, Ref<GraphicsContextGLOpenGL>&& context, GraphicsContextGLAttributes attributes)
    : WebGLRenderingContextBase(canvas, WTFMove(context), attributes)
{
    initializeShaderExtensions();
    initializeVertexArrayObjects();
    initializeTransformFeedbackBufferCache();
    initializeSamplerCache();
}

WebGL2RenderingContext::~WebGL2RenderingContext()
{
    // Remove all references to WebGLObjects so if they are the last reference
    // they will be freed before the last context is removed from the context group.
    m_boundTransformFeedback = nullptr;
    m_boundTransformFeedbackBuffers.clear();
    m_activeQueries.clear();
}

void WebGL2RenderingContext::initializeVertexArrayObjects()
{
    m_defaultVertexArrayObject = WebGLVertexArrayObject::create(*this, WebGLVertexArrayObject::Type::Default);
    addContextObject(*m_defaultVertexArrayObject);
#if USE(OPENGL_ES)
    m_boundVertexArrayObject = m_defaultVertexArrayObject;
#else
    bindVertexArray(nullptr); // The default VAO was removed in OpenGL 3.3 but not from WebGL 2; bind the default for WebGL to use.
#endif
    if (!isGLES2Compliant())
        initVertexAttrib0();
}

void WebGL2RenderingContext::initializeShaderExtensions()
{
    m_context->getExtensions().ensureEnabled("GL_OES_standard_derivatives");
    m_context->getExtensions().ensureEnabled("GL_EXT_draw_buffers");
    m_context->getExtensions().ensureEnabled("GL_EXT_shader_texture_lod");
    m_context->getExtensions().ensureEnabled("GL_EXT_frag_depth");
}

void WebGL2RenderingContext::initializeTransformFeedbackBufferCache()
{
    int maxTransformFeedbackAttribs = getIntParameter(GraphicsContextGL::MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS);
    ASSERT(maxTransformFeedbackAttribs >= 4);

    m_boundTransformFeedbackBuffers.resize(maxTransformFeedbackAttribs);
}

void WebGL2RenderingContext::initializeSamplerCache()
{
    ASSERT(m_textureUnits.size() >= 8);
    m_boundSamplers.resize(m_textureUnits.size());
}

RefPtr<ArrayBufferView> WebGL2RenderingContext::arrayBufferViewSliceFactory(const char* const functionName, const ArrayBufferView& data, unsigned startByte,  unsigned numElements)
{
    RefPtr<ArrayBufferView> slice;

    switch (data.getType()) {
#define FACTORY_CASE(type) \
    case JSC::Type##type: \
        slice = JSC::type##Array::tryCreate(data.possiblySharedBuffer(), startByte, numElements); \
        break;

    FOR_EACH_TYPED_ARRAY_TYPE_EXCLUDING_DATA_VIEW(FACTORY_CASE);
#undef FACTORY_CASE
    case JSC::TypeDataView:
        slice = Uint8Array::tryCreate(data.possiblySharedBuffer(), startByte, numElements);
        break;
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    if (!slice)
        synthesizeGLError(GraphicsContextGL::OUT_OF_MEMORY, functionName, "Could not create intermediate ArrayBufferView");

    return slice;
}

RefPtr<ArrayBufferView> WebGL2RenderingContext::sliceArrayBufferView(const char* const functionName, const ArrayBufferView& data, GCGLuint srcOffset, GCGLuint length)
{
    if (data.getType() == JSC::NotTypedArray) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "Invalid type of Array Buffer View");
        return nullptr;
    }

    auto elementSize = JSC::elementSize(data.getType());
    Checked<GCGLuint, RecordOverflow> checkedElementSize(elementSize);

    Checked<GCGLuint, RecordOverflow> checkedSrcOffset(srcOffset);
    Checked<GCGLuint, RecordOverflow> checkedByteSrcOffset = checkedSrcOffset * checkedElementSize;
    Checked<GCGLuint, RecordOverflow> checkedLength(length);
    Checked<GCGLuint, RecordOverflow> checkedByteLength = checkedLength * checkedElementSize;

    if (checkedByteSrcOffset.hasOverflowed()
        || checkedByteLength.hasOverflowed()
        || checkedByteSrcOffset.unsafeGet() > data.byteLength()
        || checkedByteLength.unsafeGet() > data.byteLength() - checkedByteSrcOffset.unsafeGet()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "srcOffset or length is out of bounds");
        return nullptr;
    }

    return arrayBufferViewSliceFactory(functionName, data, data.byteOffset() + checkedByteSrcOffset.unsafeGet(), length);
}

void WebGL2RenderingContext::bufferData(GCGLenum target, const ArrayBufferView& data, GCGLenum usage, GCGLuint srcOffset, GCGLuint length)
{
    if (auto slice = sliceArrayBufferView("bufferData", data, srcOffset, length))
        WebGLRenderingContextBase::bufferData(target, BufferDataSource(slice.get()), usage);
}

void WebGL2RenderingContext::bufferSubData(GCGLenum target, long long offset, const ArrayBufferView& data, GCGLuint srcOffset, GCGLuint length)
{
    if (auto slice = sliceArrayBufferView("bufferSubData", data, srcOffset, length))
        WebGLRenderingContextBase::bufferSubData(target, offset, BufferDataSource(slice.get()));
}

void WebGL2RenderingContext::copyBufferSubData(GCGLenum readTarget, GCGLenum writeTarget, GCGLint64 readOffset, GCGLint64 writeOffset, GCGLint64 size)
{
    if (isContextLostOrPending())
        return;
    if ((readTarget == GraphicsContextGL::ELEMENT_ARRAY_BUFFER && writeTarget != GraphicsContextGL::ELEMENT_ARRAY_BUFFER)
        || (writeTarget == GraphicsContextGL::ELEMENT_ARRAY_BUFFER && readTarget != GraphicsContextGL::ELEMENT_ARRAY_BUFFER)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "copyBufferSubData", "Either both targets need to be ELEMENT_ARRAY_BUFFER or neither should be ELEMENT_ARRAY_BUFFER.");
        return;
    }
    if (readOffset < 0 || writeOffset < 0 || size < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "copyBufferSubData", "offset < 0");
        return;
    }
    RefPtr<WebGLBuffer> readBuffer = validateBufferDataParameters("copyBufferSubData", readTarget, GraphicsContextGL::STATIC_DRAW);
    RefPtr<WebGLBuffer> writeBuffer = validateBufferDataParameters("copyBufferSubData", writeTarget, GraphicsContextGL::STATIC_DRAW);
    if (!readBuffer || !writeBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "copyBufferSubData", "Invalid readTarget or writeTarget");
        return;
    }

    Checked<GCGLintptr, RecordOverflow> checkedReadOffset(readOffset);
    Checked<GCGLintptr, RecordOverflow> checkedWriteOffset(writeOffset);
    Checked<GCGLsizeiptr, RecordOverflow> checkedSize(size);
    if (checkedReadOffset.hasOverflowed() || checkedWriteOffset.hasOverflowed() || checkedSize.hasOverflowed()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "copyBufferSubData", "Offset or size is too big");
        return;
    }

    if (!writeBuffer->associateCopyBufferSubData(*readBuffer, checkedReadOffset.unsafeGet(), checkedWriteOffset.unsafeGet(), checkedSize.unsafeGet())) {
        this->synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "copyBufferSubData", "offset out of range");
        return;
    }

    m_context->moveErrorsToSyntheticErrorList();
#if PLATFORM(COCOA)
    m_context->copyBufferSubData(readTarget, writeTarget, checkedReadOffset.unsafeGet(), checkedWriteOffset.unsafeGet(), checkedSize.unsafeGet());
#endif
    if (m_context->moveErrorsToSyntheticErrorList()) {
        // The bufferSubData function failed. Tell the buffer it doesn't have the data it thinks it does.
        writeBuffer->disassociateBufferData();
    }
}

void WebGL2RenderingContext::getBufferSubData(GCGLenum target, long long srcByteOffset, RefPtr<ArrayBufferView>&& dstData, GCGLuint dstOffset, GCGLuint length)
{
    if (isContextLostOrPending())
        return;
    RefPtr<WebGLBuffer> buffer = validateBufferDataParameters("bufferSubData", target, GraphicsContextGL::STATIC_DRAW);
    if (!buffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getBufferSubData", "No WebGLBuffer is bound to target");
        return;
    }

    // FIXME: Implement "If target is TRANSFORM_FEEDBACK_BUFFER, and any transform feedback object is currently active, an INVALID_OPERATION error is generated."

    if (!dstData) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getBufferSubData", "Null dstData");
        return;
    }

    if (dstData->getType() == JSC::NotTypedArray) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getBufferSubData", "Invalid type of Array Buffer View");
        return;
    }

    auto elementSize = JSC::elementSize(dstData->getType());
    auto dstDataLength = dstData->byteLength() / elementSize;

    if (dstOffset > dstDataLength) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getBufferSubData", "dstOffset is larger than the length of the destination buffer.");
        return;
    }

    GCGLuint copyLength = length ? length : dstDataLength - dstOffset;

    Checked<GCGLuint, RecordOverflow> checkedDstOffset(dstOffset);
    Checked<GCGLuint, RecordOverflow> checkedCopyLength(copyLength);
    auto checkedDestinationEnd = checkedDstOffset + checkedCopyLength;
    if (checkedDestinationEnd.hasOverflowed()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getBufferSubData", "dstOffset + copyLength is too high");
        return;
    }

    if (checkedDestinationEnd.unsafeGet() > dstDataLength) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getBufferSubData", "end of written destination is past the end of the buffer");
        return;
    }

    if (srcByteOffset < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getBufferSubData", "srcByteOffset is less than 0");
        return;
    }

    Checked<GCGLintptr, RecordOverflow> checkedSrcByteOffset(srcByteOffset);
    Checked<GCGLintptr, RecordOverflow> checkedCopyLengthPtr(copyLength);
    Checked<GCGLintptr, RecordOverflow> checkedElementSize(elementSize);
    auto checkedSourceEnd = checkedSrcByteOffset + checkedCopyLengthPtr * checkedElementSize;
    if (checkedSourceEnd.hasOverflowed() || checkedSourceEnd.unsafeGet() > buffer->byteLength()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getBufferSubData", "Parameters would read outside the bounds of the source buffer");
        return;
    }

    m_context->moveErrorsToSyntheticErrorList();
#if PLATFORM(COCOA)
    // FIXME: Coalesce multiple getBufferSubData() calls to use a single map() call
    void* ptr = m_context->mapBufferRange(target, checkedSrcByteOffset.unsafeGet(), static_cast<GCGLsizeiptr>(checkedCopyLengthPtr.unsafeGet() * checkedElementSize.unsafeGet()), GraphicsContextGL::MAP_READ_BIT);
    if (ptr)
        memcpy(static_cast<char*>(dstData->baseAddress()) + dstData->byteOffset() + dstOffset * elementSize, ptr, copyLength * elementSize);

    if (!m_context->unmapBuffer(target))
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getBufferSubData", "Failed while unmapping buffer");
#endif
    m_context->moveErrorsToSyntheticErrorList();
}

void WebGL2RenderingContext::blitFramebuffer(GCGLint, GCGLint, GCGLint, GCGLint, GCGLint, GCGLint, GCGLint, GCGLint, GCGLbitfield, GCGLenum)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] blitFramebuffer()");
}

void WebGL2RenderingContext::framebufferTextureLayer(GCGLenum, GCGLenum, WebGLTexture*, GCGLint, GCGLint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] framebufferTextureLayer()");
}

#if !USE(OPENGL_ES)
static bool isRenderableInternalformat(GCGLenum internalformat)
{
    // OpenGL ES 3: internalformat must be a color-renderable, depth-renderable, or stencil-renderable format, as shown in Table 1 below.
    switch (internalformat) {
    case GraphicsContextGL::R8:
    case GraphicsContextGL::R8UI:
    case GraphicsContextGL::R16UI:
    case GraphicsContextGL::R16I:
    case GraphicsContextGL::R32UI:
    case GraphicsContextGL::R32I:
    case GraphicsContextGL::RG8:
    case GraphicsContextGL::RG8UI:
    case GraphicsContextGL::RG8I:
    case GraphicsContextGL::RG16UI:
    case GraphicsContextGL::RG16I:
    case GraphicsContextGL::RG32UI:
    case GraphicsContextGL::RG32I:
    case GraphicsContextGL::RGB8:
    case GraphicsContextGL::RGB565:
    case GraphicsContextGL::RGBA8:
    case GraphicsContextGL::SRGB8_ALPHA8:
    case GraphicsContextGL::RGB5_A1:
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RGB10_A2:
    case GraphicsContextGL::RGBA8UI:
    case GraphicsContextGL::RGBA8I:
    case GraphicsContextGL::RGB10_A2UI:
    case GraphicsContextGL::RGBA16UI:
    case GraphicsContextGL::RGBA16I:
    case GraphicsContextGL::RGBA32I:
    case GraphicsContextGL::RGBA32UI:
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::DEPTH_COMPONENT24:
    case GraphicsContextGL::DEPTH_COMPONENT32F:
    case GraphicsContextGL::DEPTH24_STENCIL8:
    case GraphicsContextGL::DEPTH32F_STENCIL8:
    case GraphicsContextGL::STENCIL_INDEX8:
        return true;
    }
    return false;
}
#endif

WebGLAny WebGL2RenderingContext::getInternalformatParameter(GCGLenum target, GCGLenum internalformat, GCGLenum pname)
{
    if (isContextLostOrPending())
        return nullptr;

    if (pname != GraphicsContextGL::SAMPLES) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getInternalformatParameter", "invalid parameter name");
        return nullptr;
    }

    int numValues = 0;
#if USE(OPENGL_ES)
    m_context->getInternalformativ(target, internalformat, GraphicsContextGL::NUM_SAMPLE_COUNTS, 1, &numValues);

    GCGLint params[numValues];
    m_context->getInternalformativ(target, internalformat, pname, numValues, params);
#else
    // On desktop OpenGL 4.1 or below we must emulate glGetInternalformativ.

    // GL_INVALID_ENUM is generated if target is not GL_RENDERBUFFER.
    if (target != GraphicsContextGL::RENDERBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getInternalformatParameter", "invalid target");
        return nullptr;
    }

    // GL_INVALID_ENUM is generated if internalformat is not color-, depth-, or stencil-renderable.
    if (!isRenderableInternalformat(internalformat)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getInternalformatParameter", "invalid internal format");
        return nullptr;
    }

    Vector<GCGLint> samples;
    // The way I understand this is that this will return a MINIMUM numSamples for all accepeted internalformats.
    // However, the true value of this on supported GL versions is gleaned via a getInternalformativ call that depends on internalformat.
    int numSamplesMask = getIntParameter(GraphicsContextGL::MAX_SAMPLES);

    while (numSamplesMask > 0) {
        samples.append(numSamplesMask);
        numSamplesMask = numSamplesMask >> 1;
    }

    // Since multisampling is not supported for signed and unsigned integer internal formats,
    // the value of GL_NUM_SAMPLE_COUNTS will be zero for such formats.
    numValues = isIntegerFormat(internalformat) ? 0 : samples.size();
    GCGLint params[numValues];
    for (size_t i = 0; i < samples.size(); ++i)
        params[i] = samples[i];
#endif

    return Int32Array::tryCreate(params, numValues);
}

void WebGL2RenderingContext::invalidateFramebuffer(GCGLenum, const Vector<GCGLenum>&)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] invalidateFramebuffer()");
}

void WebGL2RenderingContext::invalidateSubFramebuffer(GCGLenum, const Vector<GCGLenum>&, GCGLint, GCGLint, GCGLsizei, GCGLsizei)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] invalidateSubFramebuffer()");
}

void WebGL2RenderingContext::readBuffer(GCGLenum)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] readBuffer()");
}

void WebGL2RenderingContext::renderbufferStorageMultisample(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    // To be backward compatible with WebGL 1, also accepts internal format DEPTH_STENCIL,
    // which should be mapped to DEPTH24_STENCIL8 by implementations.
    if (internalformat == GraphicsContextGL::DEPTH_STENCIL)
        internalformat = GraphicsContextGL::DEPTH24_STENCIL8;

    // ES 3: GL_INVALID_OPERATION is generated if internalformat is a signed or unsigned integer format and samples is greater than 0.
    if (isIntegerFormat(internalformat) && samples > 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "renderbufferStorageMultisample", "multisampling not supported for this format");
        return;
    }

    switch (internalformat) {
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::DEPTH_COMPONENT32F:
    case GraphicsContextGL::DEPTH_COMPONENT24:
    case GraphicsContextGL::RGBA32I:
    case GraphicsContextGL::RGBA32UI:
    case GraphicsContextGL::RGBA16I:
    case GraphicsContextGL::RGBA16UI:
    case GraphicsContextGL::RGBA8:
    case GraphicsContextGL::RGBA8I:
    case GraphicsContextGL::RGBA8UI:
    case GraphicsContextGL::RGB10_A2:
    case GraphicsContextGL::RGB10_A2UI:
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RG32I:
    case GraphicsContextGL::RG32UI:
    case GraphicsContextGL::RG16I:
    case GraphicsContextGL::RG16UI:
    case GraphicsContextGL::RG8:
    case GraphicsContextGL::RG8I:
    case GraphicsContextGL::RG8UI:
    case GraphicsContextGL::R32I:
    case GraphicsContextGL::R32UI:
    case GraphicsContextGL::R16I:
    case GraphicsContextGL::R16UI:
    case GraphicsContextGL::R8:
    case GraphicsContextGL::R8I:
    case GraphicsContextGL::R8UI:
    case GraphicsContextGL::RGB5_A1:
    case GraphicsContextGL::RGB565:
    case GraphicsContextGL::RGB8:
    case GraphicsContextGL::STENCIL_INDEX8:
    case GraphicsContextGL::SRGB8_ALPHA8:
        m_context->renderbufferStorageMultisample(target, samples, internalformat, width, height);
        m_renderbufferBinding->setInternalFormat(internalformat);
        m_renderbufferBinding->setIsValid(true);
        m_renderbufferBinding->setSize(width, height);
        break;
    case GraphicsContextGL::DEPTH32F_STENCIL8:
    case GraphicsContextGL::DEPTH24_STENCIL8:
        if (!isDepthStencilSupported()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "renderbufferStorage", "invalid internalformat");
            return;
        }
        m_context->renderbufferStorageMultisample(target, samples, internalformat, width, height);
        m_renderbufferBinding->setSize(width, height);
        m_renderbufferBinding->setIsValid(isDepthStencilSupported());
        m_renderbufferBinding->setInternalFormat(internalformat);
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "renderbufferStorage", "invalid internalformat");
        return;
    }
    applyStencilTest();
}

bool WebGL2RenderingContext::validateTexStorageFuncParameters(GCGLenum target, GCGLsizei levels, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, const char* functionName)
{
    if (width < 0 || height < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "width or height < 0");
        return false;
    }

    if (width > m_maxTextureSize || height > m_maxTextureSize) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "texture dimensions are larger than the maximum texture size");
        return false;
    }

    if (target == GraphicsContextGL::TEXTURE_CUBE_MAP) {
        if (width != height) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "width != height for cube map");
            return false;
        }
    } else if (target != GraphicsContextGL::TEXTURE_2D) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
        return false;
    }

    if (levels < 0 || levels > m_maxTextureLevel) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "number of levels is out of bounds");
        return false;
    }

    switch (internalFormat) {
    case GraphicsContextGL::R8:
    case GraphicsContextGL::R8_SNORM:
    case GraphicsContextGL::R16F:
    case GraphicsContextGL::R32F:
    case GraphicsContextGL::R8UI:
    case GraphicsContextGL::R8I:
    case GraphicsContextGL::R16UI:
    case GraphicsContextGL::R16I:
    case GraphicsContextGL::R32UI:
    case GraphicsContextGL::R32I:
    case GraphicsContextGL::RG8:
    case GraphicsContextGL::RG8_SNORM:
    case GraphicsContextGL::RG16F:
    case GraphicsContextGL::RG32F:
    case GraphicsContextGL::RG8UI:
    case GraphicsContextGL::RG8I:
    case GraphicsContextGL::RG16UI:
    case GraphicsContextGL::RG16I:
    case GraphicsContextGL::RG32UI:
    case GraphicsContextGL::RG32I:
    case GraphicsContextGL::RGB8:
    case GraphicsContextGL::SRGB8:
    case GraphicsContextGL::RGB565:
    case GraphicsContextGL::RGB8_SNORM:
    case GraphicsContextGL::R11F_G11F_B10F:
    case GraphicsContextGL::RGB9_E5:
    case GraphicsContextGL::RGB16F:
    case GraphicsContextGL::RGB32F:
    case GraphicsContextGL::RGB8UI:
    case GraphicsContextGL::RGB8I:
    case GraphicsContextGL::RGB16UI:
    case GraphicsContextGL::RGB16I:
    case GraphicsContextGL::RGB32UI:
    case GraphicsContextGL::RGB32I:
    case GraphicsContextGL::RGBA8:
    case GraphicsContextGL::SRGB8_ALPHA8:
    case GraphicsContextGL::RGBA8_SNORM:
    case GraphicsContextGL::RGB5_A1:
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RGB10_A2:
    case GraphicsContextGL::RGBA16F:
    case GraphicsContextGL::RGBA32F:
    case GraphicsContextGL::RGBA8UI:
    case GraphicsContextGL::RGBA8I:
    case GraphicsContextGL::RGB10_A2UI:
    case GraphicsContextGL::RGBA16UI:
    case GraphicsContextGL::RGBA16I:
    case GraphicsContextGL::RGBA32I:
    case GraphicsContextGL::RGBA32UI:
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::DEPTH_COMPONENT24:
    case GraphicsContextGL::DEPTH_COMPONENT32F:
    case GraphicsContextGL::DEPTH24_STENCIL8:
    case GraphicsContextGL::DEPTH32F_STENCIL8:
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "Unknown internalFormat");
        return false;
    }

    return true;
}

void WebGL2RenderingContext::texStorage2D(GCGLenum target, GCGLsizei levels, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height)
{
    if (isContextLostOrPending())
        return;

    auto texture = validateTextureBinding("texStorage2D", target, false);
    if (!texture)
        return;

    if (!validateTexStorageFuncParameters(target, levels, internalFormat, width, height, "texStorage2D"))
        return;

    if (!validateNPOTTextureLevel(width, height, levels, "texStorage2D"))
        return;

    if (texture->immutable()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texStorage2D", "texStorage2D already called on this texture");
        return;
    }
    texture->setImmutable();

    m_context->texStorage2D(target, levels, internalFormat, width, height);

    {
        GCGLenum format;
        GCGLenum type;
        if (!GraphicsContextGLOpenGL::possibleFormatAndTypeForInternalFormat(internalFormat, format, type)) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "texStorage2D", "Texture has unknown internal format");
            return;
        }

        GCGLsizei levelWidth = width;
        GCGLsizei levelHeight = height;

        unsigned size;
        GCGLenum error = m_context->computeImageSizeInBytes(format, type, width, height, m_unpackAlignment, &size, nullptr);
        if (error != GraphicsContextGL::NO_ERROR) {
            synthesizeGLError(error, "texStorage2D", "bad dimensions");
            return;
        }

        Vector<char> data(size);
        memset(data.data(), 0, size);

        for (GCGLsizei level = 0; level < levels; ++level) {
            if (target == GraphicsContextGL::TEXTURE_CUBE_MAP) {
                m_context->texSubImage2D(GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_X, level, 0, 0, levelWidth, levelHeight, format, type, data.data());
                m_context->texSubImage2D(GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_X, level, 0, 0, levelWidth, levelHeight, format, type, data.data());
                m_context->texSubImage2D(GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Y, level, 0, 0, levelWidth, levelHeight, format, type, data.data());
                m_context->texSubImage2D(GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Y, level, 0, 0, levelWidth, levelHeight, format, type, data.data());
                m_context->texSubImage2D(GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Z, level, 0, 0, levelWidth, levelHeight, format, type, data.data());
                m_context->texSubImage2D(GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Z, level, 0, 0, levelWidth, levelHeight, format, type, data.data());
            } else
                m_context->texSubImage2D(target, level, 0, 0, levelWidth, levelHeight, format, type, data.data());
            levelWidth = std::max(1, levelWidth / 2);
            levelHeight = std::max(1, levelHeight / 2);
        }
    }

    for (GCGLsizei level = 0; level < levels; ++level) {
        if (target == GraphicsContextGL::TEXTURE_CUBE_MAP) {
            texture->setLevelInfo(GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_X, level, internalFormat, width, height, GraphicsContextGL::UNSIGNED_BYTE);
            texture->setLevelInfo(GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_X, level, internalFormat, width, height, GraphicsContextGL::UNSIGNED_BYTE);
            texture->setLevelInfo(GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Y, level, internalFormat, width, height, GraphicsContextGL::UNSIGNED_BYTE);
            texture->setLevelInfo(GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Y, level, internalFormat, width, height, GraphicsContextGL::UNSIGNED_BYTE);
            texture->setLevelInfo(GraphicsContextGL::TEXTURE_CUBE_MAP_POSITIVE_Z, level, internalFormat, width, height, GraphicsContextGL::UNSIGNED_BYTE);
            texture->setLevelInfo(GraphicsContextGL::TEXTURE_CUBE_MAP_NEGATIVE_Z, level, internalFormat, width, height, GraphicsContextGL::UNSIGNED_BYTE);
        } else
            texture->setLevelInfo(target, level, internalFormat, width, height, GraphicsContextGL::UNSIGNED_BYTE);
    }
}

void WebGL2RenderingContext::texStorage3D(GCGLenum, GCGLsizei, GCGLenum, GCGLsizei, GCGLsizei, GCGLsizei)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] texStorage3D()");
}

void WebGL2RenderingContext::texImage2D(GCGLenum, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLint, GCGLenum, GCGLenum, GCGLintptr)
{
    // Covered by textures/misc/tex-unpack-params.html.
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] texImage2D(PIXEL_UNPACK_BUFFER)");
}

ExceptionOr<void> WebGL2RenderingContext::texImage2D(GCGLenum, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLint, GCGLenum, GCGLenum, TexImageSource&&)
{
    // Covered by textures/misc/origin-clean-conformance-offscreencanvas.html?
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] texImage2D(TexImageSource)");
    return { };
}

RefPtr<ArrayBufferView> WebGL2RenderingContext::sliceTypedArrayBufferView(const char* const functionName, RefPtr<ArrayBufferView>& srcData, GCGLuint srcOffset)
{
    if (!srcData)
        return nullptr;

    if (!isTypedView(srcData->getType())) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "Invalid type of ArrayBufferView");
        return nullptr;
    }

    auto elementSize = JSC::elementSize(srcData->getType());
    auto startingByte = WTF::checkedProduct<unsigned>(elementSize, srcOffset);
    if (startingByte.hasOverflowed() || startingByte >= srcData->byteLength()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "Invalid element offset!");
        return nullptr;
    }

    auto numElements = (srcData->byteLength() - startingByte.unsafeGet()) / elementSize;

    return arrayBufferViewSliceFactory(functionName, *srcData, startingByte.unsafeGet(), numElements);
}

void WebGL2RenderingContext::texImage2D(GCGLenum target, GCGLint level, GCGLint internalFormat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData, GCGLuint srcOffset)
{
    if (isContextLostOrPending())
        return;

    auto slicedData = sliceTypedArrayBufferView("texImage2D", srcData, srcOffset);

    WebGLRenderingContextBase::texImage2D(target, level, internalFormat, width, height, border, format, type, WTFMove(slicedData));
}

void WebGL2RenderingContext::texImage3D(GCGLenum, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLsizei, GCGLint, GCGLenum, GCGLenum, GCGLint64)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] texImage3D(PIXEL_UNPACK_BUFFER)");
}

ExceptionOr<void> WebGL2RenderingContext::texImage3D(GCGLenum, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLsizei, GCGLint, GCGLenum, GCGLenum, TexImageSource&&)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] texImage3D(TexImageSource)");
    return { };
}

void WebGL2RenderingContext::texImage3D(GCGLenum, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLsizei, GCGLint, GCGLenum, GCGLenum, RefPtr<ArrayBufferView>&&)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] texImage3D(ArrayBufferView?)");
}

void WebGL2RenderingContext::texImage3D(GCGLenum, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLsizei, GCGLint, GCGLenum, GCGLenum, RefPtr<ArrayBufferView>&&, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] texImage3D(ArrayBufferView, srcOffset)");
}

void WebGL2RenderingContext::texSubImage2D(GCGLenum, GCGLint, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, GCGLintptr)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] texSubImage2D(PIXEL_UNPACK_BUFFER)");
}

ExceptionOr<void> WebGL2RenderingContext::texSubImage2D(GCGLenum, GCGLint, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, TexImageSource&&)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] texSubImage2D(TexImageSource)");
    return { };
}

void WebGL2RenderingContext::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData, GCGLuint srcOffset)
{
    if (isContextLostOrPending())
        return;

    auto slicedData = sliceTypedArrayBufferView("texSubImage2D", srcData, srcOffset);

    WebGLRenderingContextBase::texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, WTFMove(slicedData));
}

void WebGL2RenderingContext::texSubImage3D(GCGLenum, GCGLint, GCGLint, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, GCGLint64)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] texSubImage3D(PIXEL_UNPACK_BUFFER)");
}

void WebGL2RenderingContext::texSubImage3D(GCGLenum, GCGLint, GCGLint, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, RefPtr<ArrayBufferView>&&, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] texSubImage3D(ArrayBufferView, srcOffset)");
}

ExceptionOr<void> WebGL2RenderingContext::texSubImage3D(GCGLenum, GCGLint, GCGLint, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLsizei, GCGLenum, GCGLenum, TexImageSource&&)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] texSubImage3D(TexImageSource)");
    return { };
}

void WebGL2RenderingContext::copyTexSubImage3D(GCGLenum, GCGLint, GCGLint, GCGLint, GCGLint, GCGLint, GCGLint, GCGLsizei, GCGLsizei)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] copyTexSubImage3D()");
}

void WebGL2RenderingContext::compressedTexImage3D(GCGLenum, GCGLint, GCGLenum, GCGLsizei, GCGLsizei, GCGLsizei, GCGLint, GCGLsizei, GCGLint64)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] compressedTexImage3D(PIXEL_UNPACK_BUFFER)");
}

void WebGL2RenderingContext::compressedTexImage3D(GCGLenum, GCGLint, GCGLenum, GCGLsizei, GCGLsizei, GCGLsizei, GCGLint, ArrayBufferView&, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] compressedTexImage3D(ArrayBufferView)");
}

void WebGL2RenderingContext::compressedTexSubImage3D(GCGLenum, GCGLint, GCGLint, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLsizei, GCGLenum, GCGLsizei, GCGLint64)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] compressedTexSubImage3D(PIXEL_UNPACK_BUFFER)");
}

void WebGL2RenderingContext::compressedTexSubImage3D(GCGLenum, GCGLint, GCGLint, GCGLint, GCGLint, GCGLsizei, GCGLsizei, GCGLsizei, GCGLenum, ArrayBufferView&, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] compressedTexSubImage3D(ArrayBufferView)");
}

GCGLint WebGL2RenderingContext::getFragDataLocation(WebGLProgram&, const String&)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] getFragDataLocation()");
    return 0;
}

void WebGL2RenderingContext::uniform1ui(WebGLUniformLocation*, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform1ui()");
}

void WebGL2RenderingContext::uniform2ui(WebGLUniformLocation*, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform2ui()");
}

void WebGL2RenderingContext::uniform3ui(WebGLUniformLocation*, GCGLuint, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform3ui()");
}

void WebGL2RenderingContext::uniform4ui(WebGLUniformLocation*, GCGLuint, GCGLuint, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform4ui()");
}

void WebGL2RenderingContext::uniform1uiv(WebGLUniformLocation*, Uint32List&&, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform1uiv()");
}

void WebGL2RenderingContext::uniform2uiv(WebGLUniformLocation*, Uint32List&&, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform2uiv()");
}

void WebGL2RenderingContext::uniform3uiv(WebGLUniformLocation*, Uint32List&&, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform3uiv()");
}

void WebGL2RenderingContext::uniform4uiv(WebGLUniformLocation*, Uint32List&&, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform4uiv()");
}

void WebGL2RenderingContext::uniformMatrix2x3fv(WebGLUniformLocation*, GCGLboolean, Float32List&&, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniformMatrix2x3fv()");
}

void WebGL2RenderingContext::uniformMatrix3x2fv(WebGLUniformLocation*, GCGLboolean, Float32List&&, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniformMatrix3x2fv()");
}

void WebGL2RenderingContext::uniformMatrix2x4fv(WebGLUniformLocation*, GCGLboolean, Float32List&&, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniformMatrix2x4fv()");
}

void WebGL2RenderingContext::uniformMatrix4x2fv(WebGLUniformLocation*, GCGLboolean, Float32List&&, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniformMatrix4x2fv()");
}

void WebGL2RenderingContext::uniformMatrix3x4fv(WebGLUniformLocation*, GCGLboolean, Float32List&&, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniformMatrix3x4fv()");
}

void WebGL2RenderingContext::uniformMatrix4x3fv(WebGLUniformLocation*, GCGLboolean, Float32List&&, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniformMatrix4x3fv()");
}

void WebGL2RenderingContext::vertexAttribI4i(GCGLuint, GCGLint, GCGLint, GCGLint, GCGLint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] vertexAttribI4i()");
}

void WebGL2RenderingContext::vertexAttribI4iv(GCGLuint, Int32List&&)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] vertexAttribI4iv()");
}

void WebGL2RenderingContext::vertexAttribI4ui(GCGLuint, GCGLuint, GCGLuint, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] vertexAttribI4ui()");
}

void WebGL2RenderingContext::vertexAttribI4uiv(GCGLuint, Uint32List&&)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] vertexAttribI4uiv()");
}

void WebGL2RenderingContext::vertexAttribIPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLsizei stride, GCGLint64 offset)
{
    if (isContextLostOrPending())
        return;

    m_context->vertexAttribIPointer(index, size, type, stride, offset);
}

void WebGL2RenderingContext::clear(GCGLbitfield mask)
{
    if (isContextLostOrPending())
        return;
    if (mask & ~(GraphicsContextGL::COLOR_BUFFER_BIT | GraphicsContextGL::DEPTH_BUFFER_BIT | GraphicsContextGL::STENCIL_BUFFER_BIT)) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "clear", "invalid mask");
        return;
    }
    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(graphicsContextGL(), &reason)) {
        synthesizeGLError(GraphicsContextGL::INVALID_FRAMEBUFFER_OPERATION, "clear", reason);
        return;
    }
    if (m_framebufferBinding && (mask & GraphicsContextGL::COLOR_BUFFER_BIT) && isIntegerFormat(m_framebufferBinding->getColorBufferFormat())) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "clear", "cannot clear an integer buffer");
        return;
    }
    if (!clearIfComposited(mask))
        m_context->clear(mask);
    markContextChangedAndNotifyCanvasObserver();
}

void WebGL2RenderingContext::vertexAttribDivisor(GCGLuint index, GCGLuint divisor)
{
    if (isContextLostOrPending())
        return;
    
    WebGLRenderingContextBase::vertexAttribDivisor(index, divisor);
}

void WebGL2RenderingContext::drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei instanceCount)
{
    if (isContextLostOrPending())
        return;

    WebGLRenderingContextBase::drawArraysInstanced(mode, first, count, instanceCount);
}

void WebGL2RenderingContext::drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLint64 offset, GCGLsizei instanceCount)
{
    if (isContextLostOrPending())
        return;

    WebGLRenderingContextBase::drawElementsInstanced(mode, count, type, offset, instanceCount);
}

void WebGL2RenderingContext::drawRangeElements(GCGLenum, GCGLuint, GCGLuint, GCGLsizei, GCGLenum, GCGLint64)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] drawRangeElements()");
}

void WebGL2RenderingContext::drawBuffers(const Vector<GCGLenum>& buffers)
{
    if (isContextLost())
        return;
    GCGLsizei n = buffers.size();
    const GCGLenum* bufs = buffers.data();
    if (!m_framebufferBinding) {
        if (n != 1) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "drawBuffers", "more than one buffer");
            return;
        }
        if (bufs[0] != GraphicsContextGL::BACK && bufs[0] != GraphicsContextGL::NONE) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "drawBuffers", "BACK or NONE");
            return;
        }
        // Because the backbuffer is simulated on all current WebKit ports, we need to change BACK to COLOR_ATTACHMENT0.
        GCGLenum value = (bufs[0] == GraphicsContextGL::BACK) ? GraphicsContextGL::COLOR_ATTACHMENT0 : GraphicsContextGL::NONE;
        graphicsContextGL()->getExtensions().drawBuffersEXT(1, &value);
        setBackDrawBuffer(bufs[0]);
    } else {
        if (n > getMaxDrawBuffers()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "drawBuffers", "more than max draw buffers");
            return;
        }
        for (GCGLsizei i = 0; i < n; ++i) {
            if (bufs[i] != GraphicsContextGL::NONE && bufs[i] != static_cast<GCGLenum>(GraphicsContextGL::COLOR_ATTACHMENT0 + i)) {
                synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "drawBuffers", "COLOR_ATTACHMENTi or NONE");
                return;
            }
        }
        m_framebufferBinding->drawBuffers(buffers);
    }
}

void WebGL2RenderingContext::clearBufferiv(GCGLenum buffer, GCGLint drawbuffer, Int32List&&, GCGLuint)
{
    switch (buffer) {
    case GraphicsContextGL::COLOR:
        if (drawbuffer < 0 || drawbuffer >= getMaxDrawBuffers()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "clearBufferiv", "buffer index out of range");
            return;
        }
        // TODO: Call clearBufferiv, requires gl3.h and ES3/gl.h
        break;
    case GraphicsContextGL::STENCIL:
        if (drawbuffer) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "clearBufferiv", "buffer index must be 0");
            return;
        }
        // TODO: Call clearBufferiv, requires gl3.h and ES3/gl.h
        break;
    case GraphicsContextGL::DEPTH:
    case GraphicsContextGL::DEPTH_STENCIL:
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "clearBufferiv", "buffer argument must be COLOR or STENCIL");
        break;
    }
}

void WebGL2RenderingContext::clearBufferuiv(GCGLenum buffer, GCGLint drawbuffer, Uint32List&&, GCGLuint)
{
    switch (buffer) {
    case GraphicsContextGL::COLOR:
        if (drawbuffer < 0 || drawbuffer >= getMaxDrawBuffers()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "clearBufferuiv", "buffer index out of range");
            return;
        }
        // TODO: Call clearBufferuiv, requires gl3.h and ES3/gl.h
        break;
    case GraphicsContextGL::DEPTH:
    case GraphicsContextGL::STENCIL:
    case GraphicsContextGL::DEPTH_STENCIL:
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "clearBufferuiv", "buffer argument must be COLOR");
        break;
    }
}

void WebGL2RenderingContext::clearBufferfv(GCGLenum buffer, GCGLint drawbuffer, Float32List&&, GCGLuint)
{
    switch (buffer) {
    case GraphicsContextGL::COLOR:
        if (drawbuffer < 0 || drawbuffer >= getMaxDrawBuffers()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "clearBufferfv", "buffer index out of range");
            return;
        }
        // TODO: Call clearBufferfv, requires gl3.h and ES3/gl.h
        break;
    case GraphicsContextGL::DEPTH:
        if (drawbuffer) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "clearBufferfv", "buffer index must be 0");
            return;
        }
        // TODO: Call clearBufferfv, requires gl3.h and ES3/gl.h
        break;
    case GraphicsContextGL::STENCIL:
    case GraphicsContextGL::DEPTH_STENCIL:
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "clearBufferfv", "buffer argument must be COLOR OR DEPTH");
        break;
    }
}

void WebGL2RenderingContext::clearBufferfi(GCGLenum buffer, GCGLint drawbuffer, GCGLfloat, GCGLint)
{
    switch (buffer) {
    case GraphicsContextGL::DEPTH_STENCIL:
        if (drawbuffer) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "clearBufferfv", "buffer index must be 0");
            return;
        }
        // TODO: Call clearBufferfi, requires gl3.h and ES3/gl.h
        break;
    case GraphicsContextGL::COLOR:
    case GraphicsContextGL::DEPTH:
    case GraphicsContextGL::STENCIL:
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "clearBufferfv", "buffer argument must be DEPTH_STENCIL");
        break;
    }
}

RefPtr<WebGLQuery> WebGL2RenderingContext::createQuery()
{
    if (isContextLostOrPending())
        return nullptr;

    auto query = WebGLQuery::create(*this);
    addSharedObject(query.get());
    return query;
}

void WebGL2RenderingContext::deleteQuery(WebGLQuery*)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] deleteQuery()");
}

GCGLboolean WebGL2RenderingContext::isQuery(WebGLQuery*)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] isQuery()");
    return false;
}

void WebGL2RenderingContext::beginQuery(GCGLenum target, WebGLQuery& query)
{
    if (isContextLostOrPending())
        return;

    // FIXME: Add validation to prevent bad caching.

    // Only one query object can be active per target.
    auto targetKey = (target == GraphicsContextGL::ANY_SAMPLES_PASSED_CONSERVATIVE) ? GraphicsContextGL::ANY_SAMPLES_PASSED : target;

    auto addResult = m_activeQueries.add(targetKey, makeRefPtr(&query));

    if (!addResult.isNewEntry) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginQuery", "Query object of target is already active");
        return;
    }

    m_context->beginQuery(target, query.object());
}

void WebGL2RenderingContext::endQuery(GCGLenum target)
{
    if (isContextLostOrPending() || !scriptExecutionContext())
        return;

    auto targetKey = (target == GraphicsContextGL::ANY_SAMPLES_PASSED_CONSERVATIVE) ? GraphicsContextGL::ANY_SAMPLES_PASSED : target;

    auto query = m_activeQueries.take(targetKey);
    if (!query) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "endQuery", "Query object of target is not active");
        return;
    }

    m_context->endQuery(target);

    // A query's result must not be made available until control has returned to the user agent's main loop.
    scriptExecutionContext()->eventLoop().queueMicrotask([query] {
        query->makeResultAvailable();
    });
}

RefPtr<WebGLQuery> WebGL2RenderingContext::getQuery(GCGLenum, GCGLenum)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] getquery()");
    return nullptr;
}

WebGLAny WebGL2RenderingContext::getQueryParameter(WebGLQuery& query, GCGLenum pname)
{
    if (isContextLostOrPending())
        return nullptr;

    switch (pname) {
    case GraphicsContextGL::QUERY_RESULT:
    case GraphicsContextGL::QUERY_RESULT_AVAILABLE:
        if (!query.isResultAvailable())
            return 0;
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getQueryParameter", "Invalid pname");
        return nullptr;
    }

    unsigned result = 0;
    m_context->getQueryObjectuiv(query.object(), pname, &result);
    return result;
}

RefPtr<WebGLSampler> WebGL2RenderingContext::createSampler()
{
    if (isContextLostOrPending())
        return nullptr;

    auto sampler = WebGLSampler::create(*this);
    addSharedObject(sampler.get());
    return sampler;
}

void WebGL2RenderingContext::deleteSampler(WebGLSampler* sampler)
{
    if (isContextLostOrPending())
        return;

    // One sampler can be bound to multiple texture units.
    if (sampler) {
        for (auto& samplerSlot : m_boundSamplers) {
            if (samplerSlot == sampler)
                samplerSlot = nullptr;
        }
    }

    deleteObject(sampler);
}

GCGLboolean WebGL2RenderingContext::isSampler(WebGLSampler* sampler)
{
    if (isContextLostOrPending() || !sampler || sampler->isDeleted() || !validateWebGLObject("isSampler", sampler))
        return false;

    return m_context->isSampler(sampler->object());
}

void WebGL2RenderingContext::bindSampler(GCGLuint unit, WebGLSampler* sampler)
{
    if (isContextLostOrPending() || m_boundSamplers[unit] == sampler)
        return;

    if (sampler && sampler->isDeleted()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "bindSampler", "cannot bind a deleted Sampler object");
        return;
    }

    m_context->bindSampler(unit, objectOrZero(sampler));
    m_boundSamplers[unit] = sampler;
}

void WebGL2RenderingContext::samplerParameteri(WebGLSampler& sampler, GCGLenum pname, GCGLint value)
{
    if (isContextLostOrPending())
        return;

    m_context->samplerParameteri(sampler.object(), pname, value);
}

void WebGL2RenderingContext::samplerParameterf(WebGLSampler& sampler, GCGLenum pname, GCGLfloat value)
{
    if (isContextLostOrPending())
        return;

    m_context->samplerParameterf(sampler.object(), pname, value);
}

WebGLAny WebGL2RenderingContext::getSamplerParameter(WebGLSampler& sampler, GCGLenum pname)
{
    if (isContextLostOrPending())
        return nullptr;

    switch (pname) {
    case GraphicsContextGL::TEXTURE_COMPARE_FUNC:
    case GraphicsContextGL::TEXTURE_COMPARE_MODE:
    case GraphicsContextGL::TEXTURE_MAG_FILTER:
    case GraphicsContextGL::TEXTURE_MIN_FILTER:
    case GraphicsContextGL::TEXTURE_WRAP_R:
    case GraphicsContextGL::TEXTURE_WRAP_S:
    case GraphicsContextGL::TEXTURE_WRAP_T: {
        int value = 0;
        m_context->getSamplerParameteriv(sampler.object(), pname, &value);
        return value;
    }
    case GraphicsContextGL::TEXTURE_MAX_LOD:
    case GraphicsContextGL::TEXTURE_MIN_LOD: {
        float value = 0;
        m_context->getSamplerParameterfv(sampler.object(), pname, &value);
        return value;
    }
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getSamplerParameter", "Invalid pname");
        return nullptr;
    }
}

RefPtr<WebGLSync> WebGL2RenderingContext::fenceSync(GCGLenum, GCGLbitfield)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] fenceSync()");
    return nullptr;
}

GCGLboolean WebGL2RenderingContext::isSync(WebGLSync*)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] isSync()");
    return false;
}

void WebGL2RenderingContext::deleteSync(WebGLSync*)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] deleteSync()");
}

GCGLenum WebGL2RenderingContext::clientWaitSync(WebGLSync&, GCGLbitfield, GCGLuint64)
{
    // Note: Do not implement this function without consulting webkit-dev and WebGL
    // reviewers beforehand. Apple folks, see <rdar://problem/36666458>.
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] clientWaitSync()");
    return 0;
}

void WebGL2RenderingContext::waitSync(WebGLSync&, GCGLbitfield, GCGLint64)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] waitSync()");
}

WebGLAny WebGL2RenderingContext::getSyncParameter(WebGLSync&, GCGLenum)
{
    // Note: Do not implement this function without consulting webkit-dev and WebGL
    // reviewers beforehand. Apple folks, see <rdar://problem/36666458>.
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] getSyncParameter()");
    return nullptr;
}

RefPtr<WebGLTransformFeedback> WebGL2RenderingContext::createTransformFeedback()
{
    if (isContextLostOrPending())
        return nullptr;

    auto transformFeedback = WebGLTransformFeedback::create(*this);
    addSharedObject(transformFeedback.get());
    return transformFeedback;
}

void WebGL2RenderingContext::deleteTransformFeedback(WebGLTransformFeedback* feedbackObject)
{
    if (isContextLostOrPending())
        return;

    if (m_boundTransformFeedback == feedbackObject)
        m_boundTransformFeedback = nullptr;

    deleteObject(feedbackObject);
}

GCGLboolean WebGL2RenderingContext::isTransformFeedback(WebGLTransformFeedback* feedbackObject)
{
    if (isContextLostOrPending() || !feedbackObject || feedbackObject->isDeleted() || !validateWebGLObject("isTransformFeedback", feedbackObject))
        return false;

    return m_context->isTransformFeedback(feedbackObject->object());
}

void WebGL2RenderingContext::bindTransformFeedback(GCGLenum target, WebGLTransformFeedback* feedbackObject)
{
    if (isContextLostOrPending())
        return;

    if (feedbackObject) {
        if (feedbackObject->isDeleted()) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "bindTransformFeedback", "cannot bind a deleted Transform Feedback object");
            return;
        }

        if (!validateWebGLObject("bindTransformFeedback", feedbackObject))
            return;
    }

    m_context->bindTransformFeedback(target, objectOrZero(feedbackObject));
    m_boundTransformFeedback = feedbackObject;
}

void WebGL2RenderingContext::beginTransformFeedback(GCGLenum primitiveMode)
{
    if (isContextLostOrPending())
        return;

    m_context->beginTransformFeedback(primitiveMode);
}

void WebGL2RenderingContext::endTransformFeedback()
{
    if (isContextLostOrPending())
        return;

    m_context->endTransformFeedback();
}

void WebGL2RenderingContext::transformFeedbackVaryings(WebGLProgram& program, const Vector<String>& varyings, GCGLenum bufferMode)
{
    if (isContextLostOrPending() || varyings.isEmpty() || !validateWebGLObject("transformFeedbackVaryings", &program))
        return;

    m_context->transformFeedbackVaryings(program.object(), varyings, bufferMode);
}

RefPtr<WebGLActiveInfo> WebGL2RenderingContext::getTransformFeedbackVarying(WebGLProgram& program, GCGLuint index)
{
    if (isContextLostOrPending() || !validateWebGLObject("getTransformFeedbackVarying", &program))
        return nullptr;

    GraphicsContextGL::ActiveInfo info;
    m_context->getTransformFeedbackVarying(program.object(), index, info);

    if (!info.name || !info.type || !info.size)
        return nullptr;

    return WebGLActiveInfo::create(info.name, info.type, info.size);
}

void WebGL2RenderingContext::pauseTransformFeedback()
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] pauseTransformFeedback()");
}

void WebGL2RenderingContext::resumeTransformFeedback()
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] resumeTransformFeedback()");
}

void WebGL2RenderingContext::bindBufferBase(GCGLenum target, GCGLuint index, WebGLBuffer* buffer)
{
    if (isContextLostOrPending())
        return;

    switch (target) {
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER:
        if (index >= m_boundTransformFeedbackBuffers.size()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bindBufferBase", "index out of range");
            return;
        }
        break;
    case GraphicsContextGL::UNIFORM_BUFFER:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "bindBufferBase", "target not yet supported");
        return;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "bindBufferBase", "invalid target");
        return;
    }

    if (!validateAndCacheBufferBinding("bindBufferBase", target, buffer))
        return;

    m_context->bindBufferBase(target, index, objectOrZero(buffer));
}

void WebGL2RenderingContext::bindBufferRange(GCGLenum, GCGLuint, WebGLBuffer*, GCGLint64, GCGLint64)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] bindBufferRange()");
}

WebGLAny WebGL2RenderingContext::getIndexedParameter(GCGLenum target, GCGLuint index)
{
    if (isContextLostOrPending())
        return nullptr;

    switch (target) {
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER_BINDING:
        if (index >= m_boundTransformFeedbackBuffers.size()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getIndexedParameter", "index out of range");
            return nullptr;
        }
        return m_boundTransformFeedbackBuffers[index];
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER_SIZE:
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER_START:
    case GraphicsContextGL::UNIFORM_BUFFER_BINDING:
    case GraphicsContextGL::UNIFORM_BUFFER_SIZE:
    case GraphicsContextGL::UNIFORM_BUFFER_START:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getIndexedParameter", "parameter name not yet supported");
        return nullptr;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getIndexedParameter", "invalid parameter name");
        return nullptr;
    }
}

Optional<Vector<GCGLuint>> WebGL2RenderingContext::getUniformIndices(WebGLProgram&, const Vector<String>&)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] getUniformIndices()");
    return WTF::nullopt;
}

WebGLAny WebGL2RenderingContext::getActiveUniforms(WebGLProgram& program, const Vector<GCGLuint>& uniformIndices, GCGLenum pname)
{
    if (isContextLostOrPending() || !validateWebGLObject("getActiveUniforms", &program))
        return nullptr;

    switch (pname) {
    case GraphicsContextGL::UNIFORM_TYPE:
    case GraphicsContextGL::UNIFORM_SIZE:
    case GraphicsContextGL::UNIFORM_BLOCK_INDEX:
    case GraphicsContextGL::UNIFORM_OFFSET:
    case GraphicsContextGL::UNIFORM_ARRAY_STRIDE:
    case GraphicsContextGL::UNIFORM_MATRIX_STRIDE:
    case GraphicsContextGL::UNIFORM_IS_ROW_MAJOR:
        {
            Vector<GCGLint> params(uniformIndices.size(), 0);
            m_context->getActiveUniforms(program.object(), uniformIndices, pname, params);
            return WTFMove(params);
        }
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getActiveUniforms", "invalid parameter name");
        return nullptr;
    }
}

GCGLuint WebGL2RenderingContext::getUniformBlockIndex(WebGLProgram&, const String&)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] getUniformBlockIndex()");
    return 0;
}

WebGLAny WebGL2RenderingContext::getActiveUniformBlockParameter(WebGLProgram&, GCGLuint, GCGLenum)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] getActiveUniformBlockParameter()");
    return nullptr;
}

WebGLAny WebGL2RenderingContext::getActiveUniformBlockName(WebGLProgram&, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] getActiveUniformBlockName()");
    return nullptr;
}

void WebGL2RenderingContext::uniformBlockBinding(WebGLProgram&, GCGLuint, GCGLuint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniformBlockBinding()");
}

RefPtr<WebGLVertexArrayObject> WebGL2RenderingContext::createVertexArray()
{
    if (isContextLost())
        return nullptr;
    
    auto object = WebGLVertexArrayObject::create(*this, WebGLVertexArrayObject::Type::User);
    addContextObject(object.get());
    return WTFMove(object);
}

void WebGL2RenderingContext::deleteVertexArray(WebGLVertexArrayObject* arrayObject)
{
    if (!arrayObject || isContextLost())
        return;
    
    if (arrayObject->isDeleted())
        return;
    
    if (!arrayObject->isDefaultObject() && arrayObject == m_boundVertexArrayObject)
#if USE(OPENGL_ES)
        setBoundVertexArrayObject(nullptr);
#else
        bindVertexArray(nullptr); // The default VAO was removed in OpenGL 3.3 but not from WebGL 2; bind the default for WebGL to use.
#endif
    
    arrayObject->deleteObject(graphicsContextGL());
}

GCGLboolean WebGL2RenderingContext::isVertexArray(WebGLVertexArrayObject* arrayObject)
{
    if (!arrayObject || isContextLost())
        return false;
    
    if (!arrayObject->hasEverBeenBound() || !arrayObject->validate(0, *this))
        return false;
    
    return m_context->isVertexArray(arrayObject->object());
}

void WebGL2RenderingContext::bindVertexArray(WebGLVertexArrayObject* arrayObject)
{
    if (isContextLost())
        return;
    
    if (arrayObject && (arrayObject->isDeleted() || !arrayObject->validate(0, *this) || !m_contextObjects.contains(arrayObject))) {
        m_context->synthesizeGLError(GraphicsContextGL::INVALID_OPERATION);
        return;
    }
    if (arrayObject && !arrayObject->isDefaultObject() && arrayObject->object()) {
        m_context->bindVertexArray(arrayObject->object());
        
        arrayObject->setHasEverBeenBound();
        setBoundVertexArrayObject(arrayObject);
    } else {
        m_context->bindVertexArray(m_defaultVertexArrayObject->object());
        setBoundVertexArrayObject(m_defaultVertexArrayObject.get());
    }
}

WebGLExtension* WebGL2RenderingContext::getExtension(const String& name)
{
    if (isContextLostOrPending())
        return nullptr;

#define ENABLE_IF_REQUESTED(type, variable, nameLiteral, canEnable) \
    if (equalIgnoringASCIICase(name, nameLiteral)) { \
        if (!variable) { \
            variable = (canEnable) ? makeUnique<type>(*this) : nullptr; \
            if (variable != nullptr) \
                InspectorInstrumentation::didEnableExtension(*this, name); \
        } \
        return variable.get(); \
    }

    ENABLE_IF_REQUESTED(EXTTextureFilterAnisotropic, m_extTextureFilterAnisotropic, "EXT_texture_filter_anisotropic", enableSupportedExtension("GL_EXT_texture_filter_anisotropic"_s));
    ENABLE_IF_REQUESTED(OESTextureFloat, m_oesTextureFloat, "OES_texture_float", enableSupportedExtension("GL_OES_texture_float"_s));
    ENABLE_IF_REQUESTED(OESTextureFloatLinear, m_oesTextureFloatLinear, "OES_texture_float_linear", enableSupportedExtension("GL_OES_texture_float_linear"_s));
    ENABLE_IF_REQUESTED(OESTextureHalfFloat, m_oesTextureHalfFloat, "OES_texture_half_float", enableSupportedExtension("GL_OES_texture_half_float"_s));
    ENABLE_IF_REQUESTED(OESTextureHalfFloatLinear, m_oesTextureHalfFloatLinear, "OES_texture_half_float_linear", enableSupportedExtension("GL_OES_texture_half_float_linear"_s));
    ENABLE_IF_REQUESTED(WebGLLoseContext, m_webglLoseContext, "WEBGL_lose_context", true);
    ENABLE_IF_REQUESTED(WebGLCompressedTextureASTC, m_webglCompressedTextureASTC, "WEBGL_compressed_texture_astc", WebGLCompressedTextureASTC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureATC, m_webglCompressedTextureATC, "WEBKIT_WEBGL_compressed_texture_atc", WebGLCompressedTextureATC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureETC, m_webglCompressedTextureETC, "WEBGL_compressed_texture_etc", WebGLCompressedTextureETC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureETC1, m_webglCompressedTextureETC1, "WEBGL_compressed_texture_etc1", WebGLCompressedTextureETC1::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTexturePVRTC, m_webglCompressedTexturePVRTC, "WEBKIT_WEBGL_compressed_texture_pvrtc", WebGLCompressedTexturePVRTC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureS3TC, m_webglCompressedTextureS3TC, "WEBGL_compressed_texture_s3tc", WebGLCompressedTextureS3TC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLDepthTexture, m_webglDepthTexture, "WEBGL_depth_texture", WebGLDepthTexture::supported(*graphicsContextGL()));
    ENABLE_IF_REQUESTED(WebGLDebugRendererInfo, m_webglDebugRendererInfo, "WEBGL_debug_renderer_info", true);
    ENABLE_IF_REQUESTED(WebGLDebugShaders, m_webglDebugShaders, "WEBGL_debug_shaders", m_context->getExtensions().supports("GL_ANGLE_translated_shader_source"_s));
    return nullptr;
}

Optional<Vector<String>> WebGL2RenderingContext::getSupportedExtensions()
{
    if (isContextLost())
        return WTF::nullopt;

    Vector<String> result;
    
    if (m_isPendingPolicyResolution)
        return result;

    auto& extensions = m_context->getExtensions();
    if (extensions.supports("GL_OES_texture_float"_s))
        result.append("OES_texture_float"_s);
    if (extensions.supports("GL_OES_texture_float_linear"_s))
        result.append("OES_texture_float_linear"_s);
    if (extensions.supports("GL_OES_texture_half_float"_s))
        result.append("OES_texture_half_float"_s);
    if (extensions.supports("GL_OES_texture_half_float_linear"_s))
        result.append("OES_texture_half_float_linear"_s);
    if (extensions.supports("GL_EXT_texture_filter_anisotropic"_s))
        result.append("EXT_texture_filter_anisotropic"_s);
    if (WebGLCompressedTextureASTC::supported(*this))
        result.append("WEBGL_compressed_texture_astc"_s);
    if (WebGLCompressedTextureATC::supported(*this))
        result.append("WEBKIT_WEBGL_compressed_texture_atc"_s);
    if (WebGLCompressedTextureETC::supported(*this))
        result.append("WEBGL_compressed_texture_etc"_s);
    if (WebGLCompressedTextureETC1::supported(*this))
        result.append("WEBGL_compressed_texture_etc1"_s);
    if (WebGLCompressedTexturePVRTC::supported(*this))
        result.append("WEBKIT_WEBGL_compressed_texture_pvrtc"_s);
    if (WebGLCompressedTextureS3TC::supported(*this))
        result.append("WEBGL_compressed_texture_s3tc"_s);
    if (WebGLDepthTexture::supported(*graphicsContextGL()))
        result.append("WEBGL_depth_texture"_s);
    result.append("WEBGL_lose_context"_s);
    if (extensions.supports("GL_ANGLE_translated_shader_source"_s))
        result.append("WEBGL_debug_shaders"_s);
    result.append("WEBGL_debug_renderer_info"_s);

    return result;
}

static bool validateDefaultFramebufferAttachment(GCGLenum& attachment)
{
    switch (attachment) {
    case GraphicsContextGL::BACK:
        // Because the backbuffer is simulated on all current WebKit ports, we need to change BACK to COLOR_ATTACHMENT0.
        attachment = GraphicsContextGL::COLOR_ATTACHMENT0;
        return true;
    case GraphicsContextGL::DEPTH:
    case GraphicsContextGL::STENCIL:
        return true;
    }

    return false;
}

WebGLAny WebGL2RenderingContext::getFramebufferAttachmentParameter(GCGLenum target, GCGLenum attachment, GCGLenum pname)
{
    const char* functionName = "getFramebufferAttachmentParameter";
    if (isContextLostOrPending() || !validateFramebufferTarget(functionName, target))
        return nullptr;

    auto targetFramebuffer = (target == GraphicsContextGL::READ_FRAMEBUFFER) ? m_readFramebufferBinding : m_framebufferBinding;

    if (!targetFramebuffer) {
        // OpenGL ES 3: Default framebuffer is bound.
        if (!validateDefaultFramebufferAttachment(attachment)) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid attachment");
            return nullptr;
        }
        GCGLint value = 0;
        m_context->getFramebufferAttachmentParameteriv(target, attachment, pname, &value);
        return value;
    }
    if (!validateNonDefaultFramebufferAttachment(functionName, attachment))
        return nullptr;

    auto object = makeRefPtr(targetFramebuffer->getAttachmentObject(attachment));
    if (!object) {
        if (pname == GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
            return static_cast<unsigned>(GraphicsContextGL::NONE);
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "invalid parameter name");
        return nullptr;
    }

    switch (pname) {
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_RED_SIZE:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE: {
        GCGLint value = 0;
        m_context->getFramebufferAttachmentParameteriv(target, attachment, pname, &value);
        return value;
    }
    }
    
    if (object->isTexture()) {
        switch (pname) {
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return static_cast<unsigned>(GraphicsContextGL::TEXTURE);
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            return makeRefPtr(reinterpret_cast<WebGLTexture&>(*object));
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING: {
            GCGLint value = 0;
            m_context->getFramebufferAttachmentParameteriv(target, attachment, pname, &value);
            return value;
        }
        default:
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid parameter name for texture attachment");
            return nullptr;
        }
    } else {
        ASSERT(object->isRenderbuffer());
        switch (pname) {
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return static_cast<unsigned>(GraphicsContextGL::RENDERBUFFER);
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            return makeRefPtr(reinterpret_cast<WebGLRenderbuffer&>(*object));
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING : {
            auto& renderBuffer = reinterpret_cast<WebGLRenderbuffer&>(*object);
            auto format = renderBuffer.getInternalFormat();
            if (format == GraphicsContextGL::SRGB8_ALPHA8
                || format == GraphicsContextGL::COMPRESSED_SRGB8_ETC2
                || format == GraphicsContextGL::COMPRESSED_SRGB8_ALPHA8_ETC2_EAC
                || format == GraphicsContextGL::COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2) {
                return static_cast<unsigned>(GraphicsContextGL::SRGB);
            }
            return static_cast<unsigned>(GraphicsContextGL::LINEAR);
        }
        default:
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid parameter name for renderbuffer attachment");
            return nullptr;
        }
    }
}

bool WebGL2RenderingContext::validateFramebufferFuncParameters(const char* functionName, GCGLenum target, GCGLenum attachment)
{
    return validateFramebufferTarget(functionName, target) && validateNonDefaultFramebufferAttachment(functionName, attachment);
}

bool WebGL2RenderingContext::validateFramebufferTarget(const char* functionName, GCGLenum target)
{
    switch (target) {
    case GraphicsContextGL::FRAMEBUFFER:
    case GraphicsContextGL::DRAW_FRAMEBUFFER:
    case GraphicsContextGL::READ_FRAMEBUFFER:
        return true;
    }

    synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
    return false;
}

bool WebGL2RenderingContext::validateNonDefaultFramebufferAttachment(const char* functionName, GCGLenum attachment)
{
    switch (attachment) {
    case GraphicsContextGL::DEPTH_ATTACHMENT:
    case GraphicsContextGL::STENCIL_ATTACHMENT:
    case GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT:
        return true;
    default:
        if (attachment >= GraphicsContextGL::COLOR_ATTACHMENT0 && attachment < static_cast<GCGLenum>(GraphicsContextGL::COLOR_ATTACHMENT0 + getMaxColorAttachments()))
            return true;
    }

    synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid attachment");
    return false;
}

GCGLint WebGL2RenderingContext::getMaxDrawBuffers()
{
    if (!m_maxDrawBuffers)
        m_context->getIntegerv(GraphicsContextGL::MAX_DRAW_BUFFERS, &m_maxDrawBuffers);
    return m_maxDrawBuffers;
}

GCGLint WebGL2RenderingContext::getMaxColorAttachments()
{
    // DrawBuffers requires MAX_COLOR_ATTACHMENTS == MAX_DRAW_BUFFERS
    if (!m_maxColorAttachments)
        m_context->getIntegerv(GraphicsContextGL::MAX_DRAW_BUFFERS, &m_maxColorAttachments);
    return m_maxColorAttachments;
}

void WebGL2RenderingContext::renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (isContextLostOrPending())
        return;
    if (target != GraphicsContextGL::RENDERBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "renderbufferStorage", "invalid target");
        return;
    }
    if (!m_renderbufferBinding || !m_renderbufferBinding->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "renderbufferStorage", "no bound renderbuffer");
        return;
    }
    if (!validateSize("renderbufferStorage", width, height))
        return;
    switch (internalformat) {
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::DEPTH_COMPONENT32F:
    case GraphicsContextGL::DEPTH_COMPONENT24:
    case GraphicsContextGL::RGBA32I:
    case GraphicsContextGL::RGBA32UI:
    case GraphicsContextGL::RGBA16I:
    case GraphicsContextGL::RGBA16UI:
    case GraphicsContextGL::RGBA8:
    case GraphicsContextGL::RGBA8I:
    case GraphicsContextGL::RGBA8UI:
    case GraphicsContextGL::RGB10_A2:
    case GraphicsContextGL::RGB10_A2UI:
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RG32I:
    case GraphicsContextGL::RG32UI:
    case GraphicsContextGL::RG16I:
    case GraphicsContextGL::RG16UI:
    case GraphicsContextGL::RG8:
    case GraphicsContextGL::RG8I:
    case GraphicsContextGL::RG8UI:
    case GraphicsContextGL::R32I:
    case GraphicsContextGL::R32UI:
    case GraphicsContextGL::R16I:
    case GraphicsContextGL::R16UI:
    case GraphicsContextGL::R8:
    case GraphicsContextGL::R8I:
    case GraphicsContextGL::R8UI:
    case GraphicsContextGL::RGB5_A1:
    case GraphicsContextGL::RGB565:
    case GraphicsContextGL::RGB8:
    case GraphicsContextGL::STENCIL_INDEX8:
    case GraphicsContextGL::SRGB8_ALPHA8:
        m_context->renderbufferStorage(target, internalformat, width, height);
        m_renderbufferBinding->setInternalFormat(internalformat);
        m_renderbufferBinding->setIsValid(true);
        m_renderbufferBinding->setSize(width, height);
        break;
    case GraphicsContextGL::DEPTH32F_STENCIL8:
    case GraphicsContextGL::DEPTH24_STENCIL8:
        if (!isDepthStencilSupported()) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "renderbufferStorage", "invalid internalformat");
            return;
        }
        m_context->renderbufferStorage(target, internalformat, width, height);
        m_renderbufferBinding->setSize(width, height);
        m_renderbufferBinding->setIsValid(isDepthStencilSupported());
        m_renderbufferBinding->setInternalFormat(internalformat);
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "renderbufferStorage", "invalid internalformat");
        return;
    }
    applyStencilTest();
}

void WebGL2RenderingContext::hint(GCGLenum target, GCGLenum mode)
{
    if (isContextLostOrPending())
        return;
    bool isValid = false;
    switch (target) {
    case GraphicsContextGL::GENERATE_MIPMAP_HINT:
    case GraphicsContextGL::FRAGMENT_SHADER_DERIVATIVE_HINT:
        isValid = true;
        break;
    }
    if (!isValid) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "hint", "invalid target");
        return;
    }
    m_context->hint(target, mode);
}

GCGLenum WebGL2RenderingContext::baseInternalFormatFromInternalFormat(GCGLenum internalformat)
{
    // Handles sized, unsized, and compressed internal formats.
    switch (internalformat) {
    case GraphicsContextGL::R8:
    case GraphicsContextGL::R8_SNORM:
    case GraphicsContextGL::R16F:
    case GraphicsContextGL::R32F:
    case GraphicsContextGL::COMPRESSED_R11_EAC:
    case GraphicsContextGL::COMPRESSED_SIGNED_R11_EAC:
        return GraphicsContextGL::RED;
    case GraphicsContextGL::R8I:
    case GraphicsContextGL::R8UI:
    case GraphicsContextGL::R16I:
    case GraphicsContextGL::R16UI:
    case GraphicsContextGL::R32I:
    case GraphicsContextGL::R32UI:
        return GraphicsContextGL::RED_INTEGER;
    case GraphicsContextGL::RG8:
    case GraphicsContextGL::RG8_SNORM:
    case GraphicsContextGL::RG16F:
    case GraphicsContextGL::RG32F:
    case GraphicsContextGL::COMPRESSED_RG11_EAC:
    case GraphicsContextGL::COMPRESSED_SIGNED_RG11_EAC:
        return GraphicsContextGL::RG;
    case GraphicsContextGL::RG8I:
    case GraphicsContextGL::RG8UI:
    case GraphicsContextGL::RG16I:
    case GraphicsContextGL::RG16UI:
    case GraphicsContextGL::RG32I:
    case GraphicsContextGL::RG32UI:
        return GraphicsContextGL::RG_INTEGER;
    case GraphicsContextGL::RGB8:
    case GraphicsContextGL::RGB8_SNORM:
    case GraphicsContextGL::RGB565:
    case GraphicsContextGL::SRGB8:
    case GraphicsContextGL::RGB16F:
    case GraphicsContextGL::RGB32F:
    case GraphicsContextGL::RGB:
    case GraphicsContextGL::COMPRESSED_RGB8_ETC2:
    case GraphicsContextGL::COMPRESSED_SRGB8_ETC2:
        return GraphicsContextGL::RGB;
    case GraphicsContextGL::RGB8I:
    case GraphicsContextGL::RGB8UI:
    case GraphicsContextGL::RGB16I:
    case GraphicsContextGL::RGB16UI:
    case GraphicsContextGL::RGB32I:
    case GraphicsContextGL::RGB32UI:
        return GraphicsContextGL::RGB_INTEGER;
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RGB5_A1:
    case GraphicsContextGL::RGBA8:
    case GraphicsContextGL::RGBA8_SNORM:
    case GraphicsContextGL::RGB10_A2:
    case GraphicsContextGL::SRGB8_ALPHA8:
    case GraphicsContextGL::RGBA16F:
    case GraphicsContextGL::RGBA32F:
    case GraphicsContextGL::RGBA:
    case GraphicsContextGL::COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GraphicsContextGL::COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GraphicsContextGL::COMPRESSED_RGBA8_ETC2_EAC:
    case GraphicsContextGL::COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
        return GraphicsContextGL::RGBA;
    case GraphicsContextGL::RGBA8I:
    case GraphicsContextGL::RGBA8UI:
    case GraphicsContextGL::RGB10_A2UI:
    case GraphicsContextGL::RGBA16I:
    case GraphicsContextGL::RGBA16UI:
    case GraphicsContextGL::RGBA32I:
    case GraphicsContextGL::RGBA32UI:
        return GraphicsContextGL::RGBA_INTEGER;
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::DEPTH_COMPONENT24:
    case GraphicsContextGL::DEPTH_COMPONENT32F:
        return GraphicsContextGL::DEPTH_COMPONENT;
    case GraphicsContextGL::DEPTH24_STENCIL8:
    case GraphicsContextGL::DEPTH32F_STENCIL8:
        return GraphicsContextGL::DEPTH_STENCIL;
    case GraphicsContextGL::LUMINANCE:
    case GraphicsContextGL::LUMINANCE_ALPHA:
    case GraphicsContextGL::ALPHA:
        return internalformat;
    default:
        ASSERT_NOT_REACHED();
        return GraphicsContextGL::NONE;
    }
}

bool WebGL2RenderingContext::isIntegerFormat(GCGLenum internalformat)
{
    switch (baseInternalFormatFromInternalFormat(internalformat)) {
    case GraphicsContextGL::RED_INTEGER:
    case GraphicsContextGL::RG_INTEGER:
    case GraphicsContextGL::RGB_INTEGER:
    case GraphicsContextGL::RGBA_INTEGER:
        return true;
    }
    return false;
}

WebGLAny WebGL2RenderingContext::getParameter(GCGLenum pname)
{
    if (isContextLostOrPending())
        return nullptr;
    switch (pname) {
    case GraphicsContextGL::ACTIVE_TEXTURE:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::ALIASED_LINE_WIDTH_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContextGL::ALIASED_POINT_SIZE_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContextGL::ALPHA_BITS:
        if (!m_framebufferBinding && !m_attributes.alpha)
            return 0;
        return getIntParameter(pname);
    case GraphicsContextGL::ARRAY_BUFFER_BINDING:
        return m_boundArrayBuffer;
    case GraphicsContextGL::BLEND:
        return getBooleanParameter(pname);
    case GraphicsContextGL::BLEND_COLOR:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContextGL::BLEND_DST_ALPHA:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLEND_DST_RGB:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLEND_EQUATION_ALPHA:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLEND_EQUATION_RGB:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLEND_SRC_ALPHA:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLEND_SRC_RGB:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::BLUE_BITS:
        return getIntParameter(pname);
    case GraphicsContextGL::COLOR_CLEAR_VALUE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContextGL::COLOR_WRITEMASK:
        return getBooleanArrayParameter(pname);
    case GraphicsContextGL::COMPRESSED_TEXTURE_FORMATS:
        return Uint32Array::tryCreate(m_compressedTextureFormats.data(), m_compressedTextureFormats.size());
    case GraphicsContextGL::CULL_FACE:
        return getBooleanParameter(pname);
    case GraphicsContextGL::CULL_FACE_MODE:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::CURRENT_PROGRAM:
        return m_currentProgram;
    case GraphicsContextGL::DEPTH_BITS:
        if (!m_framebufferBinding && !m_attributes.depth)
            return 0;
        return getIntParameter(pname);
    case GraphicsContextGL::DEPTH_CLEAR_VALUE:
        return getFloatParameter(pname);
    case GraphicsContextGL::DEPTH_FUNC:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::DEPTH_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContextGL::DEPTH_TEST:
        return getBooleanParameter(pname);
    case GraphicsContextGL::DEPTH_WRITEMASK:
        return getBooleanParameter(pname);
    case GraphicsContextGL::DITHER:
        return getBooleanParameter(pname);
    case GraphicsContextGL::ELEMENT_ARRAY_BUFFER_BINDING:
        return makeRefPtr(m_boundVertexArrayObject->getElementArrayBuffer());
    case GraphicsContextGL::FRAMEBUFFER_BINDING:
        return m_framebufferBinding;
    case GraphicsContextGL::FRONT_FACE:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::GENERATE_MIPMAP_HINT:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::GREEN_BITS:
        return getIntParameter(pname);
    case GraphicsContextGL::IMPLEMENTATION_COLOR_READ_FORMAT:
        return getIntParameter(pname);
    case GraphicsContextGL::IMPLEMENTATION_COLOR_READ_TYPE:
        return getIntParameter(pname);
    case GraphicsContextGL::LINE_WIDTH:
        return getFloatParameter(pname);
    case GraphicsContextGL::MAX_COMBINED_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_CUBE_MAP_TEXTURE_SIZE:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_FRAGMENT_UNIFORM_VECTORS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_RENDERBUFFER_SIZE:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_TEXTURE_SIZE:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VARYING_VECTORS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VERTEX_ATTRIBS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VERTEX_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VERTEX_UNIFORM_VECTORS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VIEWPORT_DIMS:
        return getWebGLIntArrayParameter(pname);
    case GraphicsContextGL::NUM_SHADER_BINARY_FORMATS:
        return getIntParameter(pname);
    case GraphicsContextGL::PACK_ALIGNMENT:
        return getIntParameter(pname);
    case GraphicsContextGL::POLYGON_OFFSET_FACTOR:
        return getFloatParameter(pname);
    case GraphicsContextGL::POLYGON_OFFSET_FILL:
        return getBooleanParameter(pname);
    case GraphicsContextGL::POLYGON_OFFSET_UNITS:
        return getFloatParameter(pname);
    case GraphicsContextGL::RED_BITS:
        return getIntParameter(pname);
    case GraphicsContextGL::RENDERBUFFER_BINDING:
        return m_renderbufferBinding;
    case GraphicsContextGL::RENDERER:
        return "WebKit WebGL"_str;
    case GraphicsContextGL::SAMPLE_BUFFERS:
        return getIntParameter(pname);
    case GraphicsContextGL::SAMPLE_COVERAGE_INVERT:
        return getBooleanParameter(pname);
    case GraphicsContextGL::SAMPLE_COVERAGE_VALUE:
        return getFloatParameter(pname);
    case GraphicsContextGL::SAMPLES:
        return getIntParameter(pname);
    case GraphicsContextGL::SCISSOR_BOX:
        return getWebGLIntArrayParameter(pname);
    case GraphicsContextGL::SCISSOR_TEST:
        return getBooleanParameter(pname);
    case GraphicsContextGL::SHADING_LANGUAGE_VERSION:
        return "WebGL GLSL ES 1.0 (" + m_context->getString(GraphicsContextGL::SHADING_LANGUAGE_VERSION) + ")";
    case GraphicsContextGL::STENCIL_BACK_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_FUNC:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_PASS_DEPTH_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_PASS_DEPTH_PASS:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_REF:
        return getIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_VALUE_MASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BACK_WRITEMASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_BITS:
        if (!m_framebufferBinding && !m_attributes.stencil)
            return 0;
        return getIntParameter(pname);
    case GraphicsContextGL::STENCIL_CLEAR_VALUE:
        return getIntParameter(pname);
    case GraphicsContextGL::STENCIL_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_FUNC:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_PASS_DEPTH_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_PASS_DEPTH_PASS:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_REF:
        return getIntParameter(pname);
    case GraphicsContextGL::STENCIL_TEST:
        return getBooleanParameter(pname);
    case GraphicsContextGL::STENCIL_VALUE_MASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::STENCIL_WRITEMASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContextGL::SUBPIXEL_BITS:
        return getIntParameter(pname);
    case GraphicsContextGL::TEXTURE_BINDING_2D:
        return m_textureUnits[m_activeTextureUnit].texture2DBinding;
    case GraphicsContextGL::TEXTURE_BINDING_CUBE_MAP:
        return m_textureUnits[m_activeTextureUnit].textureCubeMapBinding;
    case GraphicsContextGL::UNPACK_ALIGNMENT:
        return getIntParameter(pname);
    case GraphicsContextGL::UNPACK_FLIP_Y_WEBGL:
        return m_unpackFlipY;
    case GraphicsContextGL::UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        return m_unpackPremultiplyAlpha;
    case GraphicsContextGL::UNPACK_COLORSPACE_CONVERSION_WEBGL:
        return m_unpackColorspaceConversion;
    case GraphicsContextGL::VENDOR:
        return "WebKit"_str;
    case GraphicsContextGL::VERSION:
        return "WebGL 2.0"_str;
    case GraphicsContextGL::VIEWPORT:
        return getWebGLIntArrayParameter(pname);
    case WebGLDebugRendererInfo::UNMASKED_RENDERER_WEBGL:
        if (m_webglDebugRendererInfo) {
#if PLATFORM(IOS_FAMILY)
            return "Apple GPU"_str;
#else
            return m_context->getString(GraphicsContextGL::RENDERER);
#endif
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_debug_renderer_info not enabled");
        return nullptr;
    case WebGLDebugRendererInfo::UNMASKED_VENDOR_WEBGL:
        if (m_webglDebugRendererInfo)
            return m_context->getString(GraphicsContextGL::VENDOR);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_debug_renderer_info not enabled");
        return nullptr;
    case ExtensionsGL::MAX_TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
        if (m_extTextureFilterAnisotropic)
            return getFloatParameter(ExtensionsGL::MAX_TEXTURE_MAX_ANISOTROPY_EXT);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
        return nullptr;
    case GraphicsContextGL::FRAGMENT_SHADER_DERIVATIVE_HINT:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_3D_TEXTURE_SIZE:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_ARRAY_TEXTURE_LAYERS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_COLOR_ATTACHMENTS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
        return getInt64Parameter(pname);
    case GraphicsContextGL::MAX_COMBINED_UNIFORM_BLOCKS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
        return getInt64Parameter(pname);
    case GraphicsContextGL::MAX_DRAW_BUFFERS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_ELEMENT_INDEX:
        return getInt64Parameter(pname);
    case GraphicsContextGL::MAX_ELEMENTS_INDICES:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_ELEMENTS_VERTICES:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_FRAGMENT_UNIFORM_COMPONENTS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_FRAGMENT_UNIFORM_BLOCKS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_PROGRAM_TEXEL_OFFSET:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_SAMPLES:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_SERVER_WAIT_TIMEOUT:
        return getInt64Parameter(pname);
    case GraphicsContextGL::MAX_TEXTURE_LOD_BIAS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_UNIFORM_BLOCK_SIZE:
        return getInt64Parameter(pname);
    case GraphicsContextGL::MAX_UNIFORM_BUFFER_BINDINGS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VARYING_COMPONENTS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VERTEX_OUTPUT_COMPONENTS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VERTEX_UNIFORM_BLOCKS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_VERTEX_UNIFORM_COMPONENTS: 
        return getIntParameter(pname);                            
    case GraphicsContextGL::MIN_PROGRAM_TEXEL_OFFSET:
        return getIntParameter(pname);
    case GraphicsContextGL::PACK_ROW_LENGTH:
        return getIntParameter(pname);
    case GraphicsContextGL::PACK_SKIP_PIXELS:
        return getIntParameter(pname);
    case GraphicsContextGL::PACK_SKIP_ROWS:
        return getIntParameter(pname);
    case GraphicsContextGL::UNPACK_IMAGE_HEIGHT:
        return getIntParameter(pname);
    case GraphicsContextGL::UNPACK_ROW_LENGTH:
        return getIntParameter(pname);
    case GraphicsContextGL::UNPACK_SKIP_IMAGES:
        return getIntParameter(pname);
    case GraphicsContextGL::UNPACK_SKIP_PIXELS:
        return getIntParameter(pname);
    case GraphicsContextGL::UNPACK_SKIP_ROWS:
        return getIntParameter(pname);
    case GraphicsContextGL::RASTERIZER_DISCARD:
        return getBooleanParameter(pname);
    case GraphicsContextGL::SAMPLE_ALPHA_TO_COVERAGE:
        return getBooleanParameter(pname);
    case GraphicsContextGL::SAMPLE_COVERAGE:
        return getBooleanParameter(pname);
    case GraphicsContextGL::TRANSFORM_FEEDBACK_ACTIVE:
        return getBooleanParameter(pname);
    case GraphicsContextGL::TRANSFORM_FEEDBACK_PAUSED:
        return getBooleanParameter(pname);
    case GraphicsContextGL::UNIFORM_BUFFER_OFFSET_ALIGNMENT:
        return getIntParameter(pname);
    case GraphicsContextGL::VERTEX_ARRAY_BINDING:
        if (m_boundVertexArrayObject->isDefaultObject())
            return nullptr;
        return makeRefPtr(static_cast<WebGLVertexArrayObject&>(*m_boundVertexArrayObject));
    case GraphicsContextGL::DRAW_BUFFER0:
    case GraphicsContextGL::DRAW_BUFFER1:
    case GraphicsContextGL::DRAW_BUFFER2:
    case GraphicsContextGL::DRAW_BUFFER3:
    case GraphicsContextGL::DRAW_BUFFER4:
    case GraphicsContextGL::DRAW_BUFFER5:
    case GraphicsContextGL::DRAW_BUFFER6:
    case GraphicsContextGL::DRAW_BUFFER7:
    case GraphicsContextGL::DRAW_BUFFER8:
    case GraphicsContextGL::DRAW_BUFFER9:
    case GraphicsContextGL::DRAW_BUFFER10:
    case GraphicsContextGL::DRAW_BUFFER11:
    case GraphicsContextGL::DRAW_BUFFER12:
    case GraphicsContextGL::DRAW_BUFFER13:
    case GraphicsContextGL::DRAW_BUFFER14:
    case GraphicsContextGL::DRAW_BUFFER15:
        if (m_framebufferBinding)
            return m_framebufferBinding->getDrawBuffer(pname);
        return m_backDrawBuffer; // emulated backbuffer
    case GraphicsContextGL::READ_FRAMEBUFFER_BINDING:
        return m_readFramebufferBinding;
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BINDING:
        return m_boundTransformFeedback;
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER_BINDING:
        return m_boundTransformFeedbackBuffer;
    case GraphicsContextGL::SAMPLER_BINDING:
        return m_boundSamplers[m_activeTextureUnit];
    case GraphicsContextGL::COPY_READ_BUFFER:
    case GraphicsContextGL::COPY_WRITE_BUFFER:
    case GraphicsContextGL::PIXEL_PACK_BUFFER_BINDING:   
    case GraphicsContextGL::PIXEL_UNPACK_BUFFER_BINDING:
    case GraphicsContextGL::READ_BUFFER:
    case GraphicsContextGL::TEXTURE_BINDING_2D_ARRAY:
    case GraphicsContextGL::TEXTURE_BINDING_3D:
    case GraphicsContextGL::UNIFORM_BUFFER_BINDING:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "parameter name not yet supported");
        return nullptr;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name");
        return nullptr;
    }
}

bool WebGL2RenderingContext::validateIndexArrayConservative(GCGLenum type, unsigned& numElementsRequired)
{
    // Performs conservative validation by caching a maximum index of
    // the given type per element array buffer. If all of the bound
    // array buffers have enough elements to satisfy that maximum
    // index, skips the expensive per-draw-call iteration in
    // validateIndexArrayPrecise.
    
    RefPtr<WebGLBuffer> elementArrayBuffer = m_boundVertexArrayObject->getElementArrayBuffer();
    
    if (!elementArrayBuffer)
        return false;
    
    GCGLsizeiptr numElements = elementArrayBuffer->byteLength();
    // The case count==0 is already dealt with in drawElements before validateIndexArrayConservative.
    if (!numElements)
        return false;
    auto buffer = elementArrayBuffer->elementArrayBuffer();
    ASSERT(buffer);
    
    Optional<unsigned> maxIndex = elementArrayBuffer->getCachedMaxIndex(type);
    if (!maxIndex) {
        // Compute the maximum index in the entire buffer for the given type of index.
        switch (type) {
        case GraphicsContextGL::UNSIGNED_BYTE:
            maxIndex = getMaxIndex<GCGLubyte>(buffer, 0, numElements);
            break;
        case GraphicsContextGL::UNSIGNED_SHORT:
            maxIndex = getMaxIndex<GCGLushort>(buffer, 0, numElements / sizeof(GCGLushort));
            break;
        case GraphicsContextGL::UNSIGNED_INT:
            maxIndex = getMaxIndex<GCGLuint>(buffer, 0, numElements / sizeof(GCGLuint));
            break;
        default:
            return false;
        }
        if (maxIndex)
            elementArrayBuffer->setCachedMaxIndex(type, maxIndex.value());
    }
    
    if (!maxIndex)
        return false;

    // The number of required elements is one more than the maximum
    // index that will be accessed.
    auto checkedNumElementsRequired = checkedAddAndMultiply<unsigned>(maxIndex.value(), 1, 1);
    if (!checkedNumElementsRequired)
        return false;
    numElementsRequired = checkedNumElementsRequired.value();

    return true;
}

bool WebGL2RenderingContext::validateBlendEquation(const char* functionName, GCGLenum mode)
{
    switch (mode) {
    case GraphicsContextGL::FUNC_ADD:
    case GraphicsContextGL::FUNC_SUBTRACT:
    case GraphicsContextGL::FUNC_REVERSE_SUBTRACT:
    case GraphicsContextGL::MIN:
    case GraphicsContextGL::MAX:
        return true;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid mode");
        return false;
    }
}

bool WebGL2RenderingContext::validateCapability(const char* functionName, GCGLenum cap)
{
    switch (cap) {
    case GraphicsContextGL::BLEND:
    case GraphicsContextGL::CULL_FACE:
    case GraphicsContextGL::DEPTH_TEST:
    case GraphicsContextGL::DITHER:
    case GraphicsContextGL::POLYGON_OFFSET_FILL:
    case GraphicsContextGL::SAMPLE_ALPHA_TO_COVERAGE:
    case GraphicsContextGL::SAMPLE_COVERAGE:
    case GraphicsContextGL::SCISSOR_TEST:
    case GraphicsContextGL::STENCIL_TEST:
    case GraphicsContextGL::RASTERIZER_DISCARD:
        return true;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid capability");
        return false;
    }
}

void WebGL2RenderingContext::compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLsizei imageSize, GCGLint64 offset)
{
    UNUSED_PARAM(target);
    UNUSED_PARAM(level);
    UNUSED_PARAM(internalformat);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(border);
    UNUSED_PARAM(imageSize);
    UNUSED_PARAM(offset);

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] compressedTexImage2D(PIXEL_UNPACK_BUFFER)");
}

void WebGL2RenderingContext::compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, ArrayBufferView& srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride)
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

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] compressedTexImage2D(ArrayBufferView)");
}

void WebGL2RenderingContext::compressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, GLintptr offset)
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

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] compressedTexSubImage2D(PIXEL_UNPACK_BUFFER)");
}

void WebGL2RenderingContext::compressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, ArrayBufferView& srcData, GLuint srcOffset, GLuint srcLengthOverride)
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

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] compressedTexSubImage2D(ArrayBufferView)");

}

void WebGL2RenderingContext::uniform1fv(WebGLUniformLocation* location, Float32List data, GLuint srcOffset, GLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform1fv()");
}

void WebGL2RenderingContext::uniform2fv(WebGLUniformLocation* location, Float32List data, GLuint srcOffset, GLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform2fv()");
}

void WebGL2RenderingContext::uniform3fv(WebGLUniformLocation* location, Float32List data, GLuint srcOffset, GLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform3fv()");
}

void WebGL2RenderingContext::uniform4fv(WebGLUniformLocation* location, Float32List data, GLuint srcOffset, GLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform4fv()");
}

void WebGL2RenderingContext::uniform1iv(WebGLUniformLocation* location, Int32List data, GLuint srcOffset, GLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform1iv()");
}

void WebGL2RenderingContext::uniform2iv(WebGLUniformLocation* location, Int32List data, GLuint srcOffset, GLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform2iv()");
}

void WebGL2RenderingContext::uniform3iv(WebGLUniformLocation* location, Int32List data, GLuint srcOffset, GLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform3iv()");
}

void WebGL2RenderingContext::uniform4iv(WebGLUniformLocation* location, Int32List data, GLuint srcOffset, GLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniform4iv()");
}

void WebGL2RenderingContext::uniformMatrix2fv(WebGLUniformLocation* location, GLboolean transpose, Float32List data, GLuint srcOffset, GLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniformMatrix2fv()");
}

void WebGL2RenderingContext::uniformMatrix3fv(WebGLUniformLocation* location, GLboolean transpose, Float32List data, GLuint srcOffset, GLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniformMatrix3fv()");
}

void WebGL2RenderingContext::uniformMatrix4fv(WebGLUniformLocation* location, GLboolean transpose, Float32List data, GLuint srcOffset, GLuint srcLength)
{
    UNUSED_PARAM(location);
    UNUSED_PARAM(transpose);
    UNUSED_PARAM(data);
    UNUSED_PARAM(srcOffset);
    UNUSED_PARAM(srcLength);

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniformMatrix4fv()");
}

void WebGL2RenderingContext::readPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLintptr offset)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(offset);

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] readPixels()");
}

void WebGL2RenderingContext::readPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, ArrayBufferView& dstData, GLuint dstOffset)
{
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(format);
    UNUSED_PARAM(type);
    UNUSED_PARAM(dstData);
    UNUSED_PARAM(dstOffset);

    LOG(WebGL, "[[ NOT IMPLEMENTED ]] readPixels(ArrayBufferView)");
}

void WebGL2RenderingContext::uncacheDeletedBuffer(WebGLBuffer* buffer)
{
    ASSERT(buffer);

    WebGLRenderingContextBase::uncacheDeletedBuffer(buffer);

    size_t index = m_boundTransformFeedbackBuffers.find(buffer);
    if (index < m_boundTransformFeedbackBuffers.size())
        m_boundTransformFeedbackBuffers[index] = nullptr;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
