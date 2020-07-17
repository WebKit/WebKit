/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
#include "EXTColorBufferFloat.h"
#include "EXTTextureFilterAnisotropic.h"
#include "EventLoop.h"
#include "ExtensionsGL.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBitmap.h"
#include "ImageData.h"
#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "OESTextureFloat.h"
#include "OESTextureFloatLinear.h"
#include "OESTextureHalfFloat.h"
#include "OESTextureHalfFloatLinear.h"
#include "RenderBox.h"
#include "RuntimeEnabledFeatures.h"
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

const GCGLuint64 MaxClientWaitTimeout = 0u;

WTF_MAKE_ISO_ALLOCATED_IMPL(WebGL2RenderingContext);

std::unique_ptr<WebGL2RenderingContext> WebGL2RenderingContext::create(CanvasBase& canvas, GraphicsContextGLAttributes attributes)
{
    auto renderingContext = std::unique_ptr<WebGL2RenderingContext>(new WebGL2RenderingContext(canvas, attributes));
    // This context is pending policy resolution, so don't call initializeNewContext on it yet.

    InspectorInstrumentation::didCreateCanvasRenderingContext(*renderingContext);

    return renderingContext;
}

std::unique_ptr<WebGL2RenderingContext> WebGL2RenderingContext::create(CanvasBase& canvas, Ref<GraphicsContextGLOpenGL>&& context, GraphicsContextGLAttributes attributes)
{
    auto renderingContext = std::unique_ptr<WebGL2RenderingContext>(new WebGL2RenderingContext(canvas, WTFMove(context), attributes));
    // This is virtual and can't be called in the constructor.
    renderingContext->initializeNewContext();

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
    if (isContextLost())
        return;
}

WebGL2RenderingContext::~WebGL2RenderingContext()
{
    // Remove all references to WebGLObjects so if they are the last reference
    // they will be freed before the last context is removed from the context group.
    m_readFramebufferBinding = nullptr;
    m_boundTransformFeedback = nullptr;
    m_boundTransformFeedbackBuffer = nullptr;
    m_boundUniformBuffer = nullptr;
    m_boundIndexedUniformBuffers.clear();
    m_activeQueries.clear();
}

void WebGL2RenderingContext::initializeNewContext()
{
    // FIXME: NEEDS_PORT
    ASSERT(!isContextLost());

    m_readFramebufferBinding = nullptr;

    m_boundCopyReadBuffer = nullptr;
    m_boundCopyWriteBuffer = nullptr;
    m_boundPixelPackBuffer = nullptr;
    m_boundPixelUnpackBuffer = nullptr;
    m_boundTransformFeedbackBuffer = nullptr;
    m_boundUniformBuffer = nullptr;

    // NEEDS_PORT: boolean occlusion query, transform feedback primitives written query, elapsed query

    m_maxTransformFeedbackSeparateAttribs = getUnsignedIntParameter(GraphicsContextGL::MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS);
    ASSERT(m_maxTransformFeedbackSeparateAttribs >= 4);

    m_defaultTransformFeedback = createTransformFeedback();
    m_context->bindTransformFeedback(GraphicsContextGL::TRANSFORM_FEEDBACK, m_defaultTransformFeedback->object());
    m_boundTransformFeedback = m_defaultTransformFeedback;

    m_boundIndexedUniformBuffers.resize(getIntParameter(GraphicsContextGL::MAX_UNIFORM_BUFFER_BINDINGS));
    m_uniformBufferOffsetAlignment = getIntParameter(GraphicsContextGL::UNIFORM_BUFFER_OFFSET_ALIGNMENT);

    // NEEDS_PORT: set up pack parameters: row length, skip pixels, skip rows

    m_unpackRowLength = 0;
    m_unpackImageHeight = 0;
    m_unpackSkipPixels = 0;
    m_unpackSkipRows = 0;
    m_unpackSkipImages = 0;

    WebGLRenderingContextBase::initializeNewContext();

    // These rely on initialization done in the superclass.
    ASSERT(m_textureUnits.size() >= 8);
    m_boundSamplers.clear();
    m_boundSamplers.resize(m_textureUnits.size());

    // FIXME: this should likely be removed.
    initializeShaderExtensions();
}

void WebGL2RenderingContext::resetUnpackParameters()
{
    WebGLRenderingContextBase::resetUnpackParameters();

    if (m_unpackRowLength)
        m_context->pixelStorei(GraphicsContextGL::UNPACK_ROW_LENGTH, 0);
    if (m_unpackImageHeight)
        m_context->pixelStorei(GraphicsContextGL::UNPACK_IMAGE_HEIGHT, 0);
    if (m_unpackSkipPixels)
        m_context->pixelStorei(GraphicsContextGL::UNPACK_SKIP_PIXELS, 0);
    if (m_unpackSkipRows)
        m_context->pixelStorei(GraphicsContextGL::UNPACK_SKIP_ROWS, 0);
    if (m_unpackSkipImages)
        m_context->pixelStorei(GraphicsContextGL::UNPACK_SKIP_IMAGES, 0);
}

void WebGL2RenderingContext::restoreUnpackParameters()
{
    WebGLRenderingContextBase::restoreUnpackParameters();

    if (m_unpackRowLength)
        m_context->pixelStorei(GraphicsContextGL::UNPACK_ROW_LENGTH, m_unpackRowLength);
    if (m_unpackImageHeight)
        m_context->pixelStorei(GraphicsContextGL::UNPACK_IMAGE_HEIGHT, m_unpackImageHeight);
    if (m_unpackSkipPixels)
        m_context->pixelStorei(GraphicsContextGL::UNPACK_SKIP_PIXELS, m_unpackSkipPixels);
    if (m_unpackSkipRows)
        m_context->pixelStorei(GraphicsContextGL::UNPACK_SKIP_ROWS, m_unpackSkipRows);
    if (m_unpackSkipImages)
        m_context->pixelStorei(GraphicsContextGL::UNPACK_SKIP_IMAGES, m_unpackSkipImages);
}

long long WebGL2RenderingContext::getInt64Parameter(GCGLenum pname)
{
    GCGLint64 value = 0;
    m_context->getInteger64v(pname, &value);
    return value;
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
#if !USE(ANGLE)
    if (!isGLES2Compliant())
        initVertexAttrib0();
#endif
}

void WebGL2RenderingContext::initializeShaderExtensions()
{
    // FIXME: these are in the WebGL 2.0 core API and should be removed.
    m_context->getExtensions().ensureEnabled("GL_OES_standard_derivatives");
    m_context->getExtensions().ensureEnabled("GL_EXT_draw_buffers");
    m_context->getExtensions().ensureEnabled("GL_EXT_shader_texture_lod");
    m_context->getExtensions().ensureEnabled("GL_EXT_frag_depth");
}

bool WebGL2RenderingContext::validateBufferTarget(const char* functionName, GCGLenum target)
{
    switch (target) {
    case GraphicsContextGL::ARRAY_BUFFER:
    case GraphicsContextGL::COPY_READ_BUFFER:
    case GraphicsContextGL::COPY_WRITE_BUFFER:
    case GraphicsContextGL::ELEMENT_ARRAY_BUFFER:
    case GraphicsContextGL::PIXEL_PACK_BUFFER:
    case GraphicsContextGL::PIXEL_UNPACK_BUFFER:
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER:
    case GraphicsContextGL::UNIFORM_BUFFER:
        return true;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
        return false;
    }
}

bool WebGL2RenderingContext::validateBufferTargetCompatibility(const char* functionName, GCGLenum target, WebGLBuffer* buffer)
{
    ASSERT(buffer);

    switch (buffer->getTarget()) {
    case GraphicsContextGL::ELEMENT_ARRAY_BUFFER:
        switch (target) {
        case GraphicsContextGL::ARRAY_BUFFER:
        case GraphicsContextGL::PIXEL_PACK_BUFFER:
        case GraphicsContextGL::PIXEL_UNPACK_BUFFER:
        case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER:
        case GraphicsContextGL::UNIFORM_BUFFER:
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "element array buffers can not be bound to a different target");
            return false;
        default:
            break;
        }
        break;
    case GraphicsContextGL::ARRAY_BUFFER:
    case GraphicsContextGL::COPY_READ_BUFFER:
    case GraphicsContextGL::COPY_WRITE_BUFFER:
    case GraphicsContextGL::PIXEL_PACK_BUFFER:
    case GraphicsContextGL::PIXEL_UNPACK_BUFFER:
    case GraphicsContextGL::UNIFORM_BUFFER:
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER:
        if (target == GraphicsContextGL::ELEMENT_ARRAY_BUFFER) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "buffers bound to non ELEMENT_ARRAY_BUFFER targets can not be bound to ELEMENT_ARRAY_BUFFER target");
            return false;
        }
        break;
    default:
        break;
    }

    return true;
}

