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

#include "config.h"
#include "WebGL2RenderingContext.h"

#if ENABLE(WEBGL2)

#include "CachedImage.h"
#include "EXTColorBufferFloat.h"
#include "EXTColorBufferHalfFloat.h"
#include "EXTFloatBlend.h"
#include "EXTTextureCompressionRGTC.h"
#include "EXTTextureFilterAnisotropic.h"
#include "EventLoop.h"
#include "ExtensionsGL.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBitmap.h"
#include "ImageData.h"
#include "InspectorInstrumentation.h"
#include "KHRParallelShaderCompile.h"
#include "Logging.h"
#include "OESTextureFloatLinear.h"
#include "RenderBox.h"
#include "RuntimeEnabledFeatures.h"
#include "WebGLActiveInfo.h"
#include "WebGLBuffer.h"
#include "WebGLCompressedTextureASTC.h"
#include "WebGLCompressedTextureATC.h"
#include "WebGLCompressedTextureETC.h"
#include "WebGLCompressedTextureETC1.h"
#include "WebGLCompressedTexturePVRTC.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLCompressedTextureS3TCsRGB.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLFramebuffer.h"
#include "WebGLLoseContext.h"
#include "WebGLMultiDraw.h"
#include "WebGLProgram.h"
#include "WebGLQuery.h"
#include "WebGLRenderbuffer.h"
#include "WebGLSampler.h"
#include "WebGLSync.h"
#include "WebGLTexture.h"
#include "WebGLTransformFeedback.h"
#include "WebGLUniformLocation.h"
#include "WebGLVertexArrayObject.h"
#include "WebGLVertexArrayObjectOES.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <JavaScriptCore/SlotVisitor.h>
#include <JavaScriptCore/SlotVisitorInlines.h>
#include <JavaScriptCore/TypedArrayType.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>

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

std::unique_ptr<WebGL2RenderingContext> WebGL2RenderingContext::create(CanvasBase& canvas, Ref<GraphicsContextGL>&& context, GraphicsContextGLAttributes attributes)
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

WebGL2RenderingContext::WebGL2RenderingContext(CanvasBase& canvas, Ref<GraphicsContextGL>&& context, GraphicsContextGLAttributes attributes)
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

    m_packRowLength = 0;
    m_packSkipPixels = 0;
    m_packSkipRows = 0;
    m_unpackRowLength = 0;
    m_unpackImageHeight = 0;
    m_unpackSkipPixels = 0;
    m_unpackSkipRows = 0;
    m_unpackSkipImages = 0;

    m_max3DTextureSize = m_context->getInteger(GraphicsContextGL::MAX_3D_TEXTURE_SIZE);
    m_max3DTextureLevel = WebGLTexture::computeLevelCount(m_max3DTextureSize, m_max3DTextureSize);
    m_maxArrayTextureLayers = m_context->getInteger(GraphicsContextGL::MAX_ARRAY_TEXTURE_LAYERS);

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
    return m_context->getInteger64(pname);
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

bool WebGL2RenderingContext::validateAndCacheBufferBinding(const AbstractLocker& locker, const char* functionName, GCGLenum target, WebGLBuffer* buffer)
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
        m_boundVertexArrayObject->setElementArrayBuffer(locker, buffer);
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

bool WebGL2RenderingContext::validateTexFuncLayer(const char* functionName, GCGLenum texTarget, GCGLint layer)
{
    if (layer < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "layer out of range");
        return false;
    }
    switch (texTarget) {
    case GraphicsContextGL::TEXTURE_3D:
        if (layer > m_max3DTextureSize - 1) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "layer out of range");
            return false;
        }
        break;
    case GraphicsContextGL::TEXTURE_2D_ARRAY:
        if (layer > m_maxArrayTextureLayers - 1) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "layer out of range");
            return false;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
    return true;
}

GCGLint WebGL2RenderingContext::maxTextureLevelForTarget(GCGLenum target)
{
    switch (target) {
    case GraphicsContextGL::TEXTURE_3D:
        return m_max3DTextureLevel;
    case GraphicsContextGL::TEXTURE_2D_ARRAY:
        return m_maxTextureLevel;
    }
    return WebGLRenderingContextBase::maxTextureLevelForTarget(target);
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
    if (!checkedLength) {
        // Default to the remainder of the buffer.
        checkedLength = data.byteLength();
        checkedLength /= elementSize;
        checkedLength -= srcOffset;
    }
    Checked<GCGLuint, RecordOverflow> checkedByteLength = checkedLength * checkedElementSize;

    if (checkedLength.hasOverflowed() || checkedByteSrcOffset.hasOverflowed()
        || checkedByteLength.hasOverflowed()
        || checkedByteSrcOffset > data.byteLength()
        || checkedByteLength > data.byteLength() - checkedByteSrcOffset.value()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "srcOffset or length is out of bounds");
        return nullptr;
    }

    return arrayBufferViewSliceFactory(functionName, data, data.byteOffset() + checkedByteSrcOffset.value(), checkedLength);
}

void WebGL2RenderingContext::pixelStorei(GCGLenum pname, GCGLint param)
{
    if (isContextLostOrPending())
        return;
    if (param < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "pixelStorei", "negative value");
        return;
    }
    switch (pname) {
    case GraphicsContextGL::PACK_ROW_LENGTH:
        m_packRowLength = param;
        break;
    case GraphicsContextGL::PACK_SKIP_PIXELS:
        m_packSkipPixels = param;
        break;
    case GraphicsContextGL::PACK_SKIP_ROWS:
        m_packSkipRows = param;
        break;
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
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "copyBufferSubData", "Invalid readTarget or writeTarget");
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

    if (!writeBuffer->associateCopyBufferSubData(*readBuffer, checkedReadOffset, checkedWriteOffset, checkedSize)) {
        this->synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "copyBufferSubData", "offset out of range");
        return;
    }

    m_context->moveErrorsToSyntheticErrorList();
#if PLATFORM(COCOA)
    m_context->copyBufferSubData(readTarget, writeTarget, checkedReadOffset, checkedWriteOffset, checkedSize);
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
    RefPtr<WebGLBuffer> buffer = validateBufferDataParameters("getBufferSubData", target, GraphicsContextGL::STATIC_DRAW);
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
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getBufferSubData", "dstOffset is larger than the length of the destination buffer.");
        return;
    }

    GCGLuint copyLength = length ? length : dstDataLength - dstOffset;

    Checked<GCGLuint, RecordOverflow> checkedDstOffset(dstOffset);
    Checked<GCGLuint, RecordOverflow> checkedCopyLength(copyLength);
    auto checkedDestinationEnd = checkedDstOffset + checkedCopyLength;
    if (checkedDestinationEnd.hasOverflowed()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getBufferSubData", "dstOffset + copyLength is too high");
        return;
    }

    if (checkedDestinationEnd > dstDataLength) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getBufferSubData", "end of written destination is past the end of the buffer");
        return;
    }

    if (srcByteOffset < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getBufferSubData", "srcByteOffset is less than 0");
        return;
    }

    Checked<GCGLintptr, RecordOverflow> checkedSrcByteOffset(srcByteOffset);
    Checked<GCGLintptr, RecordOverflow> checkedCopyLengthPtr(copyLength);
    Checked<GCGLintptr, RecordOverflow> checkedElementSize(elementSize);
    auto checkedSourceEnd = checkedSrcByteOffset + checkedCopyLengthPtr * checkedElementSize;
    if (checkedSourceEnd.hasOverflowed() || checkedSourceEnd > buffer->byteLength()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getBufferSubData", "Parameters would read outside the bounds of the source buffer");
        return;
    }
    
    if (!copyLength)
        return;

    // FIXME: Coalesce multiple getBufferSubData() calls to use a single map() call
    m_context->getBufferSubData(target, srcByteOffset, makeGCGLSpan(static_cast<char*>(dstData->baseAddress()) + dstData->byteOffset() + dstOffset * elementSize, copyLength * elementSize));
}

void WebGL2RenderingContext::bindFramebuffer(GCGLenum target, WebGLFramebuffer* buffer)
{
    Locker locker { objectGraphLock() };

    if (!validateNullableWebGLObject("bindFramebuffer", buffer))
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

    setFramebuffer(locker, target, buffer);
}

