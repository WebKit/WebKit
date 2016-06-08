/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGL)
#include "WebGLRenderingContext.h"

#include "ANGLEInstancedArrays.h"
#include "CachedImage.h"
#include "EXTBlendMinMax.h"
#include "EXTFragDepth.h"
#include "EXTShaderTextureLOD.h"
#include "EXTTextureFilterAnisotropic.h"
#include "EXTsRGB.h"
#include "Extensions3D.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageData.h"
#include "OESElementIndexUint.h"
#include "OESStandardDerivatives.h"
#include "OESTextureFloat.h"
#include "OESTextureFloatLinear.h"
#include "OESTextureHalfFloat.h"
#include "OESTextureHalfFloatLinear.h"
#include "OESVertexArrayObject.h"
#include "RenderBox.h"
#include "WebGLCompressedTextureATC.h"
#include "WebGLCompressedTexturePVRTC.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLDepthTexture.h"
#include "WebGLDrawBuffers.h"
#include "WebGLLoseContext.h"
#include "WebGLVertexArrayObjectOES.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSCellInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>

namespace WebCore {

WebGLRenderingContext::WebGLRenderingContext(HTMLCanvasElement* passedCanvas, GraphicsContext3D::Attributes attributes)
    : WebGLRenderingContextBase(passedCanvas, attributes)
{
}

WebGLRenderingContext::WebGLRenderingContext(HTMLCanvasElement* passedCanvas, PassRefPtr<GraphicsContext3D> context,
    GraphicsContext3D::Attributes attributes) : WebGLRenderingContextBase(passedCanvas, context, attributes)
{
    initializeVertexArrayObjects();
}

void WebGLRenderingContext::initializeVertexArrayObjects()
{
    m_defaultVertexArrayObject = WebGLVertexArrayObjectOES::create(this, WebGLVertexArrayObjectOES::VAOTypeDefault);
    addContextObject(m_defaultVertexArrayObject.get());
    m_boundVertexArrayObject = m_defaultVertexArrayObject;
    if (!isGLES2Compliant())
        initVertexAttrib0();
}

WebGLExtension* WebGLRenderingContext::getExtension(const String& name)
{
    if (isContextLostOrPending())
        return nullptr;
    
    if (equalIgnoringASCIICase(name, "EXT_blend_minmax")
        && m_context->getExtensions()->supports("GL_EXT_blend_minmax")) {
        if (!m_extBlendMinMax) {
            m_context->getExtensions()->ensureEnabled("GL_EXT_blend_minmax");
            m_extBlendMinMax = std::make_unique<EXTBlendMinMax>(this);
        }
        return m_extBlendMinMax.get();
    }
    if (equalIgnoringASCIICase(name, "EXT_sRGB")
        && m_context->getExtensions()->supports("GL_EXT_sRGB")) {
        if (!m_extsRGB) {
            m_context->getExtensions()->ensureEnabled("GL_EXT_sRGB");
            m_extsRGB = std::make_unique<EXTsRGB>(this);
        }
        return m_extsRGB.get();
    }
    if (equalIgnoringASCIICase(name, "EXT_frag_depth")
        && m_context->getExtensions()->supports("GL_EXT_frag_depth")) {
        if (!m_extFragDepth) {
            m_context->getExtensions()->ensureEnabled("GL_EXT_frag_depth");
            m_extFragDepth = std::make_unique<EXTFragDepth>(this);
        }
        return m_extFragDepth.get();
    }
    if (equalIgnoringASCIICase(name, "EXT_shader_texture_lod")
        && (m_context->getExtensions()->supports("GL_EXT_shader_texture_lod") || m_context->getExtensions()->supports("GL_ARB_shader_texture_lod"))) {
        if (!m_extShaderTextureLOD) {
            m_context->getExtensions()->ensureEnabled("GL_EXT_shader_texture_lod");
            m_extShaderTextureLOD = std::make_unique<EXTShaderTextureLOD>(this);
        }
        return m_extShaderTextureLOD.get();
    }
    if ((equalIgnoringASCIICase(name, "EXT_texture_filter_anisotropic") || equalIgnoringASCIICase(name, "WEBKIT_EXT_texture_filter_anisotropic"))
        && m_context->getExtensions()->supports("GL_EXT_texture_filter_anisotropic")) {
        if (!m_extTextureFilterAnisotropic) {
            m_context->getExtensions()->ensureEnabled("GL_EXT_texture_filter_anisotropic");
            m_extTextureFilterAnisotropic = std::make_unique<EXTTextureFilterAnisotropic>(this);
        }
        return m_extTextureFilterAnisotropic.get();
    }
    if (equalIgnoringASCIICase(name, "OES_standard_derivatives")
        && m_context->getExtensions()->supports("GL_OES_standard_derivatives")) {
        if (!m_oesStandardDerivatives) {
            m_context->getExtensions()->ensureEnabled("GL_OES_standard_derivatives");
            m_oesStandardDerivatives = std::make_unique<OESStandardDerivatives>(this);
        }
        return m_oesStandardDerivatives.get();
    }
    if (equalIgnoringASCIICase(name, "OES_texture_float")
        && m_context->getExtensions()->supports("GL_OES_texture_float")) {
        if (!m_oesTextureFloat) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_float");
            m_oesTextureFloat = std::make_unique<OESTextureFloat>(this);
        }
        return m_oesTextureFloat.get();
    }
    if (equalIgnoringASCIICase(name, "OES_texture_float_linear")
        && m_context->getExtensions()->supports("GL_OES_texture_float_linear")) {
        if (!m_oesTextureFloatLinear) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_float_linear");
            m_oesTextureFloatLinear = std::make_unique<OESTextureFloatLinear>(this);
        }
        return m_oesTextureFloatLinear.get();
    }
    if (equalIgnoringASCIICase(name, "OES_texture_half_float")
        && m_context->getExtensions()->supports("GL_OES_texture_half_float")) {
        if (!m_oesTextureHalfFloat) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_half_float");
            m_oesTextureHalfFloat = std::make_unique<OESTextureHalfFloat>(this);
        }
        return m_oesTextureHalfFloat.get();
    }
    if (equalIgnoringASCIICase(name, "OES_texture_half_float_linear")
        && m_context->getExtensions()->supports("GL_OES_texture_half_float_linear")) {
        if (!m_oesTextureHalfFloatLinear) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_half_float_linear");
            m_oesTextureHalfFloatLinear = std::make_unique<OESTextureHalfFloatLinear>(this);
        }
        return m_oesTextureHalfFloatLinear.get();
    }
    if (equalIgnoringASCIICase(name, "OES_vertex_array_object")
        && m_context->getExtensions()->supports("GL_OES_vertex_array_object")) {
        if (!m_oesVertexArrayObject) {
            m_context->getExtensions()->ensureEnabled("GL_OES_vertex_array_object");
            m_oesVertexArrayObject = std::make_unique<OESVertexArrayObject>(this);
        }
        return m_oesVertexArrayObject.get();
    }
    if (equalIgnoringASCIICase(name, "OES_element_index_uint")
        && m_context->getExtensions()->supports("GL_OES_element_index_uint")) {
        if (!m_oesElementIndexUint) {
            m_context->getExtensions()->ensureEnabled("GL_OES_element_index_uint");
            m_oesElementIndexUint = std::make_unique<OESElementIndexUint>(this);
        }
        return m_oesElementIndexUint.get();
    }
    if (equalIgnoringASCIICase(name, "WEBGL_lose_context")) {
        if (!m_webglLoseContext)
            m_webglLoseContext = std::make_unique<WebGLLoseContext>(this);
        return m_webglLoseContext.get();
    }
    if ((equalIgnoringASCIICase(name, "WEBKIT_WEBGL_compressed_texture_atc"))
        && WebGLCompressedTextureATC::supported(this)) {
        if (!m_webglCompressedTextureATC)
            m_webglCompressedTextureATC = std::make_unique<WebGLCompressedTextureATC>(this);
        return m_webglCompressedTextureATC.get();
    }
    if ((equalIgnoringASCIICase(name, "WEBKIT_WEBGL_compressed_texture_pvrtc"))
        && WebGLCompressedTexturePVRTC::supported(this)) {
        if (!m_webglCompressedTexturePVRTC)
            m_webglCompressedTexturePVRTC = std::make_unique<WebGLCompressedTexturePVRTC>(this);
        return m_webglCompressedTexturePVRTC.get();
    }
    if (equalIgnoringASCIICase(name, "WEBGL_compressed_texture_s3tc")
        && WebGLCompressedTextureS3TC::supported(this)) {
        if (!m_webglCompressedTextureS3TC)
            m_webglCompressedTextureS3TC = std::make_unique<WebGLCompressedTextureS3TC>(this);
        return m_webglCompressedTextureS3TC.get();
    }
    if (equalIgnoringASCIICase(name, "WEBGL_depth_texture")
        && WebGLDepthTexture::supported(graphicsContext3D())) {
        if (!m_webglDepthTexture) {
            m_context->getExtensions()->ensureEnabled("GL_CHROMIUM_depth_texture");
            m_webglDepthTexture = std::make_unique<WebGLDepthTexture>(this);
        }
        return m_webglDepthTexture.get();
    }
    if (equalIgnoringASCIICase(name, "WEBGL_draw_buffers") && supportsDrawBuffers()) {
        if (!m_webglDrawBuffers) {
            m_context->getExtensions()->ensureEnabled("GL_EXT_draw_buffers");
            m_webglDrawBuffers = std::make_unique<WebGLDrawBuffers>(this);
        }
        return m_webglDrawBuffers.get();
    }
    if (equalIgnoringASCIICase(name, "ANGLE_instanced_arrays") && ANGLEInstancedArrays::supported(this)) {
        if (!m_angleInstancedArrays) {
            m_context->getExtensions()->ensureEnabled("GL_ANGLE_instanced_arrays");
            m_angleInstancedArrays = std::make_unique<ANGLEInstancedArrays>(this);
        }
        return m_angleInstancedArrays.get();
    }
    if (equalIgnoringASCIICase(name, "WEBGL_debug_renderer_info")) {
        if (!m_webglDebugRendererInfo)
            m_webglDebugRendererInfo = std::make_unique<WebGLDebugRendererInfo>(this);
        return m_webglDebugRendererInfo.get();
    }
    if (equalIgnoringASCIICase(name, "WEBGL_debug_shaders")
        && m_context->getExtensions()->supports("GL_ANGLE_translated_shader_source")) {
        if (!m_webglDebugShaders)
            m_webglDebugShaders = std::make_unique<WebGLDebugShaders>(this);
        return m_webglDebugShaders.get();
    }

    return nullptr;
}

