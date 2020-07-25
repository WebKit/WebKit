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
#include "WebGLRenderingContext.h"

#if ENABLE(WEBGL)

#include "ANGLEInstancedArrays.h"
#include "CachedImage.h"
#include "EXTBlendMinMax.h"
#include "EXTColorBufferHalfFloat.h"
#include "EXTFragDepth.h"
#include "EXTShaderTextureLOD.h"
#include "EXTTextureFilterAnisotropic.h"
#include "EXTsRGB.h"
#include "ExtensionsGL.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageData.h"
#include "InspectorInstrumentation.h"
#include "OESElementIndexUint.h"
#include "OESStandardDerivatives.h"
#include "OESTextureFloat.h"
#include "OESTextureFloatLinear.h"
#include "OESTextureHalfFloat.h"
#include "OESTextureHalfFloatLinear.h"
#include "OESVertexArrayObject.h"
#include "RenderBox.h"
#include "RuntimeEnabledFeatures.h"
#include "WebGLColorBufferFloat.h"
#include "WebGLCompressedTextureASTC.h"
#include "WebGLCompressedTextureATC.h"
#include "WebGLCompressedTextureETC.h"
#include "WebGLCompressedTextureETC1.h"
#include "WebGLCompressedTexturePVRTC.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLDepthTexture.h"
#include "WebGLDrawBuffers.h"
#include "WebGLLoseContext.h"
#include "WebGLVertexArrayObjectOES.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSCellInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebGLRenderingContext);

std::unique_ptr<WebGLRenderingContext> WebGLRenderingContext::create(CanvasBase& canvas, GraphicsContextGLAttributes attributes)
{
    auto renderingContext = std::unique_ptr<WebGLRenderingContext>(new WebGLRenderingContext(canvas, attributes));
    // This context is pending policy resolution, so don't call initializeNewContext on it yet.

    InspectorInstrumentation::didCreateCanvasRenderingContext(*renderingContext);

    return renderingContext;
}

std::unique_ptr<WebGLRenderingContext> WebGLRenderingContext::create(CanvasBase& canvas, Ref<GraphicsContextGLOpenGL>&& context, GraphicsContextGLAttributes attributes)
{
    auto renderingContext = std::unique_ptr<WebGLRenderingContext>(new WebGLRenderingContext(canvas, WTFMove(context), attributes));
    // This is virtual and can't be called in the constructor.
    renderingContext->initializeNewContext();

    InspectorInstrumentation::didCreateCanvasRenderingContext(*renderingContext);

    return renderingContext;
}

WebGLRenderingContext::WebGLRenderingContext(CanvasBase& canvas, GraphicsContextGLAttributes attributes)
    : WebGLRenderingContextBase(canvas, attributes)
{
}

WebGLRenderingContext::WebGLRenderingContext(CanvasBase& canvas, Ref<GraphicsContextGLOpenGL>&& context, GraphicsContextGLAttributes attributes)
    : WebGLRenderingContextBase(canvas, WTFMove(context), attributes)
{
    if (isContextLost())
        return;
}

void WebGLRenderingContext::initializeVertexArrayObjects()
{
    m_defaultVertexArrayObject = WebGLVertexArrayObjectOES::create(*this, WebGLVertexArrayObjectOES::Type::Default);
    addContextObject(*m_defaultVertexArrayObject);
    m_boundVertexArrayObject = m_defaultVertexArrayObject;
#if !USE(ANGLE)
    if (!isGLES2Compliant())
        initVertexAttrib0();
#endif
}

