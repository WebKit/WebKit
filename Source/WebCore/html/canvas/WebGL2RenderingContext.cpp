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

#include "config.h"
#include "WebGL2RenderingContext.h"

#if ENABLE(WEBGL)

#include "CachedImage.h"
#include "EXTClipControl.h"
#include "EXTColorBufferFloat.h"
#include "EXTColorBufferHalfFloat.h"
#include "EXTConservativeDepth.h"
#include "EXTDepthClamp.h"
#include "EXTDisjointTimerQueryWebGL2.h"
#include "EXTFloatBlend.h"
#include "EXTPolygonOffsetClamp.h"
#include "EXTRenderSnorm.h"
#include "EXTTextureCompressionBPTC.h"
#include "EXTTextureCompressionRGTC.h"
#include "EXTTextureFilterAnisotropic.h"
#include "EXTTextureMirrorClampToEdge.h"
#include "EXTTextureNorm16.h"
#include "EventLoop.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBitmap.h"
#include "ImageData.h"
#include "InspectorInstrumentation.h"
#include "KHRParallelShaderCompile.h"
#include "Logging.h"
#include "NVShaderNoperspectiveInterpolation.h"
#include "OESDrawBuffersIndexed.h"
#include "OESSampleVariables.h"
#include "OESShaderMultisampleInterpolation.h"
#include "OESTextureFloatLinear.h"
#include "OffscreenCanvas.h"
#include "RenderBox.h"
#include "WebCodecsVideoFrame.h"
#include "WebCoreOpaqueRootInlines.h"
#include "WebGLActiveInfo.h"
#include "WebGLBlendFuncExtended.h"
#include "WebGLBuffer.h"
#include "WebGLClipCullDistance.h"
#include "WebGLCompressedTextureASTC.h"
#include "WebGLCompressedTextureETC.h"
#include "WebGLCompressedTextureETC1.h"
#include "WebGLCompressedTexturePVRTC.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLCompressedTextureS3TCsRGB.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLDefaultFramebuffer.h"
#include "WebGLDrawInstancedBaseVertexBaseInstance.h"
#include "WebGLFramebuffer.h"
#include "WebGLLoseContext.h"
#include "WebGLMultiDraw.h"
#include "WebGLMultiDrawInstancedBaseVertexBaseInstance.h"
#include "WebGLPolygonMode.h"
#include "WebGLProgram.h"
#include "WebGLProvokingVertex.h"
#include "WebGLQuery.h"
#include "WebGLRenderSharedExponent.h"
#include "WebGLRenderbuffer.h"
#include "WebGLSampler.h"
#include "WebGLStencilTexturing.h"
#include "WebGLSync.h"
#include "WebGLTexture.h"
#include "WebGLTransformFeedback.h"
#include "WebGLUniformLocation.h"
#include "WebGLUtilities.h"
#include "WebGLVertexArrayObject.h"
#include "WebGLVertexArrayObjectOES.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <JavaScriptCore/SlotVisitor.h>
#include <JavaScriptCore/SlotVisitorInlines.h>
#include <JavaScriptCore/TypedArrayType.h>
#include <algorithm>
#include <wtf/Lock.h>
#include <wtf/Locker.h>
#include <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

const GCGLuint64 MaxClientWaitTimeout = 0u;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WebGL2RenderingContext);

std::unique_ptr<WebGL2RenderingContext> WebGL2RenderingContext::create(CanvasBase& canvas, WebGLContextAttributes&& attributes)
{
    return std::unique_ptr<WebGL2RenderingContext>(new WebGL2RenderingContext(canvas, Type::WebGL2, WTFMove(attributes)));
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
    for (auto& activeQuery : m_activeQueries)
        activeQuery = nullptr;
}

void WebGL2RenderingContext::initializeContextState()
{
    WebGLRenderingContextBase::initializeContextState();

    m_readFramebufferBinding = nullptr;

    m_boundCopyReadBuffer = nullptr;
    m_boundCopyWriteBuffer = nullptr;
    m_boundPixelPackBuffer = nullptr;
    m_boundPixelUnpackBuffer = nullptr;
    m_boundTransformFeedbackBuffer = nullptr;
    m_boundUniformBuffer = nullptr;

    // NEEDS_PORT: boolean occlusion query, transform feedback primitives written query, elapsed query

    m_maxTransformFeedbackSeparateAttribs = m_context->getInteger(GraphicsContextGL::MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS);

    m_defaultTransformFeedback = WebGLTransformFeedback::create(*this);
    m_boundTransformFeedback = m_defaultTransformFeedback;
    if (m_defaultTransformFeedback)
        m_context->bindTransformFeedback(GraphicsContextGL::TRANSFORM_FEEDBACK, m_defaultTransformFeedback->object());

    m_boundIndexedUniformBuffers.resize(m_context->getInteger(GraphicsContextGL::MAX_UNIFORM_BUFFER_BINDINGS));
    m_uniformBufferOffsetAlignment = m_context->getInteger(GraphicsContextGL::UNIFORM_BUFFER_OFFSET_ALIGNMENT);

    m_max3DTextureSize = m_context->getInteger(GraphicsContextGL::MAX_3D_TEXTURE_SIZE);
    m_max3DTextureLevel = WebGLTexture::computeLevelCount(m_max3DTextureSize, m_max3DTextureSize);
    m_maxArrayTextureLayers = m_context->getInteger(GraphicsContextGL::MAX_ARRAY_TEXTURE_LAYERS);

    m_boundSamplers.clear();
    m_boundSamplers.resize(m_textureUnits.size());
}

long long WebGL2RenderingContext::getInt64Parameter(GCGLenum pname)
{
    return m_context->getInteger64(pname);
}

void WebGL2RenderingContext::initializeDefaultObjects()
{
    WebGLRenderingContextBase::initializeDefaultObjects();
    m_defaultVertexArrayObject = WebGLVertexArrayObject::create(*this, WebGLVertexArrayObject::Type::Default);
    m_boundVertexArrayObject = m_defaultVertexArrayObject;
    if (!m_defaultVertexArrayObject)
        return;
    // The default VAO was removed in OpenGL 3.3 but not from WebGL 2; bind the default for WebGL to use.
    m_context->bindVertexArray(m_defaultVertexArrayObject->object());
}

bool WebGL2RenderingContext::validateBufferTarget(ASCIILiteral functionName, GCGLenum target)
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
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target"_s);
        return false;
    }
}

bool WebGL2RenderingContext::validateBufferTargetCompatibility(ASCIILiteral functionName, GCGLenum target, WebGLBuffer* buffer)
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
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "element array buffers can not be bound to a different target"_s);
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
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "buffers bound to non ELEMENT_ARRAY_BUFFER targets can not be bound to ELEMENT_ARRAY_BUFFER target"_s);
            return false;
        }
        break;
    default:
        break;
    }

    return true;
}

WebGLBuffer* WebGL2RenderingContext::validateBufferDataParameters(ASCIILiteral functionName, GCGLenum target, GCGLenum usage)
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
    synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid usage"_s);
    return nullptr;
}

WebGLBuffer* WebGL2RenderingContext::validateBufferDataTarget(ASCIILiteral functionName, GCGLenum target)
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
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target"_s);
        return nullptr;
    }
    if (!buffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no buffer"_s);
        return nullptr;
    }
    if (m_boundTransformFeedback->hasBoundIndexedTransformFeedbackBuffer(buffer)) {
        ASSERT(buffer != m_boundVertexArrayObject->getElementArrayBuffer());
        if (m_boundIndexedUniformBuffers.contains(buffer)
            || m_boundVertexArrayObject->hasArrayBuffer(buffer)
            || buffer == m_boundArrayBuffer
            || buffer == m_boundCopyReadBuffer
            || buffer == m_boundCopyWriteBuffer
            || buffer == m_boundPixelPackBuffer
            || buffer == m_boundPixelUnpackBuffer
            || buffer == m_boundUniformBuffer) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "buffer is bound to an indexed transform feedback binding point and some other binding point"_s);
            return nullptr;
        }
    }
    return buffer;
}

bool WebGL2RenderingContext::validateAndCacheBufferBinding(const AbstractLocker& locker, ASCIILiteral functionName, GCGLenum target, WebGLBuffer* buffer)
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
    return true;
}

IntRect WebGL2RenderingContext::getTextureSourceSubRectangle(GCGLsizei width, GCGLsizei height)
{
    return IntRect(m_unpackParameters.skipPixels, m_unpackParameters.skipRows, width, height);
}

RefPtr<WebGLTexture> WebGL2RenderingContext::validateTexImageBinding(TexImageFunctionID functionID, GCGLenum target)
{
    if (functionID == TexImageFunctionID::TexImage3D || functionID == TexImageFunctionID::TexSubImage3D)
        return validateTexture3DBinding(texImageFunctionName(functionID), target);
    return validateTexture2DBinding(texImageFunctionName(functionID), target);
}

RefPtr<WebGLTexture> WebGL2RenderingContext::validateTextureStorage2DBinding(ASCIILiteral functionName, GCGLenum target)
{
    RefPtr<WebGLTexture> texture;
    switch (target) {
    case GraphicsContextGL::TEXTURE_2D:
        texture = m_textureUnits[m_activeTextureUnit].texture2DBinding;
        break;
    case GraphicsContextGL::TEXTURE_CUBE_MAP:
        texture = m_textureUnits[m_activeTextureUnit].textureCubeMapBinding;
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture target"_s);
        return nullptr;
    }

    if (!texture)
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no texture"_s);
    return texture;
}

RefPtr<WebGLTexture> WebGL2RenderingContext::validateTexture3DBinding(ASCIILiteral functionName, GCGLenum target)
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
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid texture target"_s);
        return nullptr;
    }
    if (!texture)
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no texture bound to target"_s);
    return texture;
}

bool WebGL2RenderingContext::validateTexFuncLayer(ASCIILiteral functionName, GCGLenum texTarget, GCGLint layer)
{
    if (layer < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "layer out of range"_s);
        return false;
    }
    switch (texTarget) {
    case GraphicsContextGL::TEXTURE_3D:
        if (layer > m_max3DTextureSize - 1) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "layer out of range"_s);
            return false;
        }
        break;
    case GraphicsContextGL::TEXTURE_2D_ARRAY:
        if (layer > m_maxArrayTextureLayers - 1) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "layer out of range"_s);
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

RefPtr<ArrayBufferView> WebGL2RenderingContext::arrayBufferViewSliceFactory(ASCIILiteral functionName, const ArrayBufferView& data, unsigned startByte,  unsigned numElements)
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
        synthesizeGLError(GraphicsContextGL::OUT_OF_MEMORY, functionName, "Could not create intermediate ArrayBufferView"_s);

    return slice;
}

RefPtr<ArrayBufferView> WebGL2RenderingContext::sliceArrayBufferView(ASCIILiteral functionName, const ArrayBufferView& data, GCGLuint srcOffset, GCGLuint length)
{
    if (data.getType() == JSC::NotTypedArray) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "Invalid type of Array Buffer View"_s);
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
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "srcOffset or length is out of bounds"_s);
        return nullptr;
    }

    return arrayBufferViewSliceFactory(functionName, data, data.byteOffset() + checkedByteSrcOffset.value(), checkedLength);
}

void WebGL2RenderingContext::pixelStorei(GCGLenum pname, GCGLint param)
{
    if (isContextLost())
        return;
    if (param < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "pixelStorei"_s, "negative value"_s);
        return;
    }
    switch (pname) {
    case GraphicsContextGL::PACK_ROW_LENGTH:
        m_packParameters.rowLength = param;
        return; // Not sent to context, handled locally.
    case GraphicsContextGL::PACK_SKIP_PIXELS:
        m_packParameters.skipPixels = param;
        return; // Not sent to context, handled locally.
    case GraphicsContextGL::PACK_SKIP_ROWS:
        m_packParameters.skipRows = param;
        return; // Not sent to context, handled locally.
    case GraphicsContextGL::UNPACK_ROW_LENGTH:
        m_unpackParameters.rowLength = param;
        break;
    case GraphicsContextGL::UNPACK_IMAGE_HEIGHT:
        m_unpackParameters.imageHeight = param;
        break;
    case GraphicsContextGL::UNPACK_SKIP_PIXELS:
        m_unpackParameters.skipPixels = param;
        break;
    case GraphicsContextGL::UNPACK_SKIP_ROWS:
        m_unpackParameters.skipRows = param;
        break;
    case GraphicsContextGL::UNPACK_SKIP_IMAGES:
        m_unpackParameters.skipImages = param;
        break;
    default:
        WebGLRenderingContextBase::pixelStorei(pname, param);
        return;
    }
    m_context->pixelStorei(pname, param);
}

void WebGL2RenderingContext::bufferData(GCGLenum target, const ArrayBufferView& data, GCGLenum usage, GCGLuint srcOffset, GCGLuint length)
{
    if (auto slice = sliceArrayBufferView("bufferData"_s, data, srcOffset, length))
        WebGLRenderingContextBase::bufferData(target, BufferDataSource(slice.get()), usage);
}

void WebGL2RenderingContext::bufferSubData(GCGLenum target, long long offset, const ArrayBufferView& data, GCGLuint srcOffset, GCGLuint length)
{
    if (auto slice = sliceArrayBufferView("bufferSubData"_s, data, srcOffset, length))
        WebGLRenderingContextBase::bufferSubData(target, offset, BufferDataSource(slice.get()));
}