WebGLBuffer* WebGL2RenderingContext::validateBufferDataParameters(const char* functionName, GCGLenum target, GCGLenum usage)
{
    WebGLBuffer* buffer = validateBufferDataTarget(functionName, target);
    if (!buffer)
        return nullptr;
    switch (usage) {
    case GraphicsContextGL::STREAM_DRAW:
    case GraphicsContextGL::STATIC_DRAW:
    case GraphicsContextGL::DYNAMIC_DRAW:
        return buffer;
    case GraphicsContextGL::STREAM_COPY:
    case GraphicsContextGL::STATIC_COPY:
    case GraphicsContextGL::DYNAMIC_COPY:
    case GraphicsContextGL::STREAM_READ:
    case GraphicsContextGL::STATIC_READ:
    case GraphicsContextGL::DYNAMIC_READ:
        return buffer;
    }
    synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid usage");
    return nullptr;
}

WebGLBuffer* WebGL2RenderingContext::validateBufferDataTarget(const char* functionName, GCGLenum target)
{
    WebGLBuffer* buffer = nullptr;
    switch (target) {
    case GraphicsContextGL::ELEMENT_ARRAY_BUFFER:
        buffer = m_boundVertexArrayObject->getElementArrayBuffer();
        break;
    case GraphicsContextGL::ARRAY_BUFFER:
        buffer = m_boundArrayBuffer.get();
        break;
    case GraphicsContextGL::COPY_READ_BUFFER:
        buffer = m_boundCopyReadBuffer.get();
        break;
    case GraphicsContextGL::COPY_WRITE_BUFFER:
        buffer = m_boundCopyWriteBuffer.get();
        break;
    case GraphicsContextGL::PIXEL_PACK_BUFFER:
        buffer = m_boundPixelPackBuffer.get();
        break;
    case GraphicsContextGL::PIXEL_UNPACK_BUFFER:
        buffer = m_boundPixelUnpackBuffer.get();
        break;
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER:
        buffer = m_boundTransformFeedbackBuffer.get();
        break;
    case GraphicsContextGL::UNIFORM_BUFFER:
        buffer = m_boundUniformBuffer.get();
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
        return nullptr;
    }
    if (!buffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no buffer");
        return nullptr;
    }
    return buffer;
}

bool WebGL2RenderingContext::validateAndCacheBufferBinding(const char* functionName, GCGLenum target, WebGLBuffer* buffer)
{
    if (!validateBufferTarget(functionName, target))
        return false;

    if (buffer && !validateBufferTargetCompatibility(functionName, target, buffer))
        return false;

    switch (target) {
    case GraphicsContextGL::ARRAY_BUFFER:
        m_boundArrayBuffer = buffer;
        break;
    case GraphicsContextGL::COPY_READ_BUFFER:
        m_boundCopyReadBuffer = buffer;
        break;
    case GraphicsContextGL::COPY_WRITE_BUFFER:
        m_boundCopyWriteBuffer = buffer;
        break;
    case GraphicsContextGL::ELEMENT_ARRAY_BUFFER:
        m_boundVertexArrayObject->setElementArrayBuffer(buffer);
        break;
    case GraphicsContextGL::PIXEL_PACK_BUFFER:
        m_boundPixelPackBuffer = buffer;
        break;
    case GraphicsContextGL::PIXEL_UNPACK_BUFFER:
        m_boundPixelUnpackBuffer = buffer;
        break;
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER:
        m_boundTransformFeedbackBuffer = buffer;
        break;
    case GraphicsContextGL::UNIFORM_BUFFER:
        m_boundUniformBuffer = buffer;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    if (buffer && !buffer->getTarget())
        buffer->setTarget(target);
    return true;
}

IntRect WebGL2RenderingContext::getTextureSourceSubRectangle(GCGLsizei width, GCGLsizei height)
{
    return IntRect(m_unpackSkipPixels, m_unpackSkipRows, width, height);
}

RefPtr<WebGLTexture> WebGL2RenderingContext::validateTexImageBinding(const char* functionName, TexImageFunctionID functionID, GCGLenum target)
{
    if (functionID == TexImageFunctionID::TexImage3D || functionID == TexImageFunctionID::TexSubImage3D)
        return validateTexture3DBinding(functionName, target);
    return validateTexture2DBinding(functionName, target);
}

RefPtr<WebGLTexture> WebGL2RenderingContext::validateTexture3DBinding(const char* functionName, GCGLenum target)
{
    RefPtr<WebGLTexture> texture;
    switch (target) {
    case GraphicsContextGL::TEXTURE_2D_ARRAY:
        texture = m_textureUnits[m_activeTextureUnit].texture2DArrayBinding;
        break;
    case GraphicsContextGL::TEXTURE_3D:
        texture = m_textureUnits[m_activeTextureUnit].texture3DBinding;
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture target");
        return nullptr;
    }
    if (!texture)
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no texture bound to target");
    return texture;
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

void WebGL2RenderingContext::pixelStorei(GCGLenum pname, GCGLint param)
{
    if (isContextLostOrPending())
        return;
    if (param < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "pixelStorei", "negative value");
        return;
    }
    // FIXME: NEEDS_PORT
    // Hook up WebGL 2.0 pack parameters.
    switch (pname) {
    case GraphicsContextGL::UNPACK_ROW_LENGTH:
        m_unpackRowLength = param;
        break;
    case GraphicsContextGL::UNPACK_IMAGE_HEIGHT:
        m_unpackImageHeight = param;
        break;
    case GraphicsContextGL::UNPACK_SKIP_PIXELS:
        m_unpackSkipPixels = param;
        break;
    case GraphicsContextGL::UNPACK_SKIP_ROWS:
        m_unpackSkipRows = param;
        break;
    case GraphicsContextGL::UNPACK_SKIP_IMAGES:
        m_unpackSkipImages = param;
        break;
    default:
        WebGLRenderingContextBase::pixelStorei(pname, param);
        return;
    }
    m_context->pixelStorei(pname, param);
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

    RefPtr<WebGLBuffer> readBuffer = validateBufferDataParameters("copyBufferSubData", readTarget, GraphicsContextGL::STATIC_DRAW);
    RefPtr<WebGLBuffer> writeBuffer = validateBufferDataParameters("copyBufferSubData", writeTarget, GraphicsContextGL::STATIC_DRAW);
    if (!readBuffer || !writeBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "copyBufferSubData", "Invalid readTarget or writeTarget");
        return;
    }

    if (readOffset < 0 || writeOffset < 0 || size < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "copyBufferSubData", "offset < 0");
        return;
    }

    if ((readBuffer->getTarget() == GraphicsContextGL::ELEMENT_ARRAY_BUFFER && writeBuffer->getTarget() != GraphicsContextGL::ELEMENT_ARRAY_BUFFER)
        || (writeBuffer->getTarget() == GraphicsContextGL::ELEMENT_ARRAY_BUFFER && readBuffer->getTarget() != GraphicsContextGL::ELEMENT_ARRAY_BUFFER)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "copyBufferSubData", "Cannot copy into an element buffer destination from a non-element buffer source");
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

void WebGL2RenderingContext::bindFramebuffer(GCGLenum target, WebGLFramebuffer* buffer)
{
    if (!checkObjectToBeBound("bindFramebuffer", buffer))
        return;

    switch (target) {
    case GraphicsContextGL::DRAW_FRAMEBUFFER:
        break;
    case GraphicsContextGL::FRAMEBUFFER:
    case GraphicsContextGL::READ_FRAMEBUFFER:
        m_readFramebufferBinding = buffer;
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "bindFramebuffer", "invalid target");
        return;
    }

    setFramebuffer(target, buffer);
}

void WebGL2RenderingContext::blitFramebuffer(GCGLint srcX0, GCGLint srcY0, GCGLint srcX1, GCGLint srcY1, GCGLint dstX0, GCGLint dstY0, GCGLint dstX1, GCGLint dstY1, GCGLbitfield mask, GCGLenum filter)
{
    if (isContextLostOrPending())
        return;
    m_context->blitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

void WebGL2RenderingContext::deleteFramebuffer(WebGLFramebuffer* framebuffer)
{
    if (!deleteObject(framebuffer))
        return;

    GCGLenum target = 0;
    if (framebuffer == m_framebufferBinding) {
        if (framebuffer == m_readFramebufferBinding) {
            target = GraphicsContextGL::FRAMEBUFFER;
            m_framebufferBinding = nullptr;
            m_readFramebufferBinding = nullptr;
        } else {
            target = GraphicsContextGL::DRAW_FRAMEBUFFER;
            m_framebufferBinding = nullptr;
        }
        m_context->bindFramebuffer(GraphicsContextGL::READ_FRAMEBUFFER, 0);
    } else if (framebuffer == m_readFramebufferBinding) {
        target = GraphicsContextGL::READ_FRAMEBUFFER;
        m_readFramebufferBinding = nullptr;
    }
    if (target)
        m_context->bindFramebuffer(target, 0);
}

void WebGL2RenderingContext::framebufferTextureLayer(GCGLenum, GCGLenum, WebGLTexture*, GCGLint, GCGLint)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] framebufferTextureLayer()");
}