WebGLExtension* WebGLRenderingContext::getExtension(const String& name)
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

    ENABLE_IF_REQUESTED(EXTBlendMinMax, m_extBlendMinMax, "EXT_blend_minmax", enableSupportedExtension("GL_EXT_blend_minmax"_s));
    ENABLE_IF_REQUESTED(EXTsRGB, m_extsRGB, "EXT_sRGB", enableSupportedExtension("GL_EXT_sRGB"_s));
    ENABLE_IF_REQUESTED(EXTFragDepth, m_extFragDepth, "EXT_frag_depth", enableSupportedExtension("GL_EXT_frag_depth"_s));
    if (equalIgnoringASCIICase(name, "EXT_shader_texture_lod")) {
        if (!m_extShaderTextureLOD) {
            if (!(m_context->getExtensions().supports("GL_EXT_shader_texture_lod"_s) || m_context->getExtensions().supports("GL_ARB_shader_texture_lod"_s)))
                m_extShaderTextureLOD = nullptr;
            else {
                m_context->getExtensions().ensureEnabled("GL_EXT_shader_texture_lod"_s);
                m_extShaderTextureLOD = makeUnique<EXTShaderTextureLOD>(*this);
                InspectorInstrumentation::didEnableExtension(*this, name);
            }
        }
        return m_extShaderTextureLOD.get();
    }
    ENABLE_IF_REQUESTED(EXTTextureFilterAnisotropic, m_extTextureFilterAnisotropic, "EXT_texture_filter_anisotropic", enableSupportedExtension("GL_EXT_texture_filter_anisotropic"_s));
    ENABLE_IF_REQUESTED(OESStandardDerivatives, m_oesStandardDerivatives, "OES_standard_derivatives", enableSupportedExtension("GL_OES_standard_derivatives"_s));
    ENABLE_IF_REQUESTED(OESTextureFloat, m_oesTextureFloat, "OES_texture_float", OESTextureFloat::supported(*this));
    ENABLE_IF_REQUESTED(OESTextureFloatLinear, m_oesTextureFloatLinear, "OES_texture_float_linear", enableSupportedExtension("GL_OES_texture_float_linear"_s));
    ENABLE_IF_REQUESTED(OESTextureHalfFloat, m_oesTextureHalfFloat, "OES_texture_half_float", OESTextureHalfFloat::supported(*this));
    ENABLE_IF_REQUESTED(OESTextureHalfFloatLinear, m_oesTextureHalfFloatLinear, "OES_texture_half_float_linear", enableSupportedExtension("GL_OES_texture_half_float_linear"_s));
    ENABLE_IF_REQUESTED(OESVertexArrayObject, m_oesVertexArrayObject, "OES_vertex_array_object", enableSupportedExtension("GL_OES_vertex_array_object"_s));
    ENABLE_IF_REQUESTED(OESElementIndexUint, m_oesElementIndexUint, "OES_element_index_uint", enableSupportedExtension("GL_OES_element_index_uint"_s));
    ENABLE_IF_REQUESTED(WebGLLoseContext, m_webglLoseContext, "WEBGL_lose_context", true);
    ENABLE_IF_REQUESTED(WebGLCompressedTextureASTC, m_webglCompressedTextureASTC, "WEBGL_compressed_texture_astc", WebGLCompressedTextureASTC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureATC, m_webglCompressedTextureATC, "WEBKIT_WEBGL_compressed_texture_atc", WebGLCompressedTextureATC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureETC, m_webglCompressedTextureETC, "WEBGL_compressed_texture_etc", WebGLCompressedTextureETC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureETC1, m_webglCompressedTextureETC1, "WEBGL_compressed_texture_etc1", WebGLCompressedTextureETC1::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTexturePVRTC, m_webglCompressedTexturePVRTC, "WEBKIT_WEBGL_compressed_texture_pvrtc", WebGLCompressedTexturePVRTC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureS3TC, m_webglCompressedTextureS3TC, "WEBGL_compressed_texture_s3tc", WebGLCompressedTextureS3TC::supported(*this));
    ENABLE_IF_REQUESTED(WebGLDepthTexture, m_webglDepthTexture, "WEBGL_depth_texture", WebGLDepthTexture::supported(*m_context));
    if (equalIgnoringASCIICase(name, "WEBGL_draw_buffers")) {
        if (!m_webglDrawBuffers) {
            if (!supportsDrawBuffers())
                m_webglDrawBuffers = nullptr;
            else {
                m_context->getExtensions().ensureEnabled("GL_EXT_draw_buffers"_s);
                m_webglDrawBuffers = makeUnique<WebGLDrawBuffers>(*this);
                InspectorInstrumentation::didEnableExtension(*this, name);
            }
        }
        return m_webglDrawBuffers.get();
    }
    if (equalIgnoringASCIICase(name, "ANGLE_instanced_arrays")) {
        if (!m_angleInstancedArrays) {
            if (!ANGLEInstancedArrays::supported(*this))
                m_angleInstancedArrays = nullptr;
            else {
                m_context->getExtensions().ensureEnabled("GL_ANGLE_instanced_arrays"_s);
                m_angleInstancedArrays = makeUnique<ANGLEInstancedArrays>(*this);
                InspectorInstrumentation::didEnableExtension(*this, name);
            }
        }
        return m_angleInstancedArrays.get();
    }
    ENABLE_IF_REQUESTED(WebGLDebugRendererInfo, m_webglDebugRendererInfo, "WEBGL_debug_renderer_info", true);
    ENABLE_IF_REQUESTED(WebGLDebugShaders, m_webglDebugShaders, "WEBGL_debug_shaders", m_context->getExtensions().supports("GL_ANGLE_translated_shader_source"_s));
    ENABLE_IF_REQUESTED(EXTColorBufferHalfFloat, m_extColorBufferHalfFloat, "EXT_color_buffer_half_float", EXTColorBufferHalfFloat::supported(*this));
    ENABLE_IF_REQUESTED(WebGLColorBufferFloat, m_webglColorBufferFloat, "WEBGL_color_buffer_float", WebGLColorBufferFloat::supported(*this));
    return nullptr;
}