void WebGL2RenderingContext::copyBufferSubData(GCGLenum readTarget, GCGLenum writeTarget, GCGLint64 readOffset, GCGLint64 writeOffset, GCGLint64 size)
{
    if (isContextLost())
        return;

    RefPtr<WebGLBuffer> readBuffer = validateBufferDataParameters("copyBufferSubData"_s, readTarget, GraphicsContextGL::STATIC_DRAW);
    if (!readBuffer)
        return;

    RefPtr<WebGLBuffer> writeBuffer = validateBufferDataParameters("copyBufferSubData"_s, writeTarget, GraphicsContextGL::STATIC_DRAW);
    if (!writeBuffer)
        return;

    if (readOffset < 0 || writeOffset < 0 || size < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "copyBufferSubData"_s, "offset < 0"_s);
        return;
    }

    if ((readBuffer->getTarget() == GraphicsContextGL::ELEMENT_ARRAY_BUFFER && writeBuffer->getTarget() != GraphicsContextGL::ELEMENT_ARRAY_BUFFER)
        || (writeBuffer->getTarget() == GraphicsContextGL::ELEMENT_ARRAY_BUFFER && readBuffer->getTarget() != GraphicsContextGL::ELEMENT_ARRAY_BUFFER)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "copyBufferSubData"_s, "Cannot copy into an element buffer destination from a non-element buffer source"_s);
        return;
    }

    Checked<GCGLintptr, RecordOverflow> checkedReadOffset(readOffset);
    Checked<GCGLintptr, RecordOverflow> checkedWriteOffset(writeOffset);
    Checked<GCGLsizeiptr, RecordOverflow> checkedSize(size);
    if (checkedReadOffset.hasOverflowed() || checkedWriteOffset.hasOverflowed() || checkedSize.hasOverflowed()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "copyBufferSubData"_s, "Offset or size is too big"_s);
        return;
    }

    m_context->copyBufferSubData(readTarget, writeTarget, checkedReadOffset, checkedWriteOffset, checkedSize);
}

void WebGL2RenderingContext::getBufferSubData(GCGLenum target, long long srcByteOffset, RefPtr<ArrayBufferView>&& dstData, GCGLuint dstOffset, GCGLuint length)
{
    if (isContextLost())
        return;
    RefPtr<WebGLBuffer> buffer = validateBufferDataParameters("getBufferSubData"_s, target, GraphicsContextGL::STATIC_DRAW);
    if (!buffer)
        return;

    // FIXME: Implement "If target is TRANSFORM_FEEDBACK_BUFFER, and any transform feedback object is currently active, an INVALID_OPERATION error is generated."

    if (!dstData) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getBufferSubData"_s, "Null dstData"_s);
        return;
    }

    if (dstData->getType() == JSC::NotTypedArray) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getBufferSubData"_s, "Invalid type of Array Buffer View"_s);
        return;
    }

    auto elementSize = JSC::elementSize(dstData->getType());
    auto dstDataLength = dstData->byteLength() / elementSize;

    if (dstOffset > dstDataLength) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getBufferSubData"_s, "dstOffset is larger than the length of the destination buffer."_s);
        return;
    }

    GCGLuint copyLength = length ? length : dstDataLength - dstOffset;

    Checked<GCGLuint, RecordOverflow> checkedDstOffset(dstOffset);
    Checked<GCGLuint, RecordOverflow> checkedCopyLength(copyLength);
    auto checkedDestinationEnd = checkedDstOffset + checkedCopyLength;
    if (checkedDestinationEnd.hasOverflowed()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getBufferSubData"_s, "dstOffset + copyLength is too high"_s);
        return;
    }

    if (checkedDestinationEnd > dstDataLength) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getBufferSubData"_s, "end of written destination is past the end of the buffer"_s);
        return;
    }

    if (srcByteOffset < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getBufferSubData"_s, "srcByteOffset is less than 0"_s);
        return;
    }

    if (!copyLength)
        return;

    // FIXME: Coalesce multiple getBufferSubData() calls to use a single map() call
    m_context->getBufferSubData(target, srcByteOffset, std::span(static_cast<uint8_t*>(dstData->baseAddress()) + dstOffset * elementSize, copyLength * elementSize));
}

void WebGL2RenderingContext::bindFramebuffer(GCGLenum target, WebGLFramebuffer* buffer)
{
    if (isContextLost())
        return;
    Locker locker { objectGraphLock() };
    if (!validateNullableWebGLObject("bindFramebuffer"_s, buffer))
        return;

    switch (target) {
    case GraphicsContextGL::DRAW_FRAMEBUFFER:
        break;
    case GraphicsContextGL::FRAMEBUFFER:
    case GraphicsContextGL::READ_FRAMEBUFFER:
        m_readFramebufferBinding = buffer;
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "bindFramebuffer"_s, "invalid target"_s);
        return;
    }

    setFramebuffer(locker, target, buffer);
}

void WebGL2RenderingContext::blitFramebuffer(GCGLint srcX0, GCGLint srcY0, GCGLint srcX1, GCGLint srcY1, GCGLint dstX0, GCGLint dstY0, GCGLint dstX1, GCGLint dstY1, GCGLbitfield mask, GCGLenum filter)
{
    if (isContextLost())
        return;
    m_context->blitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    markContextChangedAndNotifyCanvasObserver(CallerTypeOther);
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
    if (isContextLost() || !validateFramebufferFuncParameters("framebufferTextureLayer"_s, target, attachment))
        return;

    if (texture && !validateWebGLObject("framebufferTextureLayer"_s, *texture))
        return;

    GCGLenum texTarget = texture ? texture->getTarget() : 0;
    if (texture) {
        if (texTarget != GraphicsContextGL::TEXTURE_3D && texTarget != GraphicsContextGL::TEXTURE_2D_ARRAY) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferTextureLayer"_s, "invalid texture type"_s);
            return;
        }
        if (!validateTexFuncLayer("framebufferTextureLayer"_s, texTarget, layer))
            return;
        if (!validateTexFuncLevel("framebufferTextureLayer"_s, texTarget, level))
            return;
    }
    WebGLFramebuffer* framebufferBinding = getFramebufferBinding(target);
    if (!framebufferBinding || !framebufferBinding->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "framebufferTextureLayer"_s, "no framebuffer bound"_s);
        return;
    }
    framebufferBinding->setAttachmentForBoundFramebuffer(target, attachment, WebGLFramebuffer::TextureLayerAttachment { texture, level, layer });
}

WebGLAny WebGL2RenderingContext::getInternalformatParameter(GCGLenum target, GCGLenum internalformat, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;

    if (pname != GraphicsContextGL::SAMPLES) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getInternalformatParameter"_s, "invalid parameter name"_s);
        return nullptr;
    }

    if (!validateForbiddenInternalFormats("getInternalformatParameter"_s, internalformat))
        return nullptr;

    updateErrors();
    GCGLint numValues = m_context->getInternalformati(target, internalformat, GraphicsContextGL::NUM_SAMPLE_COUNTS);
    if (updateErrors() || numValues < 0)
        return nullptr;

    // Integer formats do not support multisampling, so numValues == 0 may occur.

    Vector<GCGLint> params(numValues);
    if (numValues > 0) {
        m_context->getInternalformativ(target, internalformat, pname, params);
        if (updateErrors())
            return nullptr;
    }

    return Int32Array::tryCreate(params.data(), params.size());
}

void WebGL2RenderingContext::invalidateFramebuffer(GCGLenum target, const Vector<GCGLenum>& attachments)
{
    if (isContextLost())
        return;

    Vector<GCGLenum> translatedAttachments = attachments;
    if (!checkAndTranslateAttachments("invalidateFramebuffer"_s, target, translatedAttachments))
        return;
    m_context->invalidateFramebuffer(target, translatedAttachments);
}

void WebGL2RenderingContext::invalidateSubFramebuffer(GCGLenum target, const Vector<GCGLenum>& attachments, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (isContextLost())
        return;

    Vector<GCGLenum> translatedAttachments = attachments;
    if (!checkAndTranslateAttachments("invalidateSubFramebuffer"_s, target, translatedAttachments))
        return;
    m_context->invalidateSubFramebuffer(target, translatedAttachments, x, y, width, height);
}

void WebGL2RenderingContext::readBuffer(GCGLenum src)
{
    if (isContextLost())
        return;

    if (src == GraphicsContextGL::BACK) {
        // Because the backbuffer is simulated on all current WebKit ports, we need to change BACK to COLOR_ATTACHMENT0.
        if (m_readFramebufferBinding) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "readBuffer"_s, "BACK is valid for default framebuffer only"_s);
            return;
        }
        src = GraphicsContextGL::COLOR_ATTACHMENT0;
    } else if (!m_readFramebufferBinding && src != GraphicsContextGL::NONE) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "readBuffer"_s, "default framebuffer only supports NONE or BACK"_s);
        return;
    }

    m_context->readBuffer(src);
}

void WebGL2RenderingContext::renderbufferStorageMultisample(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    auto functionName = "renderbufferStorageMultisample"_s;
    if (isContextLost())
        return;
    if (target != GraphicsContextGL::RENDERBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target"_s);
        return;
    }
    if (!m_renderbufferBinding || !m_renderbufferBinding->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no bound renderbuffer"_s);
        return;
    }
    if (!validateSize(functionName, width, height))
        return;
    renderbufferStorageImpl(target, samples, internalformat, width, height, functionName);
}

WebGLAny WebGL2RenderingContext::getTexParameter(GCGLenum target, GCGLenum pname)
{
    if (isContextLost() || !validateTextureBinding("getTexParameter"_s, target))
        return nullptr;
    switch (pname) {
    case GraphicsContextGL::TEXTURE_BASE_LEVEL:
    case GraphicsContextGL::TEXTURE_MAX_LEVEL:
    case GraphicsContextGL::TEXTURE_COMPARE_FUNC:
    case GraphicsContextGL::TEXTURE_COMPARE_MODE:
    case GraphicsContextGL::TEXTURE_IMMUTABLE_LEVELS:
    case GraphicsContextGL::TEXTURE_WRAP_R:
        return m_context->getTexParameteri(target, pname);
    case GraphicsContextGL::TEXTURE_IMMUTABLE_FORMAT:
        return static_cast<bool>(m_context->getTexParameteri(target, pname));
    case GraphicsContextGL::TEXTURE_MIN_LOD:
    case GraphicsContextGL::TEXTURE_MAX_LOD:
        return m_context->getTexParameterf(target, pname);
    case GraphicsContextGL::DEPTH_STENCIL_TEXTURE_MODE_ANGLE:
        if (m_webglStencilTexturing)
            return m_context->getTexParameteri(target, pname);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getTexParameter"_s, "invalid parameter name, WEBGL_stencil_texturing not enabled"_s);
        return nullptr;
    default:
        return WebGLRenderingContextBase::getTexParameter(target, pname);
    }
}

void WebGL2RenderingContext::texStorage2D(GCGLenum target, GCGLsizei levels, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height)
{
    if (isContextLost())
        return;

    auto texture = validateTextureStorage2DBinding("texStorage2D"_s, target);
    if (!texture)
        return;

    if (!validateForbiddenInternalFormats("texStorage2D"_s, internalFormat))
        return;

    m_context->texStorage2D(target, levels, internalFormat, width, height);
}

void WebGL2RenderingContext::texStorage3D(GCGLenum target, GCGLsizei levels, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLsizei depth)
{
    if (isContextLost())
        return;

    auto texture = validateTexture3DBinding("texStorage3D"_s, target);
    if (!texture)
        return;

    if (!validateForbiddenInternalFormats("texStorage3D"_s, internalFormat))
        return;

    m_context->texStorage3D(target, levels, internalFormat, width, height, depth);
}

void WebGL2RenderingContext::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& data)
{
    if (isContextLost())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D"_s, "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    WebGLRenderingContextBase::texImage2D(target, level, internalformat, width, height, border, format, type, WTFMove(data));
}

ExceptionOr<void> WebGL2RenderingContext::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLenum format, GCGLenum type, std::optional<TexImageSource> data)
{
    if (isContextLost())
        return { };
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D"_s, "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return { };
    }
    return WebGLRenderingContextBase::texImage2D(target, level, internalformat, format, type, data);
}

void WebGL2RenderingContext::texImage2D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    if (isContextLost())
        return;
    if (!validateTexture2DBinding("texImage2D"_s, target))
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D"_s, "no bound PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    if (m_unpackFlipY || m_unpackPremultiplyAlpha) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D"_s, "FLIP_Y or PREMULTIPLY_ALPHA isn't allowed while uploading from PBO"_s);
        return;
    }
    if (!validateTexFunc(TexImageFunctionID::TexImage2D, SourceUnpackBuffer, target, level, internalformat, width, height, 1, border, format, type, 0, 0, 0))
        return;

    m_context->texImage2D(target, level, internalformat, width, height, border, format, type, offset);
}

ExceptionOr<void> WebGL2RenderingContext::texImage2D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, TexImageSource&& source)
{
    if (isContextLost())
        return { };
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D"_s, "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return { };
    }

    return WebGLRenderingContextBase::texImageSourceHelper(TexImageFunctionID::TexImage2D, target, level, internalformat, border, format, type, 0, 0, 0, getTextureSourceSubRectangle(width, height), 1, 0, WTFMove(source));
}

void WebGL2RenderingContext::texImage2D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData, GCGLuint srcOffset)
{
    if (isContextLost())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D"_s, "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return;
    }

    texImageArrayBufferViewHelper(TexImageFunctionID::TexImage2D, target, level, internalformat, width, height, 1, border, format, type, 0, 0, 0, WTFMove(srcData), NullNotReachable, srcOffset);
}