#if !USE(OPENGL_ES) && !USE(ANGLE)
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
#if USE(OPENGL_ES) || USE(ANGLE)
    m_context->moveErrorsToSyntheticErrorList();
    m_context->getInternalformativ(target, internalformat, GraphicsContextGL::NUM_SAMPLE_COUNTS, 1, &numValues);
    if (m_context->moveErrorsToSyntheticErrorList()) {
        // The getInternalformativ call failed; return null from the NUM_SAMPLE_COUNTS query.
        return nullptr;
    }

    Vector<GCGLint> params(numValues);
    m_context->getInternalformativ(target, internalformat, pname, numValues, params.data());
    if (m_context->moveErrorsToSyntheticErrorList()) {
        // The getInternalformativ call failed; return null from the NUM_SAMPLE_COUNTS query.
        return nullptr;
    }
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
    Vector<GCGLint> params(numValues);
    for (size_t i = 0; i < static_cast<size_t>(numValues); ++i)
        params[i] = samples[i];
#endif

    return Int32Array::tryCreate(params.data(), numValues);
}

void WebGL2RenderingContext::invalidateFramebuffer(GCGLenum, const Vector<GCGLenum>&)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] invalidateFramebuffer()");
}

void WebGL2RenderingContext::invalidateSubFramebuffer(GCGLenum, const Vector<GCGLenum>&, GCGLint, GCGLint, GCGLsizei, GCGLsizei)
{
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] invalidateSubFramebuffer()");
}

void WebGL2RenderingContext::readBuffer(GCGLenum src)
{
    if (src == GraphicsContextGL::BACK) {
        // Because the backbuffer is simulated on all current WebKit ports, we need to change BACK to COLOR_ATTACHMENT0.
        if (m_readFramebufferBinding) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "readBuffer", "BACK is valid for default framebuffer only");
            return;
        }
        src = GraphicsContextGL::COLOR_ATTACHMENT0;
    } else if (!m_readFramebufferBinding && src != GraphicsContextGL::NONE) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "readBuffer", "default framebuffer only supports NONE or BACK");
        return;
    }

    m_context->readBuffer(src);
}

void WebGL2RenderingContext::renderbufferStorageMultisample(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
#if USE(ANGLE)
    m_context->renderbufferStorageMultisample(target, samples, internalformat, width, height);
    return;
#endif
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

#if !USE(ANGLE)
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
#endif

WebGLAny WebGL2RenderingContext::getTexParameter(GCGLenum target, GCGLenum pname)
{
    if (isContextLostOrPending() || !validateTextureBinding("getTexParameter", target))
        return nullptr;
    switch (pname) {
    case GraphicsContextGL::TEXTURE_BASE_LEVEL:
        FALLTHROUGH;
    case GraphicsContextGL::TEXTURE_MAX_LEVEL:
        FALLTHROUGH;
    case GraphicsContextGL::TEXTURE_COMPARE_FUNC:
        FALLTHROUGH;
    case GraphicsContextGL::TEXTURE_COMPARE_MODE:
        FALLTHROUGH;
    case GraphicsContextGL::TEXTURE_IMMUTABLE_LEVELS:
        FALLTHROUGH;
    case GraphicsContextGL::TEXTURE_WRAP_R: {
        GCGLint value = 0;
        m_context->getTexParameteriv(target, pname, &value);
        return value;
    }
    case GraphicsContextGL::TEXTURE_IMMUTABLE_FORMAT: {
        GCGLint value = 0;
        m_context->getTexParameteriv(target, pname, &value);
        return static_cast<bool>(value);
    }
    case GraphicsContextGL::TEXTURE_MIN_LOD:
        FALLTHROUGH;
    case GraphicsContextGL::TEXTURE_MAX_LOD: {
        GCGLfloat value = 0;
        m_context->getTexParameterfv(target, pname, &value);
        return value;
    }
    default:
        return WebGLRenderingContextBase::getTexParameter(target, pname);
    }
}

void WebGL2RenderingContext::texStorage2D(GCGLenum target, GCGLsizei levels, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height)
{
    if (isContextLostOrPending())
        return;

    auto texture = validateTextureBinding("texStorage2D", target);
    if (!texture)
        return;

    m_context->texStorage2D(target, levels, internalFormat, width, height);
}

void WebGL2RenderingContext::texStorage3D(GCGLenum target, GCGLsizei levels, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLsizei depth)
{
    if (isContextLostOrPending())
        return;

    auto texture = validateTextureBinding("texStorage3D", target);
    if (!texture)
        return;

    m_context->texStorage3D(target, levels, internalFormat, width, height, depth);
}

void WebGL2RenderingContext::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& data)
{
    if (isContextLostOrPending())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D", "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return;
    }
    WebGLRenderingContextBase::texImage2D(target, level, internalformat, width, height, border, format, type, WTFMove(data));
}

ExceptionOr<void> WebGL2RenderingContext::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLenum format, GCGLenum type, Optional<TexImageSource> data)
{
    if (isContextLostOrPending())
        return { };
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D", "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return { };
    }
    return WebGLRenderingContextBase::texImage2D(target, level, internalformat, format, type, data);
}

void WebGL2RenderingContext::texImage2D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    if (isContextLostOrPending())
        return;
    if (!validateTexture2DBinding("texImage2D", target))
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D", "no bound PIXEL_UNPACK_BUFFER");
        return;
    }
    if (m_unpackFlipY || m_unpackPremultiplyAlpha) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D", "FLIP_Y or PREMULTIPLY_ALPHA isn't allowed while uploading from PBO");
        return;
    }
    if (!validateTexFunc("texImage2D", TexImageFunctionType::TexImage, SourceUnpackBuffer, target, level, internalformat, width, height, 1, border, format, type, 0, 0, 0))
        return;

    m_context->texImage2D(target, level, internalformat, width, height, border, format, type, reinterpret_cast<void*>(offset));
}

ExceptionOr<void> WebGL2RenderingContext::texImage2D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, TexImageSource&& source)
{
    if (isContextLostOrPending())
        return { };
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D", "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return { };
    }

    return WebGLRenderingContextBase::texImageSourceHelper(TexImageFunctionID::TexImage2D, target, level, internalformat, border, format, type, 0, 0, 0, getTextureSourceSubRectangle(width, height), 1, 0, WTFMove(source));
}

void WebGL2RenderingContext::texImage2D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData, GCGLuint srcOffset)
{
    if (isContextLostOrPending())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D", "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return;
    }

    texImageArrayBufferViewHelper(TexImageFunctionID::TexImage2D, target, level, internalformat, width, height, 1, border, format, type, 0, 0, 0, WTFMove(srcData), NullNotReachable, srcOffset);
}

void WebGL2RenderingContext::texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLint64 offset)
{
    if (isContextLostOrPending())
        return;
    if (!validateTexture3DBinding("texImage3D", target))
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage3D", "no bound PIXEL_UNPACK_BUFFER");
        return;
    }
    if (m_unpackFlipY || m_unpackPremultiplyAlpha) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage3D", "FLIP_Y or PREMULTIPLY_ALPHA isn't allowed for uploading 3D textures");
        return;
    }
    if (!validateTexFunc("texImage3D", TexImageFunctionType::TexImage, SourceUnpackBuffer, target, level, internalformat, width, height, depth, border, format, type, 0, 0, 0))
        return;

    m_context->texImage3D(target, level, internalformat, width, height, depth, border, format, type, reinterpret_cast<void*>(offset));
}

ExceptionOr<void> WebGL2RenderingContext::texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, TexImageSource&& source)
{
    if (isContextLostOrPending())
        return { };
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage3D", "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return { };
    }

    return WebGLRenderingContextBase::texImageSourceHelper(TexImageFunctionID::TexImage3D, target, level, internalformat, border, format, type, 0, 0, 0, getTextureSourceSubRectangle(width, height), depth, m_unpackImageHeight, WTFMove(source));
}