void WebGL2RenderingContext::blitFramebuffer(GCGLint srcX0, GCGLint srcY0, GCGLint srcX1, GCGLint srcY1, GCGLint dstX0, GCGLint dstY0, GCGLint dstX1, GCGLint dstY1, GCGLbitfield mask, GCGLenum filter)
{
    if (isContextLostOrPending())
        return;
    m_context->blitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    markContextChangedAndNotifyCanvasObserver();
}

void WebGL2RenderingContext::deleteFramebuffer(WebGLFramebuffer* framebuffer)
{
    Locker locker { objectGraphLock() };

    if (!deleteObject(locker, framebuffer))
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

void WebGL2RenderingContext::framebufferTextureLayer(GCGLenum target, GCGLenum attachment, WebGLTexture* texture, GCGLint level, GCGLint layer)
{
    if (isContextLostOrPending() || !validateFramebufferFuncParameters("framebufferTextureLayer", target, attachment))
        return;

    if (texture && !validateWebGLObject("framebufferTextureLayer", texture))
        return;

    GCGLenum texTarget = texture ? texture->getTarget() : 0;
    if (texture) {
        if (texTarget != GraphicsContextGL::TEXTURE_3D && texTarget != GraphicsContextGL::TEXTURE_2D_ARRAY) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferTextureLayer", "invalid texture type");
            return;
        }
        if (!validateTexFuncLayer("framebufferTextureLayer", texTarget, layer))
            return;
        if (!validateTexFuncLevel("framebufferTextureLayer", texTarget, level))
            return;
    }
    WebGLFramebuffer* framebufferBinding = getFramebufferBinding(target);
    if (!framebufferBinding || !framebufferBinding->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferTextureLayer", "no framebuffer bound");
        return;
    }
    framebufferBinding->setAttachmentForBoundFramebuffer(target, attachment, texTarget, texture, level, layer);
    applyStencilTest();
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

#if USE(OPENGL_ES) || USE(ANGLE)
    m_context->moveErrorsToSyntheticErrorList();
    GCGLint numValues = m_context->getInternalformati(target, internalformat, GraphicsContextGL::NUM_SAMPLE_COUNTS);
    if (m_context->moveErrorsToSyntheticErrorList() || numValues < 0)
        return nullptr;

    // Integer formats do not support multisampling, so numValues == 0 may occur.

    Vector<GCGLint> params(numValues);
    if (numValues > 0) {
        m_context->getInternalformativ(target, internalformat, pname, params);
        if (m_context->moveErrorsToSyntheticErrorList())
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
    GCGLint numValues = isIntegerFormat(internalformat) ? 0 : samples.size();
    Vector<GCGLint> params(numValues);
    for (size_t i = 0; i < static_cast<size_t>(numValues); ++i)
        params[i] = samples[i];
#endif

    return Int32Array::tryCreate(params.data(), params.size());
}

void WebGL2RenderingContext::invalidateFramebuffer(GCGLenum target, const Vector<GCGLenum>& attachments)
{
    if (isContextLostOrPending())
        return;

    Vector<GCGLenum> translatedAttachments = attachments;
    if (!checkAndTranslateAttachments("invalidateFramebuffer", target, translatedAttachments))
        return;
    m_context->invalidateFramebuffer(target, translatedAttachments);
}

void WebGL2RenderingContext::invalidateSubFramebuffer(GCGLenum target, const Vector<GCGLenum>& attachments, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (isContextLostOrPending())
        return;

    Vector<GCGLenum> translatedAttachments = attachments;
    if (!checkAndTranslateAttachments("invalidateSubFramebuffer", target, translatedAttachments))
        return;
    m_context->invalidateSubFramebuffer(target, translatedAttachments, x, y, width, height);
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
    const char* functionName = "renderbufferStorageMultisample";
    if (isContextLostOrPending())
        return;
    if (target != GraphicsContextGL::RENDERBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
        return;
    }
    if (!m_renderbufferBinding || !m_renderbufferBinding->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no bound renderbuffer");
        return;
    }
    if (!validateSize(functionName, width, height))
        return;
    renderbufferStorageImpl(target, samples, internalformat, width, height, functionName);
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
    case GraphicsContextGL::TEXTURE_WRAP_R:
        return m_context->getTexParameteri(target, pname);
    case GraphicsContextGL::TEXTURE_IMMUTABLE_FORMAT:
        return static_cast<bool>(m_context->getTexParameteri(target, pname));
    case GraphicsContextGL::TEXTURE_MIN_LOD:
        FALLTHROUGH;
    case GraphicsContextGL::TEXTURE_MAX_LOD:
        return m_context->getTexParameterf(target, pname);
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

ExceptionOr<void> WebGL2RenderingContext::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLenum format, GCGLenum type, std::optional<TexImageSource> data)
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

    m_context->texImage2D(target, level, internalformat, width, height, border, format, type, offset);
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

    m_context->texImage3D(target, level, internalformat, width, height, depth, border, format, type,  offset);
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

ExceptionOr<void> WebGL2RenderingContext::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLenum format, GCGLenum type, std::optional<TexImageSource>&& data)
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

    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, offset);
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
    clearIfComposited(ClearCallerOther);
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
    m_context->compressedTexImage2D(target, level, internalformat, width, height, border, imageSize, offset);
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
    m_context->compressedTexImage2D(target, level, internalformat, width, height, border, slice->byteLength(), makeGCGLSpan(slice->baseAddress(), slice->byteLength()));
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
    m_context->compressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, offset);
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
    m_context->compressedTexImage3D(
        target, level, internalformat, width, height,
        depth, border, slice->byteLength(),
        makeGCGLSpan(slice->baseAddress(), slice->byteLength()));
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

void WebGL2RenderingContext::compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset)
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
    m_context->compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, offset);
}

void WebGL2RenderingContext::compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, ArrayBufferView& srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride)
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
    m_context->compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, slice->byteLength(), makeGCGLSpan(slice->baseAddress(), slice->byteLength()));
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
    m_context->compressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, offset);
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
    m_context->compressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, slice->byteLength(), makeGCGLSpan(slice->baseAddress(), slice->byteLength()));
}