Vector<String> WebGLRenderingContext::getSupportedExtensions()
{
    Vector<String> result;
    
    if (m_isPendingPolicyResolution)
        return result;
    
    if (m_context->getExtensions()->supports("GL_EXT_blend_minmax"))
        result.append("EXT_blend_minmax");
    if (m_context->getExtensions()->supports("GL_EXT_sRGB"))
        result.append("EXT_sRGB");
    if (m_context->getExtensions()->supports("GL_EXT_frag_depth"))
        result.append("EXT_frag_depth");
    if (m_context->getExtensions()->supports("GL_OES_texture_float"))
        result.append("OES_texture_float");
    if (m_context->getExtensions()->supports("GL_OES_texture_float_linear"))
        result.append("OES_texture_float_linear");
    if (m_context->getExtensions()->supports("GL_OES_texture_half_float"))
        result.append("OES_texture_half_float");
    if (m_context->getExtensions()->supports("GL_OES_texture_half_float_linear"))
        result.append("OES_texture_half_float_linear");
    if (m_context->getExtensions()->supports("GL_OES_standard_derivatives"))
        result.append("OES_standard_derivatives");
    if (m_context->getExtensions()->supports("GL_EXT_shader_texture_lod") || m_context->getExtensions()->supports("GL_ARB_shader_texture_lod"))
        result.append("EXT_shader_texture_lod");
    if (m_context->getExtensions()->supports("GL_EXT_texture_filter_anisotropic"))
        result.append("EXT_texture_filter_anisotropic");
    if (m_context->getExtensions()->supports("GL_OES_vertex_array_object"))
        result.append("OES_vertex_array_object");
    if (m_context->getExtensions()->supports("GL_OES_element_index_uint"))
        result.append("OES_element_index_uint");
    result.append("WEBGL_lose_context");
    if (WebGLCompressedTextureATC::supported(this))
        result.append("WEBKIT_WEBGL_compressed_texture_atc");
    if (WebGLCompressedTexturePVRTC::supported(this))
        result.append("WEBKIT_WEBGL_compressed_texture_pvrtc");
    if (WebGLCompressedTextureS3TC::supported(this))
        result.append("WEBGL_compressed_texture_s3tc");
    if (WebGLDepthTexture::supported(graphicsContext3D()))
        result.append("WEBGL_depth_texture");
    if (supportsDrawBuffers())
        result.append("WEBGL_draw_buffers");
    if (ANGLEInstancedArrays::supported(this))
        result.append("ANGLE_instanced_arrays");
    if (m_context->getExtensions()->supports("GL_ANGLE_translated_shader_source"))
        result.append("WEBGL_debug_shaders");
    result.append("WEBGL_debug_renderer_info");

    return result;
}