void WebGL2RenderingContext::texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, GCGLint64 offset)
{
    if (isContextLost())
        return;
    if (!validateTexture3DBinding("texImage3D"_s, target))
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage3D"_s, "no bound PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    if (m_unpackFlipY || m_unpackPremultiplyAlpha) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage3D"_s, "FLIP_Y or PREMULTIPLY_ALPHA isn't allowed for uploading 3D textures"_s);
        return;
    }
    if (!validateTexFunc(TexImageFunctionID::TexImage3D, SourceUnpackBuffer, target, level, internalformat, width, height, depth, border, format, type, 0, 0, 0))
        return;

    m_context->texImage3D(target, level, internalformat, width, height, depth, border, format, type,  offset);
}

ExceptionOr<void> WebGL2RenderingContext::texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, TexImageSource&& source)
{
    if (isContextLost())
        return { };
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage3D"_s, "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return { };
    }

    return WebGLRenderingContextBase::texImageSourceHelper(TexImageFunctionID::TexImage3D, target, level, internalformat, border, format, type, 0, 0, 0, getTextureSourceSubRectangle(width, height), depth, m_unpackParameters.imageHeight, WTFMove(source));
}

void WebGL2RenderingContext::texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData)
{
    if (isContextLost())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage3D"_s, "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    if ((m_unpackFlipY || m_unpackPremultiplyAlpha) && srcData) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage3D"_s, "FLIP_Y or PREMULTIPLY_ALPHA isn't allowed for uploading 3D textures"_s);
        return;
    }

    texImageArrayBufferViewHelper(TexImageFunctionID::TexImage3D, target, level, internalformat, width, height, depth, border, format, type, 0, 0, 0, WTFMove(srcData), NullAllowed, 0);
}

void WebGL2RenderingContext::texImage3D(GCGLenum target, GCGLint level, GCGLint internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData, GCGLuint srcOffset)
{
    if (isContextLost())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage3D"_s, "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    if (m_unpackFlipY || m_unpackPremultiplyAlpha) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage3D"_s, "FLIP_Y or PREMULTIPLY_ALPHA isn't allowed for uploading 3D textures"_s);
        return;
    }

    texImageArrayBufferViewHelper(TexImageFunctionID::TexImage3D, target, level, internalformat, width, height, depth, border, format, type, 0, 0, 0, WTFMove(srcData), NullNotReachable, srcOffset);
}

void WebGL2RenderingContext::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData)
{
    if (isContextLost())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage2D"_s, "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    WebGLRenderingContextBase::texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, WTFMove(srcData));
}

ExceptionOr<void> WebGL2RenderingContext::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLenum format, GCGLenum type, std::optional<TexImageSource>&& data)
{
    if (isContextLost())
        return { };
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage2D"_s, "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return { };
    }
    return WebGLRenderingContextBase::texSubImage2D(target, level, xoffset, yoffset, format, type, WTFMove(data));
}

void WebGL2RenderingContext::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format , GCGLenum type, GCGLintptr offset)
{
    if (isContextLost())
        return;
    if (!validateTexture2DBinding("texSubImage2D"_s, target))
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage2D"_s, "no bound PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    if (m_unpackFlipY || m_unpackPremultiplyAlpha) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage2D"_s, "FLIP_Y or PREMULTIPLY_ALPHA isn't allowed while uploading from PBO"_s);
        return;
    }
    if (!validateTexFunc(TexImageFunctionID::TexSubImage2D, SourceUnpackBuffer, target, level, 0, width, height, 1, 0, format, type, xoffset, yoffset, 0))
        return;

    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, offset);
}

ExceptionOr<void> WebGL2RenderingContext::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, TexImageSource&& source)
{
    if (isContextLost())
        return { };
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D"_s, "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return { };
    }

    return WebGLRenderingContextBase::texImageSourceHelper(TexImageFunctionID::TexSubImage2D, target, level, 0, 0, format, type, xoffset, yoffset, 0, getTextureSourceSubRectangle(width, height), 1, 0, WTFMove(source));
}

void WebGL2RenderingContext::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData, GCGLuint srcOffset)
{
    if (isContextLost())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texImage2D"_s, "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return;
    }

    texImageArrayBufferViewHelper(TexImageFunctionID::TexSubImage2D, target, level, 0, width, height, 1, 0, format, type, xoffset, yoffset, 0, WTFMove(srcData), NullNotReachable, srcOffset);
}

void WebGL2RenderingContext::texSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, GCGLint64 offset)
{
    if (isContextLost())
        return;
    if (!validateTexture3DBinding("texSubImage3D"_s, target))
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage3D"_s, "no bound PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    if (m_unpackFlipY || m_unpackPremultiplyAlpha) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage3D"_s, "FLIP_Y or PREMULTIPLY_ALPHA isn't allowed for uploading 3D textures"_s);
        return;
    }
    if (!validateTexFunc(TexImageFunctionID::TexSubImage3D, SourceUnpackBuffer, target, level, 0, width, height, depth, 0, format, type, xoffset, yoffset, zoffset))
        return;

    m_context->texSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, offset);
}

void WebGL2RenderingContext::texSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& srcData, GCGLuint srcOffset)
{
    if (isContextLost())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage3D"_s, "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    if (m_unpackFlipY || m_unpackPremultiplyAlpha) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage3D"_s, "FLIP_Y or PREMULTIPLY_ALPHA isn't allowed for uploading 3D textures"_s);
        return;
    }

    texImageArrayBufferViewHelper(TexImageFunctionID::TexSubImage3D, target, level, 0, width, height, depth, 0, format, type, xoffset, yoffset, zoffset, WTFMove(srcData), NullNotReachable, srcOffset);
}

ExceptionOr<void> WebGL2RenderingContext::texSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, TexImageSource&& source)
{
    if (isContextLost())
        return { };
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "texSubImage3D"_s, "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return { };
    }

    return WebGLRenderingContextBase::texImageSourceHelper(TexImageFunctionID::TexSubImage3D, target, level, 0, 0, format, type, xoffset, yoffset, zoffset, getTextureSourceSubRectangle(width, height), depth, m_unpackParameters.imageHeight, WTFMove(source));
}

void WebGL2RenderingContext::copyTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (isContextLost())
        return;
    if (!validateTexture3DBinding("copyTexSubImage3D"_s, target))
        return;
    clearIfComposited(CallerTypeOther);
    m_context->copyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

void WebGL2RenderingContext::compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, ArrayBufferView& srcData)
{
    if (isContextLost())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexImage2D"_s,
            "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    WebGLRenderingContextBase::compressedTexImage2D(target, level, internalformat, width, height, border, srcData);
}

void WebGL2RenderingContext::compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLsizei imageSize, GCGLint64 offset)
{
    if (isContextLost())
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexImage2D"_s,
            "no bound PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    if (!validateTexture2DBinding("compressedTexImage2D"_s, target))
        return;
    m_context->compressedTexImage2D(target, level, internalformat, width, height, border, imageSize, offset);
}

void WebGL2RenderingContext::compressedTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, ArrayBufferView& srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride)
{
    if (isContextLost())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexImage2D"_s,
            "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    if (!validateTexture2DBinding("compressedTexImage2D"_s, target))
        return;
    auto slice = sliceArrayBufferView("compressedTexImage2D"_s, srcData, srcOffset, srcLengthOverride);
    if (!slice)
        return;
    m_context->compressedTexImage2D(target, level, internalformat, width, height, border, slice->byteLength(), std::span(static_cast<const uint8_t*>(slice->baseAddress()), slice->byteLength()));
}

void WebGL2RenderingContext::compressedTexImage3D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, GCGLsizei imageSize, GCGLint64 offset)
{
    if (isContextLost())
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexImage3D"_s,
            "no bound PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    if (!validateTexture3DBinding("compressedTexImage3D"_s, target))
        return;
    m_context->compressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, offset);
}

void WebGL2RenderingContext::compressedTexImage3D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLint border, ArrayBufferView& srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride)
{
    if (isContextLost())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexImage3D"_s,
            "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    if (!validateTexture3DBinding("compressedTexImage3D"_s, target))
        return;
    auto slice = sliceArrayBufferView("compressedTexImage3D"_s, srcData, srcOffset, srcLengthOverride);
    if (!slice)
        return;
    m_context->compressedTexImage3D(target, level, internalformat, width, height, depth, border, slice->byteLength(), std::span(static_cast<const uint8_t*>(slice->baseAddress()), slice->byteLength()));
}

void WebGL2RenderingContext::compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, ArrayBufferView& srcData)
{
    if (isContextLost())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexSubImage2D"_s,
            "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    WebGLRenderingContextBase::compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, srcData);
}

void WebGL2RenderingContext::compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset)
{
    if (isContextLost())
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexSubImage2D"_s,
            "no bound PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    if (!validateTexture2DBinding("compressedTexImage2D"_s, target))
        return;
    m_context->compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, offset);
}

void WebGL2RenderingContext::compressedTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, ArrayBufferView& srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride)
{
    if (isContextLost())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexSubImage2D"_s,
            "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    if (!validateTexture2DBinding("compressedTexSubImage2D"_s, target))
        return;
    auto slice = sliceArrayBufferView("compressedTexSubImage2D"_s, srcData, srcOffset, srcLengthOverride);
    if (!slice)
        return;
    m_context->compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, slice->byteLength(), std::span(static_cast<const uint8_t*>(slice->baseAddress()), slice->byteLength()));
}

void WebGL2RenderingContext::compressedTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLsizei imageSize, GCGLint64 offset)
{
    if (isContextLost())
        return;
    if (!m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexSubImage3D"_s,
            "no bound PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    if (!validateTexture3DBinding("compressedTexSubImage3D"_s, target))
        return;
    m_context->compressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, offset);
}

void WebGL2RenderingContext::compressedTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, ArrayBufferView& srcData, GCGLuint srcOffset, GCGLuint srcLengthOverride)
{
    if (isContextLost())
        return;
    if (m_boundPixelUnpackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "compressedTexSubImage3D"_s,
            "a buffer is bound to PIXEL_UNPACK_BUFFER"_s);
        return;
    }
    if (!validateTexture3DBinding("compressedTexSubImage3D"_s, target))
        return;
    auto slice = sliceArrayBufferView("compressedTexSubImage3D"_s, srcData, srcOffset, srcLengthOverride);
    if (!slice)
        return;
    m_context->compressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, slice->byteLength(), std::span(static_cast<const uint8_t*>(slice->baseAddress()), slice->byteLength()));
}

GCGLint WebGL2RenderingContext::getFragDataLocation(WebGLProgram& program, const String& name)
{
    if (isContextLost())
        return -1;
    if (!validateWebGLObject("getFragDataLocation"_s, program))
        return -1;
    return m_context->getFragDataLocation(program.object(), name);
}

void WebGL2RenderingContext::uniform1ui(const WebGLUniformLocation* location, GCGLuint v0)
{
    if (isContextLost() || !validateUniformLocation("uniform1ui"_s, location))
        return;
    m_context->uniform1ui(location->location(), v0);
}

void WebGL2RenderingContext::uniform2ui(const WebGLUniformLocation* location, GCGLuint v0, GCGLuint v1)
{
    if (isContextLost() || !validateUniformLocation("uniform2ui"_s, location))
        return;
    m_context->uniform2ui(location->location(), v0, v1);
}

void WebGL2RenderingContext::uniform3ui(const WebGLUniformLocation* location, GCGLuint v0, GCGLuint v1, GCGLuint v2)
{
    if (isContextLost() || !validateUniformLocation("uniform3ui"_s, location))
        return;
    m_context->uniform3ui(location->location(), v0, v1, v2);
}

void WebGL2RenderingContext::uniform4ui(const WebGLUniformLocation* location, GCGLuint v0, GCGLuint v1, GCGLuint v2, GCGLuint v3)
{
    if (isContextLost() || !validateUniformLocation("uniform4ui"_s, location))
        return;
    m_context->uniform4ui(location->location(), v0, v1, v2, v3);
}

void WebGL2RenderingContext::uniform1uiv(const WebGLUniformLocation* location, Uint32List&& value, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform1uiv"_s, location, value, 1, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniform1uiv(location->location(), data.value());
}

void WebGL2RenderingContext::uniform2uiv(const WebGLUniformLocation* location, Uint32List&& value, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform2uiv"_s, location, value, 2, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniform2uiv(location->location(), data.value());
}

void WebGL2RenderingContext::uniform3uiv(const WebGLUniformLocation* location, Uint32List&& value, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform3uiv"_s, location, value, 3, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniform3uiv(location->location(), data.value());
}

void WebGL2RenderingContext::uniform4uiv(const WebGLUniformLocation* location, Uint32List&& value, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto data = validateUniformParameters("uniform4uiv"_s, location, value, 4, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniform4uiv(location->location(), data.value());
}

void WebGL2RenderingContext::uniformMatrix2x3fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix2x3fv"_s, location, transpose, v, 6, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniformMatrix2x3fv(location->location(), transpose, data.value());
}

void WebGL2RenderingContext::uniformMatrix3x2fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix3x2fv"_s, location, transpose, v, 6, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniformMatrix3x2fv(location->location(), transpose, data.value());
}

void WebGL2RenderingContext::uniformMatrix2x4fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix2x4fv"_s, location, transpose, v, 8, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniformMatrix2x4fv(location->location(), transpose, data.value());
}

void WebGL2RenderingContext::uniformMatrix4x2fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix4x2fv"_s, location, transpose, v, 8, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniformMatrix4x2fv(location->location(), transpose, data.value());
}

void WebGL2RenderingContext::uniformMatrix3x4fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix3x4fv"_s, location, transpose, v, 12, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniformMatrix3x4fv(location->location(), transpose, data.value());
}