GCGLint WebGL2RenderingContext::getFragDataLocation(WebGLProgram& program, const String& name)
{
    if (!validateWebGLProgramOrShader("getFragDataLocation", &program))
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
    if (isContextLostOrPending())
        return;
    auto data = validateUniformParameters("uniform1uiv", location, value, 1, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniform1uiv(location->location(), data.value());
}

void WebGL2RenderingContext::uniform2uiv(WebGLUniformLocation* location, Uint32List&& value, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto data = validateUniformParameters("uniform2uiv", location, value, 2, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniform2uiv(location->location(), data.value());
}

void WebGL2RenderingContext::uniform3uiv(WebGLUniformLocation* location, Uint32List&& value, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto data = validateUniformParameters("uniform3uiv", location, value, 3, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniform3uiv(location->location(), data.value());
}

void WebGL2RenderingContext::uniform4uiv(WebGLUniformLocation* location, Uint32List&& value, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto data = validateUniformParameters("uniform4uiv", location, value, 4, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniform4uiv(location->location(), data.value());
}

void WebGL2RenderingContext::uniformMatrix2x3fv(WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix2x3fv", location, transpose, v, 6, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniformMatrix2x3fv(location->location(), transpose, data.value());
}

void WebGL2RenderingContext::uniformMatrix3x2fv(WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix3x2fv", location, transpose, v, 6, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniformMatrix3x2fv(location->location(), transpose, data.value());
}

void WebGL2RenderingContext::uniformMatrix2x4fv(WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix2x4fv", location, transpose, v, 8, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniformMatrix2x4fv(location->location(), transpose, data.value());
}

void WebGL2RenderingContext::uniformMatrix4x2fv(WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix4x2fv", location, transpose, v, 8, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniformMatrix4x2fv(location->location(), transpose, data.value());
}

void WebGL2RenderingContext::uniformMatrix3x4fv(WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix3x4fv", location, transpose, v, 12, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniformMatrix3x4fv(location->location(), transpose, data.value());
}

void WebGL2RenderingContext::uniformMatrix4x3fv(WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix4x3fv", location, transpose, v, 12, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniformMatrix4x3fv(location->location(), transpose, data.value());
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
    m_context->vertexAttribI4iv(index, makeGCGLSpan<4>(data));
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
    m_context->vertexAttribI4uiv(index, makeGCGLSpan<4>(data));
    m_vertexAttribValue[index].type = GraphicsContextGL::UNSIGNED_INT;
    memcpy(m_vertexAttribValue[index].uiValue, data, sizeof(m_vertexAttribValue[index].uiValue));
}

void WebGL2RenderingContext::vertexAttribIPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLsizei stride, GCGLint64 offset)
{
    Locker locker { objectGraphLock() };

    if (isContextLostOrPending())
        return;

    switch (type) {
    case GraphicsContextGL::BYTE:
    case GraphicsContextGL::UNSIGNED_BYTE:
    case GraphicsContextGL::SHORT:
    case GraphicsContextGL::UNSIGNED_SHORT:
    case GraphicsContextGL::FLOAT:
    case GraphicsContextGL::INT:
    case GraphicsContextGL::UNSIGNED_INT:
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "vertexAttribIPointer", "invalid type");
        return;
    }
    if (index >= m_maxVertexAttribs) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribIPointer", "index out of range");
        return;
    }
    if (size < 1 || size > 4) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribIPointer", "bad size");
        return;
    }
    if (stride < 0 || stride > 255) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribIPointer", "bad stride");
        return;
    }
    if (offset < 0 || offset > std::numeric_limits<int32_t>::max()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribIPointer", "bad offset");
        return;
    }
    if (!m_boundArrayBuffer && offset) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "vertexAttribIPointer", "no bound ARRAY_BUFFER");
        return;
    }
    // Determine the number of elements the bound buffer can hold, given the offset, size, type and stride.
    auto typeSize = sizeInBytes(type);
    if (!typeSize) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "vertexAttribIPointer", "invalid type");
        return;
    }
    if ((stride % typeSize) || (static_cast<GCGLintptr>(offset) % typeSize)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "vertexAttribIPointer", "stride or offset not valid for type");
        return;
    }
    GCGLsizei bytesPerElement = size * typeSize;

    m_boundVertexArrayObject->setVertexAttribState(locker, index, bytesPerElement, size, type, false, stride, static_cast<GCGLintptr>(offset), true, m_boundArrayBuffer.get());
    m_context->vertexAttribIPointer(index, size, type, stride, offset);
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
    for (GCGLsizei i = 0; i < n; ++i) {
        switch (bufs[i]) {
        case GraphicsContextGL::NONE:
        case GraphicsContextGL::BACK:
        case GraphicsContextGL::COLOR_ATTACHMENT0:
            continue;
        default:
            if (bufs[i] > GraphicsContextGL::COLOR_ATTACHMENT0
                && bufs[i] < GraphicsContextGL::COLOR_ATTACHMENT0 + getMaxColorAttachments()) {
                continue;
            }
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "drawBuffers", "invalid buffer");
            return;
        }
    }
    if (!m_framebufferBinding) {
        if (n != 1) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "drawBuffers", "more than one buffer");
            return;
        }
        if (bufs[0] != GraphicsContextGL::BACK && bufs[0] != GraphicsContextGL::NONE) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "drawBuffers", "BACK or NONE");
            return;
        }
        // Because the backbuffer is simulated on all current WebKit ports, we need to change BACK to COLOR_ATTACHMENT0.
        GCGLenum value[1] { (bufs[0] == GraphicsContextGL::BACK) ? GraphicsContextGL::COLOR_ATTACHMENT0 : GraphicsContextGL::NONE };
        m_context->drawBuffers(value);
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

void WebGL2RenderingContext::clearBufferiv(GCGLenum buffer, GCGLint drawbuffer, Int32List&& values, GCGLuint srcOffset)
{
    if (isContextLostOrPending())
        return;
    auto data = validateClearBuffer("clearBufferiv", buffer, values, srcOffset);
    if (!data)
        return;

    m_context->clearBufferiv(buffer, drawbuffer, data.value());
    updateBuffersToAutoClear(ClearBufferCaller::ClearBufferiv, buffer, drawbuffer);
}

void WebGL2RenderingContext::clearBufferuiv(GCGLenum buffer, GCGLint drawbuffer, Uint32List&& values, GCGLuint srcOffset)
{
    if (isContextLostOrPending())
        return;
    auto data = validateClearBuffer("clearBufferuiv", buffer, values, srcOffset);
    if (!data)
        return;
    m_context->clearBufferuiv(buffer, drawbuffer, data.value());
    updateBuffersToAutoClear(ClearBufferCaller::ClearBufferuiv, buffer, drawbuffer);
}

void WebGL2RenderingContext::clearBufferfv(GCGLenum buffer, GCGLint drawbuffer, Float32List&& values, GCGLuint srcOffset)
{
    if (isContextLostOrPending())
        return;
    auto data = validateClearBuffer("clearBufferfv", buffer, values, srcOffset);
    if (!data)
        return;

    m_context->clearBufferfv(buffer, drawbuffer, data.value());
    // clearBufferiv and clearBufferuiv will currently generate an error
    // if they're called against the default back buffer. If support for
    // extended canvas color spaces is added, this call might need to be
    // added to the other versions.
    markContextChanged();
    updateBuffersToAutoClear(ClearBufferCaller::ClearBufferfv, buffer, drawbuffer);
}

void WebGL2RenderingContext::clearBufferfi(GCGLenum buffer, GCGLint drawbuffer, GCGLfloat depth, GCGLint stencil)
{
    if (isContextLostOrPending())
        return;

    m_context->clearBufferfi(buffer, drawbuffer, depth, stencil);
    // This might have been used to clear the depth and stencil buffers
    // of the default back buffer.
    markContextChanged();
    updateBuffersToAutoClear(ClearBufferCaller::ClearBufferfi, buffer, drawbuffer);
}

RefPtr<WebGLQuery> WebGL2RenderingContext::createQuery()
{
    if (isContextLostOrPending())
        return nullptr;

    auto query = WebGLQuery::create(*this);
    addSharedObject(query.get());
    return query;
}

void WebGL2RenderingContext::deleteQuery(WebGLQuery* query)
{
    Locker locker { objectGraphLock() };

    if (isContextLostOrPending() || !query || !query->object() || !validateWebGLObject("deleteQuery", query))
        return;
    if (query->target() && query == m_activeQueries.get(query->target())) {
        m_context->endQuery(query->target());
        m_activeQueries.remove(query->target());
    }
    deleteObject(locker, query);
}

GCGLboolean WebGL2RenderingContext::isQuery(WebGLQuery* query)
{
    if (isContextLostOrPending() || !query || !query->validate(contextGroup(), *this))
        return false;

    if (query->isDeleted())
        return false;

    return m_context->isQuery(query->object());
}

bool WebGL2RenderingContext::validateQueryTarget(const char* functionName, GCGLenum target, GCGLenum* targetKey)
{
    if (target != GraphicsContextGL::ANY_SAMPLES_PASSED && target != GraphicsContextGL::ANY_SAMPLES_PASSED_CONSERVATIVE && target != GraphicsContextGL::TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
        return false;
    }
    if (targetKey)
        *targetKey = (target == GraphicsContextGL::ANY_SAMPLES_PASSED_CONSERVATIVE) ? GraphicsContextGL::ANY_SAMPLES_PASSED : target;
    return true;
}

void WebGL2RenderingContext::beginQuery(GCGLenum target, WebGLQuery& query)
{
    Locker locker { objectGraphLock() };

    GCGLenum targetKey;
    if (!validateWebGLObject("beginQuery", &query) || !validateQueryTarget("beginQuery", target, &targetKey))
        return;

    if (query.target() && query.target() != target) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginQuery", "query type does not match target");
        return;
    }

    auto addResult = m_activeQueries.add(targetKey, makeRefPtr(&query));

    // Only one query object can be active per target.
    if (!addResult.isNewEntry) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginQuery", "Query object of target is already active");
        return;
    }

    m_context->beginQuery(target, query.object());
    query.setTarget(target);
}