void WebGL2RenderingContext::texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData)
{
    if (isContextLostOrPending())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage3D", "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return;
    }
    if ((m_unpackFlipY || m_unpackPremultiplyAlpha) && srcData) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage3D", "FLIP_Y or PREMULTIPLY_ALPHA isn't allowed for uploading 3D textures");
        return;
    }

    texImageArrayBufferViewHelper(TexImageFunctionID::TexImage3D, target, level, internalformat, width, height, depth, border, format, type, 0, 0, 0, WTFMove(srcData), NullAllowed, 0);
}

void WebGL2RenderingContext::texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData, GCGLuint srcOffset)
{
    if (isContextLostOrPending())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage3D", "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return;
    }
    if (m_unpackFlipY || m_unpackPremultiplyAlpha) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage3D", "FLIP_Y or PREMULTIPLY_ALPHA isn't allowed for uploading 3D textures");
        return;
    }

    texImageArrayBufferViewHelper(TexImageFunctionID::TexImage3D, target, level, internalformat, width, height, depth, border, format, type, 0, 0, 0, WTFMove(srcData), NullNotReachable, srcOffset);
}

void WebGL2RenderingContext::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData)
{
    if (isContextLostOrPending())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage2D", "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return;
    }
    WebGLRenderingContextBase::texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, WTFMove(srcData));
}

ExceptionOr<void> WebGL2RenderingContext::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLenum format, GCGLenum type, Optional<TexImageSource>&& data)
{
    if (isContextLostOrPending())
        return { };
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage2D", "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return { };
    }
    return WebGLRenderingContextBase::texSubImage2D(target, level, xoffset, yoffset, format, type, WTFMove(data));
}

void WebGL2RenderingContext::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format , GCGLenum type, GCGLintptr offset)
{
    if (isContextLostOrPending())
        return;
    if (!validateTexture2DBinding("texSubImage2D", target))
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage2D", "no bound PIXEL_UNPACK_BUFFER");
        return;
    }
    if (m_unpackFlipY || m_unpackPremultiplyAlpha) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage2D", "FLIP_Y or PREMULTIPLY_ALPHA isn't allowed while uploading from PBO");
        return;
    }
    if (!validateTexFunc("texSubImage2D", TexImageFunctionType::TexSubImage, SourceUnpackBuffer, target, level, 0, width, height, 1, 0, format, type, xoffset, yoffset, 0))
        return;

    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, reinterpret_cast<void*>(offset));
}

ExceptionOr<void> WebGL2RenderingContext::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, TexImageSource&& source)
{
    if (isContextLostOrPending())
        return { };
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D", "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return { };
    }

    return WebGLRenderingContextBase::texImageSourceHelper(TexImageFunctionID::TexSubImage2D, target, level, 0, 0, format, type, xoffset, yoffset, 0, getTextureSourceSubRectangle(width, height), 1, 0, WTFMove(source));
}

void WebGL2RenderingContext::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData, GCGLuint srcOffset)
{
    if (isContextLostOrPending())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D", "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return;
    }

    texImageArrayBufferViewHelper(TexImageFunctionID::TexSubImage2D, target, level, 0, width, height, 1, 0, format, type, xoffset, yoffset, 0, WTFMove(srcData), NullNotReachable, srcOffset);
}

void WebGL2RenderingContext::texSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, GCGLint64 offset)
{
    if (isContextLostOrPending())
        return;
    if (!validateTexture3DBinding("texSubImage3D", target))
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage3D", "no bound PIXEL_UNPACK_BUFFER");
        return;
    }
    if (m_unpackFlipY || m_unpackPremultiplyAlpha) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage3D", "FLIP_Y or PREMULTIPLY_ALPHA isn't allowed for uploading 3D textures");
        return;
    }
    if (!validateTexFunc("texSubImage3D", TexImageFunctionType::TexSubImage, SourceUnpackBuffer, target, level, 0, width, height, depth, 0, format, type, xoffset, yoffset, zoffset))
        return;

    m_context->texSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, offset);
}

void WebGL2RenderingContext::texSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData, GCGLuint srcOffset)
{
    if (isContextLostOrPending())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage3D", "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return;
    }
    if (m_unpackFlipY || m_unpackPremultiplyAlpha) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage3D", "FLIP_Y or PREMULTIPLY_ALPHA isn't allowed for uploading 3D textures");
        return;
    }

    texImageArrayBufferViewHelper(TexImageFunctionID::TexSubImage3D, target, level, 0, width, height, depth, 0, format, type, xoffset, yoffset, zoffset, WTFMove(srcData), NullNotReachable, srcOffset);
}

ExceptionOr<void> WebGL2RenderingContext::texSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, TexImageSource&& source)
{
    if (isContextLostOrPending())
        return { };
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage3D", "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return { };
    }

    return WebGLRenderingContextBase::texImageSourceHelper(TexImageFunctionID::TexSubImage3D, target, level, 0, 0, format, type, xoffset, yoffset, zoffset, getTextureSourceSubRectangle(width, height), depth, m_unpackImageHeight, WTFMove(source));
}

void WebGL2RenderingContext::copyTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (isContextLostOrPending())
        return;
    if (!validateTexture3DBinding("copyTexSubImage3D", target))
        return;
    clearIfComposited();
    m_context->copyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void WebGL2RenderingContext::compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, ArrayBufferView& srcData)
{
    if (isContextLostOrPending())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexImage2D",
            "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return;
    }
    WebGLRenderingContextBase::compressedTexImage2D(target, level, internalformat, width, height, border, srcData);
}

void WebGL2RenderingContext::compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLsizei imageSize, GCGLint64 offset)
{
    if (isContextLostOrPending())
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexImage2D",
            "no bound PIXEL_UNPACK_BUFFER");
        return;
    }
    if (!validateTexture2DBinding("compressedTexImage2D", target))
        return;
    m_context->getExtensions().compressedTexImage2DRobustANGLE(
        target, level, internalformat, width,
        height, border, imageSize,
        0, reinterpret_cast<uint8_t*>(offset));
}

void WebGL2RenderingContext::compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, ArrayBufferView& srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride)
{
    if (isContextLostOrPending())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexImage2D",
            "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return;
    }
    if (!validateTexture2DBinding("compressedTexImage2D", target))
        return;
    auto slice = sliceArrayBufferView("compressedTexImage2D", srcData, srcOffset, srcLengthOverride);
    if (!slice)
        return;
    m_context->getExtensions().compressedTexImage2DRobustANGLE(
        target, level, internalformat, width, height,
        border, slice->byteLength(),
        slice->byteLength(), slice->baseAddress());
}

void WebGL2RenderingContext::compressedTexImage3D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLsizei imageSize, GCGLint64 offset)
{
    if (isContextLostOrPending())
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexImage3D",
            "no bound PIXEL_UNPACK_BUFFER");
        return;
    }
    if (!validateTexture3DBinding("compressedTexImage3D", target))
        return;
    m_context->getExtensions().compressedTexImage3DRobustANGLE(
        target, level, internalformat, width,
        height, depth, border, imageSize,
        0, reinterpret_cast<uint8_t*>(offset));
}

void WebGL2RenderingContext::compressedTexImage3D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, ArrayBufferView& srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride)
{
    if (isContextLostOrPending())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexImage3D",
            "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return;
    }
    if (!validateTexture3DBinding("compressedTexImage3D", target))
        return;
    auto slice = sliceArrayBufferView("compressedTexImage3D", srcData, srcOffset, srcLengthOverride);
    if (!slice)
        return;
    m_context->getExtensions().compressedTexImage3DRobustANGLE(
        target, level, internalformat, width, height,
        depth, border, slice->byteLength(),
        slice->byteLength(), slice->baseAddress());
}

void WebGL2RenderingContext::compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, ArrayBufferView& srcData)
{
    if (isContextLostOrPending())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexSubImage2D",
            "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return;
    }
    WebGLRenderingContextBase::compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, srcData);
}

void WebGL2RenderingContext::compressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, GLintptr offset)
{
    if (isContextLostOrPending())
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexSubImage2D",
            "no bound PIXEL_UNPACK_BUFFER");
        return;
    }
    if (!validateTexture2DBinding("compressedTexImage2D", target))
        return;
    m_context->getExtensions().compressedTexSubImage2DRobustANGLE(
        target, level, xoffset, yoffset,
        width, height, format, imageSize,
        0, reinterpret_cast<uint8_t*>(offset));
}