void WebGL2RenderingContext::uniformMatrix4x3fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& v, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto data = validateUniformMatrixParameters("uniformMatrix4x3fv"_s, location, transpose, v, 12, srcOffset, srcLength);
    if (!data)
        return;
    m_context->uniformMatrix4x3fv(location->location(), transpose, data.value());
}

void WebGL2RenderingContext::vertexAttribI4i(GCGLuint index, GCGLint x, GCGLint y, GCGLint z, GCGLint w)
{
    if (isContextLost())
        return;
    m_context->vertexAttribI4i(index, x, y, z, w);
    if (index < m_vertexAttribValue.size()) {
        m_vertexAttribValue[index].type = GraphicsContextGL::INT;
        m_vertexAttribValue[index].iValue[0] = x;
        m_vertexAttribValue[index].iValue[1] = y;
        m_vertexAttribValue[index].iValue[2] = z;
        m_vertexAttribValue[index].iValue[3] = w;
    }
}

void WebGL2RenderingContext::vertexAttribI4iv(GCGLuint index, Int32List&& list)
{
    if (isContextLost())
        return;
    auto data = list.data();
    if (!data) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribI4iv"_s, "no array"_s);
        return;
    }

    int size = list.length();
    if (size < 4) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribI4iv"_s, "array too small"_s);
        return;
    }
    if (index >= m_vertexAttribValue.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribI4iv"_s, "index out of range"_s);
        return;
    }
    m_context->vertexAttribI4iv(index, std::span<const GCGLint, 4> { data, 4 });
    m_vertexAttribValue[index].type = GraphicsContextGL::INT;
    memcpy(m_vertexAttribValue[index].iValue, data, sizeof(m_vertexAttribValue[index].iValue));
}

void WebGL2RenderingContext::vertexAttribI4ui(GCGLuint index, GCGLuint x, GCGLuint y, GCGLuint z, GCGLuint w)
{
    if (isContextLost())
        return;
    m_context->vertexAttribI4ui(index, x, y, z, w);
    if (index < m_vertexAttribValue.size()) {
        m_vertexAttribValue[index].type = GraphicsContextGL::UNSIGNED_INT;
        m_vertexAttribValue[index].uiValue[0] = x;
        m_vertexAttribValue[index].uiValue[1] = y;
        m_vertexAttribValue[index].uiValue[2] = z;
        m_vertexAttribValue[index].uiValue[3] = w;
    }
}

void WebGL2RenderingContext::vertexAttribI4uiv(GCGLuint index, Uint32List&& list)
{
    if (isContextLost())
        return;
    auto data = list.data();
    if (!data) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribI4uiv"_s, "no array"_s);
        return;
    }
    
    int size = list.length();
    if (size < 4) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribI4uiv"_s, "array too small"_s);
        return;
    }
    if (index >= m_vertexAttribValue.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribI4uiv"_s, "index out of range"_s);
        return;
    }
    m_context->vertexAttribI4uiv(index, std::span<const GCGLuint, 4> { data, 4 });
    m_vertexAttribValue[index].type = GraphicsContextGL::UNSIGNED_INT;
    memcpy(m_vertexAttribValue[index].uiValue, data, sizeof(m_vertexAttribValue[index].uiValue));
}

void WebGL2RenderingContext::vertexAttribIPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLsizei stride, GCGLint64 offset)
{
    Locker locker { objectGraphLock() };

    if (isContextLost())
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
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "vertexAttribIPointer"_s, "invalid type"_s);
        return;
    }
    if (index >= m_vertexAttribValue.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribIPointer"_s, "index out of range"_s);
        return;
    }
    if (size < 1 || size > 4) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribIPointer"_s, "bad size"_s);
        return;
    }
    if (stride < 0 || stride > 255) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribIPointer"_s, "bad stride"_s);
        return;
    }
    if (offset < 0 || offset > std::numeric_limits<int32_t>::max()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "vertexAttribIPointer"_s, "bad offset"_s);
        return;
    }
    if (!m_boundArrayBuffer && offset) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "vertexAttribIPointer"_s, "no bound ARRAY_BUFFER"_s);
        return;
    }
    // Determine the number of elements the bound buffer can hold, given the offset, size, type and stride.
    auto typeSize = sizeInBytes(type);
    if (!typeSize) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "vertexAttribIPointer"_s, "invalid type"_s);
        return;
    }
    if ((stride % typeSize) || (static_cast<GCGLintptr>(offset) % typeSize)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "vertexAttribIPointer"_s, "stride or offset not valid for type"_s);
        return;
    }
    GCGLsizei bytesPerElement = size * typeSize;

    m_boundVertexArrayObject->setVertexAttribState(locker, index, bytesPerElement, size, type, false, stride, static_cast<GCGLintptr>(offset), true, m_boundArrayBuffer.get());
    m_context->vertexAttribIPointer(index, size, type, stride, offset);
}

void WebGL2RenderingContext::vertexAttribDivisor(GCGLuint index, GCGLuint divisor)
{
    if (isContextLost())
        return;

    WebGLRenderingContextBase::vertexAttribDivisor(index, divisor);
}

void WebGL2RenderingContext::drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei instanceCount)
{
    if (isContextLost())
        return;

    WebGLRenderingContextBase::drawArraysInstanced(mode, first, count, instanceCount);
}

void WebGL2RenderingContext::drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLint64 offset, GCGLsizei instanceCount)
{
    if (isContextLost())
        return;

    WebGLRenderingContextBase::drawElementsInstanced(mode, count, type, offset, instanceCount);
}

void WebGL2RenderingContext::drawRangeElements(GCGLenum mode, GCGLuint start, GCGLuint end, GCGLsizei count, GCGLenum type, GCGLint64 offset)
{
    if (isContextLost())
        return;
    if (!validateVertexArrayObject("drawRangeElements"_s))
        return;

    if (m_currentProgram && InspectorInstrumentation::isWebGLProgramDisabled(*this, *m_currentProgram))
        return;

    clearIfComposited(CallerTypeDrawOrClear);

    {
        ScopedInspectorShaderProgramHighlight scopedHighlight { *this };

        m_context->drawRangeElements(mode, start, end, count, type, offset);
    }

    markContextChangedAndNotifyCanvasObserver();
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
                && bufs[i] < GraphicsContextGL::COLOR_ATTACHMENT0 + maxColorAttachments()) {
                continue;
            }
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "drawBuffers"_s, "invalid buffer"_s);
            return;
        }
    }
    if (!m_framebufferBinding) {
        if (n != 1) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "drawBuffers"_s, "more than one buffer"_s);
            return;
        }
        if (bufs[0] != GraphicsContextGL::BACK && bufs[0] != GraphicsContextGL::NONE) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "drawBuffers"_s, "BACK or NONE"_s);
            return;
        }
        // Because the backbuffer is simulated on all current WebKit ports, we need to change BACK to COLOR_ATTACHMENT0.
        GCGLenum value[1] { (bufs[0] == GraphicsContextGL::BACK) ? GraphicsContextGL::COLOR_ATTACHMENT0 : GraphicsContextGL::NONE };
        m_context->drawBuffers(value);
        setBackDrawBuffer(bufs[0]);
    } else {
        if (n > maxDrawBuffers()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "drawBuffers"_s, "more than max draw buffers"_s);
            return;
        }
        for (GCGLsizei i = 0; i < n; ++i) {
            if (bufs[i] != GraphicsContextGL::NONE && bufs[i] != static_cast<GCGLenum>(GraphicsContextGL::COLOR_ATTACHMENT0 + i)) {
                synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "drawBuffers"_s, "COLOR_ATTACHMENTi or NONE"_s);
                return;
            }
        }
        m_framebufferBinding->drawBuffers(buffers);
    }
}

void WebGL2RenderingContext::clearBufferiv(GCGLenum buffer, GCGLint drawbuffer, Int32List&& values, GCGLuint srcOffset)
{
    if (isContextLost())
        return;
    auto data = validateClearBuffer("clearBufferiv"_s, buffer, values, srcOffset);
    if (!data)
        return;

    // Flush any pending implicit clears. This cannot be done after the
    // user-requested clearBuffer call because of scissor test side effects.
    clearIfComposited(CallerTypeDrawOrClear);

    m_context->clearBufferiv(buffer, drawbuffer, data.value());
}

void WebGL2RenderingContext::clearBufferuiv(GCGLenum buffer, GCGLint drawbuffer, Uint32List&& values, GCGLuint srcOffset)
{
    if (isContextLost())
        return;
    auto data = validateClearBuffer("clearBufferuiv"_s, buffer, values, srcOffset);
    if (!data)
        return;

    // This call is not applicable to the default framebuffer attachments
    // as they cannot have UINT type. Ignore any pending implicit clears.

    m_context->clearBufferuiv(buffer, drawbuffer, data.value());
}

void WebGL2RenderingContext::clearBufferfv(GCGLenum buffer, GCGLint drawbuffer, Float32List&& values, GCGLuint srcOffset)
{
    if (isContextLost())
        return;
    auto data = validateClearBuffer("clearBufferfv"_s, buffer, values, srcOffset);
    if (!data)
        return;

    // Flush any pending implicit clears. This cannot be done after the
    // user-requested clearBuffer call because of scissor test side effects.
    clearIfComposited(CallerTypeDrawOrClear);

    m_context->clearBufferfv(buffer, drawbuffer, data.value());

    // This might have been used to clear the color buffer of the default
    // back buffer. Notification is required to update the canvas.
    markContextChangedAndNotifyCanvasObserver();
}

void WebGL2RenderingContext::clearBufferfi(GCGLenum buffer, GCGLint drawbuffer, GCGLfloat depth, GCGLint stencil)
{
    if (isContextLost())
        return;

    // Flush any pending implicit clears. This cannot be done after the
    // user-requested clearBuffer call because of scissor test side effects.
    clearIfComposited(CallerTypeDrawOrClear);

    m_context->clearBufferfi(buffer, drawbuffer, depth, stencil);
}

RefPtr<WebGLQuery> WebGL2RenderingContext::createQuery()
{
    if (isContextLost())
        return nullptr;

    return WebGLQuery::create(*this);
}

void WebGL2RenderingContext::deleteQuery(WebGLQuery* query)
{
    Locker locker { objectGraphLock() };

    if (isContextLost() || !query || !query->object() || !validateWebGLObject("deleteQuery"_s, *query))
        return;
    if (query->target()) {
        for (auto& activeQuery : m_activeQueries) {
            if (query == activeQuery) {
                m_context->endQuery(query->target());
                activeQuery = nullptr;
                break;
            }
        }
    }
    deleteObject(locker, query);
}

GCGLboolean WebGL2RenderingContext::isQuery(WebGLQuery* query)
{
    if (isContextLost())
        return false;
    if (!validateIsWebGLObject(query))
        return false;
    return m_context->isQuery(query->object());
}

std::optional<WebGL2RenderingContext::ActiveQueryKey> WebGL2RenderingContext::validateQueryTarget(ASCIILiteral functionName, GCGLenum target)
{
    switch (target) {
    case GraphicsContextGL::ANY_SAMPLES_PASSED:
    case GraphicsContextGL::ANY_SAMPLES_PASSED_CONSERVATIVE:
        return ActiveQueryKey::SamplesPassed;
    case GraphicsContextGL::TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
        return ActiveQueryKey::PrimitivesWritten;
    case GraphicsContextGL::TIME_ELAPSED_EXT:
        if (m_extDisjointTimerQueryWebGL2)
            return ActiveQueryKey::TimeElapsed;
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target, EXT_disjoint_timer_query_webgl2 not enabled"_s);
        return std::nullopt;
    }
    synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target"_s);
    return std::nullopt;
}

void WebGL2RenderingContext::beginQuery(GCGLenum target, WebGLQuery& query)
{
    if (isContextLost())
        return;
    Locker locker { objectGraphLock() };
    if (!validateWebGLObject("beginQuery"_s, query))
        return;
    auto activeQueryKey = validateQueryTarget("beginQuery"_s, target);
    if (!activeQueryKey)
        return;
    if (query.target() && query.target() != target) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginQuery"_s, "query type does not match target"_s);
        return;
    }

    // Only one query object can be active per target.
    if (m_activeQueries[*activeQueryKey]) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginQuery"_s, "query object of target is already active"_s);
        return;
    }
    m_activeQueries[*activeQueryKey] = &query;
    m_context->beginQuery(target, query.object());
    query.setTarget(target);
}

void WebGL2RenderingContext::endQuery(GCGLenum target)
{
    Locker locker { objectGraphLock() };
    if (isContextLost() || !scriptExecutionContext())
        return;
    auto activeQueryKey = validateQueryTarget("endQuery"_s, target);
    if (!activeQueryKey)
        return;
    if (!m_activeQueries[*activeQueryKey]) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "endQuery"_s, "query object of target is not active"_s);
        return;
    }
    m_context->endQuery(target);

    // A query's result must not be made available until control has returned to the user agent's main loop.
    scriptExecutionContext()->eventLoop().queueMicrotask([query = WTFMove(m_activeQueries[*activeQueryKey])] {
        query->makeResultAvailable();
    });
}