void WebGL2RenderingContext::endQuery(GCGLenum target)
{
    Locker locker { objectGraphLock() };

    GCGLenum targetKey;
    if (isContextLostOrPending() || !scriptExecutionContext() || !validateQueryTarget("beginQuery", target, &targetKey))
        return;

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

RefPtr<WebGLQuery> WebGL2RenderingContext::getQuery(GCGLenum target, GCGLenum pname)
{
    GCGLenum targetKey;
    if (isContextLostOrPending() || !scriptExecutionContext() || !validateQueryTarget("beginQuery", target, &targetKey))
        return nullptr;

    if (pname != GraphicsContextGL::CURRENT_QUERY) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getQuery", "invalid parameter name");
        return nullptr;
    }

    auto query = m_activeQueries.get(targetKey);
    if (!query || query->target() != target)
        return nullptr;
    return query;
}

WebGLAny WebGL2RenderingContext::getQueryParameter(WebGLQuery& query, GCGLenum pname)
{
    if (!validateWebGLObject("getQueryParameter", &query))
        return nullptr;

    switch (pname) {
    case GraphicsContextGL::QUERY_RESULT:
        if (!query.isResultAvailable())
            return false;
        break;
    case GraphicsContextGL::QUERY_RESULT_AVAILABLE:
        if (!query.isResultAvailable())
            return false;
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getQueryParameter", "Invalid pname");
        return nullptr;
    }

    return static_cast<unsigned>(m_context->getQueryObjectui(query.object(), pname));
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
    Locker locker { objectGraphLock() };

    if (isContextLostOrPending())
        return;

    // One sampler can be bound to multiple texture units.
    if (sampler) {
        for (auto& samplerSlot : m_boundSamplers) {
            if (samplerSlot == sampler)
                samplerSlot = nullptr;
        }
    }

    deleteObject(locker, sampler);
}

GCGLboolean WebGL2RenderingContext::isSampler(WebGLSampler* sampler)
{
    if (isContextLostOrPending() || !sampler || !sampler->validate(contextGroup(), *this) || sampler->isDeleted())
        return false;

    return m_context->isSampler(sampler->object());
}

void WebGL2RenderingContext::bindSampler(GCGLuint unit, WebGLSampler* sampler)
{
    Locker locker { objectGraphLock() };

    if (isContextLostOrPending())
        return;
    
    if (unit >= m_boundSamplers.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bindSampler", "invalid texture unit");
        return;
    }

    if (sampler && sampler->isDeleted()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "bindSampler", "cannot bind a deleted Sampler object");
        return;
    }

    if (m_boundSamplers[unit] == sampler)
        return;
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
    case GraphicsContextGL::TEXTURE_WRAP_T:
        return m_context->getSamplerParameteri(sampler.object(), pname);
    case GraphicsContextGL::TEXTURE_MAX_LOD:
    case GraphicsContextGL::TEXTURE_MIN_LOD:
        return m_context->getSamplerParameterf(sampler.object(), pname);
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getSamplerParameter", "Invalid pname");
        return nullptr;
    }
}

RefPtr<WebGLSync> WebGL2RenderingContext::fenceSync(GCGLenum condition, GCGLbitfield flags)
{
    if (isContextLostOrPending())
        return nullptr;

    if (condition != GraphicsContextGL::SYNC_GPU_COMMANDS_COMPLETE) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "fenceSync", "condition must be SYNC_GPU_COMMANDS_COMPLETE");
        return nullptr;
    }
    if (flags) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "fenceSync", "flags must be zero");
        return nullptr;
    }
    auto sync = WebGLSync::create(*this);
    sync->scheduleAllowCacheUpdate(*this);
    addSharedObject(sync.get());
    return sync;
}

GCGLboolean WebGL2RenderingContext::isSync(WebGLSync* sync)
{
    if (isContextLostOrPending() || !sync || !sync->validate(contextGroup(), *this))
        return false;

    if (sync->isDeleted())
        return false;

    return !!sync->object();
}

void WebGL2RenderingContext::deleteSync(WebGLSync* sync)
{
    Locker locker { objectGraphLock() };

    deleteObject(locker, sync);
}

GCGLenum WebGL2RenderingContext::clientWaitSync(WebGLSync& sync, GCGLbitfield flags, GCGLuint64 timeout)
{
    if (!validateWebGLObject("clientWaitSync", &sync))
        return GraphicsContextGL::WAIT_FAILED_WEBGL;

    if (timeout > MaxClientWaitTimeout) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "clientWaitSync", "timeout > MAX_CLIENT_WAIT_TIMEOUT_WEBGL");
        return GraphicsContextGL::WAIT_FAILED_WEBGL;
    }
    
    if (flags && flags != GraphicsContextGL::SYNC_FLUSH_COMMANDS_BIT) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "clientWaitSync", "invalid flags");
        return GraphicsContextGL::WAIT_FAILED_WEBGL;
    }

    if (sync.isSignaled())
        return GraphicsContextGL::ALREADY_SIGNALED;
    if (flags & GraphicsContextGL::SYNC_FLUSH_COMMANDS_BIT)
        flush();
    sync.updateCache(*this);
    if (sync.isSignaled())
        return GraphicsContextGL::CONDITION_SATISFIED;
    return GraphicsContextGL::TIMEOUT_EXPIRED;
}

void WebGL2RenderingContext::waitSync(WebGLSync& sync, GCGLbitfield flags, GCGLint64 timeout)
{
    if (!validateWebGLObject("waitSync", &sync))
        return;

    if (flags)
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "waitSync", "flags must be zero");
    else if (timeout != -1)
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "waitSync", "invalid timeout");

    // This function is a no-op in WebGL 2.
}

WebGLAny WebGL2RenderingContext::getSyncParameter(WebGLSync& sync, GCGLenum pname)
{
    if (!validateWebGLObject("getSyncParameter", &sync))
        return nullptr;

    switch (pname) {
    case GraphicsContextGL::OBJECT_TYPE:
    case GraphicsContextGL::SYNC_STATUS:
    case GraphicsContextGL::SYNC_CONDITION:
    case GraphicsContextGL::SYNC_FLAGS:
        sync.updateCache(*this);
        return sync.getCachedResult(pname);
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getSyncParameter", "invalid parameter name");
        return nullptr;
    }
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
    Locker locker { objectGraphLock() };

    // We have to short-circuit the deletion process if the transform feedback is
    // active. This requires duplication of some validation logic.
    if (isContextLostOrPending() && feedbackObject && feedbackObject->validate(contextGroup(), *this)) {
        if (feedbackObject->isActive()) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "deleteTransformFeedback", "attempt to delete an active transform feedback object");
            return;
        }
    }

    ASSERT(feedbackObject != m_defaultTransformFeedback);

    if (!deleteObject(locker, feedbackObject))
        return;

    if (m_boundTransformFeedback == feedbackObject)
        m_boundTransformFeedback = m_defaultTransformFeedback;
}

GCGLboolean WebGL2RenderingContext::isTransformFeedback(WebGLTransformFeedback* feedbackObject)
{
    if (isContextLostOrPending() || !feedbackObject || !feedbackObject->validate(contextGroup(), *this))
        return false;

    if (!feedbackObject->hasEverBeenBound())
        return false;
    if (feedbackObject->isDeleted())
        return false;

    return m_context->isTransformFeedback(feedbackObject->object());
}

void WebGL2RenderingContext::bindTransformFeedback(GCGLenum target, WebGLTransformFeedback* feedbackObject)
{
    Locker locker { objectGraphLock() };

    if (!validateNullableWebGLObject("bindTransformFeedback", feedbackObject))
        return;

    if (target != GraphicsContextGL::TRANSFORM_FEEDBACK) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "bindTransformFeedback", "target must be TRANSFORM_FEEDBACK");
        return;
    }
    if (m_boundTransformFeedback->isActive() && !m_boundTransformFeedback->isPaused()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "bindTransformFeedback", "transform feedback is active and not paused");
        return;
    }

    auto toBeBound = feedbackObject ? feedbackObject : m_defaultTransformFeedback.get();
    toBeBound->setHasEverBeenBound();
    m_context->bindTransformFeedback(target, toBeBound->object());
    m_boundTransformFeedback = toBeBound;
}