WebGLGetInfo WebGLRenderingContext::getFramebufferAttachmentParameter(GC3Denum target, GC3Denum attachment, GC3Denum pname, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (isContextLostOrPending() || !validateFramebufferFuncParameters("getFramebufferAttachmentParameter", target, attachment))
        return WebGLGetInfo();
    
    if (!m_framebufferBinding || !m_framebufferBinding->object()) {
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, "getFramebufferAttachmentParameter", "no framebuffer bound");
        return WebGLGetInfo();
    }
    
    WebGLSharedObject* object = m_framebufferBinding->getAttachmentObject(attachment);
    if (!object) {
        if (pname == GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
            return WebGLGetInfo(GraphicsContext3D::NONE);
        // OpenGL ES 2.0 specifies INVALID_ENUM in this case, while desktop GL
        // specifies INVALID_OPERATION.
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getFramebufferAttachmentParameter", "invalid parameter name");
        return WebGLGetInfo();
    }
    
    ASSERT(object->isTexture() || object->isRenderbuffer());
    if (object->isTexture()) {
        switch (pname) {
        case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return WebGLGetInfo(GraphicsContext3D::TEXTURE);
        case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            return WebGLGetInfo(PassRefPtr<WebGLTexture>(reinterpret_cast<WebGLTexture*>(object)));
        case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
        case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
        case Extensions3D::FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT: {
            GC3Dint value = 0;
            m_context->getFramebufferAttachmentParameteriv(target, attachment, pname, &value);
            return WebGLGetInfo(value);
        }
        default:
            synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getFramebufferAttachmentParameter", "invalid parameter name for texture attachment");
            return WebGLGetInfo();
        }
    } else {
        switch (pname) {
        case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
            return WebGLGetInfo(GraphicsContext3D::RENDERBUFFER);
        case GraphicsContext3D::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
            return WebGLGetInfo(PassRefPtr<WebGLRenderbuffer>(reinterpret_cast<WebGLRenderbuffer*>(object)));
        case Extensions3D::FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT: {
            if (!m_extsRGB) {
                synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getFramebufferAttachmentParameter", "invalid parameter name for renderbuffer attachment");
                return WebGLGetInfo();
            }
            WebGLRenderbuffer* renderBuffer = reinterpret_cast<WebGLRenderbuffer*>(object);
            GC3Denum renderBufferFormat = renderBuffer->getInternalFormat();
            ASSERT(renderBufferFormat != Extensions3D::SRGB_EXT && renderBufferFormat != Extensions3D::SRGB_ALPHA_EXT);
            if (renderBufferFormat == Extensions3D::SRGB8_ALPHA8_EXT)
                return WebGLGetInfo(Extensions3D::SRGB_EXT);
            return WebGLGetInfo(GraphicsContext3D::LINEAR);
        }
        default:
            synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getFramebufferAttachmentParameter", "invalid parameter name for renderbuffer attachment");
            return WebGLGetInfo();
        }
    }
}

bool WebGLRenderingContext::validateFramebufferFuncParameters(const char* functionName, GC3Denum target, GC3Denum attachment)
{
    if (target != GraphicsContext3D::FRAMEBUFFER) {
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid target");
        return false;
    }
    switch (attachment) {
    case GraphicsContext3D::COLOR_ATTACHMENT0:
    case GraphicsContext3D::DEPTH_ATTACHMENT:
    case GraphicsContext3D::STENCIL_ATTACHMENT:
    case GraphicsContext3D::DEPTH_STENCIL_ATTACHMENT:
        break;
    default:
        if (m_webglDrawBuffers
            && attachment > GraphicsContext3D::COLOR_ATTACHMENT0
            && attachment < static_cast<GC3Denum>(GraphicsContext3D::COLOR_ATTACHMENT0 + getMaxColorAttachments()))
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid attachment");
        return false;
    }
    return true;
}
    
void WebGLRenderingContext::renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    if (isContextLostOrPending())
        return;
    if (target != GraphicsContext3D::RENDERBUFFER) {
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "renderbufferStorage", "invalid target");
        return;
    }
    if (!m_renderbufferBinding || !m_renderbufferBinding->object()) {
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, "renderbufferStorage", "no bound renderbuffer");
        return;
    }
    if (!validateSize("renderbufferStorage", width, height))
        return;
    switch (internalformat) {
    case GraphicsContext3D::DEPTH_COMPONENT16:
    case GraphicsContext3D::RGBA4:
    case GraphicsContext3D::RGB5_A1:
    case GraphicsContext3D::RGB565:
    case GraphicsContext3D::STENCIL_INDEX8:
    case Extensions3D::SRGB8_ALPHA8_EXT:
        if (internalformat == Extensions3D::SRGB8_ALPHA8_EXT && !m_extsRGB) {
            synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "renderbufferStorage", "invalid internalformat");
            return;
        }
        m_context->renderbufferStorage(target, internalformat, width, height);
        m_renderbufferBinding->setInternalFormat(internalformat);
        m_renderbufferBinding->setIsValid(true);
        m_renderbufferBinding->setSize(width, height);
        break;
    case GraphicsContext3D::DEPTH_STENCIL:
        if (isDepthStencilSupported())
            m_context->renderbufferStorage(target, Extensions3D::DEPTH24_STENCIL8, width, height);
        m_renderbufferBinding->setSize(width, height);
        m_renderbufferBinding->setIsValid(isDepthStencilSupported());
        m_renderbufferBinding->setInternalFormat(internalformat);
        break;
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "renderbufferStorage", "invalid internalformat");
        return;
    }
    applyStencilTest();
}

void WebGLRenderingContext::hint(GC3Denum target, GC3Denum mode)
{
    if (isContextLostOrPending())
        return;
    bool isValid = false;
    switch (target) {
    case GraphicsContext3D::GENERATE_MIPMAP_HINT:
        isValid = true;
        break;
    case Extensions3D::FRAGMENT_SHADER_DERIVATIVE_HINT_OES: // OES_standard_derivatives
        if (m_oesStandardDerivatives)
            isValid = true;
        break;
    }
    if (!isValid) {
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "hint", "invalid target");
        return;
    }
    m_context->hint(target, mode);
}
    
void WebGLRenderingContext::clear(GC3Dbitfield mask)
{
    if (isContextLostOrPending())
        return;
    if (mask & ~(GraphicsContext3D::COLOR_BUFFER_BIT | GraphicsContext3D::DEPTH_BUFFER_BIT | GraphicsContext3D::STENCIL_BUFFER_BIT)) {
        synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "clear", "invalid mask");
        return;
    }
    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(graphicsContext3D(), !isResourceSafe(), &reason)) {
        synthesizeGLError(GraphicsContext3D::INVALID_FRAMEBUFFER_OPERATION, "clear", reason);
        return;
    }
    if (!clearIfComposited(mask))
        m_context->clear(mask);
    markContextChanged();
}