WebGLAny WebGL2RenderingContext::getQuery(GCGLenum target, GCGLenum pname)
{
    if (isContextLost() || !scriptExecutionContext())
        return nullptr;

    // Timestamp queries require special treatment because they are never active.
    if (target == GraphicsContextGL::TIMESTAMP_EXT) {
        if (!m_extDisjointTimerQueryWebGL2) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getQuery"_s, "invalid target, EXT_disjoint_timer_query_webgl2 not enabled"_s);
            return nullptr;
        }
        if (pname == GraphicsContextGL::QUERY_COUNTER_BITS_EXT)
            return m_context->getQuery(target, pname);
        if (pname != GraphicsContextGL::CURRENT_QUERY)
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getQuery"_s, "invalid parameter name"_s);
        return nullptr;
    }

    auto activeQueryKey = validateQueryTarget("getQuery"_s, target);
    if (!activeQueryKey)
        return nullptr;

    // Time elapsed queries support one more parameter name.
    if (target == GraphicsContextGL::TIME_ELAPSED_EXT && pname == GraphicsContextGL::QUERY_COUNTER_BITS_EXT)
        return m_context->getQuery(target, pname);

    if (pname != GraphicsContextGL::CURRENT_QUERY) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getQuery"_s, "invalid parameter name"_s);
        return nullptr;
    }

    auto query = m_activeQueries[*activeQueryKey];
    if (!query || query->target() != target)
        return nullptr;
    return query;
}

WebGLAny WebGL2RenderingContext::getQueryParameter(WebGLQuery& query, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;
    if (!validateWebGLObject("getQueryParameter"_s, query))
        return nullptr;
    if (!query.target()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getQueryParameter"_s, "query has not been used by beginQuery"_s);
        return nullptr;
    }
    if (std::find(std::begin(m_activeQueries), std::end(m_activeQueries), &query) != std::end(m_activeQueries)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getQueryParameter"_s, "query is currently active"_s);
        return nullptr;
    }
    switch (pname) {
    case GraphicsContextGL::QUERY_RESULT:
        if (!query.isResultAvailable())
            return 0;
        if (query.target() == GraphicsContextGL::TIME_ELAPSED_EXT || query.target() == GraphicsContextGL::TIMESTAMP_EXT)
            return static_cast<unsigned long long>(m_context->getQueryObjectui64EXT(query.object(), pname));
        return m_context->getQueryObjectui(query.object(), pname);
    case GraphicsContextGL::QUERY_RESULT_AVAILABLE:
        if (!query.isResultAvailable())
            return false;
        return static_cast<bool>(m_context->getQueryObjectui(query.object(), pname));
    }
    synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getQueryParameter"_s, "invalid parameter name"_s);
    return nullptr;
}

RefPtr<WebGLSampler> WebGL2RenderingContext::createSampler()
{
    if (isContextLost())
        return nullptr;
    return WebGLSampler::create(*this);
}

void WebGL2RenderingContext::deleteSampler(WebGLSampler* sampler)
{
    Locker locker { objectGraphLock() };

    if (!deleteObject(locker, sampler))
        return;

    // One sampler can be bound to multiple texture units.
    if (sampler) {
        for (auto& samplerSlot : m_boundSamplers) {
            if (samplerSlot == sampler)
                samplerSlot = nullptr;
        }
    }
}

GCGLboolean WebGL2RenderingContext::isSampler(WebGLSampler* sampler)
{
    if (isContextLost())
        return false;
    if (!validateIsWebGLObject(sampler))
        return false;
    return m_context->isSampler(sampler->object());
}

void WebGL2RenderingContext::bindSampler(GCGLuint unit, WebGLSampler* sampler)
{
    if (isContextLost())
        return;
    Locker locker { objectGraphLock() };
    if (!validateNullableWebGLObject("bindSampler"_s, sampler))
        return;
    
    if (unit >= m_boundSamplers.size()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bindSampler"_s, "invalid texture unit"_s);
        return;
    }

    if (m_boundSamplers[unit] == sampler)
        return;
    m_context->bindSampler(unit, objectOrZero(sampler));
    m_boundSamplers[unit] = sampler;
}

void WebGL2RenderingContext::samplerParameteri(WebGLSampler& sampler, GCGLenum pname, GCGLint value)
{
    if (isContextLost())
        return;
    if (!validateWebGLObject("samplerParameteri"_s, sampler))
        return;

    m_context->samplerParameteri(sampler.object(), pname, value);
}

void WebGL2RenderingContext::samplerParameterf(WebGLSampler& sampler, GCGLenum pname, GCGLfloat value)
{
    if (isContextLost())
        return;
    if (!validateWebGLObject("samplerParameterf"_s, sampler))
        return;

    m_context->samplerParameterf(sampler.object(), pname, value);
}

WebGLAny WebGL2RenderingContext::getSamplerParameter(WebGLSampler& sampler, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;
    if (!validateWebGLObject("getSamplerParameter"_s, sampler))
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
    // EXT_texture_filter_anisotropic
    case GraphicsContextGL::TEXTURE_MAX_ANISOTROPY_EXT:
        if (!m_extTextureFilterAnisotropic) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getSamplerParameter"_s, "invalid parameter name, EXT_texture_filter_anisotropic not enabled"_s);
            return nullptr;
        }
        return m_context->getSamplerParameterf(sampler.object(), pname);
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getSamplerParameter"_s, "invalid parameter name"_s);
        return nullptr;
    }
}

RefPtr<WebGLSync> WebGL2RenderingContext::fenceSync(GCGLenum condition, GCGLbitfield flags)
{
    if (isContextLost())
        return nullptr;

    if (condition != GraphicsContextGL::SYNC_GPU_COMMANDS_COMPLETE) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "fenceSync"_s, "condition must be SYNC_GPU_COMMANDS_COMPLETE"_s);
        return nullptr;
    }
    if (flags) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "fenceSync"_s, "flags must be zero"_s);
        return nullptr;
    }
    auto sync = WebGLSync::create(*this);
    if (!sync)
        return nullptr;
    sync->scheduleAllowCacheUpdate(*this);
    return sync;
}

GCGLboolean WebGL2RenderingContext::isSync(WebGLSync* sync)
{
    if (isContextLost())
        return false;
    if (!validateIsWebGLObject(sync))
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
    if (isContextLost())
        return GraphicsContextGL::WAIT_FAILED_WEBGL;
    if (!validateWebGLObject("clientWaitSync"_s, sync))
        return GraphicsContextGL::WAIT_FAILED_WEBGL;

    if (timeout > MaxClientWaitTimeout) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "clientWaitSync"_s, "timeout > MAX_CLIENT_WAIT_TIMEOUT_WEBGL"_s);
        return GraphicsContextGL::WAIT_FAILED_WEBGL;
    }
    
    if (flags && flags != GraphicsContextGL::SYNC_FLUSH_COMMANDS_BIT) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "clientWaitSync"_s, "invalid flags"_s);
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
    if (isContextLost())
        return;
    if (!validateWebGLObject("waitSync"_s, sync))
        return;

    if (flags)
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "waitSync"_s, "flags must be zero"_s);
    else if (timeout != -1)
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "waitSync"_s, "invalid timeout"_s);

    // This function is a no-op in WebGL 2.
}

WebGLAny WebGL2RenderingContext::getSyncParameter(WebGLSync& sync, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;
    if (!validateWebGLObject("getSyncParameter"_s, sync))
        return nullptr;

    switch (pname) {
    case GraphicsContextGL::OBJECT_TYPE:
    case GraphicsContextGL::SYNC_STATUS:
    case GraphicsContextGL::SYNC_CONDITION:
    case GraphicsContextGL::SYNC_FLAGS:
        sync.updateCache(*this);
        return sync.getCachedResult(pname);
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getSyncParameter"_s, "invalid parameter name"_s);
        return nullptr;
    }
}

RefPtr<WebGLTransformFeedback> WebGL2RenderingContext::createTransformFeedback()
{
    if (isContextLost())
        return nullptr;

    return WebGLTransformFeedback::create(*this);
}

void WebGL2RenderingContext::deleteTransformFeedback(WebGLTransformFeedback* feedbackObject)
{
    Locker locker { objectGraphLock() };

    // We have to short-circuit the deletion process if the transform feedback is
    // active. This requires duplication of some validation logic.
    if (isContextLost() || !feedbackObject)
        return;

    if (!feedbackObject->validate(*this)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "delete"_s, "object does not belong to this context"_s);
        return;
    }

    if (feedbackObject->isDeleted())
        return;

    if (feedbackObject->isActive()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "deleteTransformFeedback"_s, "attempt to delete an active transform feedback object"_s);
        return;
    }

    ASSERT(feedbackObject != m_defaultTransformFeedback);

    if (!deleteObject(locker, feedbackObject))
        return;

    if (m_boundTransformFeedback == feedbackObject)
        m_boundTransformFeedback = m_defaultTransformFeedback;
}

GCGLboolean WebGL2RenderingContext::isTransformFeedback(WebGLTransformFeedback* feedbackObject)
{
    if (isContextLost())
        return false;
    if (!validateIsWebGLObject(feedbackObject))
        return false;
    return m_context->isTransformFeedback(feedbackObject->object());
}

void WebGL2RenderingContext::bindTransformFeedback(GCGLenum target, WebGLTransformFeedback* feedbackObject)
{
    if (isContextLost())
        return;
    Locker locker { objectGraphLock() };
    if (!validateNullableWebGLObject("bindTransformFeedback"_s, feedbackObject))
        return;

    if (target != GraphicsContextGL::TRANSFORM_FEEDBACK) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "bindTransformFeedback"_s, "target must be TRANSFORM_FEEDBACK"_s);
        return;
    }
    if (m_boundTransformFeedback->isActive() && !m_boundTransformFeedback->isPaused()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "bindTransformFeedback"_s, "transform feedback is active and not paused"_s);
        return;
    }

    auto toBeBound = feedbackObject ? feedbackObject : m_defaultTransformFeedback.get();
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
    if (isContextLost())
        return;
    
    if (!ValidateTransformFeedbackPrimitiveMode(primitiveMode)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "beginTransformFeedback"_s, "invalid transform feedback primitive mode"_s);
        return;
    }
    if (!m_currentProgram) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginTransformFeedback"_s, "no program is active"_s);
        return;
    }
    if (m_boundTransformFeedback->isActive()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginTransformFeedback"_s, "transform feedback is already active"_s);
        return;
    }
    int requiredBufferCount = m_currentProgram->requiredTransformFeedbackBufferCount();
    if (!requiredBufferCount) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginTransformFeedback"_s, "current active program does not specify any transform feedback varyings to record"_s);
        return;
    }
    if (!m_boundTransformFeedback->hasEnoughBuffers(requiredBufferCount)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "beginTransformFeedback"_s, "not enough transform feedback buffers bound"_s);
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
    if (isContextLost())
        return;

    if (!m_boundTransformFeedback->isActive()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "endTransformFeedback"_s, "transform feedback is not active"_s);
        return;
    }
    
    m_context->endTransformFeedback();

    m_boundTransformFeedback->setPaused(false);
    m_boundTransformFeedback->setActive(false);
}

void WebGL2RenderingContext::transformFeedbackVaryings(WebGLProgram& program, const Vector<String>& varyings, GCGLenum bufferMode)
{
    if (isContextLost())
        return;
    if (!validateWebGLObject("transformFeedbackVaryings"_s, program))
        return;
    
    switch (bufferMode) {
    case GraphicsContextGL::SEPARATE_ATTRIBS:
        if (varyings.size() > m_maxTransformFeedbackSeparateAttribs) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "transformFeedbackVaryings"_s, "too many varyings"_s);
            return;
        }
        break;
    case GraphicsContextGL::INTERLEAVED_ATTRIBS:
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "transformFeedbackVaryings"_s, "invalid buffer mode"_s);
        return;
    }
    program.setRequiredTransformFeedbackBufferCount(
        bufferMode == GraphicsContextGL::INTERLEAVED_ATTRIBS ? std::min(static_cast<size_t>(1), varyings.size()) : varyings.size());

    m_context->transformFeedbackVaryings(program.object(), varyings, bufferMode);
}

RefPtr<WebGLActiveInfo> WebGL2RenderingContext::getTransformFeedbackVarying(WebGLProgram& program, GCGLuint index)
{
    if (isContextLost())
        return nullptr;
    if (!validateWebGLObject("getTransformFeedbackVarying"_s, program))
        return nullptr;
    if (!program.getLinkStatus()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getTransformFeedbackVarying"_s, "program not linked"_s);
        return nullptr;
    }
    GraphicsContextGLActiveInfo info;
    m_context->getTransformFeedbackVarying(program.object(), index, info);

    if (!info.name || !info.type || !info.size)
        return nullptr;

    return WebGLActiveInfo::create(info.name, info.type, info.size);
}

void WebGL2RenderingContext::pauseTransformFeedback()
{
    if (isContextLost())
        return;

    if (!m_boundTransformFeedback->isActive()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "pauseTransformFeedback"_s, "transform feedback is not active"_s);
        return;
    }

    if (m_boundTransformFeedback->isPaused()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "pauseTransformFeedback"_s, "transform feedback is already paused"_s);
        return;
    }

    m_boundTransformFeedback->setPaused(true);
    m_context->pauseTransformFeedback();
}

void WebGL2RenderingContext::resumeTransformFeedback()
{
    if (isContextLost())
        return;

    if (!m_boundTransformFeedback->validateProgramForResume(m_currentProgram.get())) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "resumeTransformFeedback"_s, "the current program is not the same as when beginTransformFeedback was called"_s);
        return;
    }
    if (!m_boundTransformFeedback->isActive() || !m_boundTransformFeedback->isPaused()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "resumeTransformFeedback"_s, "transform feedback is not active or not paused"_s);
        return;
    }

    m_boundTransformFeedback->setPaused(false);
    m_context->resumeTransformFeedback();
}

bool WebGL2RenderingContext::isTransformFeedbackActiveAndNotPaused()
{
    return m_boundTransformFeedback->isActive() && !m_boundTransformFeedback->isPaused();
}