static bool ValidateTransformFeedbackPrimitiveMode(GCGLenum mode)
{
    switch (mode) {
    case GraphicsContextGL::POINTS:
    case GraphicsContextGL::LINES:
    case GraphicsContextGL::TRIANGLES:
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

    Locker locker { objectGraphLock() };
    m_boundTransformFeedback->setProgram(locker, *m_currentProgram);
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
    if (!validateWebGLProgramOrShader("transformFeedbackVaryings", &program))
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
    if (!validateWebGLProgramOrShader("getTransformFeedbackVarying", &program))
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
    Locker locker { objectGraphLock() };

    if (!validateNullableWebGLObject(functionName, buffer))
        return false;

    switch (target) {
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER:
        if (index >= m_maxTransformFeedbackSeparateAttribs) {
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

    if (!validateAndCacheBufferBinding(locker, functionName, target, buffer))
        return false;

    switch (target) {
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER:
        m_boundTransformFeedback->setBoundIndexedTransformFeedbackBuffer(locker, index, buffer);
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
    case GraphicsContextGL::UNIFORM_BUFFER_START:
        return m_context->getInteger64i(target, index);
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

std::optional<Vector<GCGLuint>> WebGL2RenderingContext::getUniformIndices(WebGLProgram& program, const Vector<String>& names)
{
    if (!validateWebGLProgramOrShader("getUniformIndices", &program))
        return std::nullopt;
    return m_context->getUniformIndices(program.object(), names);
}

WebGLAny WebGL2RenderingContext::getActiveUniforms(WebGLProgram& program, const Vector<GCGLuint>& uniformIndices, GCGLenum pname)
{
    if (!validateWebGLProgramOrShader("getActiveUniforms", &program))
        return nullptr;

    switch (pname) {
    case GraphicsContextGL::UNIFORM_TYPE: {
        auto result = m_context->getActiveUniforms(program.object(), uniformIndices, pname);
        return result.map([](auto x) { return static_cast<GCGLenum>(x); });
    }
    case GraphicsContextGL::UNIFORM_SIZE: {
        auto result = m_context->getActiveUniforms(program.object(), uniformIndices, pname);
        return result.map([](auto x) { return static_cast<GCGLuint>(x); });
    }
    case GraphicsContextGL::UNIFORM_BLOCK_INDEX:
    case GraphicsContextGL::UNIFORM_OFFSET:
    case GraphicsContextGL::UNIFORM_ARRAY_STRIDE:
    case GraphicsContextGL::UNIFORM_MATRIX_STRIDE:
        return m_context->getActiveUniforms(program.object(), uniformIndices, pname);
    case GraphicsContextGL::UNIFORM_IS_ROW_MAJOR: {
        auto result = m_context->getActiveUniforms(program.object(), uniformIndices, pname);
        return result.map([](auto x) { return static_cast<bool>(x); });
    }
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getActiveUniforms", "invalid parameter name");
        return nullptr;
    }
}

GCGLuint WebGL2RenderingContext::getUniformBlockIndex(WebGLProgram& program, const String& uniformBlockName)
{
    if (!validateWebGLProgramOrShader("getUniformBlockIndex", &program))
        return 0;
    return m_context->getUniformBlockIndex(program.object(), uniformBlockName);
}

WebGLAny WebGL2RenderingContext::getActiveUniformBlockParameter(WebGLProgram& program, GCGLuint uniformBlockIndex, GCGLenum pname)
{
    if (!validateWebGLProgramOrShader("getActiveUniformBlockParameter", &program))
        return nullptr;
    switch (pname) {
    case GraphicsContextGL::UNIFORM_BLOCK_BINDING:
    case GraphicsContextGL::UNIFORM_BLOCK_DATA_SIZE:
    case GraphicsContextGL::UNIFORM_BLOCK_ACTIVE_UNIFORMS:
        return static_cast<GCGLuint>(m_context->getActiveUniformBlocki(program.object(), uniformBlockIndex, pname));
    case GraphicsContextGL::UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES: {
        GCGLint size = m_context->getActiveUniformBlocki(program.object(), uniformBlockIndex, GraphicsContextGL::UNIFORM_BLOCK_ACTIVE_UNIFORMS);
        Vector<GCGLint> params(size, 0);
        m_context->getActiveUniformBlockiv(program.object(), uniformBlockIndex, pname, params);
        return Uint32Array::tryCreate(reinterpret_cast<GCGLuint*>(params.data()), params.size());
    }
    case GraphicsContextGL::UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER:
    case GraphicsContextGL::UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER:
        return static_cast<bool>(m_context->getActiveUniformBlocki(program.object(), uniformBlockIndex, pname));
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getActiveUniformBlockParameter", "invalid parameter name");
        return nullptr;
    }
}

WebGLAny WebGL2RenderingContext::getActiveUniformBlockName(WebGLProgram& program, GCGLuint index)
{
    if (!validateWebGLProgramOrShader("getActiveUniformBlockName", &program))
        return String();
    if (!program.getLinkStatus()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getActiveUniformBlockName", "program not linked");
        return nullptr;
    }
    String name = m_context->getActiveUniformBlockName(program.object(), index);
    if (name.isNull())
        return nullptr;
    return name;
}

void WebGL2RenderingContext::uniformBlockBinding(WebGLProgram& program, GCGLuint uniformBlockIndex, GCGLuint uniformBlockBinding)
{
    if (!validateWebGLProgramOrShader("uniformBlockBinding", &program))
        return;
    m_context->uniformBlockBinding(program.object(), uniformBlockIndex, uniformBlockBinding);
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
    Locker locker { objectGraphLock() };

    // validateWebGLObject generates an error if the object has already been
    // deleted, so we must replicate most of its checks here.
    if (!arrayObject || isContextLost())
        return;

    if (!arrayObject->validate(contextGroup(), *this)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "deleteVertexArray", "object does not belong to this context");
        return;
    }

    if (arrayObject->isDeleted())
        return;

    if (!arrayObject->isDefaultObject() && arrayObject == m_boundVertexArrayObject) {
        // bindVertexArray grabs the lock internally.
        DropLockForScope noLockScope(locker);
        bindVertexArray(nullptr); // The default VAO was removed in OpenGL 3.3 but not from WebGL 2; bind the default for WebGL to use.
    }

    arrayObject->deleteObject(locker, graphicsContextGL());
}

GCGLboolean WebGL2RenderingContext::isVertexArray(WebGLVertexArrayObject* arrayObject)
{
    if (!arrayObject || isContextLost() || !arrayObject->validate(0, *this))
        return false;

    if (!arrayObject->hasEverBeenBound())
        return false;
    if (arrayObject->isDeleted())
        return false;

    return m_context->isVertexArray(arrayObject->object());
}

void WebGL2RenderingContext::bindVertexArray(WebGLVertexArrayObject* arrayObject)
{
    Locker locker { objectGraphLock() };

    if (!validateNullableWebGLObject("bindVertexArray", arrayObject))
        return;

    if (arrayObject && !arrayObject->isDefaultObject() && arrayObject->object()) {
        m_context->bindVertexArray(arrayObject->object());

        arrayObject->setHasEverBeenBound();
        setBoundVertexArrayObject(locker, arrayObject);
    } else {
        m_context->bindVertexArray(m_defaultVertexArrayObject->object());
        setBoundVertexArrayObject(locker, m_defaultVertexArrayObject.get());
    }
}

WebGLExtension* WebGL2RenderingContext::getExtension(const String& name)
{
    if (isContextLostOrPending())
        return nullptr;

#define ENABLE_IF_REQUESTED(type, variable, nameLiteral, canEnable) \
    if (equalIgnoringASCIICase(name, nameLiteral)) { \
        if (!variable) { \
            variable = (canEnable) ? adoptRef(new type(*this)) : nullptr; \
            if (variable != nullptr) \
                InspectorInstrumentation::didEnableExtension(*this, name); \
        } \
        return variable.get(); \
    }

    ENABLE_IF_REQUESTED(EXTTextureCompressionRGTC, m_extTextureCompressionRGTC, "EXT_texture_compression_rgtc", enableSupportedExtension("GL_EXT_texture_compression_rgtc"_s));
    ENABLE_IF_REQUESTED(EXTTextureFilterAnisotropic, m_extTextureFilterAnisotropic, "EXT_texture_filter_anisotropic", enableSupportedExtension("GL_EXT_texture_filter_anisotropic"_s));
    ENABLE_IF_REQUESTED(OESTextureFloatLinear, m_oesTextureFloatLinear, "OES_texture_float_linear", enableSupportedExtension("GL_OES_texture_float_linear"_s));
    ENABLE_IF_REQUESTED(WebGLLoseContext, m_webglLoseContext, "WEBGL_lose_context", true);
    ENABLE_IF_REQUESTED(WebGLCompressedTextureASTC, m_webglCompressedTextureASTC, "WEBGL_compressed_texture_astc", WebGLCompressedTextureASTC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureATC, m_webglCompressedTextureATC, "WEBKIT_WEBGL_compressed_texture_atc", WebGLCompressedTextureATC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureETC, m_webglCompressedTextureETC, "WEBGL_compressed_texture_etc", WebGLCompressedTextureETC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureETC1, m_webglCompressedTextureETC1, "WEBGL_compressed_texture_etc1", WebGLCompressedTextureETC1::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTexturePVRTC, m_webglCompressedTexturePVRTC, "WEBKIT_WEBGL_compressed_texture_pvrtc", WebGLCompressedTexturePVRTC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTexturePVRTC, m_webglCompressedTexturePVRTC, "WEBGL_compressed_texture_pvrtc", WebGLCompressedTexturePVRTC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureS3TC, m_webglCompressedTextureS3TC, "WEBGL_compressed_texture_s3tc", WebGLCompressedTextureS3TC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureS3TCsRGB, m_webglCompressedTextureS3TCsRGB, "WEBGL_compressed_texture_s3tc_srgb", WebGLCompressedTextureS3TCsRGB::supported(*this));
    ENABLE_IF_REQUESTED(WebGLDebugRendererInfo, m_webglDebugRendererInfo, "WEBGL_debug_renderer_info", true);
    ENABLE_IF_REQUESTED(WebGLDebugShaders, m_webglDebugShaders, "WEBGL_debug_shaders", m_context->getExtensions().supports("GL_ANGLE_translated_shader_source"_s));
    ENABLE_IF_REQUESTED(EXTColorBufferFloat, m_extColorBufferFloat, "EXT_color_buffer_float", EXTColorBufferFloat::supported(*this));
    ENABLE_IF_REQUESTED(EXTColorBufferHalfFloat, m_extColorBufferHalfFloat, "EXT_color_buffer_half_float", EXTColorBufferHalfFloat::supported(*this));
    ENABLE_IF_REQUESTED(EXTFloatBlend, m_extFloatBlend, "EXT_float_blend", EXTFloatBlend::supported(*this));
    ENABLE_IF_REQUESTED(KHRParallelShaderCompile, m_khrParallelShaderCompile, "KHR_parallel_shader_compile", KHRParallelShaderCompile::supported(*this));
    ENABLE_IF_REQUESTED(WebGLMultiDraw, m_webglMultiDraw, "WEBGL_multi_draw", true);
    return nullptr;
}

std::optional<Vector<String>> WebGL2RenderingContext::getSupportedExtensions()
{
    if (isContextLost())
        return std::nullopt;

    Vector<String> result;

    if (m_isPendingPolicyResolution)
        return result;

    auto& extensions = m_context->getExtensions();
    if (extensions.supports("GL_OES_texture_float_linear"_s))
        result.append("OES_texture_float_linear"_s);
    if (extensions.supports("GL_EXT_texture_compression_rgtc"_s))
        result.append("EXT_texture_compression_rgtc"_s);
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
    if (WebGLCompressedTexturePVRTC::supported(*this)) {
        result.append("WEBKIT_WEBGL_compressed_texture_pvrtc"_s);
        result.append("WEBGL_compressed_texture_pvrtc"_s);
    }
    if (WebGLCompressedTextureS3TC::supported(*this))
        result.append("WEBGL_compressed_texture_s3tc"_s);
    if (WebGLCompressedTextureS3TCsRGB::supported(*this))
        result.append("WEBGL_compressed_texture_s3tc_srgb"_s);
    result.append("WEBGL_lose_context"_s);
    if (extensions.supports("GL_ANGLE_translated_shader_source"_s))
        result.append("WEBGL_debug_shaders"_s);
    result.append("WEBGL_debug_renderer_info"_s);
    if (EXTColorBufferFloat::supported(*this))
        result.append("EXT_color_buffer_float"_s);
    if (EXTColorBufferHalfFloat::supported(*this))
        result.append("EXT_color_buffer_half_float"_s);
    if (EXTFloatBlend::supported(*this))
        result.append("EXT_float_blend"_s);
    if (KHRParallelShaderCompile::supported(*this))
        result.append("KHR_parallel_shader_compile"_s);
    if (WebGLMultiDraw::supported(*this))
        result.append("WEBGL_multi_draw"_s);

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
        if (!validateDefaultFramebufferAttachment(attachment)) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid attachment");
            return nullptr;
        }

        // We can look directly at m_attributes because in WebGL 2,
        // they are required to be honored.
        bool hasDepth = m_attributes.depth;
        bool hasStencil = m_attributes.stencil;
        bool missingImage = (attachment == GraphicsContextGL::DEPTH && !hasDepth) || (attachment == GraphicsContextGL::STENCIL && !hasStencil);
        if (missingImage) {
            switch (pname) {
            case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
                return static_cast<unsigned>(GraphicsContextGL::NONE);
            default:
                synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "invalid parameter name");
                return nullptr;
            }
        }

        switch (pname) {
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return GraphicsContextGL::FRAMEBUFFER_DEFAULT;
        default:
            return m_context->getFramebufferAttachmentParameteri(target, attachment, pname);
        }
    }

#if ENABLE(WEBXR)
    if (targetFramebuffer->isOpaque()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getFramebufferAttachmentParameter", "An opaque framebuffer's attachments cannot be inspected or changed");
        return nullptr;
    }
#endif

    if (!validateNonDefaultFramebufferAttachment(functionName, attachment))
        return nullptr;

    RefPtr<WebGLSharedObject> attachmentObject;
    if (attachment == GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT) {
        attachmentObject = makeRefPtr(targetFramebuffer->getAttachmentObject(GraphicsContextGL::DEPTH_ATTACHMENT));
        auto stencilAttachment = makeRefPtr(targetFramebuffer->getAttachmentObject(GraphicsContextGL::STENCIL_ATTACHMENT));
        if (attachmentObject != stencilAttachment) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "different objects bound to DEPTH_ATTACHMENT and STENCIL_ATTACHMENT");
            return nullptr;
        }
    } else
        attachmentObject = makeRefPtr(targetFramebuffer->getAttachmentObject(attachment));

    if (!attachmentObject) {
        if (pname == GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
            return static_cast<unsigned>(GraphicsContextGL::NONE);
        if (pname == GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME)
            return nullptr;
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "invalid parameter name");
        return nullptr;
    }

    switch (pname) {
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        if (attachmentObject->isTexture())
            return static_cast<unsigned>(GraphicsContextGL::TEXTURE);
        return static_cast<unsigned>(GraphicsContextGL::RENDERBUFFER);
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        if (attachmentObject->isTexture())
            return makeRefPtr(reinterpret_cast<WebGLTexture&>(*attachmentObject));
        return makeRefPtr(reinterpret_cast<WebGLRenderbuffer&>(*attachmentObject));
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        if (!attachmentObject->isTexture())
            break;
        FALLTHROUGH;
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_RED_SIZE:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
        return m_context->getFramebufferAttachmentParameteri(target, attachment, pname);
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
        if (attachment == GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "COMPONENT_TYPE can't be queried for DEPTH_STENCIL_ATTACHMENT");
            return nullptr;
        }
        return m_context->getFramebufferAttachmentParameteri(target, attachment, pname);
    }

    synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid parameter name for attachment");
    return nullptr;
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
        m_maxDrawBuffers = m_context->getInteger(GraphicsContextGL::MAX_DRAW_BUFFERS);
    return m_maxDrawBuffers;
}

