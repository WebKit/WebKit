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
#include "WebGLRenderingContext.h"

#if ENABLE(WEBGL)

#include "ANGLEInstancedArrays.h"
#include "CachedImage.h"
#include "EXTBlendMinMax.h"
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

    InspectorInstrumentation::didCreateCanvasRenderingContext(*renderingContext);

    return renderingContext;
}

std::unique_ptr<WebGLRenderingContext> WebGLRenderingContext::create(CanvasBase& canvas, Ref<GraphicsContextGLOpenGL>&& context, GraphicsContextGLAttributes attributes)
{
    auto renderingContext = std::unique_ptr<WebGLRenderingContext>(new WebGLRenderingContext(canvas, WTFMove(context), attributes));

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
    initializeVertexArrayObjects();
}

void WebGLRenderingContext::initializeVertexArrayObjects()
{
    m_defaultVertexArrayObject = WebGLVertexArrayObjectOES::create(*this, WebGLVertexArrayObjectOES::Type::Default);
    addContextObject(*m_defaultVertexArrayObject);
    m_boundVertexArrayObject = m_defaultVertexArrayObject;
    if (!isGLES2Compliant())
        initVertexAttrib0();
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
    ENABLE_IF_REQUESTED(OESTextureFloat, m_oesTextureFloat, "OES_texture_float", enableSupportedExtension("GL_OES_texture_float"_s));
    ENABLE_IF_REQUESTED(OESTextureFloatLinear, m_oesTextureFloatLinear, "OES_texture_float_linear", enableSupportedExtension("GL_OES_texture_float_linear"_s));
    ENABLE_IF_REQUESTED(OESTextureHalfFloat, m_oesTextureHalfFloat, "OES_texture_half_float", enableSupportedExtension("GL_OES_texture_half_float"_s));
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

bool WebGLRenderingContext::validateFramebufferFuncParameters(const char* functionName, GCGLenum target, GCGLenum attachment)
{
    if (target != GraphicsContextGL::FRAMEBUFFER) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid target");
        return false;
    }
    // FIXME: Why does this return true unconditionally for COLOR_ATTACHMENT0,
    // but false for other COLOR_ATTACHMENT values if m_webglDrawBuffers is false?
    switch (attachment) {
    case GraphicsContextGL::COLOR_ATTACHMENT0:
    case GraphicsContextGL::DEPTH_ATTACHMENT:
    case GraphicsContextGL::STENCIL_ATTACHMENT:
    case GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT:
        return true;
    default:
        if (m_webglDrawBuffers
            && attachment >= GraphicsContextGL::COLOR_ATTACHMENT0
            && attachment < static_cast<GCGLenum>(GraphicsContextGL::COLOR_ATTACHMENT0 + getMaxColorAttachments()))
            return true;
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid attachment");
        return false;
    }
}
    
void WebGLRenderingContext::renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
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
    case GraphicsContextGL::RGBA4:
    case GraphicsContextGL::RGB5_A1:
    case GraphicsContextGL::RGB565:
    case GraphicsContextGL::STENCIL_INDEX8:
    case ExtensionsGL::SRGB8_ALPHA8_EXT:
        if (internalformat == ExtensionsGL::SRGB8_ALPHA8_EXT && !m_extsRGB) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "renderbufferStorage", "invalid internalformat");
            return;
        }
        m_context->renderbufferStorage(target, internalformat, width, height);
        m_renderbufferBinding->setInternalFormat(internalformat);
        m_renderbufferBinding->setIsValid(true);
        m_renderbufferBinding->setSize(width, height);
        break;
    case GraphicsContextGL::DEPTH_STENCIL:
        if (isDepthStencilSupported())
            m_context->renderbufferStorage(target, ExtensionsGL::DEPTH24_STENCIL8, width, height);
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

void WebGLRenderingContext::hint(GCGLenum target, GCGLenum mode)
{
    if (isContextLostOrPending())
        return;
    bool isValid = false;
    switch (target) {
    case GraphicsContextGL::GENERATE_MIPMAP_HINT:
        isValid = true;
        break;
    case ExtensionsGL::FRAGMENT_SHADER_DERIVATIVE_HINT_OES: // OES_standard_derivatives
        if (m_oesStandardDerivatives)
            isValid = true;
        break;
    }
    if (!isValid) {
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "hint", "invalid target");
        return;
    }
    m_context->hint(target, mode);
}
    
void WebGLRenderingContext::clear(GCGLbitfield mask)
{
    if (isContextLostOrPending())
        return;
    if (mask & ~(GraphicsContextGL::COLOR_BUFFER_BIT | GraphicsContextGL::DEPTH_BUFFER_BIT | GraphicsContextGL::STENCIL_BUFFER_BIT)) {
        synthesizeGLError(GraphicsContextGL::INVALID_VALUE, "clear", "invalid mask");
        return;
    }
    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(m_context.get(), &reason)) {
        synthesizeGLError(GraphicsContextGL::INVALID_FRAMEBUFFER_OPERATION, "clear", reason);
        return;
    }
    if (!clearIfComposited(mask))
        m_context->clear(mask);
    markContextChangedAndNotifyCanvasObserver();
}

WebGLAny WebGLRenderingContext::getParameter(GCGLenum pname)
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
        return "WebGL 1.0"_str;
    case GraphicsContextGL::VIEWPORT:
        return getWebGLIntArrayParameter(pname);
    case ExtensionsGL::FRAGMENT_SHADER_DERIVATIVE_HINT_OES: // OES_standard_derivatives
        if (m_oesStandardDerivatives)
            return getUnsignedIntParameter(ExtensionsGL::FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, OES_standard_derivatives not enabled");
        return nullptr;
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
    case ExtensionsGL::VERTEX_ARRAY_BINDING_OES: // OES_vertex_array_object
        if (m_oesVertexArrayObject) {
            if (m_boundVertexArrayObject->isDefaultObject())
                return nullptr;
            return makeRefPtr(static_cast<WebGLVertexArrayObjectOES&>(*m_boundVertexArrayObject));
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, OES_vertex_array_object not enabled");
        return nullptr;
    case ExtensionsGL::MAX_TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
        if (m_extTextureFilterAnisotropic)
            return getUnsignedIntParameter(ExtensionsGL::MAX_TEXTURE_MAX_ANISOTROPY_EXT);
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
        return nullptr;
    case ExtensionsGL::MAX_COLOR_ATTACHMENTS_EXT: // EXT_draw_buffers BEGIN
        if (m_webglDrawBuffers)
            return getMaxColorAttachments();
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_draw_buffers not enabled");
        return nullptr;
    case ExtensionsGL::MAX_DRAW_BUFFERS_EXT:
        if (m_webglDrawBuffers)
            return getMaxDrawBuffers();
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_draw_buffers not enabled");
        return nullptr;
    default:
        if (m_webglDrawBuffers
            && pname >= ExtensionsGL::DRAW_BUFFER0_EXT
            && pname < static_cast<GCGLenum>(ExtensionsGL::DRAW_BUFFER0_EXT + getMaxDrawBuffers())) {
            GCGLint value = GraphicsContextGL::NONE;
            if (m_framebufferBinding)
                value = m_framebufferBinding->getDrawBuffer(pname);
            else // emulated backbuffer
                value = m_backDrawBuffer;
            return value;
        }
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, "getParameter", "invalid parameter name");
        return nullptr;
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

bool WebGLRenderingContext::validateCapability(const char* functionName, GCGLenum cap)
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
        return true;
    default:
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid capability");
        return false;
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