bool WebGL2RenderingContext::setIndexedBufferBinding(ASCIILiteral functionName, GCGLenum target, GCGLuint index, WebGLBuffer* buffer)
{
    if (isContextLost())
        return false;
    Locker locker { objectGraphLock() };
    if (!validateNullableWebGLObject(functionName, buffer))
        return false;

    switch (target) {
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER:
        if (m_boundTransformFeedback->isActive()) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "transform feedback is active"_s);
            return false;
        }
        if (index >= m_maxTransformFeedbackSeparateAttribs) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "index out of range"_s);
            return false;
        }
        break;
    case GraphicsContextGL::UNIFORM_BUFFER:
        if (index >= m_boundIndexedUniformBuffers.size()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "index out of range"_s);
            return false;
        }
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target"_s);
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
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target"_s);
        return false;
    }
    return true;
}

void WebGL2RenderingContext::bindBufferBase(GCGLenum target, GCGLuint index, WebGLBuffer* buffer)
{
    if (setIndexedBufferBinding("bindBufferBase"_s, target, index, buffer))
        m_context->bindBufferBase(target, index, objectOrZero(buffer));
}

void WebGL2RenderingContext::bindBufferRange(GCGLenum target, GCGLuint index, WebGLBuffer* buffer, GCGLint64 offset, GCGLint64 size)
{
    if (target == GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER && (offset % 4 || size % 4)) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bindBufferRange"_s, "invalid offset or size"_s);
        return;
    }
    if (target == GraphicsContextGL::UNIFORM_BUFFER && (offset % m_uniformBufferOffsetAlignment)) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "bindBufferRange"_s, "invalid offset"_s);
        return;
    }
    if (setIndexedBufferBinding("bindBufferRange"_s, target, index, buffer))
        m_context->bindBufferRange(target, index, objectOrZero(buffer), offset, size);
}

WebGLAny WebGL2RenderingContext::getIndexedParameter(GCGLenum target, GCGLuint index)
{
    if (isContextLost())
        return nullptr;

    switch (target) {
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER_BINDING: {
        WebGLBuffer* buffer;
        bool success = m_boundTransformFeedback->getBoundIndexedTransformFeedbackBuffer(index, &buffer);
        if (!success) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getIndexedParameter"_s, "index out of range"_s);
            return nullptr;
        }
        return RefPtr { buffer };
    }
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER_SIZE:
    case GraphicsContextGL::TRANSFORM_FEEDBACK_BUFFER_START:
    case GraphicsContextGL::UNIFORM_BUFFER_SIZE:
    case GraphicsContextGL::UNIFORM_BUFFER_START:
        return static_cast<long long>(m_context->getInteger64i(target, index));
    case GraphicsContextGL::UNIFORM_BUFFER_BINDING:
        if (index >= m_boundIndexedUniformBuffers.size()) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "getIndexedParameter"_s, "index out of range"_s);
            return nullptr;
        }
        return m_boundIndexedUniformBuffers[index];
    // OES_draw_buffers_indexed
    case GraphicsContextGL::BLEND_EQUATION_RGB:
    case GraphicsContextGL::BLEND_EQUATION_ALPHA:
    case GraphicsContextGL::BLEND_SRC_RGB:
    case GraphicsContextGL::BLEND_SRC_ALPHA:
    case GraphicsContextGL::BLEND_DST_RGB:
    case GraphicsContextGL::BLEND_DST_ALPHA:
    case GraphicsContextGL::COLOR_WRITEMASK:
        if (!m_oesDrawBuffersIndexed) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getIndexedParameter"_s, "invalid parameter name, OES_draw_buffers_indexed not enabled"_s);
            return nullptr;
        }
        if (target == GraphicsContextGL::COLOR_WRITEMASK)
            return getIndexedBooleanArrayParameter(target, index);
        return m_context->getIntegeri(target, index);
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getIndexedParameter"_s, "invalid parameter name"_s);
        return nullptr;
    }
}

Vector<bool> WebGL2RenderingContext::getIndexedBooleanArrayParameter(GCGLenum pname, GCGLuint index)
{
    ASSERT(pname == GraphicsContextGL::COLOR_WRITEMASK);
    GCGLint value[4];
    m_context->getIntegeri_v(pname, index, value);
    Vector<bool> vector(4);
    for (unsigned i = 0; i < 4; ++i)
        vector[i] = value[i];
    return vector;
}

std::optional<Vector<GCGLuint>> WebGL2RenderingContext::getUniformIndices(WebGLProgram& program, const Vector<String>& names)
{
    if (isContextLost())
        return std::nullopt;
    if (!validateWebGLObject("getUniformIndices"_s, program))
        return std::nullopt;
    return m_context->getUniformIndices(program.object(), names);
}

WebGLAny WebGL2RenderingContext::getActiveUniforms(WebGLProgram& program, const Vector<GCGLuint>& uniformIndices, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;
    if (!validateWebGLObject("getActiveUniforms"_s, program))
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
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getActiveUniforms"_s, "invalid parameter name"_s);
        return nullptr;
    }
}

GCGLuint WebGL2RenderingContext::getUniformBlockIndex(WebGLProgram& program, const String& uniformBlockName)
{
    if (isContextLost())
        return 0;
    if (!validateWebGLObject("getUniformBlockIndex"_s, program))
        return 0;
    return m_context->getUniformBlockIndex(program.object(), uniformBlockName);
}

WebGLAny WebGL2RenderingContext::getActiveUniformBlockParameter(WebGLProgram& program, GCGLuint uniformBlockIndex, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;
    if (!validateWebGLObject("getActiveUniformBlockParameter"_s, program))
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
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getActiveUniformBlockParameter"_s, "invalid parameter name"_s);
        return nullptr;
    }
}

WebGLAny WebGL2RenderingContext::getActiveUniformBlockName(WebGLProgram& program, GCGLuint index)
{
    if (isContextLost())
        return String { };
    if (!validateWebGLObject("getActiveUniformBlockName"_s, program))
        return String();
    if (!program.getLinkStatus()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getActiveUniformBlockName"_s, "program not linked"_s);
        return nullptr;
    }
    String name = m_context->getActiveUniformBlockName(program.object(), index);
    if (name.isNull())
        return nullptr;
    return name;
}

void WebGL2RenderingContext::uniformBlockBinding(WebGLProgram& program, GCGLuint uniformBlockIndex, GCGLuint uniformBlockBinding)
{
    if (isContextLost())
        return;
    if (!validateWebGLObject("uniformBlockBinding"_s, program))
        return;
    m_context->uniformBlockBinding(program.object(), uniformBlockIndex, uniformBlockBinding);
}

RefPtr<WebGLVertexArrayObject> WebGL2RenderingContext::createVertexArray()
{
    if (isContextLost())
        return nullptr;

    return WebGLVertexArrayObject::create(*this, WebGLVertexArrayObject::Type::User);
}

void WebGL2RenderingContext::deleteVertexArray(WebGLVertexArrayObject* arrayObject)
{
    Locker locker { objectGraphLock() };

    // validateWebGLObject generates an error if the object has already been
    // deleted, so we must replicate most of its checks here.
    if (isContextLost() || !arrayObject)
        return;

    if (!arrayObject->validate(*this)) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "delete"_s, "object does not belong to this context"_s);
        return;
    }

    if (arrayObject->isDeleted())
        return;

    if (!arrayObject->isDefaultObject() && arrayObject == m_boundVertexArrayObject) {
        // The default VAO was removed in OpenGL 3.3 but not from WebGL 2; bind the default for WebGL to use.
        m_context->bindVertexArray(m_defaultVertexArrayObject->object());
        setBoundVertexArrayObject(locker, m_defaultVertexArrayObject.get());
    }

    arrayObject->deleteObject(locker, protectedGraphicsContextGL().get());
}

GCGLboolean WebGL2RenderingContext::isVertexArray(WebGLVertexArrayObject* arrayObject)
{
    if (isContextLost())
        return false;
    if (!validateIsWebGLObject(arrayObject))
        return false;
    return m_context->isVertexArray(arrayObject->object());
}

void WebGL2RenderingContext::bindVertexArray(WebGLVertexArrayObject* arrayObject)
{
    if (isContextLost())
        return;
    Locker locker { objectGraphLock() };
    if (!validateNullableWebGLObject("bindVertexArray"_s, arrayObject))
        return;

    if (arrayObject && !arrayObject->isDefaultObject() && arrayObject->object()) {
        m_context->bindVertexArray(arrayObject->object());
        setBoundVertexArrayObject(locker, arrayObject);
    } else {
        m_context->bindVertexArray(m_defaultVertexArrayObject->object());
        setBoundVertexArrayObject(locker, m_defaultVertexArrayObject.get());
    }
}

std::optional<WebGLExtensionAny> WebGL2RenderingContext::getExtension(const String& name)
{
    if (isContextLost())
        return std::nullopt;

    // When adding extensions that use enableDraftExtensions, add them to the webgl-draft-extensions-flag.js test.
    const bool enableDraftExtensions = scriptExecutionContext()->settingsValues().webGLDraftExtensionsEnabled;

#define ENABLE_IF_REQUESTED(type, variable, nameLiteral, canEnable) \
    if (equalIgnoringASCIICase(name, nameLiteral)) { \
        if (!(canEnable)) \
            return std::nullopt; \
        if (!variable) { \
            variable = adoptRef(new type(*this)); \
            InspectorInstrumentation::didEnableExtension(*this, name); \
        } \
        return *variable; \
    }

    ENABLE_IF_REQUESTED(EXTClipControl, m_extClipControl, "EXT_clip_control"_s, EXTClipControl::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTColorBufferFloat, m_extColorBufferFloat, "EXT_color_buffer_float"_s, EXTColorBufferFloat::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTColorBufferHalfFloat, m_extColorBufferHalfFloat, "EXT_color_buffer_half_float"_s, EXTColorBufferHalfFloat::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTConservativeDepth, m_extConservativeDepth, "EXT_conservative_depth"_s, EXTConservativeDepth::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTDepthClamp, m_extDepthClamp, "EXT_depth_clamp"_s, EXTDepthClamp::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTDisjointTimerQueryWebGL2, m_extDisjointTimerQueryWebGL2, "EXT_disjoint_timer_query_webgl2"_s, EXTDisjointTimerQueryWebGL2::supported(*m_context) && scriptExecutionContext()->settingsValues().webGLTimerQueriesEnabled);
    ENABLE_IF_REQUESTED(EXTFloatBlend, m_extFloatBlend, "EXT_float_blend"_s, EXTFloatBlend::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTPolygonOffsetClamp, m_extPolygonOffsetClamp, "EXT_polygon_offset_clamp"_s, EXTPolygonOffsetClamp::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTRenderSnorm, m_extRenderSnorm, "EXT_render_snorm"_s, EXTRenderSnorm::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTTextureCompressionBPTC, m_extTextureCompressionBPTC, "EXT_texture_compression_bptc"_s, EXTTextureCompressionBPTC::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTTextureCompressionRGTC, m_extTextureCompressionRGTC, "EXT_texture_compression_rgtc"_s, EXTTextureCompressionRGTC::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTTextureFilterAnisotropic, m_extTextureFilterAnisotropic, "EXT_texture_filter_anisotropic"_s, EXTTextureFilterAnisotropic::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTTextureMirrorClampToEdge, m_extTextureMirrorClampToEdge, "EXT_texture_mirror_clamp_to_edge"_s, EXTTextureMirrorClampToEdge::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTTextureNorm16, m_extTextureNorm16, "EXT_texture_norm16"_s, EXTTextureNorm16::supported(*m_context));
    ENABLE_IF_REQUESTED(KHRParallelShaderCompile, m_khrParallelShaderCompile, "KHR_parallel_shader_compile"_s, KHRParallelShaderCompile::supported(*m_context));
    ENABLE_IF_REQUESTED(NVShaderNoperspectiveInterpolation, m_nvShaderNoperspectiveInterpolation, "NV_shader_noperspective_interpolation"_s, NVShaderNoperspectiveInterpolation::supported(*m_context));
    ENABLE_IF_REQUESTED(OESDrawBuffersIndexed, m_oesDrawBuffersIndexed, "OES_draw_buffers_indexed"_s, OESDrawBuffersIndexed::supported(*m_context));
    ENABLE_IF_REQUESTED(OESSampleVariables, m_oesSampleVariables, "OES_sample_variables"_s, OESSampleVariables::supported(*m_context));
    ENABLE_IF_REQUESTED(OESShaderMultisampleInterpolation, m_oesShaderMultisampleInterpolation, "OES_shader_multisample_interpolation"_s, OESShaderMultisampleInterpolation::supported(*m_context));
    ENABLE_IF_REQUESTED(OESTextureFloatLinear, m_oesTextureFloatLinear, "OES_texture_float_linear"_s, OESTextureFloatLinear::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLBlendFuncExtended, m_webglBlendFuncExtended, "WEBGL_blend_func_extended"_s, WebGLBlendFuncExtended::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLClipCullDistance, m_webglClipCullDistance, "WEBGL_clip_cull_distance"_s, WebGLClipCullDistance::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureASTC, m_webglCompressedTextureASTC, "WEBGL_compressed_texture_astc"_s, WebGLCompressedTextureASTC::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureETC, m_webglCompressedTextureETC, "WEBGL_compressed_texture_etc"_s, WebGLCompressedTextureETC::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureETC1, m_webglCompressedTextureETC1, "WEBGL_compressed_texture_etc1"_s, WebGLCompressedTextureETC1::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLCompressedTexturePVRTC, m_webglCompressedTexturePVRTC, "WEBGL_compressed_texture_pvrtc"_s, WebGLCompressedTexturePVRTC::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLCompressedTexturePVRTC, m_webglCompressedTexturePVRTC, "WEBKIT_WEBGL_compressed_texture_pvrtc"_s, WebGLCompressedTexturePVRTC::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureS3TC, m_webglCompressedTextureS3TC, "WEBGL_compressed_texture_s3tc"_s, WebGLCompressedTextureS3TC::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureS3TCsRGB, m_webglCompressedTextureS3TCsRGB, "WEBGL_compressed_texture_s3tc_srgb"_s, WebGLCompressedTextureS3TCsRGB::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLDebugRendererInfo, m_webglDebugRendererInfo, "WEBGL_debug_renderer_info"_s, true);
    ENABLE_IF_REQUESTED(WebGLDebugShaders, m_webglDebugShaders, "WEBGL_debug_shaders"_s, WebGLDebugShaders::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLDrawInstancedBaseVertexBaseInstance, m_webglDrawInstancedBaseVertexBaseInstance, "WEBGL_draw_instanced_base_vertex_base_instance"_s, WebGLDrawInstancedBaseVertexBaseInstance::supported(*m_context) && enableDraftExtensions);
    ENABLE_IF_REQUESTED(WebGLLoseContext, m_webglLoseContext, "WEBGL_lose_context"_s, true);
    ENABLE_IF_REQUESTED(WebGLMultiDraw, m_webglMultiDraw, "WEBGL_multi_draw"_s, WebGLMultiDraw::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLMultiDrawInstancedBaseVertexBaseInstance, m_webglMultiDrawInstancedBaseVertexBaseInstance, "WEBGL_multi_draw_instanced_base_vertex_base_instance"_s, WebGLMultiDrawInstancedBaseVertexBaseInstance::supported(*m_context) && enableDraftExtensions);
    ENABLE_IF_REQUESTED(WebGLPolygonMode, m_webglPolygonMode, "WEBGL_polygon_mode"_s, WebGLPolygonMode::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLProvokingVertex, m_webglProvokingVertex, "WEBGL_provoking_vertex"_s, WebGLProvokingVertex::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLRenderSharedExponent, m_webglRenderSharedExponent, "WEBGL_render_shared_exponent"_s, WebGLRenderSharedExponent::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLStencilTexturing, m_webglStencilTexturing, "WEBGL_stencil_texturing"_s, WebGLStencilTexturing::supported(*m_context));
    return std::nullopt;
}