GCGLint WebGL2RenderingContext::getMaxColorAttachments()
{
    // DrawBuffers requires MAX_COLOR_ATTACHMENTS == MAX_DRAW_BUFFERS
    if (!m_maxColorAttachments)
        m_maxColorAttachments = m_context->getInteger(GraphicsContextGL::MAX_DRAW_BUFFERS);
    return m_maxColorAttachments;
}

void WebGL2RenderingContext::renderbufferStorageImpl(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, const char* functionName)
{
    switch (internalformat) {
    case GraphicsContextGL::R8UI:
    case GraphicsContextGL::R8I:
    case GraphicsContextGL::R16UI:
    case GraphicsContextGL::R16I:
    case GraphicsContextGL::R32UI:
    case GraphicsContextGL::R32I:
    case GraphicsContextGL::RG8UI:
    case GraphicsContextGL::RG8I:
    case GraphicsContextGL::RG16UI:
    case GraphicsContextGL::RG16I:
    case GraphicsContextGL::RG32UI:
    case GraphicsContextGL::RG32I:
    case GraphicsContextGL::RGBA8UI:
    case GraphicsContextGL::RGBA8I:
    case GraphicsContextGL::RGB10_A2UI:
    case GraphicsContextGL::RGBA16UI:
    case GraphicsContextGL::RGBA16I:
    case GraphicsContextGL::RGBA32UI:
    case GraphicsContextGL::RGBA32I:
        if (samples > 0) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "for integer formats, samples > 0 is not allowed");
            return;
        }
        FALLTHROUGH;
    case GraphicsContextGL::R8:
    case GraphicsContextGL::RG8:
    case GraphicsContextGL::RGB8:
    case GraphicsContextGL::RGB565:
    case GraphicsContextGL::RGBA8:
    case GraphicsContextGL::SRGB8_ALPHA8:
    case GraphicsContextGL::RGB5_A1:
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RGB10_A2:
    case GraphicsContextGL::DEPTH_COMPONENT16:
    case GraphicsContextGL::DEPTH_COMPONENT24:
    case GraphicsContextGL::DEPTH_COMPONENT32F:
    case GraphicsContextGL::DEPTH24_STENCIL8:
    case GraphicsContextGL::DEPTH32F_STENCIL8:
    case GraphicsContextGL::STENCIL_INDEX8:
        renderbufferStorageHelper(target, samples, internalformat, width, height);
        break;
    case GraphicsContextGL::DEPTH_STENCIL:
        // To be WebGL 1 backward compatible.
        if (samples) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid internalformat");
            return;
        }
        renderbufferStorageHelper(target, 0, GraphicsContextGL::DEPTH24_STENCIL8, width, height);
        break;
    case GraphicsContextGL::R16F:
    case GraphicsContextGL::RG16F:
    case GraphicsContextGL::RGBA16F:
        if (!extensionIsEnabled("EXT_color_buffer_float"_s) && !extensionIsEnabled("EXT_color_buffer_half_float"_s)) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "EXT_color_buffer_float or EXT_color_buffer_half_float not enabled");
            return;
        }
        renderbufferStorageHelper(target, samples, internalformat, width, height);
        break;
    case GraphicsContextGL::R32F:
    case GraphicsContextGL::RG32F:
    case GraphicsContextGL::RGBA32F:
    case GraphicsContextGL::R11F_G11F_B10F:
        if (!extensionIsEnabled("EXT_color_buffer_float"_s)) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "EXT_color_buffer_float not enabled");
            return;
        }
        renderbufferStorageHelper(target, samples, internalformat, width, height);
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid internalformat");
        return;
    }
    m_renderbufferBinding->setInternalFormat(internalformat);
    m_renderbufferBinding->setSize(width, height);
    m_renderbufferBinding->setIsValid(true);
}