void WebGL2RenderingContext::compressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, ArrayBufferView& srcData, GLuint srcOffset, GLuint srcLengthOverride)
{
    if (isContextLostOrPending())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexSubImage2D",
            "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return;
    }
    if (!validateTexture2DBinding("compressedTexSubImage2D", target))
        return;
    auto slice = sliceArrayBufferView("compressedTexSubImage2D", srcData, srcOffset, srcLengthOverride);
    if (!slice)
        return;
    m_context->getExtensions().compressedTexSubImage2DRobustANGLE(
        target, level, xoffset, yoffset,
        width, height, format, slice->byteLength(),
        slice->byteLength(), slice->baseAddress());
}

void WebGL2RenderingContext::compressedTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLsizei imageSize, GCGLint64 offset)
{
    if (isContextLostOrPending())
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexSubImage3D",
            "no bound PIXEL_UNPACK_BUFFER");
        return;
    }
    if (!validateTexture3DBinding("compressedTexSubImage3D", target))
        return;
    m_context->getExtensions().compressedTexSubImage3DRobustANGLE(
        target, level, xoffset, yoffset, zoffset,
        width, height, depth, format, imageSize,
        0, reinterpret_cast<uint8_t*>(offset));
}

void WebGL2RenderingContext::compressedTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, ArrayBufferView& srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride)
{
    if (isContextLostOrPending())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexSubImage3D",
            "a buffer is bound to PIXEL_UNPACK_BUFFER");
        return;
    }
    if (!validateTexture3DBinding("compressedTexSubImage3D", target))
        return;
    auto slice = sliceArrayBufferView("compressedTexSubImage3D", srcData, srcOffset, srcLengthOverride);
    if (!slice)
        return;
    m_context->getExtensions().compressedTexSubImage3DRobustANGLE(
        target, level, xoffset, yoffset, zoffset,
        width, height, depth, format, slice->byteLength(),
        slice->byteLength(), slice->baseAddress());
}

GCGLint WebGL2RenderingContext::getFragDataLocation(WebGLProgram& program, const String& name)
{
    if (isContextLostOrPending() || !validateWebGLObject("getFragDataLocation", &program))
        return -1;
    return m_context->getFragDataLocation(program.object(), name);
}

void WebGL2RenderingContext::uniform1ui(WebGLUniformLocation* location, GCGLuint v0)
{
    if (isContextLostOrPending() || !validateUniformLocation("uniform1ui", location))
        return;
    m_context->uniform1ui(location->location(), v0);
}

void WebGL2RenderingContext::uniform2ui(WebGLUniformLocation* location, GCGLuint v0, GCGLuint v1)
{
    if (isContextLostOrPending() || !validateUniformLocation("uniform2ui", location))
        return;
    m_context->uniform2ui(location->location(), v0, v1);
}

void WebGL2RenderingContext::uniform3ui(WebGLUniformLocation* location, GCGLuint v0, GCGLuint v1, GCGLuint v2)
{
    if (isContextLostOrPending() || !validateUniformLocation("uniform3ui", location))
        return;
    m_context->uniform3ui(location->location(), v0, v1, v2);
}

void WebGL2RenderingContext::uniform4ui(WebGLUniformLocation* location, GCGLuint v0, GCGLuint v1, GCGLuint v2, GCGLuint v3)
{
    if (isContextLostOrPending() || !validateUniformLocation("uniform4ui", location))
        return;
    m_context->uniform4ui(location->location(), v0, v1, v2, v3);
}

void WebGL2RenderingContext::uniform1uiv(WebGLUniformLocation* location, Uint32List&& value, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform1uiv", location, value, 1, srcOffset, srcLength))
        return;
    m_context->uniform1uiv(location->location(), value.data(), srcOffset, srcLength ? srcLength : (value.length() - srcOffset));
}

void WebGL2RenderingContext::uniform2uiv(WebGLUniformLocation* location, Uint32List&& value, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform2uiv", location, value, 2, srcOffset, srcLength))
        return;
    m_context->uniform2uiv(location->location(), value.data(), srcOffset, srcLength ? srcLength : (value.length() - srcOffset) / 2);
}

void WebGL2RenderingContext::uniform3uiv(WebGLUniformLocation* location, Uint32List&& value, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform3uiv", location, value, 3, srcOffset, srcLength))
        return;
    m_context->uniform3uiv(location->location(), value.data(), srcOffset, srcLength ? srcLength : (value.length() - srcOffset) / 3);
}

void WebGL2RenderingContext::uniform4uiv(WebGLUniformLocation* location, Uint32List&& value, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform4uiv", location, value, 4, srcOffset, srcLength))
        return;
    m_context->uniform4uiv(location->location(), value.data(), srcOffset, srcLength ? srcLength : (value.length() - srcOffset) / 4);
}

void WebGL2RenderingContext::uniformMatrix2x3fv(WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformMatrixParameters("uniformMatrix2x3fv", location, transpose, v, 6, srcOffset, srcLength))
        return;
    m_context->uniformMatrix2x3fv(location->location(), transpose, v.data(), srcOffset, srcLength ? srcLength : (v.length() - srcOffset) / 6);
}

void WebGL2RenderingContext::uniformMatrix3x2fv(WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformMatrixParameters("uniformMatrix3x2fv", location, transpose, v, 6, srcOffset, srcLength))
        return;
    m_context->uniformMatrix3x2fv(location->location(), transpose, v.data(), srcOffset, srcLength ? srcLength : (v.length() - srcOffset) / 6);
}

void WebGL2RenderingContext::uniformMatrix2x4fv(WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformMatrixParameters("uniformMatrix2x4fv", location, transpose, v, 8, srcOffset, srcLength))
        return;
    m_context->uniformMatrix2x4fv(location->location(), transpose, v.data(), srcOffset, srcLength ? srcLength : (v.length() - srcOffset) / 8);
}

void WebGL2RenderingContext::uniformMatrix4x2fv(WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformMatrixParameters("uniformMatrix4x2fv", location, transpose, v, 8, srcOffset, srcLength))
        return;
    m_context->uniformMatrix4x2fv(location->location(), transpose, v.data(), srcOffset, srcLength ? srcLength : (v.length() - srcOffset) / 8);
}

void WebGL2RenderingContext::uniformMatrix3x4fv(WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformMatrixParameters("uniformMatrix3x4fv", location, transpose, v, 12, srcOffset, srcLength))
        return;
    m_context->uniformMatrix3x4fv(location->location(), transpose, v.data(), srcOffset, srcLength ? srcLength : (v.length() - srcOffset) / 12);
}

void WebGL2RenderingContext::uniformMatrix4x3fv(WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformMatrixParameters("uniformMatrix4x3fv", location, transpose, v, 12, srcOffset, srcLength))
        return;
    m_context->uniformMatrix4x3fv(location->location(), transpose, v.data(), srcOffset, srcLength ? srcLength : (v.length() - srcOffset) / 12);
}

void WebGL2RenderingContext::vertexAttribI4i(GCGLuint index, GCGLint x, GCGLint y, GCGLint z, GCGLint w)
{
    if (isContextLostOrPending())
        return;
    m_context->vertexAttribI4i(index, x, y, z, w);
    if (index < m_maxVertexAttribs) {
        m_vertexAttribValue[index].type = GraphicsContextGL::INT;
        m_vertexAttribValue[index].iValue[0] = x;
        m_vertexAttribValue[index].iValue[1] = y;
        m_vertexAttribValue[index].iValue[2] = z;
        m_vertexAttribValue[index].iValue[3] = w;
    }
}

void WebGL2RenderingContext::vertexAttribI4iv(GCGLuint index, Int32List&& list)
{
    if (isContextLostOrPending())
        return;
    auto data = list.data();
    if (!data) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribI4iv", "no array");
        return;
    }

    int size = list.length();
    if (size < 4) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribI4iv", "array too small");
        return;
    }
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribI4iv", "index out of range");
        return;
    }
    m_context->vertexAttribI4iv(index, data);
    m_vertexAttribValue[index].type = GraphicsContextGL::INT;
    memcpy(m_vertexAttribValue[index].iValue, data, sizeof(m_vertexAttribValue[index].iValue));
}

void WebGL2RenderingContext::vertexAttribI4ui(GCGLuint index, GCGLuint x, GCGLuint y, GCGLuint z, GCGLuint w)
{
    if (isContextLostOrPending())
        return;
    m_context->vertexAttribI4ui(index, x, y, z, w);
    if (index < m_maxVertexAttribs) {
        m_vertexAttribValue[index].type = GraphicsContextGL::UNSIGNED_INT;
        m_vertexAttribValue[index].uiValue[0] = x;
        m_vertexAttribValue[index].uiValue[1] = y;
        m_vertexAttribValue[index].uiValue[2] = z;
        m_vertexAttribValue[index].uiValue[3] = w;
    }
}