std::optional<Vector<String>> WebGL2RenderingContext::getSupportedExtensions()
{
    if (isContextLost())
        return std::nullopt;

    Vector<String> result;

    const bool enableDraftExtensions = scriptExecutionContext()->settingsValues().webGLDraftExtensionsEnabled;

#define APPEND_IF_SUPPORTED(nameLiteral, condition) \
    if (condition) \
        result.append(nameLiteral ## _s);

    APPEND_IF_SUPPORTED("EXT_clip_control", EXTClipControl::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_color_buffer_float", EXTColorBufferFloat::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_color_buffer_half_float", EXTColorBufferHalfFloat::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_conservative_depth", EXTConservativeDepth::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_depth_clamp", EXTDepthClamp::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_disjoint_timer_query_webgl2", EXTDisjointTimerQueryWebGL2::supported(*m_context) && scriptExecutionContext()->settingsValues().webGLTimerQueriesEnabled)
    APPEND_IF_SUPPORTED("EXT_float_blend", EXTFloatBlend::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_polygon_offset_clamp", EXTPolygonOffsetClamp::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_render_snorm", EXTRenderSnorm::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_texture_compression_bptc", EXTTextureCompressionBPTC::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_texture_compression_rgtc", EXTTextureCompressionRGTC::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_texture_filter_anisotropic", EXTTextureFilterAnisotropic::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_texture_mirror_clamp_to_edge", EXTTextureMirrorClampToEdge::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_texture_norm16", EXTTextureNorm16::supported(*m_context))
    APPEND_IF_SUPPORTED("KHR_parallel_shader_compile", KHRParallelShaderCompile::supported(*m_context))
    APPEND_IF_SUPPORTED("NV_shader_noperspective_interpolation", NVShaderNoperspectiveInterpolation::supported(*m_context))
    APPEND_IF_SUPPORTED("OES_draw_buffers_indexed", OESDrawBuffersIndexed::supported(*m_context))
    APPEND_IF_SUPPORTED("OES_sample_variables", OESSampleVariables::supported(*m_context))
    APPEND_IF_SUPPORTED("OES_shader_multisample_interpolation", OESShaderMultisampleInterpolation::supported(*m_context))
    APPEND_IF_SUPPORTED("OES_texture_float_linear", OESTextureFloatLinear::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_blend_func_extended", WebGLBlendFuncExtended::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_clip_cull_distance", WebGLClipCullDistance::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_compressed_texture_astc", WebGLCompressedTextureASTC::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_compressed_texture_etc", WebGLCompressedTextureETC::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_compressed_texture_etc1", WebGLCompressedTextureETC1::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_compressed_texture_pvrtc", WebGLCompressedTexturePVRTC::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBKIT_WEBGL_compressed_texture_pvrtc", WebGLCompressedTexturePVRTC::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_compressed_texture_s3tc", WebGLCompressedTextureS3TC::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_compressed_texture_s3tc_srgb", WebGLCompressedTextureS3TCsRGB::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_debug_renderer_info", true)
    APPEND_IF_SUPPORTED("WEBGL_debug_shaders", WebGLDebugShaders::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_draw_instanced_base_vertex_base_instance", WebGLDrawInstancedBaseVertexBaseInstance::supported(*m_context) && enableDraftExtensions)
    APPEND_IF_SUPPORTED("WEBGL_lose_context", true)
    APPEND_IF_SUPPORTED("WEBGL_multi_draw", WebGLMultiDraw::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_multi_draw_instanced_base_vertex_base_instance", WebGLMultiDrawInstancedBaseVertexBaseInstance::supported(*m_context) && enableDraftExtensions)
    APPEND_IF_SUPPORTED("WEBGL_polygon_mode", WebGLPolygonMode::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_provoking_vertex", WebGLProvokingVertex::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_render_shared_exponent", WebGLRenderSharedExponent::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_stencil_texturing", WebGLStencilTexturing::supported(*m_context))

    return result;
}

static bool validateDefaultFramebufferAttachment(GCGLenum attachment)
{
    switch (attachment) {
    case GraphicsContextGL::BACK:
    case GraphicsContextGL::DEPTH:
    case GraphicsContextGL::STENCIL:
        return true;
    }

    return false;
}

WebGLAny WebGL2RenderingContext::getFramebufferAttachmentParameter(GCGLenum target, GCGLenum attachment, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;

    auto functionName = "getFramebufferAttachmentParameter"_s;
    if (!validateFramebufferTarget(target)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target"_s);
        return nullptr;
    }
    RefPtr<WebGLFramebuffer> targetFramebuffer = (target == GraphicsContextGL::READ_FRAMEBUFFER) ? m_readFramebufferBinding : m_framebufferBinding;

    if (!targetFramebuffer) {
        if (!validateDefaultFramebufferAttachment(attachment)) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid attachment"_s);
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
                synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "invalid parameter name"_s);
                return nullptr;
            }
        }

        switch (pname) {
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return static_cast<unsigned>(GraphicsContextGL::FRAMEBUFFER_DEFAULT);
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_RED_SIZE:
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
            return attachment == GraphicsContextGL::BACK ? 8 : 0;
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
            return attachment == GraphicsContextGL::BACK && m_attributes.alpha ? 8 : 0;
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
            return attachment == GraphicsContextGL::DEPTH ? 24 : 0;
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
            return attachment == GraphicsContextGL::STENCIL ? 8 : 0;
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE:
            return static_cast<unsigned>(GraphicsContextGL::UNSIGNED_NORMALIZED);
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING:
            return static_cast<unsigned>(GraphicsContextGL::LINEAR);
        default:
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid parameter name"_s);
            return nullptr;
        }
    }

#if ENABLE(WEBXR)
    if (targetFramebuffer->isOpaque()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getFramebufferAttachmentParameter"_s, "An opaque framebuffer's attachments cannot be inspected or changed"_s);
        return nullptr;
    }
#endif

    if (!validateNonDefaultFramebufferAttachment(functionName, attachment))
        return nullptr;

    std::optional<WebGLFramebuffer::AttachmentObject> attachmentObject;
    if (attachment == GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT) {
        attachmentObject = targetFramebuffer->getAttachmentObject(GraphicsContextGL::DEPTH_ATTACHMENT);
        auto stencilAttachment = targetFramebuffer->getAttachmentObject(GraphicsContextGL::STENCIL_ATTACHMENT);
        if (attachmentObject != stencilAttachment) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "different objects bound to DEPTH_ATTACHMENT and STENCIL_ATTACHMENT"_s);
            return nullptr;
        }
    } else
        attachmentObject = targetFramebuffer->getAttachmentObject(attachment);

    if (!attachmentObject) {
        if (pname == GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
            return static_cast<unsigned>(GraphicsContextGL::NONE);
        if (pname == GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME)
            return nullptr;
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "invalid parameter name"_s);
        return nullptr;
    }

    const bool isTexture = std::holds_alternative<RefPtr<WebGLTexture>>(*attachmentObject);
    switch (pname) {
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        if (isTexture)
            return static_cast<unsigned>(GraphicsContextGL::TEXTURE);
        return static_cast<unsigned>(GraphicsContextGL::RENDERBUFFER);
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        if (isTexture)
            return std::get<RefPtr<WebGLTexture>>(WTFMove(*attachmentObject));
        return std::get<RefPtr<WebGLRenderbuffer>>(WTFMove(*attachmentObject));
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER:
        if (!isTexture)
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
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "component type cannot be queried for DEPTH_STENCIL_ATTACHMENT"_s);
            return nullptr;
        }
        return m_context->getFramebufferAttachmentParameteri(target, attachment, pname);
    }

    synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid parameter name"_s);
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

bool WebGL2RenderingContext::validateNonDefaultFramebufferAttachment(ASCIILiteral functionName, GCGLenum attachment)
{
    switch (attachment) {
    case GraphicsContextGL::DEPTH_ATTACHMENT:
    case GraphicsContextGL::STENCIL_ATTACHMENT:
    case GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT:
        return true;
    default:
        if (attachment >= GraphicsContextGL::COLOR_ATTACHMENT0 && attachment < static_cast<GCGLenum>(GraphicsContextGL::COLOR_ATTACHMENT0 + maxColorAttachments()))
            return true;
    }

    synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid attachment"_s);
    return false;
}

GCGLint WebGL2RenderingContext::maxDrawBuffers()
{
    if (!m_maxDrawBuffers)
        m_maxDrawBuffers = m_context->getInteger(GraphicsContextGL::MAX_DRAW_BUFFERS);
    return m_maxDrawBuffers;
}

GCGLint WebGL2RenderingContext::maxColorAttachments()
{
    // DrawBuffers requires MAX_COLOR_ATTACHMENTS == MAX_DRAW_BUFFERS
    if (!m_maxColorAttachments)
        m_maxColorAttachments = m_context->getInteger(GraphicsContextGL::MAX_DRAW_BUFFERS);
    return m_maxColorAttachments;
}

void WebGL2RenderingContext::renderbufferStorageImpl(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, ASCIILiteral functionName)
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
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "for integer formats, samples > 0 is not allowed"_s);
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
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "invalid internalformat for samples > 0"_s);
            return;
        }
        renderbufferStorageHelper(target, 0, GraphicsContextGL::DEPTH24_STENCIL8, width, height);
        break;
    case GraphicsContextGL::R16F:
    case GraphicsContextGL::RG16F:
    case GraphicsContextGL::RGBA16F:
        if (!m_extColorBufferFloat && !m_extColorBufferHalfFloat) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "EXT_color_buffer_float or EXT_color_buffer_half_float not enabled"_s);
            return;
        }
        renderbufferStorageHelper(target, samples, internalformat, width, height);
        break;
    case GraphicsContextGL::R32F:
    case GraphicsContextGL::RG32F:
    case GraphicsContextGL::RGBA32F:
    case GraphicsContextGL::R11F_G11F_B10F:
        if (!m_extColorBufferFloat) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "EXT_color_buffer_float not enabled"_s);
            return;
        }
        renderbufferStorageHelper(target, samples, internalformat, width, height);
        break;
    case GraphicsContextGL::RGB9_E5:
        if (!m_webglRenderSharedExponent) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "WEBGL_render_shared_exponent not enabled"_s);
            return;
        }
        renderbufferStorageHelper(target, samples, internalformat, width, height);
        break;
    case GraphicsContextGL::R16_EXT:
    case GraphicsContextGL::RG16_EXT:
    case GraphicsContextGL::RGBA16_EXT:
        if (!m_extTextureNorm16) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "EXT_texture_norm16 not enabled"_s);
            return;
        }
        renderbufferStorageHelper(target, samples, internalformat, width, height);
        break;
    case GraphicsContextGL::R8_SNORM:
    case GraphicsContextGL::RG8_SNORM:
    case GraphicsContextGL::RGBA8_SNORM:
        if (!m_extRenderSnorm) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "EXT_render_snorm not enabled"_s);
            return;
        }
        renderbufferStorageHelper(target, samples, internalformat, width, height);
        break;
    case GraphicsContextGL::R16_SNORM_EXT:
    case GraphicsContextGL::RG16_SNORM_EXT:
    case GraphicsContextGL::RGBA16_SNORM_EXT:
        if (!m_extRenderSnorm || !m_extTextureNorm16) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "EXT_render_snorm or EXT_texture_norm16 not enabled"_s);
            return;
        }
        renderbufferStorageHelper(target, samples, internalformat, width, height);
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid internalformat"_s);
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