void WebGL2RenderingContext::renderbufferStorageHelper(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (samples)
        m_context->renderbufferStorageMultisample(target, samples, internalformat, width, height);
    else
        m_context->renderbufferStorage(target, internalformat, width, height);
}

GCGLuint WebGL2RenderingContext::maxTransformFeedbackSeparateAttribs() const
{
    return m_maxTransformFeedbackSeparateAttribs;
}

WebGLRenderingContextBase::PixelStoreParams WebGL2RenderingContext::getPackPixelStoreParams() const
{
    PixelStoreParams params;
    params.alignment = m_packAlignment;
    params.rowLength = m_packRowLength;
    params.skipPixels = m_packSkipPixels;
    params.skipRows = m_packSkipRows;
    return params;
}

WebGLRenderingContextBase::PixelStoreParams WebGL2RenderingContext::getUnpackPixelStoreParams(TexImageDimension dimension) const
{
    PixelStoreParams params;
    params.alignment = m_unpackAlignment;
    params.rowLength = m_unpackRowLength;
    params.skipPixels = m_unpackSkipPixels;
    params.skipRows = m_unpackSkipRows;
    if (dimension == TexImageDimension::Tex3D) {
        params.imageHeight = m_unpackImageHeight;
        params.skipImages = m_unpackSkipImages;
    }
    return params;
}

bool WebGL2RenderingContext::checkAndTranslateAttachments(const char* functionName, GCGLenum target, Vector<GCGLenum>& attachments)
{
    if (!validateFramebufferTarget(target)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
        return false;
    }

    if (!getFramebufferBinding(target)) {
        // For the default framebuffer, translate GL_COLOR/GL_DEPTH/GL_STENCIL.
        // The default framebuffer of WebGL is not fb 0, it is an internal fbo.
        for (size_t i = 0; i < attachments.size(); ++i) {
            switch (attachments[i]) {
            case GraphicsContextGL::COLOR:
                attachments[i] = GraphicsContextGL::COLOR_ATTACHMENT0;
                break;
            case GraphicsContextGL::DEPTH:
                attachments[i] = GraphicsContextGL::DEPTH_ATTACHMENT;
                break;
            case GraphicsContextGL::STENCIL:
                attachments[i] = GraphicsContextGL::STENCIL_ATTACHMENT;
                break;
            default:
                synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid attachment");
                return false;
            }
        }
    }
    return true;
}

void WebGL2RenderingContext::addMembersToOpaqueRoots(JSC::AbstractSlotVisitor& visitor)
{
    WebGLRenderingContextBase::addMembersToOpaqueRoots(visitor);

    Locker locker { objectGraphLock() };
    visitor.addOpaqueRoot(m_readFramebufferBinding.get());
    if (m_readFramebufferBinding)
        m_readFramebufferBinding->addMembersToOpaqueRoots(locker, visitor);

    visitor.addOpaqueRoot(m_boundTransformFeedback.get());
    if (m_boundTransformFeedback)
        m_boundTransformFeedback->addMembersToOpaqueRoots(locker, visitor);

    visitor.addOpaqueRoot(m_boundCopyReadBuffer.get());
    visitor.addOpaqueRoot(m_boundCopyWriteBuffer.get());
    visitor.addOpaqueRoot(m_boundPixelPackBuffer.get());
    visitor.addOpaqueRoot(m_boundPixelUnpackBuffer.get());
    visitor.addOpaqueRoot(m_boundTransformFeedbackBuffer.get());
    visitor.addOpaqueRoot(m_boundUniformBuffer.get());

    for (auto& buffer : m_boundIndexedUniformBuffers)
        visitor.addOpaqueRoot(buffer.get());

    for (auto& entry : m_activeQueries)
        visitor.addOpaqueRoot(entry.value.get());

    for (auto& entry : m_boundSamplers)
        visitor.addOpaqueRoot(entry.get());
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
        if (!scriptExecutionContext()->settingsValues().maskWebGLStringsEnabled)
            return makeString("WebGL GLSL ES 3.00 (", m_context->getString(GraphicsContextGL::SHADING_LANGUAGE_VERSION), ')');
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
        GCGLint value = getIntParameter(pname);
        if (!m_readFramebufferBinding && value != GraphicsContextGL::NONE)
            return static_cast<GCGLint>(GraphicsContextGL::BACK);
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

    std::optional<unsigned> maxIndex = elementArrayBuffer->getCachedMaxIndex(type);
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
    case GraphicsContextGL::RASTERIZER_DISCARD:
        return true;
    default:
        return WebGLRenderingContextBase::validateCapability(functionName, cap);
    }
}