void WebGLRenderingContext::copyTexImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Dint border)
{
    if (isContextLostOrPending())
        return;
    if (!validateTexFuncParameters("copyTexImage2D", CopyTexImage, target, level, internalformat, width, height, border, internalformat, GraphicsContext3D::UNSIGNED_BYTE))
        return;
    if (!validateSettableTexFormat("copyTexImage2D", internalformat))
        return;
    WebGLTexture* tex = validateTextureBinding("copyTexImage2D", target, true);
    if (!tex)
        return;
    if (!isTexInternalFormatColorBufferCombinationValid(internalformat, getBoundFramebufferColorFormat())) {
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, "copyTexImage2D", "framebuffer is incompatible format");
        return;
    }
    if (!isGLES2NPOTStrict() && level && WebGLTexture::isNPOT(width, height)) {
        synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "copyTexImage2D", "level > 0 not power of 2");
        return;
    }
    const char* reason = "framebuffer incomplete";
    if (m_framebufferBinding && !m_framebufferBinding->onAccess(graphicsContext3D(), !isResourceSafe(), &reason)) {
        synthesizeGLError(GraphicsContext3D::INVALID_FRAMEBUFFER_OPERATION, "copyTexImage2D", reason);
        return;
    }
    clearIfComposited();
    if (isResourceSafe())
        m_context->copyTexImage2D(target, level, internalformat, x, y, width, height, border);
    else {
        GC3Dint clippedX, clippedY;
        GC3Dsizei clippedWidth, clippedHeight;
        if (clip2D(x, y, width, height, getBoundFramebufferWidth(), getBoundFramebufferHeight(), &clippedX, &clippedY, &clippedWidth, &clippedHeight)) {
            m_context->texImage2DResourceSafe(target, level, internalformat, width, height, border,
                internalformat, GraphicsContext3D::UNSIGNED_BYTE, m_unpackAlignment);
            if (clippedWidth > 0 && clippedHeight > 0) {
                m_context->copyTexSubImage2D(target, level, clippedX - x, clippedY - y,
                    clippedX, clippedY, clippedWidth, clippedHeight);
            }
        } else
            m_context->copyTexImage2D(target, level, internalformat, x, y, width, height, border);
    }
    // FIXME: if the framebuffer is not complete, none of the below should be executed.
    tex->setLevelInfo(target, level, internalformat, width, height, GraphicsContext3D::UNSIGNED_BYTE);
}

void WebGLRenderingContext::texSubImage2DBase(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum internalformat, GC3Denum format, GC3Denum type, const void* pixels, ExceptionCode& ec)
{
    UNUSED_PARAM(internalformat);
    // FIXME: For now we ignore any errors returned
    ec = 0;
    ASSERT(!isContextLost());
    ASSERT(validateTexFuncParameters("texSubImage2D", TexSubImage, target, level, format, width, height, 0, format, type));
    ASSERT(validateSize("texSubImage2D", xoffset, yoffset));
    ASSERT(validateSettableTexFormat("texSubImage2D", format));
    WebGLTexture* tex = validateTextureBinding("texSubImage2D", target, true);
    if (!tex) {
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT((xoffset + width) >= 0);
    ASSERT((yoffset + height) >= 0);
    ASSERT(tex->getWidth(target, level) >= (xoffset + width));
    ASSERT(tex->getHeight(target, level) >= (yoffset + height));
    ASSERT(tex->getInternalFormat(target, level) == format);
    ASSERT(tex->getType(target, level) == type);
    m_context->texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void WebGLRenderingContext::texSubImage2DImpl(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, Image* image, GraphicsContext3D::ImageHtmlDomSource domSource, bool flipY, bool premultiplyAlpha, ExceptionCode& ec)
{
    ec = 0;
    Vector<uint8_t> data;
    GraphicsContext3D::ImageExtractor imageExtractor(image, domSource, premultiplyAlpha, m_unpackColorspaceConversion == GraphicsContext3D::NONE);
    if (!imageExtractor.extractSucceeded()) {
        synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "texSubImage2D", "bad image");
        return;
    }
    GraphicsContext3D::DataFormat sourceDataFormat = imageExtractor.imageSourceFormat();
    GraphicsContext3D::AlphaOp alphaOp = imageExtractor.imageAlphaOp();
    const void* imagePixelData = imageExtractor.imagePixelData();
    
    bool needConversion = true;
    if (type == GraphicsContext3D::UNSIGNED_BYTE && sourceDataFormat == GraphicsContext3D::DataFormatRGBA8 && format == GraphicsContext3D::RGBA && alphaOp == GraphicsContext3D::AlphaDoNothing && !flipY)
        needConversion = false;
    else {
        if (!m_context->packImageData(image, imagePixelData, format, type, flipY, alphaOp, sourceDataFormat, imageExtractor.imageWidth(), imageExtractor.imageHeight(), imageExtractor.imageSourceUnpackAlignment(), data)) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "texImage2D", "bad image data");
            return;
        }
    }
    
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, 1);

    texSubImage2DBase(target, level, xoffset, yoffset, image->width(), image->height(), format, format, type, needConversion ? data.data() : imagePixelData, ec);
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGLRenderingContext::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, RefPtr<ArrayBufferView>&& pixels, ExceptionCode& ec)
{
    if (isContextLostOrPending() || !validateTexFuncData("texSubImage2D", level, width, height, format, format, type, pixels.get(), NullNotAllowed) || !validateTexFunc("texSubImage2D", TexSubImage, SourceArrayBufferView, target, level, format, width, height, 0, format, type, xoffset, yoffset))
        return;
    
    void* data = pixels->baseAddress();
    Vector<uint8_t> tempData;
    bool changeUnpackAlignment = false;
    if (data && (m_unpackFlipY || m_unpackPremultiplyAlpha)) {
        if (!m_context->extractTextureData(width, height, format, type,
            m_unpackAlignment,
            m_unpackFlipY, m_unpackPremultiplyAlpha,
            data,
            tempData))
            return;
        data = tempData.data();
        changeUnpackAlignment = true;
    }
    if (changeUnpackAlignment)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, 1);

    texSubImage2DBase(target, level, xoffset, yoffset, width, height, format, format, type, data, ec);
    if (changeUnpackAlignment)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGLRenderingContext::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, ImageData* pixels, ExceptionCode& ec)
{
    ec = 0;
    if (isContextLostOrPending() || !pixels || !validateTexFunc("texSubImage2D", TexSubImage, SourceImageData, target, level, format,  pixels->width(), pixels->height(), 0, format, type, xoffset, yoffset))
        return;
    
    Vector<uint8_t> data;
    bool needConversion = true;
    // The data from ImageData is always of format RGBA8.
    // No conversion is needed if destination format is RGBA and type is USIGNED_BYTE and no Flip or Premultiply operation is required.
    if (format == GraphicsContext3D::RGBA && type == GraphicsContext3D::UNSIGNED_BYTE && !m_unpackFlipY && !m_unpackPremultiplyAlpha)
        needConversion = false;
    else {
        if (!m_context->extractImageData(pixels, format, type, m_unpackFlipY, m_unpackPremultiplyAlpha, data)) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, "texSubImage2D", "bad image data");
            return;
        }
    }
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, 1);
    
    texSubImage2DBase(target, level, xoffset, yoffset, pixels->width(), pixels->height(), format, format, type, needConversion ? data.data() : pixels->data()->data(), ec);
    if (m_unpackAlignment != 1)
        m_context->pixelStorei(GraphicsContext3D::UNPACK_ALIGNMENT, m_unpackAlignment);
}