void WebGL2RenderingContext::vertexAttribI4uiv(GCGLuint index, Uint32List&& list)
{
    if (isContextLostOrPending())
        return;
    auto data = list.data();
    if (!data) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribI4uiv", "no array");
        return;
    }
    
    int size = list.length();
    if (size < 4) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribI4uiv", "array too small");
        return;
    }
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribI4uiv", "index out of range");
        return;
    }
    m_context->vertexAttribI4uiv(index, data);
    m_vertexAttribValue[index].type = GraphicsContextGL::UNSIGNED_INT;
    memcpy(m_vertexAttribValue[index].uiValue, data, sizeof(m_vertexAttribValue[index].uiValue));
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
#if !USE(ANGLE)
    if (mask & ~(GraphicsContextGL::COLOR_BUFFER_BIT | GraphicsContextGL::DEPTH_BUFFER_BIT | GraphicsContextGL::STENCIL_BUFFER_BIT)) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "clear", "invalid mask");
        return;
    }
    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(graphicsContextGL(), &reason)) {
        synthesizeGLError(GraphicsContextGL::INVALID_FRAMEBUFFER_OPERATION, "clear", reason);
        return;
    }
    if (m_framebufferBinding && (mask & GraphicsContextGL::COLOR_BUFFER_BIT) && m_framebufferBinding->getColorBufferFormat() && isIntegerFormat(m_framebufferBinding->getColorBufferFormat())) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "clear", "cannot clear an integer buffer");
        return;
    }
#endif
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

void WebGL2RenderingContext::drawRangeElements(GCGLenum mode, GCGLuint start, GCGLuint end, GCGLsizei count, GCGLenum type, GCGLint64 offset)
{
    if (isContextLostOrPending())
        return;

    m_context->drawRangeElements(mode, start, end, count, type, offset);
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
    if (isContextLostOrPending() || !feedbackObject || feedbackObject->isDeleted() || !validateWebGLObject("deleteTransformFeedback", feedbackObject))
        return;
    
    ASSERT(feedbackObject != m_defaultTransformFeedback);
    
    if (feedbackObject->isActive()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "deleteTransformFeedback", "attempt to delete an active transform feedback object");
        return;
    }

    if (m_boundTransformFeedback == feedbackObject)
        m_boundTransformFeedback = m_defaultTransformFeedback;

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

    if (target != GraphicsContextGL::TRANSFORM_FEEDBACK) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "bindTransformFeedback", "target must be TRANSFORM_FEEDBACK");
        return;
    }
    if (m_boundTransformFeedback->isActive() && !m_boundTransformFeedback->isPaused()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "bindTransformFeedback", "transform feedback is active and not paused");
        return;
    }

    auto toBeBound = feedbackObject ? feedbackObject : m_defaultTransformFeedback.get();
    m_context->bindTransformFeedback(target, toBeBound->object());
    m_boundTransformFeedback = toBeBound;
}

static bool ValidateTransformFeedbackPrimitiveMode(GCGLenum mode)
{
    switch (mode) {
    case GL_POINTS:
    case GL_LINES:
    case GL_TRIANGLES:
        return true;
    default:
        return false;
    }
}

void WebGL2RenderingContext::beginTransformFeedback(GCGLenum primitiveMode)
{
    if (isContextLostOrPending())
        return;
    
    if (!ValidateTransformFeedbackPrimitiveMode(primitiveMode)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "beginTransformFeedback", "invalid transform feedback primitive mode");
        return;
    }
    if (!m_currentProgram) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginTransformFeedback", "no program is active");
        return;
    }
    if (m_boundTransformFeedback->isActive()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginTransformFeedback", "transform feedback is already active");
        return;
    }
    int requiredBufferCount = m_currentProgram->requiredTransformFeedbackBufferCount();
    if (!requiredBufferCount) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginTransformFeedback", "current active program does not specify any transform feedback varyings to record");
        return;
    }
    if (!m_boundTransformFeedback->hasEnoughBuffers(requiredBufferCount)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginTransformFeedback", "not enough transform feedback buffers bound");
        return;
    }

    m_context->beginTransformFeedback(primitiveMode);

    m_boundTransformFeedback->setProgram(*m_currentProgram);
    m_boundTransformFeedback->setActive(true);
    m_boundTransformFeedback->setPaused(false);
}

void WebGL2RenderingContext::endTransformFeedback()
{
    if (isContextLostOrPending())
        return;

    if (!m_boundTransformFeedback->isActive()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "endTransformFeedback", "transform feedback is not active");
        return;
    }
    
    m_context->endTransformFeedback();

    m_boundTransformFeedback->setPaused(false);
    m_boundTransformFeedback->setActive(false);
}

void WebGL2RenderingContext::transformFeedbackVaryings(WebGLProgram& program, const Vector<String>& varyings, GCGLenum bufferMode)
{
    if (isContextLostOrPending() || !validateWebGLObject("transformFeedbackVaryings", &program))
        return;
    
    switch (bufferMode) {
    case GraphicsContextGL::SEPARATE_ATTRIBS:
        if (varyings.size() > m_maxTransformFeedbackSeparateAttribs) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "transformFeedbackVaryings", "too many varyings");
            return;
        }
        break;
    case GraphicsContextGL::INTERLEAVED_ATTRIBS:
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "transformFeedbackVaryings", "invalid buffer mode");
        return;
    }
    program.setRequiredTransformFeedbackBufferCount(bufferMode == GraphicsContextGL::INTERLEAVED_ATTRIBS ? 1 : varyings.size());

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
    if (isContextLostOrPending())
        return;

    if (!m_boundTransformFeedback->isActive()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "pauseTransformFeedback", "transform feedback is not active");
        return;
    }

    if (m_boundTransformFeedback->isPaused()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "pauseTransformFeedback", "transform feedback is already paused");
        return;
    }

    m_boundTransformFeedback->setPaused(true);
    m_context->pauseTransformFeedback();
}

void WebGL2RenderingContext::resumeTransformFeedback()
{
    if (isContextLostOrPending())
        return;

    if (!m_boundTransformFeedback->validateProgramForResume(m_currentProgram.get())) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "resumeTransformFeedback", "the current program is not the same as when beginTransformFeedback was called");
        return;
    }
    if (!m_boundTransformFeedback->isActive() || !m_boundTransformFeedback->isPaused()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "resumeTransformFeedback", "transform feedback is not active or not paused");
        return;
    }

    m_boundTransformFeedback->setPaused(false);
    m_context->resumeTransformFeedback();
}

bool WebGL2RenderingContext::setIndexedBufferBinding(const char *functionName, GCGLenum target, GCGLuint index, WebGLBuffer* buffer)
{
    if (isContextLostOrPending())
        return false;

    if (!checkObjectToBeBound(functionName, buffer))
        return false;

    switch (target) {
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER:
        if (index > m_maxTransformFeedbackSeparateAttribs) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "index out of range");
            return false;
        }
        break;
    case GraphicsContextGL::UNIFORM_BUFFER:
        if (index >= m_boundIndexedUniformBuffers.size()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "index out of range");
            return false;
        }
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
        return false;
    }

    if (!validateAndCacheBufferBinding(functionName, target, buffer))
        return false;

    switch (target) {
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER:
        m_boundTransformFeedback->setBoundIndexedTransformFeedbackBuffer(index, buffer);
        break;
    case GraphicsContextGL::UNIFORM_BUFFER:
        m_boundIndexedUniformBuffers[index] = buffer;
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
        return false;
    }
    return true;
}

void WebGL2RenderingContext::bindBufferBase(GCGLenum target, GCGLuint index, WebGLBuffer* buffer)
{
    if (setIndexedBufferBinding("bindBufferBase", target, index, buffer))
        m_context->bindBufferBase(target, index, objectOrZero(buffer));
}

void WebGL2RenderingContext::bindBufferRange(GCGLenum target, GCGLuint index, WebGLBuffer* buffer, GCGLint64 offset, GCGLint64 size)
{
    if (target == GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER && (offset % 4 || size % 4)) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bindBufferRange", "invalid offset or size");
        return;
    }
    if (target == GraphicsContextGL::UNIFORM_BUFFER && (offset % m_uniformBufferOffsetAlignment)) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bindBufferRange", "invalid offset");
        return;
    }
    if (setIndexedBufferBinding("bindBufferRange", target, index, buffer))
        m_context->bindBufferRange(target, index, objectOrZero(buffer), offset, size);
}

