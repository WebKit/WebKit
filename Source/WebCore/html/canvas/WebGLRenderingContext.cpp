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
#include "EXTClipControl.h"
#include "EXTColorBufferFloat.h"
#include "EXTColorBufferHalfFloat.h"
#include "EXTConservativeDepth.h"
#include "EXTDepthClamp.h"
#include "EXTDisjointTimerQuery.h"
#include "EXTDisjointTimerQueryWebGL2.h"
#include "EXTFloatBlend.h"
#include "EXTFragDepth.h"
#include "EXTPolygonOffsetClamp.h"
#include "EXTRenderSnorm.h"
#include "EXTShaderTextureLOD.h"
#include "EXTTextureCompressionBPTC.h"
#include "EXTTextureCompressionRGTC.h"
#include "EXTTextureFilterAnisotropic.h"
#include "EXTTextureMirrorClampToEdge.h"
#include "EXTTextureNorm16.h"
#include "EXTsRGB.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageData.h"
#include "InspectorInstrumentation.h"
#include "KHRParallelShaderCompile.h"
#include "NVShaderNoperspectiveInterpolation.h"
#include "OESDrawBuffersIndexed.h"
#include "OESElementIndexUint.h"
#include "OESFBORenderMipmap.h"
#include "OESSampleVariables.h"
#include "OESShaderMultisampleInterpolation.h"
#include "OESStandardDerivatives.h"
#include "OESTextureFloat.h"
#include "OESTextureFloatLinear.h"
#include "OESTextureHalfFloat.h"
#include "OESTextureHalfFloatLinear.h"
#include "OESVertexArrayObject.h"
#include "RenderBox.h"
#include "WebCodecsVideoFrame.h"
#include "WebCoreOpaqueRootInlines.h"
#include "WebGLBlendFuncExtended.h"
#include "WebGLBuffer.h"
#include "WebGLClipCullDistance.h"
#include "WebGLColorBufferFloat.h"
#include "WebGLCompressedTextureASTC.h"
#include "WebGLCompressedTextureETC.h"
#include "WebGLCompressedTextureETC1.h"
#include "WebGLCompressedTexturePVRTC.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLCompressedTextureS3TCsRGB.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLDepthTexture.h"
#include "WebGLDrawBuffers.h"
#include "WebGLDrawInstancedBaseVertexBaseInstance.h"
#include "WebGLFramebuffer.h"
#include "WebGLLoseContext.h"
#include "WebGLMultiDraw.h"
#include "WebGLMultiDrawInstancedBaseVertexBaseInstance.h"
#include "WebGLPolygonMode.h"
#include "WebGLProgram.h"
#include "WebGLProvokingVertex.h"
#include "WebGLRenderSharedExponent.h"
#include "WebGLRenderbuffer.h"
#include "WebGLSampler.h"
#include "WebGLStencilTexturing.h"
#include "WebGLTexture.h"
#include "WebGLTransformFeedback.h"
#include "WebGLUtilities.h"
#include "WebGLVertexArrayObject.h"
#include "WebGLVertexArrayObjectOES.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSCellInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebGLRenderingContext);

std::unique_ptr<WebGLRenderingContext> WebGLRenderingContext::create(CanvasBase& canvas, WebGLContextAttributes&& attributes)
{
    return std::unique_ptr<WebGLRenderingContext>(new WebGLRenderingContext(canvas, WTFMove(attributes)));
}

WebGLRenderingContext::~WebGLRenderingContext()
{
    m_activeQuery = nullptr;
}

void WebGLRenderingContext::initializeDefaultObjects()
{
    WebGLRenderingContextBase::initializeDefaultObjects();
    m_defaultVertexArrayObject = WebGLVertexArrayObjectOES::createDefault(*this);
    m_boundVertexArrayObject = m_defaultVertexArrayObject;
}