Optional<Vector<String>> WebGLRenderingContext::getSupportedExtensions()
{
    if (isContextLost())
        return WTF::nullopt;

    Vector<String> result;

    if (m_isPendingPolicyResolution)
        return result;

    if (m_context->getExtensions().supports("GL_EXT_blend_minmax"_s))
        result.append("EXT_blend_minmax"_s);
    if (m_context->getExtensions().supports("GL_EXT_sRGB"_s))
        result.append("EXT_sRGB"_s);
    if (m_context->getExtensions().supports("GL_EXT_frag_depth"_s))
        result.append("EXT_frag_depth"_s);
    if (m_context->getExtensions().supports("GL_OES_texture_float"_s))
        result.append("OES_texture_float"_s);
    if (m_context->getExtensions().supports("GL_OES_texture_float_linear"_s))
        result.append("OES_texture_float_linear"_s);
    if (m_context->getExtensions().supports("GL_OES_texture_half_float"_s))
        result.append("OES_texture_half_float"_s);
    if (m_context->getExtensions().supports("GL_OES_texture_half_float_linear"_s))
        result.append("OES_texture_half_float_linear"_s);
    if (m_context->getExtensions().supports("GL_OES_standard_derivatives"_s))
        result.append("OES_standard_derivatives"_s);
    if (m_context->getExtensions().supports("GL_EXT_shader_texture_lod"_s) || m_context->getExtensions().supports("GL_ARB_shader_texture_lod"_s))
        result.append("EXT_shader_texture_lod"_s);
    if (m_context->getExtensions().supports("GL_EXT_texture_filter_anisotropic"_s))
        result.append("EXT_texture_filter_anisotropic"_s);
    if (m_context->getExtensions().supports("GL_OES_vertex_array_object"_s))
        result.append("OES_vertex_array_object"_s);
    if (m_context->getExtensions().supports("GL_OES_element_index_uint"_s))
        result.append("OES_element_index_uint"_s);
    result.append("WEBGL_lose_context"_s);
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
    if (WebGLDepthTexture::supported(*m_context))
        result.append("WEBGL_depth_texture"_s);
    if (supportsDrawBuffers())
        result.append("WEBGL_draw_buffers"_s);
    if (ANGLEInstancedArrays::supported(*this))
        result.append("ANGLE_instanced_arrays"_s);
    if (m_context->getExtensions().supports("GL_ANGLE_translated_shader_source"_s))
        result.append("WEBGL_debug_shaders"_s);
    result.append("WEBGL_debug_renderer_info"_s);
    if (EXTColorBufferHalfFloat::supported(*this))
        result.append("EXT_color_buffer_half_float"_s);
    if (WebGLColorBufferFloat::supported(*this))
        result.append("WEBGL_color_buffer_float"_s);

    return result;
}

WebGLAny WebGLRenderingContext::getFramebufferAttachmentParameter(GCGLenum target, GCGLenum attachment, GCGLenum pname)
{
    if (isContextLostOrPending() || !validateFramebufferFuncParameters("getFramebufferAttachmentParameter", target, attachment))
        return nullptr;

    if (!m_framebufferBinding || !m_framebufferBinding->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, "getFramebufferAttachmentParameter", "no framebuffer bound");
        return nullptr;
    }

    auto object = makeRefPtr(m_framebufferBinding->getAttachmentObject(attachment));
    if (!object) {
        if (pname == GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
            return static_cast<unsigned>(GraphicsContextGL::NONE);
        // OpenGL ES 2.0 specifies INVALID_ENUM in this case, while desktop GL
        // specifies INVALID_OPERATION.
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getFramebufferAttachmentParameter", "invalid parameter name");
        return nullptr;
    }

    if (object->isTexture()) {
        switch (pname) {
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return static_cast<unsigned>(GraphicsContextGL::TEXTURE);
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            return makeRefPtr(reinterpret_cast<WebGLTexture&>(*object));
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
        case ExtensionsGL::FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT: {
            GCGLint value = 0;
            m_context->getFramebufferAttachmentParameteriv(target, attachment, pname, &value);
            return value;
        }
        default:
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getFramebufferAttachmentParameter", "invalid parameter name for texture attachment");
            return nullptr;
        }
    } else {
        ASSERT(object->isRenderbuffer());
        switch (pname) {
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return static_cast<unsigned>(GraphicsContextGL::RENDERBUFFER);
        case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            return makeRefPtr(reinterpret_cast<WebGLRenderbuffer&>(*object));
        case ExtensionsGL::FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT: {
            if (!m_extsRGB) {
                synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getFramebufferAttachmentParameter", "invalid parameter name for renderbuffer attachment");
                return nullptr;
            }
            RefPtr<WebGLRenderbuffer> renderBuffer = reinterpret_cast<WebGLRenderbuffer*>(object.get());
            GCGLenum renderBufferFormat = renderBuffer->getInternalFormat();
            ASSERT(renderBufferFormat != ExtensionsGL::SRGB_EXT && renderBufferFormat != ExtensionsGL::SRGB_ALPHA_EXT);
            if (renderBufferFormat == ExtensionsGL::SRGB8_ALPHA8_EXT)
                return static_cast<unsigned>(ExtensionsGL::SRGB_EXT);
            return static_cast<unsigned>(GraphicsContextGL::LINEAR);
        }
        default:
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getFramebufferAttachmentParameter", "invalid parameter name for renderbuffer attachment");
            return nullptr;
        }
    }
}