bool WebGL2RenderingContext::checkAndTranslateAttachments(ASCIILiteral functionName, GCGLenum target, Vector<GCGLenum>& attachments)
{
    if (!validateFramebufferTarget(target)) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target"_s);
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
                synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid attachment"_s);
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
    addWebCoreOpaqueRoot(visitor, m_readFramebufferBinding.get());
    if (m_readFramebufferBinding)
        m_readFramebufferBinding->addMembersToOpaqueRoots(locker, visitor);

    addWebCoreOpaqueRoot(visitor, m_boundTransformFeedback.get());
    if (m_boundTransformFeedback)
        m_boundTransformFeedback->addMembersToOpaqueRoots(locker, visitor);

    addWebCoreOpaqueRoot(visitor, m_boundCopyReadBuffer.get());
    addWebCoreOpaqueRoot(visitor, m_boundCopyWriteBuffer.get());
    addWebCoreOpaqueRoot(visitor, m_boundPixelPackBuffer.get());
    addWebCoreOpaqueRoot(visitor, m_boundPixelUnpackBuffer.get());
    addWebCoreOpaqueRoot(visitor, m_boundTransformFeedbackBuffer.get());
    addWebCoreOpaqueRoot(visitor, m_boundUniformBuffer.get());

    for (auto& buffer : m_boundIndexedUniformBuffers)
        addWebCoreOpaqueRoot(visitor, buffer.get());

    for (auto& entry : m_activeQueries)
        addWebCoreOpaqueRoot(visitor, entry.get());

    for (auto& entry : m_boundSamplers)
        addWebCoreOpaqueRoot(visitor, entry.get());
}

WebGLAny WebGL2RenderingContext::getParameter(GCGLenum pname)
{
    if (isContextLost())
        return nullptr;
    switch (pname) {
    case GraphicsContextGL::SHADING_LANGUAGE_VERSION:
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
        return maxColorAttachments();
    case GraphicsContextGL::MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
        return getInt64Parameter(pname);
    case GraphicsContextGL::MAX_COMBINED_UNIFORM_BLOCKS:
        return getIntParameter(pname);
    case GraphicsContextGL::MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
        return getInt64Parameter(pname);
    case GraphicsContextGL::MAX_DRAW_BUFFERS:
        return maxDrawBuffers();
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
        return maxSamples();
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
        return m_packParameters.rowLength;
    case GraphicsContextGL::PACK_SKIP_PIXELS:
        return m_packParameters.skipPixels;
    case GraphicsContextGL::PACK_SKIP_ROWS:
        return m_packParameters.skipRows;
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
        return m_boundTransformFeedback == m_defaultTransformFeedback ? nullptr : RefPtr { m_boundTransformFeedback.get() };
    case GraphicsContextGL::TRANSFORM_FEEDBACK_PAUSED:
        return getBooleanParameter(pname);
    case GraphicsContextGL::UNIFORM_BUFFER_BINDING:
        return m_boundUniformBuffer;
    case GraphicsContextGL::UNIFORM_BUFFER_OFFSET_ALIGNMENT:
        return getIntParameter(pname);
    case GraphicsContextGL::UNPACK_IMAGE_HEIGHT:
        return m_unpackParameters.imageHeight;
    case GraphicsContextGL::UNPACK_ROW_LENGTH:
        return m_unpackParameters.rowLength;
    case GraphicsContextGL::UNPACK_SKIP_IMAGES:
        return m_unpackParameters.skipImages;
    case GraphicsContextGL::UNPACK_SKIP_PIXELS:
        return m_unpackParameters.skipPixels;
    case GraphicsContextGL::UNPACK_SKIP_ROWS:
        return m_unpackParameters.skipRows;
    case GraphicsContextGL::VERTEX_ARRAY_BINDING:
        if (m_boundVertexArrayObject->isDefaultObject())
            return nullptr;
        return RefPtr { static_cast<WebGLVertexArrayObject*>(m_boundVertexArrayObject.get()) };
    case GraphicsContextGL::MAX_CLIP_DISTANCES_ANGLE:
    case GraphicsContextGL::MAX_CULL_DISTANCES_ANGLE:
    case GraphicsContextGL::MAX_COMBINED_CLIP_AND_CULL_DISTANCES_ANGLE:
    case GraphicsContextGL::CLIP_DISTANCE0_ANGLE:
    case GraphicsContextGL::CLIP_DISTANCE1_ANGLE:
    case GraphicsContextGL::CLIP_DISTANCE2_ANGLE:
    case GraphicsContextGL::CLIP_DISTANCE3_ANGLE:
    case GraphicsContextGL::CLIP_DISTANCE4_ANGLE:
    case GraphicsContextGL::CLIP_DISTANCE5_ANGLE:
    case GraphicsContextGL::CLIP_DISTANCE6_ANGLE:
    case GraphicsContextGL::CLIP_DISTANCE7_ANGLE:
        if (m_webglClipCullDistance) {
            if (pname >= GraphicsContextGL::CLIP_DISTANCE0_ANGLE && pname <= GraphicsContextGL::CLIP_DISTANCE7_ANGLE)
                return getBooleanParameter(pname);
            return getUnsignedIntParameter(pname);
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, WEBGL_clip_cull_distance not enabled"_s);
        return nullptr;
    case GraphicsContextGL::PROVOKING_VERTEX_ANGLE:
        if (m_webglProvokingVertex)
            return getUnsignedIntParameter(GraphicsContextGL::PROVOKING_VERTEX_ANGLE);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, WEBGL_provoking_vertex not enabled"_s);
        return nullptr;
    case GraphicsContextGL::MIN_FRAGMENT_INTERPOLATION_OFFSET_OES:
    case GraphicsContextGL::MAX_FRAGMENT_INTERPOLATION_OFFSET_OES:
    case GraphicsContextGL::FRAGMENT_INTERPOLATION_OFFSET_BITS_OES:
        if (m_oesShaderMultisampleInterpolation) {
            if (pname == GraphicsContextGL::FRAGMENT_INTERPOLATION_OFFSET_BITS_OES)
                return getIntParameter(pname);
            return getFloatParameter(pname);
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter"_s, "invalid parameter name, OES_shader_multisample_interpolation not enabled"_s);
        return nullptr;
    default:
        return WebGLRenderingContextBase::getParameter(pname);
    }
}

bool WebGL2RenderingContext::validateCapability(ASCIILiteral functionName, GCGLenum cap)
{
    switch (cap) {
    case GraphicsContextGL::CLIP_DISTANCE0_ANGLE:
    case GraphicsContextGL::CLIP_DISTANCE1_ANGLE:
    case GraphicsContextGL::CLIP_DISTANCE2_ANGLE:
    case GraphicsContextGL::CLIP_DISTANCE3_ANGLE:
    case GraphicsContextGL::CLIP_DISTANCE4_ANGLE:
    case GraphicsContextGL::CLIP_DISTANCE5_ANGLE:
    case GraphicsContextGL::CLIP_DISTANCE6_ANGLE:
    case GraphicsContextGL::CLIP_DISTANCE7_ANGLE:
        if (m_webglClipCullDistance)
            return true;
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid capability, WEBGL_clip_cull_distance not enabled"_s);
        return false;
    case GraphicsContextGL::RASTERIZER_DISCARD:
        return true;
    default:
        return WebGLRenderingContextBase::validateCapability(functionName, cap);
    }
}

template<typename T, typename TypedArrayType>
std::optional<std::span<const T>> WebGL2RenderingContext::validateClearBuffer(ASCIILiteral functionName, GCGLenum buffer, TypedList<TypedArrayType, T>& values, GCGLuint srcOffset)
{
    Checked<GCGLsizei, RecordOverflow> checkedSize(values.length());
    checkedSize -= srcOffset;
    if (checkedSize.hasOverflowed()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid array size / srcOffset"_s);
        return { };
    }
    switch (buffer) {
    case GraphicsContextGL::COLOR:
        if (checkedSize < 4) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid array size / srcOffset"_s);
            return { };
        }
        return std::span { values.data() + srcOffset, 4 };
    case GraphicsContextGL::DEPTH:
    case GraphicsContextGL::STENCIL:
        if (checkedSize < 1) {
            synthesizeGLError(GraphicsContextGL::INVALID_VALUE, functionName, "invalid array size / srcOffset"_s);
            return { };
        }
        return std::span { values.data() + srcOffset, 1 };

    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid buffer"_s);
        return { };
    }
}

void WebGL2RenderingContext::uniform1fv(const WebGLUniformLocation* location, Float32List&& data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto result = validateUniformParameters("uniform1fv"_s, location, data, 1, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform1fv(location->location(), result.value());
}

void WebGL2RenderingContext::uniform2fv(const WebGLUniformLocation* location, Float32List&& data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto result = validateUniformParameters("uniform2fv"_s, location, data, 2, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform2fv(location->location(), result.value());
}

void WebGL2RenderingContext::uniform3fv(const WebGLUniformLocation* location, Float32List&& data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto result = validateUniformParameters("uniform3fv"_s, location, data, 3, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform3fv(location->location(), result.value());
}

void WebGL2RenderingContext::uniform4fv(const WebGLUniformLocation* location, Float32List&& data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto result = validateUniformParameters("uniform4fv"_s, location, data, 4, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform4fv(location->location(), result.value());
}

void WebGL2RenderingContext::uniform1iv(const WebGLUniformLocation* location, Int32List&& data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto result = validateUniformParameters("uniform1iv"_s, location, data, 1, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform1iv(location->location(), result.value());
}

void WebGL2RenderingContext::uniform2iv(const WebGLUniformLocation* location, Int32List&& data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto result = validateUniformParameters("uniform2iv"_s, location, data, 2, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform2iv(location->location(), result.value());
}

void WebGL2RenderingContext::uniform3iv(const WebGLUniformLocation* location, Int32List&& data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto result = validateUniformParameters("uniform3iv"_s, location, data, 3, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform3iv(location->location(), result.value());
}

void WebGL2RenderingContext::uniform4iv(const WebGLUniformLocation* location, Int32List&& data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto result = validateUniformParameters("uniform4iv"_s, location, data, 4, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniform4iv(location->location(), result.value());
}

void WebGL2RenderingContext::uniformMatrix2fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto result = validateUniformMatrixParameters("uniformMatrix2fv"_s, location, transpose, data, 2*2, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniformMatrix2fv(location->location(), transpose, result.value());
}

void WebGL2RenderingContext::uniformMatrix3fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto result = validateUniformMatrixParameters("uniformMatrix3fv"_s, location, transpose, data, 3*3, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniformMatrix3fv(location->location(), transpose, result.value());
}

void WebGL2RenderingContext::uniformMatrix4fv(const WebGLUniformLocation* location, GCGLboolean transpose, Float32List&& data, GCGLuint srcOffset, GCGLuint srcLength)
{
    if (isContextLost())
        return;
    auto result = validateUniformMatrixParameters("uniformMatrix4fv"_s, location, transpose, data, 4*4, srcOffset, srcLength);
    if (!result)
        return;
    m_context->uniformMatrix4fv(location->location(), transpose, result.value());
}

void WebGL2RenderingContext::readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, RefPtr<ArrayBufferView>&& pixels)
{
    if (isContextLost())
        return;
    if (m_boundPixelPackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "readPixels"_s,
            "a buffer is bound to PIXEL_PACK_BUFFER"_s);
        return;
    }
    WebGLRenderingContextBase::readPixels(x, y, width, height, format, type, WTFMove(pixels));
}

void WebGL2RenderingContext::readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    if (isContextLost())
        return;
    if (!m_boundPixelPackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "readPixels"_s,
            "no buffer is bound to PIXEL_PACK_BUFFER"_s);
        return;
    }
    if (offset < 0) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "readPixels"_s, "negative offset"_s);
        return;
    }
    if (type == GraphicsContextGL::UNSIGNED_INT_24_8) {
        // This type is always invalid for readbacks in WebGL.
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "readPixels"_s, "invalid type UNSIGNED_INT_24_8"_s);
        return;
    }
    if (!validateImageFormatAndType("readPixels"_s, format, type))
        return;

    if (!validateReadPixelsDimensions(width, height))
        return;

    IntRect rect { x, y, width, height };
    auto packSizes = GraphicsContextGL::computeImageSize(format, type, rect.size(), 1, m_packParameters);
    if (!packSizes) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "readPixels"_s, "invalid texture dimensions"_s);
        return;
    }
    auto offsetAndSkip = checkedSum<GCGLintptr>(offset, packSizes->initialSkipBytes);
    if (offsetAndSkip.hasOverflowed()) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "readPixels"_s, "invalid pack parameters"_s);
        return;
    }
    // Due to WebGL's same-origin restrictions, it is not possible to
    // taint the origin using the WebGL API.
    ASSERT(canvasBase().originClean());

    clearIfComposited(CallerTypeOther);

    m_context->readPixelsBufferObject(rect, format, type, offsetAndSkip.value(), m_packParameters.alignment, m_packParameters.rowLength);
}

void WebGL2RenderingContext::readPixels(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, ArrayBufferView& dstData, GCGLuint dstOffset)
{
    if (isContextLost())
        return;
    if (m_boundPixelPackBuffer) {
        synthesizeGLError(
            GraphicsContextGL::INVALID_OPERATION, "readPixels"_s,
            "a buffer is bound to PIXEL_PACK_BUFFER"_s);
        return;
    }
    auto slice = sliceArrayBufferView("readPixels"_s, dstData, dstOffset, 0);
    if (!slice)
        return;
    WebGLRenderingContextBase::readPixels(x, y, width, height, format, type, WTFMove(slice));
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

    for (auto& boundBuffer : m_boundIndexedUniformBuffers) {
        if (boundBuffer == buffer)
            boundBuffer = nullptr;
    }

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

    m_defaultFramebuffer->markBuffersClear(buffersToClear);
}

} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBGL)