void WebGLRenderingContext::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, HTMLImageElement* image, ExceptionCode& ec)
{
    ec = 0;
    if (isContextLostOrPending() || !validateHTMLImageElement("texSubImage2D", image, ec))
        return;
    
    RefPtr<Image> imageForRender = image->cachedImage()->imageForRenderer(image->renderer());
    if (!imageForRender)
        return;

    if (imageForRender->isSVGImage())
        imageForRender = drawImageIntoBuffer(*imageForRender, image->width(), image->height(), 1);
    
    if (!imageForRender || !validateTexFunc("texSubImage2D", TexSubImage, SourceHTMLImageElement, target, level, format, imageForRender->width(), imageForRender->height(), 0, format, type, xoffset, yoffset))
        return;
    
    texSubImage2DImpl(target, level, xoffset, yoffset, format, type, imageForRender.get(), GraphicsContext3D::HtmlDomImage, m_unpackFlipY, m_unpackPremultiplyAlpha, ec);
}

void WebGLRenderingContext::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, HTMLCanvasElement* canvas, ExceptionCode& ec)
{
    ec = 0;
    if (isContextLostOrPending() || !validateHTMLCanvasElement("texSubImage2D", canvas, ec)
        || !validateTexFunc("texSubImage2D", TexSubImage, SourceHTMLCanvasElement, target, level, format, canvas->width(), canvas->height(), 0, format, type, xoffset, yoffset))
        return;
    
    RefPtr<ImageData> imageData = canvas->getImageData();
    if (imageData)
        texSubImage2D(target, level, xoffset, yoffset, format, type, imageData.get(), ec);
    else
        texSubImage2DImpl(target, level, xoffset, yoffset, format, type, canvas->copiedImage(), GraphicsContext3D::HtmlDomCanvas, m_unpackFlipY, m_unpackPremultiplyAlpha, ec);
}

#if ENABLE(VIDEO)
void WebGLRenderingContext::texSubImage2D(GC3Denum target, GC3Dint level, GC3Dint xoffset, GC3Dint yoffset, GC3Denum format, GC3Denum type, HTMLVideoElement* video, ExceptionCode& ec)
{
    ec = 0;
    if (isContextLostOrPending() || !validateHTMLVideoElement("texSubImage2D", video, ec)
        || !validateTexFunc("texSubImage2D", TexSubImage, SourceHTMLVideoElement, target, level, format, video->videoWidth(), video->videoHeight(), 0, format, type, xoffset, yoffset))
        return;
    
    RefPtr<Image> image = videoFrameToImage(video, ImageBuffer::fastCopyImageMode(), ec);
    if (!image)
        return;
    texSubImage2DImpl(target, level, xoffset, yoffset, format, type, image.get(), GraphicsContext3D::HtmlDomVideo, m_unpackFlipY, m_unpackPremultiplyAlpha, ec);
}
#endif

bool WebGLRenderingContext::validateTexFuncParameters(const char* functionName,
    TexFuncValidationFunctionType functionType,
    GC3Denum target, GC3Dint level,
    GC3Denum internalformat,
    GC3Dsizei width, GC3Dsizei height, GC3Dint border,
    GC3Denum format, GC3Denum type)
{
    // We absolutely have to validate the format and type combination.
    // The texImage2D entry points taking HTMLImage, etc. will produce
    // temporary data based on this combination, so it must be legal.
    if (!validateTexFuncFormatAndType(functionName, internalformat, format, type, level) || !validateTexFuncLevel(functionName, target, level))
        return false;
    
    if (width < 0 || height < 0) {
        synthesizeGLError(GraphicsContext3D::INVALID_VALUE, functionName, "width or height < 0");
        return false;
    }
    
    GC3Dint maxTextureSizeForLevel = pow(2.0, m_maxTextureLevel - 1 - level);
    switch (target) {
    case GraphicsContext3D::TEXTURE_2D:
        if (width > maxTextureSizeForLevel || height > maxTextureSizeForLevel) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, functionName, "width or height out of range");
            return false;
        }
        break;
    case GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_X:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GraphicsContext3D::TEXTURE_CUBE_MAP_NEGATIVE_Z:
        if (functionType != TexSubImage && width != height) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, functionName, "width != height for cube map");
            return false;
        }
        // No need to check height here. For texImage width == height.
        // For texSubImage that will be checked when checking yoffset + height is in range.
        if (width > maxTextureSizeForLevel) {
            synthesizeGLError(GraphicsContext3D::INVALID_VALUE, functionName, "width or height out of range for cube map");
            return false;
        }
        break;
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid target");
        return false;
    }

    if (format != internalformat) {
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "format != internalformat");
        return false;
    }
    
    if (border) {
        synthesizeGLError(GraphicsContext3D::INVALID_VALUE, functionName, "border != 0");
        return false;
    }
    
    return true;
}

bool WebGLRenderingContext::validateTexFuncFormatAndType(const char* functionName, GC3Denum internalformat, GC3Denum format, GC3Denum type, GC3Dint level)
{
    UNUSED_PARAM(internalformat);
    switch (format) {
    case GraphicsContext3D::ALPHA:
    case GraphicsContext3D::LUMINANCE:
    case GraphicsContext3D::LUMINANCE_ALPHA:
    case GraphicsContext3D::RGB:
    case GraphicsContext3D::RGBA:
        break;
    case GraphicsContext3D::DEPTH_STENCIL:
    case GraphicsContext3D::DEPTH_COMPONENT:
        if (m_webglDepthTexture)
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "depth texture formats not enabled");
        return false;
    case Extensions3D::SRGB_EXT:
    case Extensions3D::SRGB_ALPHA_EXT:
    default:
        if ((format == Extensions3D::SRGB_EXT || format == Extensions3D::SRGB_ALPHA_EXT)
            && m_extsRGB)
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid texture format");
        return false;
    }
    
    switch (type) {
    case GraphicsContext3D::UNSIGNED_BYTE:
    case GraphicsContext3D::UNSIGNED_SHORT_5_6_5:
    case GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1:
        break;
    case GraphicsContext3D::FLOAT:
        if (m_oesTextureFloat)
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid texture type");
        return false;
    case GraphicsContext3D::HALF_FLOAT_OES:
        if (m_oesTextureHalfFloat)
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid texture type");
        return false;
    case GraphicsContext3D::UNSIGNED_INT:
    case GraphicsContext3D::UNSIGNED_INT_24_8:
    case GraphicsContext3D::UNSIGNED_SHORT:
        if (m_webglDepthTexture)
            break;
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid texture type");
        return false;
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid texture type");
        return false;
    }
    
    // Verify that the combination of format and type is supported.
    switch (format) {
    case GraphicsContext3D::ALPHA:
    case GraphicsContext3D::LUMINANCE:
    case GraphicsContext3D::LUMINANCE_ALPHA:
        if (type != GraphicsContext3D::UNSIGNED_BYTE
            && type != GraphicsContext3D::FLOAT
            && type != GraphicsContext3D::HALF_FLOAT_OES) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid type for format");
            return false;
        }
        break;
    case GraphicsContext3D::RGB:
    case Extensions3D::SRGB_EXT:
        if (type != GraphicsContext3D::UNSIGNED_BYTE
            && type != GraphicsContext3D::UNSIGNED_SHORT_5_6_5
            && type != GraphicsContext3D::FLOAT
            && type != GraphicsContext3D::HALF_FLOAT_OES) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid type for RGB format");
            return false;
        }
        break;
    case GraphicsContext3D::RGBA:
    case Extensions3D::SRGB_ALPHA_EXT:
        if (type != GraphicsContext3D::UNSIGNED_BYTE
            && type != GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4
            && type != GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1
            && type != GraphicsContext3D::FLOAT
            && type != GraphicsContext3D::HALF_FLOAT_OES) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid type for RGBA format");
            return false;
        }
        break;
    case GraphicsContext3D::DEPTH_COMPONENT:
        if (!m_webglDepthTexture) {
            synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid format. DEPTH_COMPONENT not enabled");
            return false;
        }
        if (type != GraphicsContext3D::UNSIGNED_SHORT
            && type != GraphicsContext3D::UNSIGNED_INT) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid type for DEPTH_COMPONENT format");
            return false;
        }
        if (level > 0) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "level must be 0 for DEPTH_COMPONENT format");
            return false;
        }
        break;
    case GraphicsContext3D::DEPTH_STENCIL:
        if (!m_webglDepthTexture) {
            synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid format. DEPTH_STENCIL not enabled");
            return false;
        }
        if (type != GraphicsContext3D::UNSIGNED_INT_24_8) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "invalid type for DEPTH_STENCIL format");
            return false;
        }
        if (level > 0) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "level must be 0 for DEPTH_STENCIL format");
            return false;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    
    return true;
}

