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
#include "EXTBlendMinMax.h"
#include "EXTFragDepth.h"
#include "EXTShaderTextureLOD.h"
#include "EXTTextureFilterAnisotropic.h"
#include "EXTsRGB.h"
#include "Extensions3D.h"
#include "OESElementIndexUint.h"
#include "OESStandardDerivatives.h"
#include "OESTextureFloat.h"
#include "OESTextureFloatLinear.h"
#include "OESTextureHalfFloat.h"
#include "OESTextureHalfFloatLinear.h"
#include "OESVertexArrayObject.h"
#include "WebGLCompressedTextureATC.h"
#include "WebGLCompressedTexturePVRTC.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLDepthTexture.h"
#include "WebGLDrawBuffers.h"
#include "WebGLLoseContext.h"

namespace WebCore {

WebGLRenderingContext::WebGLRenderingContext(HTMLCanvasElement* passedCanvas, GraphicsContext3D::Attributes attributes)
    : WebGLRenderingContextBase(passedCanvas, attributes)
{
}

WebGLRenderingContext::WebGLRenderingContext(HTMLCanvasElement* passedCanvas, PassRefPtr<GraphicsContext3D> context,
    GraphicsContext3D::Attributes attributes) : WebGLRenderingContextBase(passedCanvas, context, attributes)
{
}


WebGLExtension* WebGLRenderingContext::getExtension(const String& name)
{
    if (isContextLostOrPending())
        return nullptr;
    
    if (equalIgnoringCase(name, "EXT_blend_minmax")
        && m_context->getExtensions()->supports("GL_EXT_blend_minmax")) {
        if (!m_extBlendMinMax) {
            m_context->getExtensions()->ensureEnabled("GL_EXT_blend_minmax");
            m_extBlendMinMax = std::make_unique<EXTBlendMinMax>(this);
        }
        return m_extBlendMinMax.get();
    }
    if (equalIgnoringCase(name, "EXT_sRGB")
        && m_context->getExtensions()->supports("GL_EXT_sRGB")) {
        if (!m_extsRGB) {
            m_context->getExtensions()->ensureEnabled("GL_EXT_sRGB");
            m_extsRGB = std::make_unique<EXTsRGB>(this);
        }
        return m_extsRGB.get();
    }
    if (equalIgnoringCase(name, "EXT_frag_depth")
        && m_context->getExtensions()->supports("GL_EXT_frag_depth")) {
        if (!m_extFragDepth) {
            m_context->getExtensions()->ensureEnabled("GL_EXT_frag_depth");
            m_extFragDepth = std::make_unique<EXTFragDepth>(this);
        }
        return m_extFragDepth.get();
    }
    if (equalIgnoringCase(name, "EXT_shader_texture_lod")
        && (m_context->getExtensions()->supports("GL_EXT_shader_texture_lod") || m_context->getExtensions()->supports("GL_ARB_shader_texture_lod"))) {
        if (!m_extShaderTextureLOD) {
            m_context->getExtensions()->ensureEnabled("GL_EXT_shader_texture_lod");
            m_extShaderTextureLOD = std::make_unique<EXTShaderTextureLOD>(this);
        }
        return m_extShaderTextureLOD.get();
    }
    if (equalIgnoringCase(name, "WEBKIT_EXT_texture_filter_anisotropic")
        && m_context->getExtensions()->supports("GL_EXT_texture_filter_anisotropic")) {
        if (!m_extTextureFilterAnisotropic) {
            m_context->getExtensions()->ensureEnabled("GL_EXT_texture_filter_anisotropic");
            m_extTextureFilterAnisotropic = std::make_unique<EXTTextureFilterAnisotropic>(this);
        }
        return m_extTextureFilterAnisotropic.get();
    }
    if (equalIgnoringCase(name, "OES_standard_derivatives")
        && m_context->getExtensions()->supports("GL_OES_standard_derivatives")) {
        if (!m_oesStandardDerivatives) {
            m_context->getExtensions()->ensureEnabled("GL_OES_standard_derivatives");
            m_oesStandardDerivatives = std::make_unique<OESStandardDerivatives>(this);
        }
        return m_oesStandardDerivatives.get();
    }
    if (equalIgnoringCase(name, "OES_texture_float")
        && m_context->getExtensions()->supports("GL_OES_texture_float")) {
        if (!m_oesTextureFloat) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_float");
            m_oesTextureFloat = std::make_unique<OESTextureFloat>(this);
        }
        return m_oesTextureFloat.get();
    }
    if (equalIgnoringCase(name, "OES_texture_float_linear")
        && m_context->getExtensions()->supports("GL_OES_texture_float_linear")) {
        if (!m_oesTextureFloatLinear) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_float_linear");
            m_oesTextureFloatLinear = std::make_unique<OESTextureFloatLinear>(this);
        }
        return m_oesTextureFloatLinear.get();
    }
    if (equalIgnoringCase(name, "OES_texture_half_float")
        && m_context->getExtensions()->supports("GL_OES_texture_half_float")) {
        if (!m_oesTextureHalfFloat) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_half_float");
            m_oesTextureHalfFloat = std::make_unique<OESTextureHalfFloat>(this);
        }
        return m_oesTextureHalfFloat.get();
    }
    if (equalIgnoringCase(name, "OES_texture_half_float_linear")
        && m_context->getExtensions()->supports("GL_OES_texture_half_float_linear")) {
        if (!m_oesTextureHalfFloatLinear) {
            m_context->getExtensions()->ensureEnabled("GL_OES_texture_half_float_linear");
            m_oesTextureHalfFloatLinear = std::make_unique<OESTextureHalfFloatLinear>(this);
        }
        return m_oesTextureHalfFloatLinear.get();
    }
    if (equalIgnoringCase(name, "OES_vertex_array_object")
        && m_context->getExtensions()->supports("GL_OES_vertex_array_object")) {
        if (!m_oesVertexArrayObject) {
            m_context->getExtensions()->ensureEnabled("GL_OES_vertex_array_object");
            m_oesVertexArrayObject = std::make_unique<OESVertexArrayObject>(this);
        }
        return m_oesVertexArrayObject.get();
    }
    if (equalIgnoringCase(name, "OES_element_index_uint")
        && m_context->getExtensions()->supports("GL_OES_element_index_uint")) {
        if (!m_oesElementIndexUint) {
            m_context->getExtensions()->ensureEnabled("GL_OES_element_index_uint");
            m_oesElementIndexUint = std::make_unique<OESElementIndexUint>(this);
        }
        return m_oesElementIndexUint.get();
    }
    if (equalIgnoringCase(name, "WEBGL_lose_context")) {
        if (!m_webglLoseContext)
            m_webglLoseContext = std::make_unique<WebGLLoseContext>(this);
        return m_webglLoseContext.get();
    }
    if ((equalIgnoringCase(name, "WEBKIT_WEBGL_compressed_texture_atc"))
        && WebGLCompressedTextureATC::supported(this)) {
        if (!m_webglCompressedTextureATC)
            m_webglCompressedTextureATC = std::make_unique<WebGLCompressedTextureATC>(this);
        return m_webglCompressedTextureATC.get();
    }
    if ((equalIgnoringCase(name, "WEBKIT_WEBGL_compressed_texture_pvrtc"))
        && WebGLCompressedTexturePVRTC::supported(this)) {
        if (!m_webglCompressedTexturePVRTC)
            m_webglCompressedTexturePVRTC = std::make_unique<WebGLCompressedTexturePVRTC>(this);
        return m_webglCompressedTexturePVRTC.get();
    }
    if (equalIgnoringCase(name, "WEBGL_compressed_texture_s3tc")
        && WebGLCompressedTextureS3TC::supported(this)) {
        if (!m_webglCompressedTextureS3TC)
            m_webglCompressedTextureS3TC = std::make_unique<WebGLCompressedTextureS3TC>(this);
        return m_webglCompressedTextureS3TC.get();
    }
    if (equalIgnoringCase(name, "WEBGL_depth_texture")
        && WebGLDepthTexture::supported(graphicsContext3D())) {
        if (!m_webglDepthTexture) {
            m_context->getExtensions()->ensureEnabled("GL_CHROMIUM_depth_texture");
            m_webglDepthTexture = std::make_unique<WebGLDepthTexture>(this);
        }
        return m_webglDepthTexture.get();
    }
    if (equalIgnoringCase(name, "WEBGL_draw_buffers") && supportsDrawBuffers()) {
        if (!m_webglDrawBuffers) {
            m_context->getExtensions()->ensureEnabled("GL_EXT_draw_buffers");
            m_webglDrawBuffers = std::make_unique<WebGLDrawBuffers>(this);
        }
        return m_webglDrawBuffers.get();
    }
    if (equalIgnoringCase(name, "ANGLE_instanced_arrays") && ANGLEInstancedArrays::supported(this)) {
        if (!m_angleInstancedArrays) {
            m_context->getExtensions()->ensureEnabled("GL_ANGLE_instanced_arrays");
            m_angleInstancedArrays = std::make_unique<ANGLEInstancedArrays>(this);
        }
        return m_angleInstancedArrays.get();
    }
    if (allowPrivilegedExtensions()) {
        if (equalIgnoringCase(name, "WEBGL_debug_renderer_info")) {
            if (!m_webglDebugRendererInfo)
                m_webglDebugRendererInfo = std::make_unique<WebGLDebugRendererInfo>(this);
            return m_webglDebugRendererInfo.get();
        }
        if (equalIgnoringCase(name, "WEBGL_debug_shaders")
            && m_context->getExtensions()->supports("GL_ANGLE_translated_shader_source")) {
            if (!m_webglDebugShaders)
                m_webglDebugShaders = std::make_unique<WebGLDebugShaders>(this);
            return m_webglDebugShaders.get();
        }
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
        result.append("WEBKIT_EXT_texture_filter_anisotropic");
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
    
    if (allowPrivilegedExtensions()) {
        if (m_context->getExtensions()->supports("GL_ANGLE_translated_shader_source"))
            result.append("WEBGL_debug_shaders");
        result.append("WEBGL_debug_renderer_info");
    }
    
    return result;
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