GCGLint WebGLRenderingContext::getMaxDrawBuffers()
{
    if (!supportsDrawBuffers())
        return 0;
    if (!m_maxDrawBuffers)
        m_context->getIntegerv(ExtensionsGL::MAX_DRAW_BUFFERS_EXT, &m_maxDrawBuffers);
    if (!m_maxColorAttachments)
        m_context->getIntegerv(ExtensionsGL::MAX_COLOR_ATTACHMENTS_EXT, &m_maxColorAttachments);
    // WEBGL_draw_buffers requires MAX_COLOR_ATTACHMENTS >= MAX_DRAW_BUFFERS.
    return std::min(m_maxDrawBuffers, m_maxColorAttachments);
}

GCGLint WebGLRenderingContext::getMaxColorAttachments()
{
    if (!supportsDrawBuffers())
        return 0;
    if (!m_maxColorAttachments)
        m_context->getIntegerv(ExtensionsGL::MAX_COLOR_ATTACHMENTS_EXT, &m_maxColorAttachments);
    return m_maxColorAttachments;
}

bool WebGLRenderingContext::validateIndexArrayConservative(GCGLenum type, unsigned& numElementsRequired)
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
        case GraphicsContextGL::UNSIGNED_BYTE: {
            const GCGLubyte* p = static_cast<const GCGLubyte*>(buffer->data());
            for (GCGLsizeiptr i = 0; i < numElements; i++)
                maxIndex = maxIndex ? std::max(maxIndex.value(), static_cast<unsigned>(p[i])) : static_cast<unsigned>(p[i]);
            break;
        }
        case GraphicsContextGL::UNSIGNED_SHORT: {
            numElements /= sizeof(GCGLushort);
            const GCGLushort* p = static_cast<const GCGLushort*>(buffer->data());
            for (GCGLsizeiptr i = 0; i < numElements; i++)
                maxIndex = maxIndex ? std::max(maxIndex.value(), static_cast<unsigned>(p[i])) : static_cast<unsigned>(p[i]);
            break;
        }
        case GraphicsContextGL::UNSIGNED_INT: {
            if (!m_oesElementIndexUint)
                return false;
            numElements /= sizeof(GCGLuint);
            const GCGLuint* p = static_cast<const GCGLuint*>(buffer->data());
            for (GCGLsizeiptr i = 0; i < numElements; i++)
                maxIndex = maxIndex ? std::max(maxIndex.value(), static_cast<unsigned>(p[i])) : static_cast<unsigned>(p[i]);
            break;
        }
        default:
            return false;
        }
        if (maxIndex)
            elementArrayBuffer->setCachedMaxIndex(type, maxIndex.value());
    }

    if (!maxIndex)
        return false;

    // The number of required elements is one more than the maximum index that will be accessed.
    auto checkedNumElementsRequired = checkedAddAndMultiply<unsigned>(maxIndex.value(), 1, 1);
    if (!checkedNumElementsRequired)
        return false;
    numElementsRequired = checkedNumElementsRequired.value();

    return true;
}

bool WebGLRenderingContext::validateBlendEquation(const char* functionName, GCGLenum mode)
{
    switch (mode) {
    case GraphicsContextGL::FUNC_ADD:
    case GraphicsContextGL::FUNC_SUBTRACT:
    case GraphicsContextGL::FUNC_REVERSE_SUBTRACT:
    case ExtensionsGL::MIN_EXT:
    case ExtensionsGL::MAX_EXT:
        if ((mode == ExtensionsGL::MIN_EXT || mode == ExtensionsGL::MAX_EXT) && !m_extBlendMinMax) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid mode");
            return false;
        }
        return true;
        break;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid mode");
        return false;
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