bool WebGLRenderingContext::validateTexFuncData(const char* functionName, GC3Dint level, GC3Dsizei width, GC3Dsizei height, GC3Denum internalformat, GC3Denum format, GC3Denum type, ArrayBufferView* pixels, NullDisposition disposition)
{
    if (!pixels) {
        if (disposition == NullAllowed)
            return true;
        synthesizeGLError(GraphicsContext3D::INVALID_VALUE, functionName, "no pixels");
        return false;
    }

    if (!validateTexFuncFormatAndType(functionName, internalformat, format, type, level))
        return false;
    if (!validateSettableTexFormat(functionName, format))
        return false;
    
    switch (type) {
    case GraphicsContext3D::UNSIGNED_BYTE:
        if (pixels->getType() != JSC::TypeUint8) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "type UNSIGNED_BYTE but ArrayBufferView not Uint8Array");
            return false;
        }
        break;
    case GraphicsContext3D::UNSIGNED_SHORT_5_6_5:
    case GraphicsContext3D::UNSIGNED_SHORT_4_4_4_4:
    case GraphicsContext3D::UNSIGNED_SHORT_5_5_5_1:
        if (pixels->getType() != JSC::TypeUint16) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "type UNSIGNED_SHORT but ArrayBufferView not Uint16Array");
            return false;
        }
        break;
    case GraphicsContext3D::FLOAT: // OES_texture_float
        if (pixels->getType() != JSC::TypeFloat32) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "type FLOAT but ArrayBufferView not Float32Array");
            return false;
        }
        break;
    case GraphicsContext3D::HALF_FLOAT_OES: // OES_texture_half_float
        // As per the specification, ArrayBufferView should be null when
        // OES_texture_half_float is enabled.
        if (pixels) {
            synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "type HALF_FLOAT_OES but ArrayBufferView is not NULL");
            return false;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }
    
    unsigned totalBytesRequired;
    GC3Denum error = m_context->computeImageSizeInBytes(format, type, width, height, m_unpackAlignment, &totalBytesRequired, 0);
    if (error != GraphicsContext3D::NO_ERROR) {
        synthesizeGLError(error, functionName, "invalid texture dimensions");
        return false;
    }
    if (pixels->byteLength() < totalBytesRequired) {
        if (m_unpackAlignment != 1) {
            m_context->computeImageSizeInBytes(format, type, width, height, 1, &totalBytesRequired, 0);
            if (pixels->byteLength() == totalBytesRequired) {
                synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "ArrayBufferView not big enough for request with UNPACK_ALIGNMENT > 1");
                return false;
            }
        }
        synthesizeGLError(GraphicsContext3D::INVALID_OPERATION, functionName, "ArrayBufferView not big enough for request");
        return false;
    }
    return true;
}