template<typename T, typename TypedArrayType>
std::optional<GCGLSpan<const T>> WebGL2RenderingContext::validateClearBuffer(const char* functionName, GCGLenum buffer, TypedList<TypedArrayType, T>& values, GCGLuint srcOffset)
{
    Checked<GCGLsizei, RecordOverflow> checkedSize(values.length());
    checkedSize -= srcOffset;
    if (checkedSize.hasOverflowed()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid array size / srcOffset");
        return { };
    }
    switch (buffer) {
    case GraphicsContextGL::COLOR:
        if (checkedSize < 4) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid array size / srcOffset");
            return { };
        }
        return makeGCGLSpan(values.data() + srcOffset, 4);
    case GraphicsContextGL::DEPTH:
    case GraphicsContextGL::STENCIL:
        if (checkedSize < 1) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid array size / srcOffset");
            return { };
        }
        return makeGCGLSpan(values.data() + srcOffset, 1);

    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid buffer");
        return { };
    }
}

void WebGL2RenderingContext::uniform1fv(WebGLUniformLocation* location, Float32List data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto result = validateUniformParameters("uniform1fv", location, data, 1, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform1fv(location->location(), result.value());
}

void WebGL2RenderingContext::uniform2fv(WebGLUniformLocation* location, Float32List data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto result = validateUniformParameters("uniform2fv", location, data, 2, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform2fv(location->location(), result.value());
}

void WebGL2RenderingContext::uniform3fv(WebGLUniformLocation* location, Float32List data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto result = validateUniformParameters("uniform3fv", location, data, 3, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform3fv(location->location(), result.value());
}

void WebGL2RenderingContext::uniform4fv(WebGLUniformLocation* location, Float32List data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto result = validateUniformParameters("uniform4fv", location, data, 4, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform4fv(location->location(), result.value());
}

void WebGL2RenderingContext::uniform1iv(WebGLUniformLocation* location, Int32List data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto result = validateUniformParameters("uniform1iv", location, data, 1, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform1iv(location->location(), result.value());
}

void WebGL2RenderingContext::uniform2iv(WebGLUniformLocation* location, Int32List data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto result = validateUniformParameters("uniform2iv", location, data, 2, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform2iv(location->location(), result.value());
}

void WebGL2RenderingContext::uniform3iv(WebGLUniformLocation* location, Int32List data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto result = validateUniformParameters("uniform3iv", location, data, 3, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform3iv(location->location(), result.value());
}

void WebGL2RenderingContext::uniform4iv(WebGLUniformLocation* location, Int32List data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto result = validateUniformParameters("uniform4iv", location, data, 4, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform4iv(location->location(), result.value());
}

void WebGL2RenderingContext::uniformMatrix2fv(WebGLUniformLocation* location, GCGLboolean transpose, Float32List data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto result = validateUniformMatrixParameters("uniformMatrix2fv", location, transpose, data, 2*2, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniformMatrix2fv(location->location(), transpose, result.value());
}

void WebGL2RenderingContext::uniformMatrix3fv(WebGLUniformLocation* location, GCGLboolean transpose, Float32List data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto result = validateUniformMatrixParameters("uniformMatrix3fv", location, transpose, data, 3*3, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniformMatrix3fv(location->location(), transpose, result.value());
}

void WebGL2RenderingContext::uniformMatrix4fv(WebGLUniformLocation* location, GCGLboolean transpose, Float32List data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLostOrPending())
        return;
    auto result = validateUniformMatrixParameters("uniformMatrix4fv", location, transpose, data, 4*4, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniformMatrix4fv(location->location(), transpose, result.value());
}

void WebGL2RenderingContext::readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, ArrayBufferView& pixels)
{
    if (isContextLostOrPending())
        return;
    if (m_boundPixelPackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "readPixels",
            "a buffer is bound to PIXEL_PACK_BUFFER");
        return;
    }
    WebGLRenderingContextBase::readPixels(x, y, width, height, format, type, pixels);
}

void WebGL2RenderingContext::readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    if (isContextLostOrPending())
        return;
    if (!m_boundPixelPackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "readPixels",
            "no buffer is bound to PIXEL_PACK_BUFFER");
        return;
    }
    if (offset < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "readPixels", "negative offset");
        return;
    }
    if (type == GraphicsContextGL::UNSIGNED_INT_24_8) {
        // This type is always invalid for readbacks in WebGL.
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "readPixels", "invalid type UNSIGNED_INT_24_8");
        return;
    }

    // Due to WebGL's same-origin restrictions, it is not possible to
    // taint the origin using the WebGL API.
    ASSERT(canvasBase().originClean());

    clearIfComposited(ClearCallerOther);

    m_context->readnPixels(x, y, width, height, format, type, offset);
}

void WebGL2RenderingContext::readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, ArrayBufferView& dstData, GCGLuint dstOffset)
{
    if (isContextLostOrPending())
        return;
    if (m_boundPixelPackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "readPixels",
            "a buffer is bound to PIXEL_PACK_BUFFER");
        return;
    }
    auto slice = sliceArrayBufferView("readPixels", dstData, dstOffset, 0);
    if (!slice)
        return;
    WebGLRenderingContextBase::readPixels(x, y, width, height, format, type, *slice);
}

#define REMOVE_BUFFER_FROM_BINDING(binding) \
    if (binding == buffer) \
        binding = nullptr;

void WebGL2RenderingContext::uncacheDeletedBuffer(const AbstractLocker& locker, WebGLBuffer* buffer)
{
    ASSERT(buffer);

    REMOVE_BUFFER_FROM_BINDING(m_boundCopyReadBuffer);
    REMOVE_BUFFER_FROM_BINDING(m_boundCopyWriteBuffer);
    REMOVE_BUFFER_FROM_BINDING(m_boundPixelPackBuffer);
    REMOVE_BUFFER_FROM_BINDING(m_boundPixelUnpackBuffer);
    REMOVE_BUFFER_FROM_BINDING(m_boundTransformFeedbackBuffer);
    REMOVE_BUFFER_FROM_BINDING(m_boundUniformBuffer);
    m_boundTransformFeedback->unbindBuffer(locker, *buffer);

    WebGLRenderingContextBase::uncacheDeletedBuffer(locker, buffer);
}

#undef REMOVE_BUFFER_FROM_BINDING

void WebGL2RenderingContext::updateBuffersToAutoClear(ClearBufferCaller caller, GCGLenum buffer, GCGLint drawbuffer)
{
    // This method makes sure that we don't auto-clear any buffers which the
    // user has manually cleared using the new ES 3.0 clearBuffer* APIs.

    // If the user has a framebuffer bound, don't update the auto-clear
    // state of the built-in back buffer.
    if (m_framebufferBinding)
        return;

    // If the scissor test is on, assume that we can't short-circuit
    // these clears.
    if (m_scissorEnabled)
        return;

    // The default back buffer only has one color attachment.
    if (drawbuffer)
        return;

    // If the call to the driver generated an error, don't claim that
    // we've auto-cleared these buffers. The early returns below are for
    // cases where errors will be produced.

    // The default back buffer is currently always RGB(A)8, which
    // restricts the variants which can legally be used to clear the
    // color buffer. TODO(crbug.com/829632): this needs to be
    // generalized.
    switch (caller) {
    case ClearBufferCaller::ClearBufferiv:
        if (buffer != GraphicsContextGL::STENCIL)
            return;
        break;
    case ClearBufferCaller::ClearBufferfv:
        if (buffer != GraphicsContextGL::COLOR && buffer != GraphicsContextGL::DEPTH)
            return;
        break;
    case ClearBufferCaller::ClearBufferuiv:
        return;
    case ClearBufferCaller::ClearBufferfi:
        if (buffer != GraphicsContextGL::DEPTH_STENCIL)
            return;
        break;
    }

    GCGLbitfield buffersToClear = 0;

    // Turn it into a bitfield and mask it off.
    switch (buffer) {
    case GraphicsContextGL::COLOR:
        buffersToClear = GraphicsContextGL::COLOR_BUFFER_BIT;
        break;
    case GraphicsContextGL::DEPTH:
        buffersToClear = GraphicsContextGL::DEPTH_BUFFER_BIT;
        break;
    case GraphicsContextGL::STENCIL:
        buffersToClear = GraphicsContextGL::STENCIL_BUFFER_BIT;
        break;
    case GraphicsContextGL::DEPTH_STENCIL:
        buffersToClear = GraphicsContextGL::DEPTH_BUFFER_BIT | GraphicsContextGL::STENCIL_BUFFER_BIT;
        break;
    default:
        // Illegal value.
        return;
    }

    m_context->setBuffersToAutoClear(m_context->getBuffersToAutoClear() & (~buffersToClear));
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