std::optional<WebGLExtensionAny> WebGLRenderingContext::getExtension(const String& name)
{
    if (isContextLost())
        return std::nullopt;

    // When adding extensions that use enableDraftExtensions, add them to the webgl-draft-extensions-flag.js test.
    [[maybe_unused]] const bool enableDraftExtensions = scriptExecutionContext()->settingsValues().webGLDraftExtensionsEnabled;

#define ENABLE_IF_REQUESTED(type, variable, nameLiteral, canEnable) \
    if (equalIgnoringASCIICase(name, nameLiteral ## _s)) { \
        if (!(canEnable)) \
            return std::nullopt; \
        if (!variable) { \
            variable = adoptRef(new type(*this)); \
            InspectorInstrumentation::didEnableExtension(*this, name); \
        } \
        return *variable; \
    }

    ENABLE_IF_REQUESTED(ANGLEInstancedArrays, m_angleInstancedArrays, "ANGLE_instanced_arrays", ANGLEInstancedArrays::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTBlendMinMax, m_extBlendMinMax, "EXT_blend_minmax", EXTBlendMinMax::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTClipControl, m_extClipControl, "EXT_clip_control", EXTClipControl::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTColorBufferHalfFloat, m_extColorBufferHalfFloat, "EXT_color_buffer_half_float", EXTColorBufferHalfFloat::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTDepthClamp, m_extDepthClamp, "EXT_depth_clamp", EXTDepthClamp::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTDisjointTimerQuery, m_extDisjointTimerQuery, "EXT_disjoint_timer_query", EXTDisjointTimerQuery::supported(*m_context) && scriptExecutionContext()->settingsValues().webGLTimerQueriesEnabled);
    ENABLE_IF_REQUESTED(EXTFloatBlend, m_extFloatBlend, "EXT_float_blend", EXTFloatBlend::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTFragDepth, m_extFragDepth, "EXT_frag_depth", EXTFragDepth::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTPolygonOffsetClamp, m_extPolygonOffsetClamp, "EXT_polygon_offset_clamp", EXTPolygonOffsetClamp::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTShaderTextureLOD, m_extShaderTextureLOD, "EXT_shader_texture_lod", EXTShaderTextureLOD::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTTextureCompressionBPTC, m_extTextureCompressionBPTC, "EXT_texture_compression_bptc", EXTTextureCompressionBPTC::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTTextureCompressionRGTC, m_extTextureCompressionRGTC, "EXT_texture_compression_rgtc", EXTTextureCompressionRGTC::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTTextureFilterAnisotropic, m_extTextureFilterAnisotropic, "EXT_texture_filter_anisotropic", EXTTextureFilterAnisotropic::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTTextureMirrorClampToEdge, m_extTextureMirrorClampToEdge, "EXT_texture_mirror_clamp_to_edge", EXTTextureMirrorClampToEdge::supported(*m_context));
    ENABLE_IF_REQUESTED(EXTsRGB, m_extsRGB, "EXT_sRGB", EXTsRGB::supported(*m_context));
    ENABLE_IF_REQUESTED(KHRParallelShaderCompile, m_khrParallelShaderCompile, "KHR_parallel_shader_compile", KHRParallelShaderCompile::supported(*m_context));
    ENABLE_IF_REQUESTED(OESElementIndexUint, m_oesElementIndexUint, "OES_element_index_uint", OESElementIndexUint::supported(*m_context));
    ENABLE_IF_REQUESTED(OESFBORenderMipmap, m_oesFBORenderMipmap, "OES_fbo_render_mipmap", OESFBORenderMipmap::supported(*m_context));
    ENABLE_IF_REQUESTED(OESStandardDerivatives, m_oesStandardDerivatives, "OES_standard_derivatives", OESStandardDerivatives::supported(*m_context));
    ENABLE_IF_REQUESTED(OESTextureFloat, m_oesTextureFloat, "OES_texture_float", OESTextureFloat::supported(*m_context));
    ENABLE_IF_REQUESTED(OESTextureFloatLinear, m_oesTextureFloatLinear, "OES_texture_float_linear", OESTextureFloatLinear::supported(*m_context));
    ENABLE_IF_REQUESTED(OESTextureHalfFloat, m_oesTextureHalfFloat, "OES_texture_half_float", OESTextureHalfFloat::supported(*m_context));
    ENABLE_IF_REQUESTED(OESTextureHalfFloatLinear, m_oesTextureHalfFloatLinear, "OES_texture_half_float_linear", OESTextureHalfFloatLinear::supported(*m_context));
    ENABLE_IF_REQUESTED(OESVertexArrayObject, m_oesVertexArrayObject, "OES_vertex_array_object", OESVertexArrayObject::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLBlendFuncExtended, m_webglBlendFuncExtended, "WEBGL_blend_func_extended", WebGLBlendFuncExtended::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLColorBufferFloat, m_webglColorBufferFloat, "WEBGL_color_buffer_float", WebGLColorBufferFloat::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureASTC, m_webglCompressedTextureASTC, "WEBGL_compressed_texture_astc", WebGLCompressedTextureASTC::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureETC, m_webglCompressedTextureETC, "WEBGL_compressed_texture_etc", WebGLCompressedTextureETC::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureETC1, m_webglCompressedTextureETC1, "WEBGL_compressed_texture_etc1", WebGLCompressedTextureETC1::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLCompressedTexturePVRTC, m_webglCompressedTexturePVRTC, "WEBGL_compressed_texture_pvrtc", WebGLCompressedTexturePVRTC::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLCompressedTexturePVRTC, m_webglCompressedTexturePVRTC, "WEBKIT_WEBGL_compressed_texture_pvrtc", WebGLCompressedTexturePVRTC::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureS3TC, m_webglCompressedTextureS3TC, "WEBGL_compressed_texture_s3tc", WebGLCompressedTextureS3TC::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLCompressedTextureS3TCsRGB, m_webglCompressedTextureS3TCsRGB, "WEBGL_compressed_texture_s3tc_srgb", WebGLCompressedTextureS3TCsRGB::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLDebugRendererInfo, m_webglDebugRendererInfo, "WEBGL_debug_renderer_info", true);
    ENABLE_IF_REQUESTED(WebGLDebugShaders, m_webglDebugShaders, "WEBGL_debug_shaders", WebGLDebugShaders::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLDepthTexture, m_webglDepthTexture, "WEBGL_depth_texture", WebGLDepthTexture::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLDrawBuffers, m_webglDrawBuffers, "WEBGL_draw_buffers", supportsDrawBuffers());
    ENABLE_IF_REQUESTED(WebGLLoseContext, m_webglLoseContext, "WEBGL_lose_context", true);
    ENABLE_IF_REQUESTED(WebGLMultiDraw, m_webglMultiDraw, "WEBGL_multi_draw", WebGLMultiDraw::supported(*m_context));
    ENABLE_IF_REQUESTED(WebGLPolygonMode, m_webglPolygonMode, "WEBGL_polygon_mode", WebGLPolygonMode::supported(*m_context));
    return std::nullopt;
}

std::optional<Vector<String>> WebGLRenderingContext::getSupportedExtensions()
{
    if (isContextLost())
        return std::nullopt;

    Vector<String> result;

    [[maybe_unused]] const bool enableDraftExtensions = scriptExecutionContext()->settingsValues().webGLDraftExtensionsEnabled;

#define APPEND_IF_SUPPORTED(nameLiteral, condition) \
    if (condition) \
        result.append(nameLiteral ## _s);

    APPEND_IF_SUPPORTED("ANGLE_instanced_arrays", ANGLEInstancedArrays::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_blend_minmax", EXTBlendMinMax::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_clip_control", EXTClipControl::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_color_buffer_half_float", EXTColorBufferHalfFloat::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_depth_clamp", EXTDepthClamp::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_disjoint_timer_query", EXTDisjointTimerQuery::supported(*m_context) && scriptExecutionContext()->settingsValues().webGLTimerQueriesEnabled)
    APPEND_IF_SUPPORTED("EXT_float_blend", EXTFloatBlend::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_frag_depth", EXTFragDepth::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_polygon_offset_clamp", EXTPolygonOffsetClamp::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_shader_texture_lod", EXTShaderTextureLOD::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_texture_compression_bptc", EXTTextureCompressionBPTC::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_texture_compression_rgtc", EXTTextureCompressionRGTC::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_texture_filter_anisotropic", EXTTextureFilterAnisotropic::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_texture_mirror_clamp_to_edge", EXTTextureMirrorClampToEdge::supported(*m_context))
    APPEND_IF_SUPPORTED("EXT_sRGB", EXTsRGB::supported(*m_context))
    APPEND_IF_SUPPORTED("KHR_parallel_shader_compile", KHRParallelShaderCompile::supported(*m_context))
    APPEND_IF_SUPPORTED("OES_element_index_uint", OESElementIndexUint::supported(*m_context))
    APPEND_IF_SUPPORTED("OES_fbo_render_mipmap", OESFBORenderMipmap::supported(*m_context))
    APPEND_IF_SUPPORTED("OES_standard_derivatives", OESStandardDerivatives::supported(*m_context))
    APPEND_IF_SUPPORTED("OES_texture_float", OESTextureFloat::supported(*m_context))
    APPEND_IF_SUPPORTED("OES_texture_float_linear", OESTextureFloatLinear::supported(*m_context))
    APPEND_IF_SUPPORTED("OES_texture_half_float", OESTextureHalfFloat::supported(*m_context))
    APPEND_IF_SUPPORTED("OES_texture_half_float_linear", OESTextureHalfFloatLinear::supported(*m_context))
    APPEND_IF_SUPPORTED("OES_vertex_array_object", OESVertexArrayObject::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_blend_func_extended", WebGLBlendFuncExtended::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_color_buffer_float", WebGLColorBufferFloat::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_compressed_texture_astc", WebGLCompressedTextureASTC::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_compressed_texture_etc", WebGLCompressedTextureETC::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_compressed_texture_etc1", WebGLCompressedTextureETC1::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_compressed_texture_pvrtc", WebGLCompressedTexturePVRTC::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBKIT_WEBGL_compressed_texture_pvrtc", WebGLCompressedTexturePVRTC::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_compressed_texture_s3tc", WebGLCompressedTextureS3TC::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_compressed_texture_s3tc_srgb", WebGLCompressedTextureS3TCsRGB::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_debug_renderer_info", true)
    APPEND_IF_SUPPORTED("WEBGL_debug_shaders", WebGLDebugShaders::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_depth_texture", WebGLDepthTexture::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_draw_buffers", supportsDrawBuffers())
    APPEND_IF_SUPPORTED("WEBGL_lose_context", true)
    APPEND_IF_SUPPORTED("WEBGL_multi_draw", WebGLMultiDraw::supported(*m_context))
    APPEND_IF_SUPPORTED("WEBGL_polygon_mode", WebGLPolygonMode::supported(*m_context))

    return result;
}

WebGLAny WebGLRenderingContext::getFramebufferAttachmentParameter(GCGLenum target, GCGLenum attachment, GCGLenum pname)
{
    if (isContextLost())
        return nullptr;

    const char* functionName = "getFramebufferAttachmentParameter";
    if (!validateFramebufferFuncParameters(functionName, target, attachment))
        return nullptr;

    if (!m_framebufferBinding || !m_framebufferBinding->object()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "no framebuffer bound");
        return nullptr;
    }

#if ENABLE(WEBXR)
    if (m_framebufferBinding->isOpaque()) {
        synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "An opaque framebuffer's attachments cannot be inspected or changed");
        return nullptr;
    }
#endif

    auto object = m_framebufferBinding->getAttachmentObject(attachment);
    if (!object) {
        if (pname == GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE)
            return static_cast<unsigned>(GraphicsContextGL::NONE);
        // OpenGL ES 2.0 specifies INVALID_ENUM in this case, while desktop GL
        // specifies INVALID_OPERATION.
        synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid parameter name");
        return nullptr;
    }

    const bool isTexture = std::holds_alternative<RefPtr<WebGLTexture>>(*object);
    switch (pname) {
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
        if (isTexture)
            return static_cast<unsigned>(GraphicsContextGL::TEXTURE);
        return static_cast<unsigned>(GraphicsContextGL::RENDERBUFFER);
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
        if (isTexture)
            return std::get<RefPtr<WebGLTexture>>(WTFMove(*object));
        return std::get<RefPtr<WebGLRenderbuffer>>(WTFMove(*object));
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
        if (!isTexture)
            break;
        return m_context->getFramebufferAttachmentParameteri(target, attachment, pname);
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT:
        if (!m_extsRGB) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid parameter name, EXT_sRGB not enabled");
            return nullptr;
        }
        return m_context->getFramebufferAttachmentParameteri(target, attachment, pname);
    case GraphicsContextGL::FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE_EXT:
        if (!m_extColorBufferHalfFloat && !m_webglColorBufferFloat) {
            synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid parameter name, EXT_color_buffer_half_float or WEBGL_color_buffer_float not enabled");
            return nullptr;
        }
        if (attachment == GraphicsContextGL::DEPTH_STENCIL_ATTACHMENT) {
            synthesizeGLError(GraphicsContextGL::INVALID_OPERATION, functionName, "component type cannot be queried for DEPTH_STENCIL_ATTACHMENT");
            return nullptr;
        }
        return m_context->getFramebufferAttachmentParameteri(target, attachment, pname);
    }

    synthesizeGLError(GraphicsContextGL::INVALID_ENUM, functionName, "invalid parameter name");
    return nullptr;
}

long long WebGLRenderingContext::getInt64Parameter(GCGLenum pname)
{
    return m_context->getInteger64EXT(pname);
}

GCGLint WebGLRenderingContext::maxDrawBuffers()
{
    if (!supportsDrawBuffers())
        return 0;
    if (!m_maxDrawBuffers)
        m_maxDrawBuffers = m_context->getInteger(GraphicsContextGL::MAX_DRAW_BUFFERS_EXT);
    if (!m_maxColorAttachments)
        m_maxColorAttachments = m_context->getInteger(GraphicsContextGL::MAX_COLOR_ATTACHMENTS_EXT);
    // WEBGL_draw_buffers requires MAX_COLOR_ATTACHMENTS >= MAX_DRAW_BUFFERS.
    return std::min(m_maxDrawBuffers, m_maxColorAttachments);
}

GCGLint WebGLRenderingContext::maxColorAttachments()
{
    if (!supportsDrawBuffers())
        return 0;
    if (!m_maxColorAttachments)
        m_maxColorAttachments = m_context->getInteger(GraphicsContextGL::MAX_COLOR_ATTACHMENTS_EXT);
    return m_maxColorAttachments;
}

bool WebGLRenderingContext::validateBlendEquation(const char* functionName, GCGLenum mode)
{
    switch (mode) {
    case GraphicsContextGL::FUNC_ADD:
    case GraphicsContextGL::FUNC_SUBTRACT:
    case GraphicsContextGL::FUNC_REVERSE_SUBTRACT:
    case GraphicsContextGL::MIN_EXT:
    case GraphicsContextGL::MAX_EXT:
        if ((mode == GraphicsContextGL::MIN_EXT || mode == GraphicsContextGL::MAX_EXT) && !m_extBlendMinMax) {
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

void WebGLRenderingContext::addMembersToOpaqueRoots(JSC::AbstractSlotVisitor& visitor)
{
    WebGLRenderingContextBase::addMembersToOpaqueRoots(visitor);

    Locker locker { objectGraphLock() };
    addWebCoreOpaqueRoot(visitor, m_activeQuery.get());
}

WebCoreOpaqueRoot root(const WebGLExtension<WebGLRenderingContext>* extension)
{
    return WebCoreOpaqueRoot { extension->opaqueRoot() };
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