WebGLGetInfo WebGLRenderingContext::getParameter(GC3Denum pname, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    if (isContextLostOrPending())
        return WebGLGetInfo();
    const int intZero = 0;
    switch (pname) {
    case GraphicsContext3D::ACTIVE_TEXTURE:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::ALIASED_LINE_WIDTH_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContext3D::ALIASED_POINT_SIZE_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContext3D::ALPHA_BITS:
        return getIntParameter(pname);
    case GraphicsContext3D::ARRAY_BUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLBuffer>(m_boundArrayBuffer));
    case GraphicsContext3D::BLEND:
        return getBooleanParameter(pname);
    case GraphicsContext3D::BLEND_COLOR:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContext3D::BLEND_DST_ALPHA:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::BLEND_DST_RGB:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::BLEND_EQUATION_ALPHA:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::BLEND_EQUATION_RGB:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::BLEND_SRC_ALPHA:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::BLEND_SRC_RGB:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::BLUE_BITS:
        return getIntParameter(pname);
    case GraphicsContext3D::COLOR_CLEAR_VALUE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContext3D::COLOR_WRITEMASK:
        return getBooleanArrayParameter(pname);
    case GraphicsContext3D::COMPRESSED_TEXTURE_FORMATS:
        return WebGLGetInfo(PassRefPtr<Uint32Array>(Uint32Array::create(m_compressedTextureFormats.data(), m_compressedTextureFormats.size())));
    case GraphicsContext3D::CULL_FACE:
        return getBooleanParameter(pname);
    case GraphicsContext3D::CULL_FACE_MODE:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::CURRENT_PROGRAM:
        return WebGLGetInfo(PassRefPtr<WebGLProgram>(m_currentProgram));
    case GraphicsContext3D::DEPTH_BITS:
        if (!m_framebufferBinding && !m_attributes.depth)
            return WebGLGetInfo(intZero);
        return getIntParameter(pname);
    case GraphicsContext3D::DEPTH_CLEAR_VALUE:
        return getFloatParameter(pname);
    case GraphicsContext3D::DEPTH_FUNC:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::DEPTH_RANGE:
        return getWebGLFloatArrayParameter(pname);
    case GraphicsContext3D::DEPTH_TEST:
        return getBooleanParameter(pname);
    case GraphicsContext3D::DEPTH_WRITEMASK:
        return getBooleanParameter(pname);
    case GraphicsContext3D::DITHER:
        return getBooleanParameter(pname);
    case GraphicsContext3D::ELEMENT_ARRAY_BUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLBuffer>(m_boundVertexArrayObject->getElementArrayBuffer()));
    case GraphicsContext3D::FRAMEBUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLFramebuffer>(m_framebufferBinding));
    case GraphicsContext3D::FRONT_FACE:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::GENERATE_MIPMAP_HINT:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::GREEN_BITS:
        return getIntParameter(pname);
    case GraphicsContext3D::IMPLEMENTATION_COLOR_READ_FORMAT:
        return getIntParameter(pname);
    case GraphicsContext3D::IMPLEMENTATION_COLOR_READ_TYPE:
        return getIntParameter(pname);
    case GraphicsContext3D::LINE_WIDTH:
        return getFloatParameter(pname);
    case GraphicsContext3D::MAX_COMBINED_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_CUBE_MAP_TEXTURE_SIZE:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_FRAGMENT_UNIFORM_VECTORS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_RENDERBUFFER_SIZE:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_TEXTURE_SIZE:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_VARYING_VECTORS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_VERTEX_ATTRIBS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_VERTEX_TEXTURE_IMAGE_UNITS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_VERTEX_UNIFORM_VECTORS:
        return getIntParameter(pname);
    case GraphicsContext3D::MAX_VIEWPORT_DIMS:
        return getWebGLIntArrayParameter(pname);
    case GraphicsContext3D::NUM_SHADER_BINARY_FORMATS:
        return getIntParameter(pname);
    case GraphicsContext3D::PACK_ALIGNMENT:
        return getIntParameter(pname);
    case GraphicsContext3D::POLYGON_OFFSET_FACTOR:
        return getFloatParameter(pname);
    case GraphicsContext3D::POLYGON_OFFSET_FILL:
        return getBooleanParameter(pname);
    case GraphicsContext3D::POLYGON_OFFSET_UNITS:
        return getFloatParameter(pname);
    case GraphicsContext3D::RED_BITS:
        return getIntParameter(pname);
    case GraphicsContext3D::RENDERBUFFER_BINDING:
        return WebGLGetInfo(PassRefPtr<WebGLRenderbuffer>(m_renderbufferBinding));
    case GraphicsContext3D::RENDERER:
        return WebGLGetInfo(String("WebKit WebGL"));
    case GraphicsContext3D::SAMPLE_BUFFERS:
        return getIntParameter(pname);
    case GraphicsContext3D::SAMPLE_COVERAGE_INVERT:
        return getBooleanParameter(pname);
    case GraphicsContext3D::SAMPLE_COVERAGE_VALUE:
        return getFloatParameter(pname);
    case GraphicsContext3D::SAMPLES:
        return getIntParameter(pname);
    case GraphicsContext3D::SCISSOR_BOX:
        return getWebGLIntArrayParameter(pname);
    case GraphicsContext3D::SCISSOR_TEST:
        return getBooleanParameter(pname);
    case GraphicsContext3D::SHADING_LANGUAGE_VERSION:
        return WebGLGetInfo("WebGL GLSL ES 1.0 (" + m_context->getString(GraphicsContext3D::SHADING_LANGUAGE_VERSION) + ")");
    case GraphicsContext3D::STENCIL_BACK_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_FUNC:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_PASS_DEPTH_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_PASS_DEPTH_PASS:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_REF:
        return getIntParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_VALUE_MASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_BACK_WRITEMASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_BITS:
        if (!m_framebufferBinding && !m_attributes.stencil)
            return WebGLGetInfo(intZero);
        return getIntParameter(pname);
    case GraphicsContext3D::STENCIL_CLEAR_VALUE:
        return getIntParameter(pname);
    case GraphicsContext3D::STENCIL_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_FUNC:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_PASS_DEPTH_FAIL:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_PASS_DEPTH_PASS:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_REF:
        return getIntParameter(pname);
    case GraphicsContext3D::STENCIL_TEST:
        return getBooleanParameter(pname);
    case GraphicsContext3D::STENCIL_VALUE_MASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::STENCIL_WRITEMASK:
        return getUnsignedIntParameter(pname);
    case GraphicsContext3D::SUBPIXEL_BITS:
        return getIntParameter(pname);
    case GraphicsContext3D::TEXTURE_BINDING_2D:
        return WebGLGetInfo(PassRefPtr<WebGLTexture>(m_textureUnits[m_activeTextureUnit].texture2DBinding));
    case GraphicsContext3D::TEXTURE_BINDING_CUBE_MAP:
        return WebGLGetInfo(PassRefPtr<WebGLTexture>(m_textureUnits[m_activeTextureUnit].textureCubeMapBinding));
    case GraphicsContext3D::UNPACK_ALIGNMENT:
        return getIntParameter(pname);
    case GraphicsContext3D::UNPACK_FLIP_Y_WEBGL:
        return WebGLGetInfo(m_unpackFlipY);
    case GraphicsContext3D::UNPACK_PREMULTIPLY_ALPHA_WEBGL:
        return WebGLGetInfo(m_unpackPremultiplyAlpha);
    case GraphicsContext3D::UNPACK_COLORSPACE_CONVERSION_WEBGL:
        return WebGLGetInfo(m_unpackColorspaceConversion);
    case GraphicsContext3D::VENDOR:
        return WebGLGetInfo(String("WebKit"));
    case GraphicsContext3D::VERSION:
        return WebGLGetInfo("WebGL 1.0 (" + m_context->getString(GraphicsContext3D::VERSION) + ")");
    case GraphicsContext3D::VIEWPORT:
        return getWebGLIntArrayParameter(pname);
    case Extensions3D::FRAGMENT_SHADER_DERIVATIVE_HINT_OES: // OES_standard_derivatives
        if (m_oesStandardDerivatives)
            return getUnsignedIntParameter(Extensions3D::FRAGMENT_SHADER_DERIVATIVE_HINT_OES);
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getParameter", "invalid parameter name, OES_standard_derivatives not enabled");
        return WebGLGetInfo();
    case WebGLDebugRendererInfo::UNMASKED_RENDERER_WEBGL:
        if (m_webglDebugRendererInfo)
            return WebGLGetInfo(m_context->getString(GraphicsContext3D::RENDERER));
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_debug_renderer_info not enabled");
        return WebGLGetInfo();
    case WebGLDebugRendererInfo::UNMASKED_VENDOR_WEBGL:
        if (m_webglDebugRendererInfo)
            return WebGLGetInfo(m_context->getString(GraphicsContext3D::VENDOR));
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_debug_renderer_info not enabled");
        return WebGLGetInfo();
    case Extensions3D::VERTEX_ARRAY_BINDING_OES: // OES_vertex_array_object
        if (m_oesVertexArrayObject) {
            if (!m_boundVertexArrayObject->isDefaultObject())
                return WebGLGetInfo(PassRefPtr<WebGLVertexArrayObjectOES>(static_cast<WebGLVertexArrayObjectOES*>(m_boundVertexArrayObject.get())));
            return WebGLGetInfo();
        }
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getParameter", "invalid parameter name, OES_vertex_array_object not enabled");
        return WebGLGetInfo();
    case Extensions3D::MAX_TEXTURE_MAX_ANISOTROPY_EXT: // EXT_texture_filter_anisotropic
        if (m_extTextureFilterAnisotropic)
            return getUnsignedIntParameter(Extensions3D::MAX_TEXTURE_MAX_ANISOTROPY_EXT);
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getParameter", "invalid parameter name, EXT_texture_filter_anisotropic not enabled");
        return WebGLGetInfo();
    case Extensions3D::MAX_COLOR_ATTACHMENTS_EXT: // EXT_draw_buffers BEGIN
        if (m_webglDrawBuffers)
            return WebGLGetInfo(getMaxColorAttachments());
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_draw_buffers not enabled");
        return WebGLGetInfo();
    case Extensions3D::MAX_DRAW_BUFFERS_EXT:
        if (m_webglDrawBuffers)
            return WebGLGetInfo(getMaxDrawBuffers());
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getParameter", "invalid parameter name, WEBGL_draw_buffers not enabled");
        return WebGLGetInfo();
    default:
        if (m_webglDrawBuffers
            && pname >= Extensions3D::DRAW_BUFFER0_EXT
            && pname < static_cast<GC3Denum>(Extensions3D::DRAW_BUFFER0_EXT + getMaxDrawBuffers())) {
            GC3Dint value = GraphicsContext3D::NONE;
            if (m_framebufferBinding)
                value = m_framebufferBinding->getDrawBuffer(pname);
            else // emulated backbuffer
                value = m_backDrawBuffer;
            return WebGLGetInfo(value);
        }
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, "getParameter", "invalid parameter name");
        return WebGLGetInfo();
    }
}