WebGLAny WebGL2RenderingContext::getIndexedParameter(GCGLenum target, GCGLuint index)
{
    if (isContextLostOrPending())
        return nullptr;

    switch (target) {
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER_BINDING: {
        WebGLBuffer* buffer;
        bool success = m_boundTransformFeedback->getBoundIndexedTransformFeedbackBuffer(index, &buffer);
        if (!success) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getIndexedParameter", "index out of range");
            return nullptr;
        }
        return makeRefPtr(buffer);
    }
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER_SIZE:
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER_START:
    case GraphicsContextGL::UNIFORM_BUFFER_SIZE:
    case GraphicsContextGL::UNIFORM_BUFFER_START: {
        GCGLint64 value = 0;
        m_context->getInteger64i_v(target, index, &value);
        return value;
    }
    case GraphicsContextGL::UNIFORM_BUFFER_BINDING:
        if (index >= m_boundIndexedUniformBuffers.size()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getIndexedParameter", "index out of range");
            return nullptr;
        }
        return m_boundIndexedUniformBuffers[index];
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getIndexedParameter", "invalid parameter name");
        return nullptr;
    }
}

Optional<Vector<GCGLuint>> WebGL2RenderingContext::getUniformIndices(WebGLProgram& program, const Vector<String>& names)
{
#if USE(ANGLE)
    if (isContextLostOrPending() || !validateWebGLObject("getUniformIndices", &program))
        return WTF::nullopt;
    return m_context->getUniformIndices(program.object(), names);
#else
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] getUniformIndices()");
    return WTF::nullopt;
#endif
}

WebGLAny WebGL2RenderingContext::getActiveUniforms(WebGLProgram& program, const Vector<GCGLuint>& uniformIndices, GCGLenum pname)
{
    if (isContextLostOrPending() || !validateWebGLObject("getActiveUniforms", &program))
        return nullptr;

    Vector<GCGLint> result(uniformIndices.size(), 0);
    
    switch (pname) {
    case GraphicsContextGL::UNIFORM_TYPE:
        m_context->getActiveUniforms(program.object(), uniformIndices, pname, result);
        return result.map([](auto x) { return static_cast<GCGLenum>(x); });
    case GraphicsContextGL::UNIFORM_SIZE:
        m_context->getActiveUniforms(program.object(), uniformIndices, pname, result);
        return result.map([](auto x) { return static_cast<GCGLuint>(x); });
    case GraphicsContextGL::UNIFORM_BLOCK_INDEX:
    case GraphicsContextGL::UNIFORM_OFFSET:
    case GraphicsContextGL::UNIFORM_ARRAY_STRIDE:
    case GraphicsContextGL::UNIFORM_MATRIX_STRIDE:
        m_context->getActiveUniforms(program.object(), uniformIndices, pname, result);
        return WTFMove(result);
    case GraphicsContextGL::UNIFORM_IS_ROW_MAJOR:
        m_context->getActiveUniforms(program.object(), uniformIndices, pname, result);
        return result.map([](auto x) { return static_cast<bool>(x); });
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getActiveUniforms", "invalid parameter name");
        return nullptr;
    }
}

GCGLuint WebGL2RenderingContext::getUniformBlockIndex(WebGLProgram& program, const String& uniformBlockName)
{
#if USE(ANGLE)
    if (isContextLostOrPending() || !validateWebGLObject("getUniformBlockIndex", &program))
        return 0;
    return m_context->getUniformBlockIndex(program.object(), uniformBlockName);
#else
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformBlockName);
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] getUniformBlockIndex()");
    return 0;
#endif
}

WebGLAny WebGL2RenderingContext::getActiveUniformBlockParameter(WebGLProgram& program, GCGLuint uniformBlockIndex, GCGLenum pname)
{
#if USE(ANGLE)
    if (isContextLostOrPending() || !validateWebGLObject("getActiveUniformBlockParameter", &program))
        return nullptr;
    GLint result = 0;
    switch (pname) {
    case GraphicsContextGL::UNIFORM_BLOCK_BINDING:
    case GraphicsContextGL::UNIFORM_BLOCK_DATA_SIZE:
    case GraphicsContextGL::UNIFORM_BLOCK_ACTIVE_UNIFORMS:
        m_context->getExtensions().getActiveUniformBlockivRobustANGLE(program.object(), uniformBlockIndex, pname, sizeof(result), nullptr, &result);
        return static_cast<GLuint>(result);
    case GraphicsContextGL::UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES: {
        GLint size = 0;
        m_context->getExtensions().getActiveUniformBlockivRobustANGLE(program.object(), uniformBlockIndex, GraphicsContextGL::UNIFORM_BLOCK_ACTIVE_UNIFORMS, sizeof(size), nullptr, &size);
        Vector<GCGLint> params(size, 0);
        m_context->getExtensions().getActiveUniformBlockivRobustANGLE(program.object(), uniformBlockIndex, pname, sizeof(params[0]) * params.size(), nullptr, params.data());
        return params.map([](auto x) { return static_cast<GCGLuint>(x); });
    }
    case GraphicsContextGL::UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
    case GraphicsContextGL::UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:
        m_context->getExtensions().getActiveUniformBlockivRobustANGLE(program.object(), uniformBlockIndex, pname, sizeof(result), nullptr, &result);
        return static_cast<bool>(result);
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getActiveUniformBlockParameter", "invalid parameter name");
        return nullptr;
    }
#else
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformBlockIndex);
    UNUSED_PARAM(pname);
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] getActiveUniformBlockParameter()");
    return nullptr;
#endif
}

WebGLAny WebGL2RenderingContext::getActiveUniformBlockName(WebGLProgram& program, GCGLuint index)
{
#if USE(ANGLE)
    if (isContextLostOrPending() || !validateWebGLObject("getActiveUniformBlockName", &program))
        return String();
    return m_context->getActiveUniformBlockName(program.object(), index);
#else
    UNUSED_PARAM(program);
    UNUSED_PARAM(index);
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] getActiveUniformBlockName()");
    return nullptr;
#endif
}

void WebGL2RenderingContext::uniformBlockBinding(WebGLProgram& program, GCGLuint uniformBlockIndex, GCGLuint uniformBlockBinding)
{
#if USE(ANGLE)
    if (isContextLostOrPending() || !validateWebGLObject("uniformBlockBinding", &program))
        return;
    m_context->uniformBlockBinding(program.object(), uniformBlockIndex, uniformBlockBinding);
#else
    UNUSED_PARAM(program);
    UNUSED_PARAM(uniformBlockIndex);
    UNUSED_PARAM(uniformBlockBinding);
    LOG(WebGL, "[[ NOT IMPLEMENTED ]] uniformBlockBinding()");
#endif
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
    ENABLE_IF_REQUESTED(EXTColorBufferFloat, m_extColorBufferFloat, "EXT_color_buffer_float", EXTColorBufferFloat::supported(*this));
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
    if (EXTColorBufferFloat::supported(*this))
        result.append("EXT_color_buffer_float"_s);

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
    if (isContextLostOrPending())
        return nullptr;

    const char* functionName = "getFramebufferAttachmentParameter";
    if (!validateFramebufferTarget(target)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
        return nullptr;
    }
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
        if (pname == GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME)
            return nullptr;
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

bool WebGL2RenderingContext::validateFramebufferTarget(GCGLenum target)
{
    switch (target) {
    case GraphicsContextGL::FRAMEBUFFER:
    case GraphicsContextGL::DRAW_FRAMEBUFFER:
    case GraphicsContextGL::READ_FRAMEBUFFER:
        return true;
    }
    return false;
}

WebGLFramebuffer* WebGL2RenderingContext::getFramebufferBinding(GCGLenum target)
{
    switch (target) {
    case GraphicsContextGL::READ_FRAMEBUFFER:
        return m_readFramebufferBinding.get();
    case GraphicsContextGL::DRAW_FRAMEBUFFER:
        return m_framebufferBinding.get();
    }
    return WebGLRenderingContextBase::getFramebufferBinding(target);
}

WebGLFramebuffer* WebGL2RenderingContext::getReadFramebufferBinding()
{
    return m_readFramebufferBinding.get();
}

void WebGL2RenderingContext::restoreCurrentFramebuffer()
{
    bindFramebuffer(GraphicsContextGL::DRAW_FRAMEBUFFER, m_framebufferBinding.get());
    bindFramebuffer(GraphicsContextGL::READ_FRAMEBUFFER, m_readFramebufferBinding.get());
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
#if USE(ANGLE)
    m_context->renderbufferStorage(target, internalformat, width, height);
    return;
#endif
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

GCGLuint WebGL2RenderingContext::maxTransformFeedbackSeparateAttribs() const
{
    return m_maxTransformFeedbackSeparateAttribs;
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
    case GraphicsContextGL::STENCIL_INDEX8:
        return GraphicsContextGL::STENCIL;
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
    case GraphicsContextGL::SHADING_LANGUAGE_VERSION:
        if (!RuntimeEnabledFeatures::sharedFeatures().maskWebGLStringsEnabled())
            return "WebGL GLSL ES 3.00 (" + m_context->getString(GraphicsContextGL::SHADING_LANGUAGE_VERSION) + ")";
        return "WebGL GLSL ES 3.00"_str;
    case GraphicsContextGL::VERSION:
        return "WebGL 2.0"_str;
    case GraphicsContextGL::COPY_READ_BUFFER_BINDING:
        return m_boundCopyReadBuffer;
    case GraphicsContextGL::COPY_WRITE_BUFFER_BINDING:
        return m_boundCopyWriteBuffer;
    case GraphicsContextGL::DRAW_FRAMEBUFFER_BINDING:
        return m_framebufferBinding;
    case GraphicsContextGL::FRAGMENT_SHADER_DERIVATIVE_HINT:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_3D_TEXTURE_SIZE:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_ARRAY_TEXTURE_LAYERS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_CLIENT_WAIT_TIMEOUT_WEBGL:
        return static_cast<long long>(MaxClientWaitTimeout);
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
    case GraphicsContextGL::MAX_FRAGMENT_INPUT_COMPONENTS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_FRAGMENT_UNIFORM_BLOCKS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_FRAGMENT_UNIFORM_COMPONENTS:
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
    case GraphicsContextGL::PIXEL_PACK_BUFFER_BINDING:
        return m_boundPixelPackBuffer;
    case GraphicsContextGL::PIXEL_UNPACK_BUFFER_BINDING:
        return m_boundPixelUnpackBuffer;
    case GraphicsContextGL::RASTERIZER_DISCARD:
        return getBooleanParameter(pname);
    case GraphicsContextGL::READ_BUFFER: {
        GLint value = getIntParameter(pname);
        if (!m_readFramebufferBinding && value != GL_NONE)
            return GL_BACK;
        return value;
    }
    case GraphicsContextGL::READ_FRAMEBUFFER_BINDING:
        return m_readFramebufferBinding;
    case GraphicsContextGL::SAMPLER_BINDING:
        return m_boundSamplers[m_activeTextureUnit];
    case GraphicsContextGL::TEXTURE_BINDING_2D_ARRAY:
        return m_textureUnits[m_activeTextureUnit].texture2DArrayBinding;
    case GraphicsContextGL::TEXTURE_BINDING_3D:
        return m_textureUnits[m_activeTextureUnit].texture3DBinding;
    case GraphicsContextGL::TRANSFORM_FEEDBACK_ACTIVE:
        return getBooleanParameter(pname);
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER_BINDING:
        return m_boundTransformFeedbackBuffer;
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BINDING:
        return m_boundTransformFeedback == m_defaultTransformFeedback ? nullptr : m_boundTransformFeedback;
    case GraphicsContextGL::TRANSFORM_FEEDBACK_PAUSED:
        return getBooleanParameter(pname);
    case GraphicsContextGL::UNIFORM_BUFFER_BINDING:
        return m_boundUniformBuffer;
    case GraphicsContextGL::UNIFORM_BUFFER_OFFSET_ALIGNMENT:
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
    case GraphicsContextGL::VERTEX_ARRAY_BINDING:
        if (m_boundVertexArrayObject->isDefaultObject())
            return nullptr;
        return makeRefPtr(static_cast<WebGLVertexArrayObject&>(*m_boundVertexArrayObject));
    default:
        return WebGLRenderingContextBase::getParameter(pname);
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

void WebGL2RenderingContext::uniform1fv(WebGLUniformLocation* location, Float32List data, GLuint srcOffset, GLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform1fv", location, data, 1, srcOffset, srcLength))
        return;
    m_context->uniform1fv(location->location(), data.data(), srcOffset, srcLength ? srcLength : (data.length() - srcOffset));
}

void WebGL2RenderingContext::uniform2fv(WebGLUniformLocation* location, Float32List data, GLuint srcOffset, GLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform2fv", location, data, 2, srcOffset, srcLength))
        return;
    m_context->uniform2fv(location->location(), data.data(), srcOffset, srcLength ? srcLength : (data.length() - srcOffset) / 2);
}

void WebGL2RenderingContext::uniform3fv(WebGLUniformLocation* location, Float32List data, GLuint srcOffset, GLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform3fv", location, data, 3, srcOffset, srcLength))
        return;
    m_context->uniform3fv(location->location(), data.data(), srcOffset, srcLength ? srcLength : (data.length() - srcOffset) / 3);
}

void WebGL2RenderingContext::uniform4fv(WebGLUniformLocation* location, Float32List data, GLuint srcOffset, GLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform4fv", location, data, 4, srcOffset, srcLength))
        return;
    m_context->uniform4fv(location->location(), data.data(), srcOffset, srcLength ? srcLength : (data.length() - srcOffset) / 4);
}

void WebGL2RenderingContext::uniform1iv(WebGLUniformLocation* location, Int32List data, GLuint srcOffset, GLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform1iv", location, data, 1, srcOffset, srcLength))
        return;
    m_context->uniform1iv(location->location(), data.data(), srcOffset, srcLength ? srcLength : (data.length() - srcOffset));
}

void WebGL2RenderingContext::uniform2iv(WebGLUniformLocation* location, Int32List data, GLuint srcOffset, GLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform2iv", location, data, 2, srcOffset, srcLength))
        return;
    m_context->uniform2iv(location->location(), data.data(), srcOffset, srcLength ? srcLength : (data.length() - srcOffset) / 2);
}

void WebGL2RenderingContext::uniform3iv(WebGLUniformLocation* location, Int32List data, GLuint srcOffset, GLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform3iv", location, data, 3, srcOffset, srcLength))
        return;
    m_context->uniform3iv(location->location(), data.data(), srcOffset, srcLength ? srcLength : (data.length() - srcOffset) / 3);
}

void WebGL2RenderingContext::uniform4iv(WebGLUniformLocation* location, Int32List data, GLuint srcOffset, GLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformParameters("uniform4iv", location, data, 4, srcOffset, srcLength))
        return;
    m_context->uniform4iv(location->location(), data.data(), srcOffset, srcLength ? srcLength : (data.length() - srcOffset) / 4);
}

void WebGL2RenderingContext::uniformMatrix2fv(WebGLUniformLocation* location, GLboolean transpose, Float32List data, GLuint srcOffset, GLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformMatrixParameters("uniformMatrix2fv", location, transpose, data, 2*2, srcOffset, srcLength))
        return;
    m_context->uniformMatrix2fv(location->location(), transpose, data.data(), srcOffset, srcLength ? srcLength : (data.length() - srcOffset) / (2*2));
}

void WebGL2RenderingContext::uniformMatrix3fv(WebGLUniformLocation* location, GLboolean transpose, Float32List data, GLuint srcOffset, GLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformMatrixParameters("uniformMatrix3fv", location, transpose, data, 3*3, srcOffset, srcLength))
        return;
    m_context->uniformMatrix3fv(location->location(), transpose, data.data(), srcOffset, srcLength ? srcLength : (data.length() - srcOffset) / (3*3));
}

void WebGL2RenderingContext::uniformMatrix4fv(WebGLUniformLocation* location, GLboolean transpose, Float32List data, GLuint srcOffset, GLuint srcLength)
{
    if (isContextLostOrPending() || !validateUniformMatrixParameters("uniformMatrix4fv", location, transpose, data, 4*4, srcOffset, srcLength))
        return;
    m_context->uniformMatrix4fv(location->location(), transpose, data.data(), srcOffset, srcLength ? srcLength : (data.length() - srcOffset) / (4*4));
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

#define REMOVE_BUFFER_FROM_BINDING(binding) \
    if (binding == buffer) \
        binding = nullptr;

void WebGL2RenderingContext::uncacheDeletedBuffer(WebGLBuffer* buffer)
{
    ASSERT(buffer);

    REMOVE_BUFFER_FROM_BINDING(m_boundCopyReadBuffer);
    REMOVE_BUFFER_FROM_BINDING(m_boundCopyWriteBuffer);
    REMOVE_BUFFER_FROM_BINDING(m_boundPixelPackBuffer);
    REMOVE_BUFFER_FROM_BINDING(m_boundPixelUnpackBuffer);
    REMOVE_BUFFER_FROM_BINDING(m_boundTransformFeedbackBuffer);
    REMOVE_BUFFER_FROM_BINDING(m_boundUniformBuffer);
    m_boundTransformFeedback->unbindBuffer(*buffer);

    WebGLRenderingContextBase::uncacheDeletedBuffer(buffer);
}

#undef REMOVE_BUFFER_FROM_BINDING

} // namespace WebCore

#endif // ENABLE(WEBGL)