GC3Dint WebGLRenderingContext::getMaxDrawBuffers()
{
    if (!supportsDrawBuffers())
        return 0;
    if (!m_maxDrawBuffers)
        m_context->getIntegerv(Extensions3D::MAX_DRAW_BUFFERS_EXT, &m_maxDrawBuffers);
    if (!m_maxColorAttachments)
        m_context->getIntegerv(Extensions3D::MAX_COLOR_ATTACHMENTS_EXT, &m_maxColorAttachments);
    // WEBGL_draw_buffers requires MAX_COLOR_ATTACHMENTS >= MAX_DRAW_BUFFERS.
    return std::min(m_maxDrawBuffers, m_maxColorAttachments);
}

GC3Dint WebGLRenderingContext::getMaxColorAttachments()
{
    if (!supportsDrawBuffers())
        return 0;
    if (!m_maxColorAttachments)
        m_context->getIntegerv(Extensions3D::MAX_COLOR_ATTACHMENTS_EXT, &m_maxColorAttachments);
    return m_maxColorAttachments;
}
    
bool WebGLRenderingContext::validateIndexArrayConservative(GC3Denum type, unsigned& numElementsRequired)
{
    // Performs conservative validation by caching a maximum index of
    // the given type per element array buffer. If all of the bound
    // array buffers have enough elements to satisfy that maximum
    // index, skips the expensive per-draw-call iteration in
    // validateIndexArrayPrecise.
    
    RefPtr<WebGLBuffer> elementArrayBuffer = m_boundVertexArrayObject->getElementArrayBuffer();
    
    if (!elementArrayBuffer)
        return false;
    
    GC3Dsizeiptr numElements = elementArrayBuffer->byteLength();
    // The case count==0 is already dealt with in drawElements before validateIndexArrayConservative.
    if (!numElements)
        return false;
    const ArrayBuffer* buffer = elementArrayBuffer->elementArrayBuffer();
    ASSERT(buffer);
    
    int maxIndex = elementArrayBuffer->getCachedMaxIndex(type);
    if (maxIndex < 0) {
        // Compute the maximum index in the entire buffer for the given type of index.
        switch (type) {
        case GraphicsContext3D::UNSIGNED_BYTE: {
            const GC3Dubyte* p = static_cast<const GC3Dubyte*>(buffer->data());
            for (GC3Dsizeiptr i = 0; i < numElements; i++)
                maxIndex = std::max(maxIndex, static_cast<int>(p[i]));
            break;
        }
        case GraphicsContext3D::UNSIGNED_SHORT: {
            numElements /= sizeof(GC3Dushort);
            const GC3Dushort* p = static_cast<const GC3Dushort*>(buffer->data());
            for (GC3Dsizeiptr i = 0; i < numElements; i++)
                maxIndex = std::max(maxIndex, static_cast<int>(p[i]));
            break;
        }
        case GraphicsContext3D::UNSIGNED_INT: {
            if (!m_oesElementIndexUint)
                return false;
            numElements /= sizeof(GC3Duint);
            const GC3Duint* p = static_cast<const GC3Duint*>(buffer->data());
            for (GC3Dsizeiptr i = 0; i < numElements; i++)
                maxIndex = std::max(maxIndex, static_cast<int>(p[i]));
            break;
        }
        default:
            return false;
    }
    elementArrayBuffer->setCachedMaxIndex(type, maxIndex);
    }
    
    if (maxIndex >= 0) {
        // The number of required elements is one more than the maximum
        // index that will be accessed.
        numElementsRequired = maxIndex + 1;
        return true;
    }
    
    return false;
}

bool WebGLRenderingContext::validateBlendEquation(const char* functionName, GC3Denum mode)
{
    switch (mode) {
    case GraphicsContext3D::FUNC_ADD:
    case GraphicsContext3D::FUNC_SUBTRACT:
    case GraphicsContext3D::FUNC_REVERSE_SUBTRACT:
    case Extensions3D::MIN_EXT:
    case Extensions3D::MAX_EXT:
        if ((mode == Extensions3D::MIN_EXT || mode == Extensions3D::MAX_EXT) && !m_extBlendMinMax) {
            synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid mode");
            return false;
        }
        return true;
        break;
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid mode");
        return false;
    }
}

bool WebGLRenderingContext::validateCapability(const char* functionName, GC3Denum cap)
{
    switch (cap) {
    case GraphicsContext3D::BLEND:
    case GraphicsContext3D::CULL_FACE:
    case GraphicsContext3D::DEPTH_TEST:
    case GraphicsContext3D::DITHER:
    case GraphicsContext3D::POLYGON_OFFSET_FILL:
    case GraphicsContext3D::SAMPLE_ALPHA_TO_COVERAGE:
    case GraphicsContext3D::SAMPLE_COVERAGE:
    case GraphicsContext3D::SCISSOR_TEST:
    case GraphicsContext3D::STENCIL_TEST:
        return true;
    default:
        synthesizeGLError(GraphicsContext3D::INVALID_ENUM, functionName, "invalid capability");
        return false;
    }
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
