/*
 * Copyright (C) 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Google Inc. All rights reserved.
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
#include "GraphicsContextGLANGLE.h"

#include "ANGLEHeaders.h"
#include "ANGLEUtilities.h"
#include "ByteArrayPixelBuffer.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Logging.h"
#include "NativeImage.h"
#include "NotImplemented.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>
#include <wtf/MallocSpan.h>
#include <wtf/RuntimeApplicationChecks.h>
#include <wtf/Seconds.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>
#include <wtf/text/CStringView.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(VIDEO) && USE(AVFOUNDATION)
#include "GraphicsContextGLCVCocoa.h"
#endif

// This one definition short-circuits the need for gl2ext.h, which
// would need more work to be included from WebCore.
#define GL_MAX_SAMPLES_EXT 0x8D57

namespace WebCore {

// List of displays ever instantiated from EGL. When terminating all EGL resources, we need to
// terminate all displays. However, we cannot ask EGL all the displays it has created.
// We must know all the displays via this set.
static HashSet<GCGLDisplay>& usedDisplays()
{
    static NeverDestroyed<HashSet<GCGLDisplay>> s_usedDisplays;
    return s_usedDisplays;
}

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
static void wipeAlphaChannelFromPixels(std::span<uint8_t> pixels)
{
    for (size_t i = 0; i < pixels.size(); i += 4)
        pixels[i + 3] = 255;
}
#endif

static inline const Vector<const void*> asPointers(std::span<const GCGLsizei> offsets)
{
    // Must cast offsets from int to void* before passing down to ANGLE.

    return WTF::map(offsets, [](const GCGLsizei offset) {
        return reinterpret_cast<const void*>(offset);
    });
}

static std::span<uint8_t> glMapBufferRangeSpan(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)
{
    void* ptr = GL_MapBufferRange(target, offset, length, access);
    if (!ptr)
        return { };
    return unsafeMakeSpan(static_cast<uint8_t*>(ptr), length);
}

static constexpr std::pair<ComparableASCIILiteral, GCGLExtension> extensionsMapping[] {
    { "GL_ANGLE_base_vertex_base_instance"_s, GCGLExtension::ANGLE_base_vertex_base_instance },
    { "GL_ANGLE_clip_cull_distance"_s, GCGLExtension::ANGLE_clip_cull_distance },
    { "GL_ANGLE_compressed_texture_etc"_s, GCGLExtension::ANGLE_compressed_texture_etc },
    { "GL_ANGLE_depth_texture"_s, GCGLExtension::ANGLE_depth_texture },
    { "GL_ANGLE_instanced_arrays"_s, GCGLExtension::ANGLE_instanced_arrays },
    { "GL_ANGLE_multi_draw"_s, GCGLExtension::ANGLE_multi_draw },
    { "GL_ANGLE_pack_reverse_row_order"_s, GCGLExtension::ANGLE_pack_reverse_row_order },
    { "GL_ANGLE_polygon_mode"_s, GCGLExtension::ANGLE_polygon_mode },
    { "GL_ANGLE_provoking_vertex"_s, GCGLExtension::ANGLE_provoking_vertex },
    { "GL_ANGLE_stencil_texturing"_s, GCGLExtension::ANGLE_stencil_texturing },
    { "GL_ANGLE_texture_compression_dxt3"_s, GCGLExtension::ANGLE_texture_compression_dxt3 },
    { "GL_ANGLE_texture_compression_dxt5"_s, GCGLExtension::ANGLE_texture_compression_dxt5 },
    { "GL_ANGLE_translated_shader_source"_s, GCGLExtension::ANGLE_translated_shader_source },
    { "GL_CHROMIUM_color_buffer_float_rgb"_s, GCGLExtension::CHROMIUM_color_buffer_float_rgb },
    { "GL_CHROMIUM_color_buffer_float_rgba"_s, GCGLExtension::CHROMIUM_color_buffer_float_rgba },
    { "GL_EXT_blend_func_extended"_s, GCGLExtension::EXT_blend_func_extended },
    { "GL_EXT_blend_minmax"_s, GCGLExtension::EXT_blend_minmax },
    { "GL_EXT_clip_control"_s, GCGLExtension::EXT_clip_control },
    { "GL_EXT_color_buffer_float"_s, GCGLExtension::EXT_color_buffer_float },
    { "GL_EXT_color_buffer_half_float"_s, GCGLExtension::EXT_color_buffer_half_float },
    { "GL_EXT_conservative_depth"_s, GCGLExtension::EXT_conservative_depth },
    { "GL_EXT_depth_clamp"_s, GCGLExtension::EXT_depth_clamp },
    { "GL_EXT_disjoint_timer_query"_s, GCGLExtension::EXT_disjoint_timer_query },
    { "GL_EXT_draw_buffers"_s, GCGLExtension::EXT_draw_buffers },
    { "GL_EXT_float_blend"_s, GCGLExtension::EXT_float_blend },
    { "GL_EXT_frag_depth"_s, GCGLExtension::EXT_frag_depth },
    { "GL_EXT_polygon_offset_clamp"_s, GCGLExtension::EXT_polygon_offset_clamp },
    { "GL_EXT_render_snorm"_s, GCGLExtension::EXT_render_snorm },
    { "GL_EXT_sRGB"_s, GCGLExtension::EXT_sRGB },
    { "GL_EXT_shader_texture_lod"_s, GCGLExtension::EXT_shader_texture_lod },
    { "GL_EXT_texture_compression_bptc"_s, GCGLExtension::EXT_texture_compression_bptc },
    { "GL_EXT_texture_compression_dxt1"_s, GCGLExtension::EXT_texture_compression_dxt1 },
    { "GL_EXT_texture_compression_rgtc"_s, GCGLExtension::EXT_texture_compression_rgtc },
    { "GL_EXT_texture_compression_s3tc_srgb"_s, GCGLExtension::EXT_texture_compression_s3tc_srgb },
    { "GL_EXT_texture_filter_anisotropic"_s, GCGLExtension::EXT_texture_filter_anisotropic },
    { "GL_EXT_texture_mirror_clamp_to_edge"_s, GCGLExtension::EXT_texture_mirror_clamp_to_edge },
    { "GL_EXT_texture_norm16"_s, GCGLExtension::EXT_texture_norm16 },
    { "GL_IMG_texture_compression_pvrtc"_s, GCGLExtension::IMG_texture_compression_pvrtc },
    { "GL_KHR_parallel_shader_compile"_s, GCGLExtension::KHR_parallel_shader_compile },
    { "GL_KHR_texture_compression_astc_hdr"_s, GCGLExtension::KHR_texture_compression_astc_hdr },
    { "GL_KHR_texture_compression_astc_ldr"_s, GCGLExtension::KHR_texture_compression_astc_ldr },
    { "GL_NV_shader_noperspective_interpolation"_s, GCGLExtension::NV_shader_noperspective_interpolation },
    { "GL_OES_compressed_ETC1_RGB8_texture"_s, GCGLExtension::OES_compressed_ETC1_RGB8_texture },
    { "GL_OES_depth_texture"_s, GCGLExtension::OES_depth_texture },
    { "GL_OES_draw_buffers_indexed"_s, GCGLExtension::OES_draw_buffers_indexed },
    { "GL_OES_element_index_uint"_s, GCGLExtension::OES_element_index_uint },
    { "GL_OES_fbo_render_mipmap"_s, GCGLExtension::OES_fbo_render_mipmap },
    { "GL_OES_packed_depth_stencil"_s, GCGLExtension::OES_packed_depth_stencil },
    { "GL_OES_sample_variables"_s, GCGLExtension::OES_sample_variables },
    { "GL_OES_shader_multisample_interpolation"_s, GCGLExtension::OES_shader_multisample_interpolation },
    { "GL_OES_standard_derivatives"_s, GCGLExtension::OES_standard_derivatives },
    { "GL_OES_texture_float"_s, GCGLExtension::OES_texture_float },
    { "GL_OES_texture_float_linear"_s, GCGLExtension::OES_texture_float_linear },
    { "GL_OES_texture_half_float"_s, GCGLExtension::OES_texture_half_float },
    { "GL_OES_texture_half_float_linear"_s, GCGLExtension::OES_texture_half_float_linear },
    { "GL_OES_vertex_array_object"_s, GCGLExtension::OES_vertex_array_object },
    { "GL_QCOM_render_shared_exponent"_s, GCGLExtension::QCOM_render_shared_exponent },
};
// Enums are expected to map the strings. Verify that the extensions are in ascending order, like the strings.
static_assert(std::is_sorted(std::begin(extensionsMapping), std::end(extensionsMapping), [](auto& a, auto b) {
    return a.second < b.second;
}));

static ASCIILiteral extensionName(GCGLExtension extension)
{
    size_t index = static_cast<size_t>(extension);
    std::span mapping { extensionsMapping };
    if (mapping.size() < index) {
        ASSERT_NOT_REACHED();
        return { };
    }
    auto [name, enumValue] = mapping[index];
    if (enumValue != extension) {
        // Verifies that the enums do not get disconnected from the string.
        ASSERT_NOT_REACHED();
        return { };
    }
    return name.literal;
}

static std::optional<GCGLExtension> extensionEnum(const CString& extension)
{
    static constexpr SortedArrayMap map { extensionsMapping };
    if (auto* result = map.tryGet(extension.span()))
        return *result;
    return std::nullopt;
}

GraphicsContextGLANGLE::GraphicsContextGLANGLE(GraphicsContextGLAttributes attributes)
    : GraphicsContextGL(attributes)
{
}

bool GraphicsContextGLANGLE::initialize()
{
    if (contextAttributes().failContextCreationForTesting == GraphicsContextGLAttributes::SimulatedCreationFailure::FailPlatformContextCreation)
        return false;
    if (!platformInitializeContext())
        return false;

    {
        StringView extensionsString { unsafeSpan(byteCast<char>(GL_GetString(GL_EXTENSIONS))) };
        for (auto extension : extensionsString.split(' '))
            m_extensions.add(extension.span8());
    }
    {
        StringView extensionsString { unsafeSpan(byteCast<char>(GL_GetString(GL_REQUESTABLE_EXTENSIONS_ANGLE))) };
        for (auto extension : extensionsString.split(' '))
            m_allRequestableExtensions.add(extension.span8());
    }

    validateAttributes();
    auto attributes = contextAttributes(); // They may have changed during validation.

    if (m_isForWebGL2 && !enableExtensionsImpl({ "GL_EXT_occlusion_query_boolean"_s, "GL_ANGLE_framebuffer_multisample"_s }))
        return false;

    if (!platformInitializeExtensions())
        return false;

    if (m_isForWebGL2)
        GL_Enable(GraphicsContextGL::PRIMITIVE_RESTART_FIXED_INDEX);

    // Create the texture that will be used for the framebuffer.
    GLenum textureTarget = drawingBufferTextureTarget();

    GL_GenTextures(1, &m_texture);
    GL_BindTexture(textureTarget, m_texture);
    GL_TexParameteri(textureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL_TexParameteri(textureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_TexParameteri(textureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL_BindTexture(textureTarget, 0);

    GL_GenFramebuffers(1, &m_fbo);
    GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    m_state.boundDrawFBO = m_state.boundReadFBO = m_fbo;

    if (!attributes.antialias && (attributes.stencil || attributes.depth))
        GL_GenRenderbuffers(1, &m_depthStencilBuffer);

    // If necessary, create another framebuffer for the multisample results.
    if (attributes.antialias) {
        GL_GenFramebuffers(1, &m_multisampleFBO);
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        m_state.boundDrawFBO = m_state.boundReadFBO = m_multisampleFBO;
        GL_GenRenderbuffers(1, &m_multisampleColorBuffer);
        if (attributes.stencil || attributes.depth)
            GL_GenRenderbuffers(1, &m_multisampleDepthStencilBuffer);
    } else if (attributes.preserveDrawingBuffer) {
        // If necessary, create another texture to handle preserveDrawingBuffer:true without
        // antialiasing.
        GL_GenTextures(1, &m_preserveDrawingBufferTexture);
        GL_BindTexture(GL_TEXTURE_2D, m_preserveDrawingBufferTexture);
        GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        GL_TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GL_BindTexture(GL_TEXTURE_2D, 0);
        // Create an FBO with which to perform BlitFramebuffer from one texture to the other.
        GL_GenFramebuffers(1, &m_preserveDrawingBufferFBO);
    }

    GL_ClearColor(0, 0, 0, 0);

    // Compute initial platform-independent max internal framebuffer size.
    GCGLint maxTextureSize;
    GCGLint maxRenderbufferSize;
    std::array<GCGLint, 2> maxViewportDims { 0, 0 };
    GL_GetIntegerv(GraphicsContextGL::MAX_TEXTURE_SIZE, &maxTextureSize);
    GL_GetIntegerv(GraphicsContextGL::MAX_RENDERBUFFER_SIZE, &maxRenderbufferSize);
    GL_GetIntegerv(GraphicsContextGL::MAX_VIEWPORT_DIMS, maxViewportDims.data());
    m_maxInternalFramebufferSize = { maxViewportDims[0], maxViewportDims[1] };
    m_maxInternalFramebufferSize.clampToMinimumSize({ maxTextureSize, maxTextureSize });
    m_maxInternalFramebufferSize.clampToMinimumSize({ maxRenderbufferSize, maxRenderbufferSize });

    if (!platformInitialize())
        return false;

    // EGL resources are only ever released if we run in process mode where EGL is used on host app threads, e.g. WK1
    // mode.
    static bool tracksUsedDisplays = !(isInWebProcess() || isInGPUProcess());
    if (tracksUsedDisplays) {
        // TODO: Move to ~GraphicsContextGLANGLE() when the function is moved to this file.
        ASSERT(m_displayObj);
        usedDisplays().add(m_displayObj);
    }

    bool khrDebugIsSupported = enableExtensionsImpl({ "GL_KHR_debug"_s });
    ASSERT_UNUSED(khrDebugIsSupported, khrDebugIsSupported);
    GL_Enable(DEBUG_OUTPUT);
    GL_Enable(DEBUG_OUTPUT_SYNCHRONOUS);
    GL_DebugMessageControlKHR(DONT_CARE, DONT_CARE, DONT_CARE, 0, nullptr, 0);
    GL_DebugMessageControlKHR(DEBUG_SOURCE_API, DONT_CARE, DONT_CARE, 0, nullptr, 1);
    auto debugMessageCallback = [](GCGLenum, GCGLenum type, GCGLenum id, GCGLenum severity, GCGLsizei length, const GCGLchar* message, const void* context) {
        auto* gl = reinterpret_cast<const GraphicsContextGLANGLE*>(context);
        if (gl->m_client)
            gl->m_client->addDebugMessage(type, id, severity, CString { unsafeMakeSpan(message, length) });
    };
    GL_DebugMessageCallbackKHR(debugMessageCallback, this);

    bool packReverseRowOrderIsSupported = enableExtensionsImpl({ "GL_ANGLE_pack_reverse_row_order"_s });
#if PLATFORM(COCOA)
    ASSERT_UNUSED(packReverseRowOrderIsSupported, packReverseRowOrderIsSupported);
#else
    UNUSED_VARIABLE(packReverseRowOrderIsSupported);
#endif

    ASSERT(GL_GetError() == NO_ERROR);

    for (auto& extensionString : m_extensions) {
        if (auto extension = extensionEnum(extensionString))
            m_knownActiveExtensions.add(*extension);
    }
    for (auto& extensionString : m_allEnabledRequestableExtensions) {
        if (auto extension = extensionEnum(extensionString))
            m_knownActiveExtensions.add(*extension);
    }
    for (auto& extensionString : m_allRequestableExtensions) {
        if (auto extension = extensionEnum(extensionString))
            m_requestableExtensions.add(*extension);
    }
    return true;
}

bool GraphicsContextGLANGLE::platformInitializeExtensions()
{
    return true;
}

bool GraphicsContextGLANGLE::platformInitialize()
{
    return true;
}

GCGLenum GraphicsContextGLANGLE::drawingBufferTextureTarget()
{
    auto [textureTarget, _] = externalImageTextureBindingPoint();
    UNUSED_VARIABLE(_);
    return textureTarget;
}

std::tuple<GCGLenum, GCGLenum> GraphicsContextGLANGLE::drawingBufferTextureBindingPoint()
{
    return externalImageTextureBindingPoint();
}

GCGLint GraphicsContextGLANGLE::EGLDrawingBufferTextureTargetForDrawingTarget(GCGLenum drawingTarget)
{
    switch (drawingTarget) {
    case TEXTURE_2D:
        return EGL_TEXTURE_2D;
    case TEXTURE_RECTANGLE_ARB:
        return EGL_TEXTURE_RECTANGLE_ANGLE;
    }
    ASSERT_WITH_MESSAGE(false, "Invalid drawing target");
    return 0;
}

bool GraphicsContextGLANGLE::releaseThreadResources(ReleaseThreadResourceBehavior releaseBehavior)
{
    platformReleaseThreadResources();

    if (!platformIsANGLEAvailable())
        return false;

    // Unset the EGL current context, since the next access might be from another thread, and the
    // context cannot be current on multiple threads.
    if (releaseBehavior == ReleaseThreadResourceBehavior::ReleaseCurrentContext) {
        EGLDisplay display = EGL_GetCurrentDisplay();
        if (display == EGL_NO_DISPLAY)
            return true;
        // At the time of writing, ANGLE does not flush on MakeCurrent. Since we are
        // potentially switching threads, we should flush.
        // Note: Here we assume also that ANGLE has only one platform context -- otherwise
        // we would need to flush each EGL context that has been used.
        GL_Flush();
        return EGL_MakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    if (releaseBehavior == ReleaseThreadResourceBehavior::TerminateAndReleaseThreadResources) {
        EGLDisplay currentDisplay = EGL_GetCurrentDisplay();
        if (currentDisplay != EGL_NO_DISPLAY) {
            ASSERT_NOT_REACHED(); // All resources must have been destroyed.
            EGL_MakeCurrent(currentDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
        for (auto display : usedDisplays())
            EGL_Terminate(display);
        usedDisplays().clear();
    }
    // Called when we do not know if we will ever see another call from this thread again.
    // Unset the EGL current context by releasing whole EGL thread state.
    return EGL_ReleaseThread();
}

RefPtr<PixelBuffer> GraphicsContextGLANGLE::readPixelsForPaintResults()
{
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, DestinationColorSpace::SRGB() };
    auto pixelBuffer = ByteArrayPixelBuffer::tryCreate(format, getInternalFramebufferSize());
    if (!pixelBuffer)
        return nullptr;
    ScopedBufferBinding scopedPixelPackBufferReset(GL_PIXEL_PACK_BUFFER, 0, m_isForWebGL2);
    setPackParameters(1, 0, false);
    GL_ReadnPixelsRobustANGLE(0, 0, pixelBuffer->size().width(), pixelBuffer->size().height(), GL_RGBA, GL_UNSIGNED_BYTE, pixelBuffer->bytes().size(), nullptr, nullptr, nullptr, pixelBuffer->bytes().data());
    // FIXME: Rendering to GL_RGB textures with a IOSurface bound to the texture image leaves
    // the alpha in the IOSurface in incorrect state. Also ANGLE GL_ReadPixels will in some
    // cases expose the non-255 values.
    // https://bugs.webkit.org/show_bug.cgi?id=215804
#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
    if (!contextAttributes().alpha)
        wipeAlphaChannelFromPixels(pixelBuffer->bytes());
#endif
    return pixelBuffer;
}

void GraphicsContextGLANGLE::validateAttributes()
{
    auto attrs = contextAttributes();
    m_internalColorFormat = attrs.alpha ? GL_RGBA8 : GL_RGB8;
    if (attrs.stencil && attrs.depth) {
        if (supportsExtensionImpl("GL_OES_packed_depth_stencil"_s))
            m_internalDepthStencilFormat = GL_DEPTH24_STENCIL8_OES;
        else {
            // Combined buffer not supported, prefer depth when both requested.
            if (supportsExtensionImpl("GL_OES_depth24"_s))
                m_internalDepthStencilFormat = GL_DEPTH_COMPONENT24_OES;
            else
                m_internalDepthStencilFormat = GL_DEPTH_COMPONENT16;
            attrs.stencil = false;
            setContextAttributes(attrs);
        }
    } else if (attrs.stencil)
        m_internalDepthStencilFormat = GL_STENCIL_INDEX8;
    else if (attrs.depth) {
        if (supportsExtensionImpl("GL_OES_depth24"_s))
            m_internalDepthStencilFormat = GL_DEPTH_COMPONENT24_OES;
        else
            m_internalDepthStencilFormat = GL_DEPTH_COMPONENT16;
    }

    if (attrs.antialias) {
        // FIXME: must adjust this when upgrading to WebGL 2.0 / OpenGL ES 3.0 support.
        if (!enableExtensionsImpl({ "GL_ANGLE_framebuffer_multisample"_s, "GL_ANGLE_framebuffer_blit"_s, "GL_OES_rgb8_rgba8"_s })) {
            attrs.antialias = false;
            setContextAttributes(attrs);
        }
    } else if (attrs.preserveDrawingBuffer) {
        // Needed for preserveDrawingBuffer:true support without antialiasing.
        bool supported = enableExtensionsImpl({ "GL_ANGLE_framebuffer_blit"_s });
        ASSERT_UNUSED(supported, supported);
    }
}

bool GraphicsContextGLANGLE::reshapeFBOs(const IntSize& size)
{
    auto attrs = contextAttributes();
    const int width = size.width();
    const int height = size.height();
    GLuint colorFormat = attrs.alpha ? GL_RGBA : GL_RGB;

    // Resize multisample FBO.
    if (attrs.antialias) {
        GLint maxSampleCount;
        GL_GetIntegerv(GL_MAX_SAMPLES_ANGLE, &maxSampleCount);
        // Using more than 4 samples is slow on some hardware and is unlikely to
        // produce a significantly better result.
        GLint sampleCount = std::min(4, maxSampleCount);
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        GL_BindRenderbuffer(GL_RENDERBUFFER, m_multisampleColorBuffer);
        GL_RenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, sampleCount, m_internalColorFormat, width, height);
        GL_FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_multisampleColorBuffer);
        if (attrs.stencil || attrs.depth) {
            ASSERT(m_internalDepthStencilFormat);
            ASSERT(!attrs.stencil || m_internalDepthStencilFormat == GL_STENCIL_INDEX8 || m_internalDepthStencilFormat == GL_DEPTH24_STENCIL8_OES);
            ASSERT(!attrs.depth || m_internalDepthStencilFormat != GL_STENCIL_INDEX8);
            GL_BindRenderbuffer(GL_RENDERBUFFER, m_multisampleDepthStencilBuffer);
            GL_RenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, sampleCount, m_internalDepthStencilFormat, width, height);
            // WebGL 1.0's rules state that combined depth/stencil renderbuffers
            // have to be attached to the synthetic DEPTH_STENCIL_ATTACHMENT point.
            if (attrs.stencil && attrs.depth)
                GL_FramebufferRenderbuffer(GL_FRAMEBUFFER, DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_multisampleDepthStencilBuffer);
            else if (attrs.stencil)
                GL_FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_multisampleDepthStencilBuffer);
            else
                GL_FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_multisampleDepthStencilBuffer);
        }
        GL_BindRenderbuffer(GL_RENDERBUFFER, 0);
        if (GL_CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            // FIXME: cleanup.
            notImplemented();
        }
    }

    // resize regular FBO
    GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    if (!reshapeDrawingBuffer()) {
        RELEASE_LOG(WebGL, "Fatal: Unable to allocate backing store of size %d x %d", width, height);
        forceContextLost();
        return true;
    }
    if (m_preserveDrawingBufferTexture) {
        // The context requires the use of an intermediate texture in order to implement
        // preserveDrawingBuffer:true without antialiasing.
        GLint texture2DBinding = 0;
        GL_GetIntegerv(GL_TEXTURE_BINDING_2D, &texture2DBinding);
        GL_BindTexture(GL_TEXTURE_2D, m_preserveDrawingBufferTexture);
        // Note that any pixel unpack buffer was unbound earlier, in reshape().
        GL_TexImage2D(GL_TEXTURE_2D, 0, colorFormat, width, height, 0, colorFormat, GL_UNSIGNED_BYTE, 0);
        // m_fbo is bound at this point.
        GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_preserveDrawingBufferTexture, 0);
        GL_BindTexture(GL_TEXTURE_2D, texture2DBinding);
        // Attach m_texture to m_preserveDrawingBufferFBO for later blitting.
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_preserveDrawingBufferFBO);
        GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, drawingBufferTextureTarget(), m_texture, 0);
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    } else
        GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, drawingBufferTextureTarget(), m_texture, 0);

    attachDepthAndStencilBufferIfNeeded(m_internalDepthStencilFormat, width, height);

    bool mustRestoreFBO = true;
    if (attrs.antialias) {
        GL_BindFramebuffer(GL_FRAMEBUFFER, m_multisampleFBO);
        if (m_state.boundDrawFBO == m_multisampleFBO && m_state.boundReadFBO == m_multisampleFBO)
            mustRestoreFBO = false;
    } else {
        if (m_state.boundDrawFBO == m_fbo && m_state.boundReadFBO == m_fbo)
            mustRestoreFBO = false;
    }

    return mustRestoreFBO;
}

void GraphicsContextGLANGLE::attachDepthAndStencilBufferIfNeeded(GLuint internalDepthStencilFormat, int width, int height)
{
    auto attrs = contextAttributes();

    if (!attrs.antialias && (attrs.stencil || attrs.depth)) {
        ASSERT(internalDepthStencilFormat);
        ASSERT(!attrs.stencil || m_internalDepthStencilFormat == GL_STENCIL_INDEX8 || m_internalDepthStencilFormat == GL_DEPTH24_STENCIL8_OES);
        ASSERT(!attrs.depth || internalDepthStencilFormat != GL_STENCIL_INDEX8);
        GL_BindRenderbuffer(GL_RENDERBUFFER, m_depthStencilBuffer);
        GL_RenderbufferStorage(GL_RENDERBUFFER, internalDepthStencilFormat, width, height);
        // WebGL 1.0's rules state that combined depth/stencil renderbuffers
        // have to be attached to the synthetic DEPTH_STENCIL_ATTACHMENT point.
        if (attrs.stencil && attrs.depth)
            GL_FramebufferRenderbuffer(GL_FRAMEBUFFER, DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
        else if (attrs.stencil)
            GL_FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
        else
            GL_FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depthStencilBuffer);
        GL_BindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    if (GL_CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // FIXME: cleanup
        notImplemented();
    }
}

void GraphicsContextGLANGLE::resolveMultisamplingIfNecessary(const IntRect& rect)
{
    prepareForDrawingBufferWrite();
    ScopedGLCapability scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
    ScopedGLCapability scopedDither(GL_DITHER, GL_FALSE);

    GLint boundFrameBuffer = 0;
    GLint boundReadFrameBuffer = 0;
    if (m_isForWebGL2) {
        GL_GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &boundFrameBuffer);
        GL_GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &boundReadFrameBuffer);
    } else
        GL_GetIntegerv(GL_FRAMEBUFFER_BINDING, &boundFrameBuffer);
    GL_BindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, m_multisampleFBO);
    GL_BindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, m_fbo);

    // FIXME: figure out more efficient solution for iOS.
    if (m_isForWebGL2) {
        // ES 3.0 has BlitFramebuffer.
        IntRect resolveRect = rect.isEmpty() ? IntRect { 0, 0, m_currentWidth, m_currentHeight } : rect;
        GL_BlitFramebuffer(resolveRect.x(), resolveRect.y(), resolveRect.maxX(), resolveRect.maxY(), resolveRect.x(), resolveRect.y(), resolveRect.maxX(), resolveRect.maxY(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    } else {
        // ES 2.0 has BlitFramebufferANGLE only.
        GL_BlitFramebufferANGLE(0, 0, m_currentWidth, m_currentHeight, 0, 0, m_currentWidth, m_currentHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    if (m_isForWebGL2) {
        GL_BindFramebuffer(GL_DRAW_FRAMEBUFFER, boundFrameBuffer);
        GL_BindFramebuffer(GL_READ_FRAMEBUFFER, boundReadFrameBuffer);
    } else
        GL_BindFramebuffer(GL_FRAMEBUFFER, boundFrameBuffer);
}

void GraphicsContextGLANGLE::renderbufferStorage(GCGLenum target, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    GL_RenderbufferStorage(target, internalformat, width, height);
}

void GraphicsContextGLANGLE::getIntegerv(GCGLenum pname, std::span<GCGLint> value)
{
    if (!makeContextCurrent())
        return;
    GL_GetIntegervRobustANGLE(pname, value.size(), nullptr, value.data());
}

void GraphicsContextGLANGLE::getIntegeri_v(GCGLenum pname, GCGLuint index, std::span<GCGLint, 4> value) // NOLINT
{
    if (!makeContextCurrent())
        return;
    GL_GetIntegeri_vRobustANGLE(pname, index, value.size(), nullptr, value.data());
}

void GraphicsContextGLANGLE::getShaderPrecisionFormat(GCGLenum shaderType, GCGLenum precisionType, std::span<GCGLint, 2> range, GCGLint* precision)
{
    if (!makeContextCurrent())
        return;

    GL_GetShaderPrecisionFormat(shaderType, precisionType, range.data(), precision);
}

void GraphicsContextGLANGLE::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, std::span<const uint8_t> pixels)
{
    if (!m_isForWebGL2)
        internalformat = adjustWebGL1TextureInternalFormat(internalformat, format, type);
    if (!makeContextCurrent())
        return;
    GL_TexImage2DRobustANGLE(target, level, internalformat, width, height, border, format, type, pixels.size(), pixels.data());
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::texImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLint border, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    if (!m_isForWebGL2)
        internalformat = adjustWebGL1TextureInternalFormat(internalformat, format, type);
    if (!makeContextCurrent())
        return;
    GL_TexImage2DRobustANGLE(target, level, internalformat, width, height, border, format, type, 0, reinterpret_cast<GLvoid*>(offset));
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoff, GCGLint yoff, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, std::span<const uint8_t> pixels)
{
    if (!makeContextCurrent())
        return;
    // FIXME: we will need to deal with PixelStore params when dealing with image buffers that differ from the subimage size.
    GL_TexSubImage2DRobustANGLE(target, level, xoff, yoff, width, height, format, type, pixels.size(), pixels.data());
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::texSubImage2D(GCGLenum target, GCGLint level, GCGLint xoff, GCGLint yoff, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;
    // FIXME: we will need to deal with PixelStore params when dealing with image buffers that differ from the subimage size.
    GL_TexSubImage2DRobustANGLE(target, level, xoff, yoff, width, height, format, type, 0, reinterpret_cast<GLvoid*>(offset));
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::compressedTexImage2D(GCGLenum target, int level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, int border, GCGLsizei imageSize, std::span<const uint8_t> data)
{
    if (!makeContextCurrent())
        return;
    GL_CompressedTexImage2DRobustANGLE(target, level, internalformat, width, height, border, imageSize, data.size(), data.data());
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::compressedTexImage2D(GCGLenum target, int level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, int border, GCGLsizei imageSize, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;
    GL_CompressedTexImage2DRobustANGLE(target, level, internalformat, width, height, border, imageSize, 0, reinterpret_cast<GLvoid*>(offset));
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::compressedTexSubImage2D(GCGLenum target, int level, int xoffset, int yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, std::span<const uint8_t> data)
{
    if (!makeContextCurrent())
        return;
    GL_CompressedTexSubImage2DRobustANGLE(target, level, xoffset, yoffset, width, height, format, imageSize, data.size(), data.data());
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::compressedTexSubImage2D(GCGLenum target, int level, int xoffset, int yoffset, GCGLsizei width, GCGLsizei height, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;
    GL_CompressedTexSubImage2DRobustANGLE(target, level, xoffset, yoffset, width, height, format, imageSize, 0, reinterpret_cast<GLvoid*>(offset));
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::depthRange(GCGLclampf zNear, GCGLclampf zFar)
{
    if (!makeContextCurrent())
        return;

    GL_DepthRangef(static_cast<float>(zNear), static_cast<float>(zFar));
}

void GraphicsContextGLANGLE::clearDepth(GCGLclampf depth)
{
    if (!makeContextCurrent())
        return;

    GL_ClearDepthf(static_cast<float>(depth));
}

void GraphicsContextGLANGLE::readPixels(IntRect rect, GCGLenum format, GCGLenum type, std::span<uint8_t> data, GCGLint alignment, GCGLint rowLength, GCGLboolean reverseRowOrder)
{
    if (!makeContextCurrent())
        return;
    ScopedBufferBinding scopedPixelPackBufferReset(GL_PIXEL_PACK_BUFFER, 0, m_isForWebGL2);
    setPackParameters(alignment, rowLength, reverseRowOrder);
    readPixelsImpl(rect, format, type, data);
}

std::optional<IntSize> GraphicsContextGLANGLE::readPixelsWithStatus(IntRect rect, GCGLenum format, GCGLenum type, GCGLboolean reverseRowOrder, std::span<uint8_t> data)
{
    if (!makeContextCurrent())
        return std::nullopt;
    ScopedBufferBinding scopedPixelPackBufferReset(GL_PIXEL_PACK_BUFFER, 0, m_isForWebGL2);
    setPackParameters(1, 0, reverseRowOrder); // Used for tight packing read only.
    return readPixelsImpl(rect, format, type, data);
}

void GraphicsContextGLANGLE::readPixelsBufferObject(IntRect rect, GCGLenum format, GCGLenum type, GCGLintptr offset, GCGLint alignment, GCGLint rowLength)
{
    if (!makeContextCurrent())
        return;
    setPackParameters(alignment, rowLength, false);
    std::span<uint8_t> data(reinterpret_cast<uint8_t*>(offset), 0);
    readPixelsImpl(rect, format, type, data);
}

std::optional<IntSize> GraphicsContextGLANGLE::readPixelsImpl(IntRect rect, GCGLenum format, GCGLenum type, std::span<uint8_t> data)
{
    // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
    // all previous rendering calls should be done before reading pixels.
    GL_Flush();
    auto attrs = contextAttributes();
    GCGLenum framebufferTarget = m_isForWebGL2 ? GraphicsContextGL::READ_FRAMEBUFFER : GraphicsContextGL::FRAMEBUFFER;
    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(rect);
        GL_BindFramebuffer(framebufferTarget, m_fbo);
        GL_Flush();
    }
    updateErrors();
    GLsizei rows = 0;
    GLsizei columns = 0;
    GL_ReadnPixelsRobustANGLE(rect.x(), rect.y(), rect.width(), rect.height(), format, type, data.size(), nullptr, &rows, &columns, data.data());
    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO)
        GL_BindFramebuffer(framebufferTarget, m_multisampleFBO);

    if (updateErrors()) {
        // ANGLE detected a failure during the ReadnPixelsRobustANGLE operation. Skip the alpha channel fixup below.
        return std::nullopt;
    }

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
    if (!data.empty() && !attrs.alpha && (format == GraphicsContextGL::RGBA || format == GraphicsContextGL::BGRA) && (type == GraphicsContextGL::UNSIGNED_BYTE) && (m_state.boundReadFBO == m_fbo || (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO)))
        wipeAlphaChannelFromPixels(data);
#endif
    return IntSize { rows, columns };
}

void GraphicsContextGLANGLE::prepareTexture()
{
    if (contextAttributes().antialias)
        resolveMultisamplingIfNecessary();

    if (m_preserveDrawingBufferTexture) {
        prepareForDrawingBufferWrite();
        // Blit m_preserveDrawingBufferTexture into m_texture.
        ScopedGLCapability scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
        ScopedGLCapability scopedDither(GL_DITHER, GL_FALSE);
        GL_BindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, m_preserveDrawingBufferFBO);
        GL_BindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, m_fbo);
        GL_BlitFramebufferANGLE(0, 0, m_currentWidth, m_currentHeight, 0, 0, m_currentWidth, m_currentHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        if (m_isForWebGL2) {
            GL_BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_state.boundDrawFBO);
            GL_BindFramebuffer(GL_READ_FRAMEBUFFER, m_state.boundReadFBO);
        } else
            GL_BindFramebuffer(GL_FRAMEBUFFER, m_state.boundDrawFBO);
    }
}

RefPtr<PixelBuffer> GraphicsContextGLANGLE::readRenderingResults()
{
    ScopedRestoreReadFramebufferBinding fboBinding(m_isForWebGL2, m_state.boundReadFBO);
    if (contextAttributes().antialias) {
        resolveMultisamplingIfNecessary();
        fboBinding.markBindingChanged();
    }
    fboBinding.bindFramebuffer(m_fbo);
    return readPixelsForPaintResults();
}

void GraphicsContextGLANGLE::reshape(int width, int height)
{
    if (width == m_currentWidth && height == m_currentHeight)
        return;

    ASSERT(width >= 0 && height >= 0);
    if (width < 0 || height < 0)
        return;

    if (width > m_maxInternalFramebufferSize.width() || height > m_maxInternalFramebufferSize.height())
        return;

    if (!makeContextCurrent())
        return;

    // FIXME: these may call makeContextCurrent again, we need to do this before changing the size.
    updateErrors();
    validateAttributes();

    m_currentWidth = width;
    m_currentHeight = height;

    ScopedGLCapability scopedScissor(GL_SCISSOR_TEST, GL_FALSE);
    ScopedGLCapability scopedDither(GL_DITHER, GL_FALSE);
    ScopedBufferBinding scopedPixelUnpackBufferReset(GL_PIXEL_UNPACK_BUFFER, 0, m_isForWebGL2);

    bool mustRestoreFBO = reshapeFBOs(IntSize(width, height));
    auto attrs = contextAttributes();

    // Initialize renderbuffers to 0.
    std::array<GLfloat, 4> clearColor { 0, 0, 0, 0 };
    GLfloat clearDepth = 0;
    GLint clearStencil = 0;
    std::array<GLboolean, 4> colorMask { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
    GLboolean depthMask = GL_TRUE;
    GLuint stencilMask = 0xffffffff, stencilMaskBack = 0xffffffff;
    GLbitfield clearMask = GL_COLOR_BUFFER_BIT;
    GL_GetFloatv(GL_COLOR_CLEAR_VALUE, clearColor.data());
    GL_ClearColor(0, 0, 0, 0);
    GL_GetBooleanv(GL_COLOR_WRITEMASK, colorMask.data());
    GL_ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (attrs.depth) {
        GL_GetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
        GL_ClearDepthf(1.0f);
        GL_GetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        GL_DepthMask(GL_TRUE);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }

    if (attrs.stencil) {
        GL_GetIntegerv(GL_STENCIL_CLEAR_VALUE, &clearStencil);
        GL_ClearStencil(0);
        GL_GetIntegerv(GL_STENCIL_WRITEMASK, reinterpret_cast<GLint*>(&stencilMask));
        GL_GetIntegerv(GL_STENCIL_BACK_WRITEMASK, reinterpret_cast<GLint*>(&stencilMaskBack));
        GL_StencilMaskSeparate(GL_FRONT, 0xffffffff);
        GL_StencilMaskSeparate(GL_BACK, 0xffffffff);
        clearMask |= GL_STENCIL_BUFFER_BIT;
    }

    GL_Clear(clearMask);

    GL_ClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    GL_ColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    if (attrs.depth) {
        GL_ClearDepthf(clearDepth);
        GL_DepthMask(depthMask);
    }
    if (attrs.stencil) {
        GL_ClearStencil(clearStencil);
        GL_StencilMaskSeparate(GL_FRONT, stencilMask);
        GL_StencilMaskSeparate(GL_BACK, stencilMaskBack);
    }

    if (mustRestoreFBO) {
        GL_BindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_state.boundDrawFBO);
        if (m_isForWebGL2 && m_state.boundDrawFBO != m_state.boundReadFBO)
            GL_BindFramebuffer(GraphicsContextGL::READ_FRAMEBUFFER, m_state.boundReadFBO);
    }

    auto error = GL_GetError();
    if (error != GL_NO_ERROR) {
        RELEASE_LOG(WebGL, "Fatal: OpenGL error during GraphicsContextGL buffer initialization (%d).", error);
        forceContextLost();
        return;
    }

    GL_Flush();
}

void GraphicsContextGLANGLE::activeTexture(GCGLenum texture)
{
    if (!makeContextCurrent())
        return;

    m_state.activeTextureUnit = texture;
    GL_ActiveTexture(texture);
}

void GraphicsContextGLANGLE::attachShader(PlatformGLObject program, PlatformGLObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    if (!makeContextCurrent())
        return;

    GL_AttachShader(program, shader);
}

void GraphicsContextGLANGLE::bindAttribLocation(PlatformGLObject program, GCGLuint index, const CString& name)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return;

    GL_BindAttribLocation(program, index, name.data());
}

void GraphicsContextGLANGLE::bindBuffer(GCGLenum target, PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    GL_BindBuffer(target, buffer);
}

void GraphicsContextGLANGLE::bindFramebuffer(GCGLenum target, PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    GLuint fbo;
    if (buffer)
        fbo = buffer;
    else
        fbo = (contextAttributes().antialias ? m_multisampleFBO : m_fbo);

    GL_BindFramebuffer(target, fbo);
    if (target == GL_FRAMEBUFFER) {
        m_state.boundReadFBO = m_state.boundDrawFBO = fbo;
    } else if (target == GL_READ_FRAMEBUFFER) {
        m_state.boundReadFBO = fbo;
    } else if (target == GL_DRAW_FRAMEBUFFER) {
        m_state.boundDrawFBO = fbo;
    }
}

void GraphicsContextGLANGLE::bindRenderbuffer(GCGLenum target, PlatformGLObject renderbuffer)
{
    if (!makeContextCurrent())
        return;

    GL_BindRenderbuffer(target, renderbuffer);
}


void GraphicsContextGLANGLE::bindTexture(GCGLenum target, PlatformGLObject texture)
{
    if (!makeContextCurrent())
        return;

    m_state.setBoundTexture(m_state.activeTextureUnit, texture, target);
    GL_BindTexture(target, texture);
}

void GraphicsContextGLANGLE::blendColor(GCGLclampf red, GCGLclampf green, GCGLclampf blue, GCGLclampf alpha)
{
    if (!makeContextCurrent())
        return;

    GL_BlendColor(red, green, blue, alpha);
}

void GraphicsContextGLANGLE::blendEquation(GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    GL_BlendEquation(mode);
}

void GraphicsContextGLANGLE::blendEquationSeparate(GCGLenum modeRGB, GCGLenum modeAlpha)
{
    if (!makeContextCurrent())
        return;

    GL_BlendEquationSeparate(modeRGB, modeAlpha);
}


void GraphicsContextGLANGLE::blendFunc(GCGLenum sfactor, GCGLenum dfactor)
{
    if (!makeContextCurrent())
        return;

    GL_BlendFunc(sfactor, dfactor);
}

void GraphicsContextGLANGLE::blendFuncSeparate(GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha)
{
    if (!makeContextCurrent())
        return;

    GL_BlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContextGLANGLE::bufferData(GCGLenum target, GCGLsizeiptr size, GCGLenum usage)
{
    if (!makeContextCurrent())
        return;

    GL_BufferData(target, size, 0, usage);
}

void GraphicsContextGLANGLE::bufferData(GCGLenum target, std::span<const uint8_t> data, GCGLenum usage)
{
    if (!makeContextCurrent())
        return;

    GL_BufferData(target, data.size(), data.data(), usage);
}

void GraphicsContextGLANGLE::bufferSubData(GCGLenum target, GCGLintptr offset, std::span<const uint8_t> data)
{
    if (!makeContextCurrent())
        return;

    GL_BufferSubData(target, offset, data.size(), data.data());
}

bool GraphicsContextGLANGLE::getBufferSubDataImpl(GCGLenum target, GCGLintptr offset, std::span<uint8_t> data)
{
    auto span = glMapBufferRangeSpan(target, offset, data.size(), GraphicsContextGL::MAP_READ_BIT);
    if (!span.data())
        return false;

    memcpySpan(data, span);

    if (!GL_UnmapBuffer(target))
        addError(GCGLErrorCode::InvalidOperation);

    return true;
}

void GraphicsContextGLANGLE::getBufferSubData(GCGLenum target, GCGLintptr offset, std::span<uint8_t> data)
{
    if (!makeContextCurrent())
        return;

    getBufferSubDataImpl(target, offset, data);
}

bool GraphicsContextGLANGLE::getBufferSubDataWithStatus(GCGLenum target, GCGLintptr offset, std::span<uint8_t> data)
{
    if (!makeContextCurrent())
        return false;

    return getBufferSubDataImpl(target, offset, data);
}

void GraphicsContextGLANGLE::copyBufferSubData(GCGLenum readTarget, GCGLenum writeTarget, GCGLintptr readOffset, GCGLintptr writeOffset, GCGLsizeiptr size)
{
    if (!makeContextCurrent())
        return;

    GL_CopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}

void GraphicsContextGLANGLE::getInternalformativ(GCGLenum target, GCGLenum internalformat, GCGLenum pname, std::span<GCGLint> data)
{
    if (!makeContextCurrent())
        return;
    GL_GetInternalformativRobustANGLE(target, internalformat, pname, data.size(), nullptr, data.data());
}

void GraphicsContextGLANGLE::renderbufferStorageMultisample(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    GL_RenderbufferStorageMultisample(target, samples, internalformat, width, height);
}

void GraphicsContextGLANGLE::renderbufferStorageMultisampleANGLE(GCGLenum target, GCGLsizei samples, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    GL_RenderbufferStorageMultisampleANGLE(target, samples, internalformat, width, height);
}

void GraphicsContextGLANGLE::texStorage2D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    GL_TexStorage2D(target, levels, internalformat, width, height);
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::texStorage3D(GCGLenum target, GCGLsizei levels, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth)
{
    if (!makeContextCurrent())
        return;

    GL_TexStorage3D(target, levels, internalformat, width, height, depth);
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::texImage3D(GCGLenum target, int level, int internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, int border, GCGLenum format, GCGLenum type, std::span<const uint8_t> pixels)
{
    if (!makeContextCurrent())
        return;
    GL_TexImage3DRobustANGLE(target, level, internalformat, width, height, depth, border, format, type, pixels.size(), pixels.data());
}

void GraphicsContextGLANGLE::texImage3D(GCGLenum target, int level, int internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, int border, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;
    GL_TexImage3DRobustANGLE(target, level, internalformat, width, height, depth, border, format, type, 0, reinterpret_cast<GLvoid*>(offset));
}

void GraphicsContextGLANGLE::texSubImage3D(GCGLenum target, int level, int xoffset, int yoffset, int zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, std::span<const uint8_t> pixels)
{
    if (!makeContextCurrent())
        return;
    GL_TexSubImage3DRobustANGLE(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels.size(), pixels.data());
}

void GraphicsContextGLANGLE::texSubImage3D(GCGLenum target, int level, int xoffset, int yoffset, int zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLenum type, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;
    GL_TexSubImage3DRobustANGLE(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, 0, reinterpret_cast<GLvoid*>(offset));
}

void GraphicsContextGLANGLE::compressedTexImage3D(GCGLenum target, int level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, int border, GCGLsizei imageSize, std::span<const uint8_t> data)
{
    if (!makeContextCurrent())
        return;
    GL_CompressedTexImage3DRobustANGLE(target, level, internalformat, width, height, depth, border, imageSize, data.size(), data.data());
}

void GraphicsContextGLANGLE::compressedTexImage3D(GCGLenum target, int level, GCGLenum internalformat, GCGLsizei width, GCGLsizei height, GCGLsizei depth, int border, GCGLsizei imageSize, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;
    GL_CompressedTexImage3DRobustANGLE(target, level, internalformat, width, height, depth, border, imageSize, 0, reinterpret_cast<GLvoid*>(offset));
}

void GraphicsContextGLANGLE::compressedTexSubImage3D(GCGLenum target, int level, int xoffset, int yoffset, int zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLsizei imageSize, std::span<const uint8_t> data)
{
    if (!makeContextCurrent())
        return;
    GL_CompressedTexSubImage3DRobustANGLE(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data.size(), data.data());
}

void GraphicsContextGLANGLE::compressedTexSubImage3D(GCGLenum target, int level, int xoffset, int yoffset, int zoffset, GCGLsizei width, GCGLsizei height, GCGLsizei depth, GCGLenum format, GCGLsizei imageSize, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;
    GL_CompressedTexSubImage3DRobustANGLE(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, 0, reinterpret_cast<GLvoid*>(offset));
}

GCGLenum GraphicsContextGLANGLE::checkFramebufferStatus(GCGLenum target)
{
    if (!makeContextCurrent())
        return GL_INVALID_OPERATION;

    return GL_CheckFramebufferStatus(target);
}

void GraphicsContextGLANGLE::clearColor(GCGLclampf r, GCGLclampf g, GCGLclampf b, GCGLclampf a)
{
    if (!makeContextCurrent())
        return;

    GL_ClearColor(r, g, b, a);
}

void GraphicsContextGLANGLE::clear(GCGLbitfield mask)
{
    if (!makeContextCurrent())
        return;
    if (mask & COLOR_BUFFER_BIT)
        prepareForDrawingBufferWriteIfBound();

    GL_Clear(mask);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::clearStencil(GCGLint s)
{
    if (!makeContextCurrent())
        return;

    GL_ClearStencil(s);
}

void GraphicsContextGLANGLE::colorMask(GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha)
{
    if (!makeContextCurrent())
        return;

    GL_ColorMask(red, green, blue, alpha);
}

void GraphicsContextGLANGLE::compileShader(PlatformGLObject shader)
{
    ASSERT(shader);
    if (!makeContextCurrent())
        return;

#if !PLATFORM(WIN)
    // We need the ANGLE_texture_rectangle extension to support IOSurface
    // backbuffers, but we don't want it exposed to WebGL user shaders.
    // Temporarily disable it during shader compilation.
    GL_Disable(GL_TEXTURE_RECTANGLE_ANGLE);
#endif
    GL_CompileShader(shader);
#if !PLATFORM(WIN)
    GL_Enable(GL_TEXTURE_RECTANGLE_ANGLE);
#endif
}

void GraphicsContextGLANGLE::copyTexImage2D(GCGLenum target, GCGLint level, GCGLenum internalformat, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height, GCGLint border)
{
    if (!makeContextCurrent())
        return;

    auto attrs = contextAttributes();
    GCGLenum framebufferTarget = m_isForWebGL2 ? GraphicsContextGL::READ_FRAMEBUFFER : GraphicsContextGL::FRAMEBUFFER;

    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        GL_BindFramebuffer(framebufferTarget, m_fbo);
    }
    GL_CopyTexImage2D(target, level, internalformat, x, y, width, height, border);
    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO)
        GL_BindFramebuffer(framebufferTarget, m_multisampleFBO);
}

void GraphicsContextGLANGLE::copyTexSubImage2D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    auto attrs = contextAttributes();
    GCGLenum framebufferTarget = m_isForWebGL2 ? GraphicsContextGL::READ_FRAMEBUFFER : GraphicsContextGL::FRAMEBUFFER;

    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        GL_BindFramebuffer(framebufferTarget, m_fbo);
    }
    GL_CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO)
        GL_BindFramebuffer(framebufferTarget, m_multisampleFBO);
}

void GraphicsContextGLANGLE::cullFace(GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    GL_CullFace(mode);
}

void GraphicsContextGLANGLE::depthFunc(GCGLenum func)
{
    if (!makeContextCurrent())
        return;

    GL_DepthFunc(func);
}

void GraphicsContextGLANGLE::depthMask(GCGLboolean flag)
{
    if (!makeContextCurrent())
        return;

    GL_DepthMask(flag);
}

void GraphicsContextGLANGLE::detachShader(PlatformGLObject program, PlatformGLObject shader)
{
    ASSERT(program);
    ASSERT(shader);
    if (!makeContextCurrent())
        return;

    GL_DetachShader(program, shader);
}

void GraphicsContextGLANGLE::disable(GCGLenum cap)
{
    if (!makeContextCurrent())
        return;
    if (cap == PRIMITIVE_RESTART_FIXED_INDEX) {
        if (m_isForWebGL2)
            addError(GCGLErrorCode::InvalidOperation);
        return;
    }
    GL_Disable(cap);
}

void GraphicsContextGLANGLE::disableVertexAttribArray(GCGLuint index)
{
    if (!makeContextCurrent())
        return;

    GL_DisableVertexAttribArray(index);
}

void GraphicsContextGLANGLE::drawArrays(GCGLenum mode, GCGLint first, GCGLsizei count)
{
    if (!makeContextCurrent())
        return;
    prepareForDrawingBufferWriteIfBound();
    GL_DrawArrays(mode, first, count);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::drawElements(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;
    prepareForDrawingBufferWriteIfBound();
    GL_DrawElements(mode, count, type, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
    checkGPUStatus();
}

void GraphicsContextGLANGLE::enable(GCGLenum cap)
{
    if (!makeContextCurrent())
        return;
    if (cap == PRIMITIVE_RESTART_FIXED_INDEX) {
        if (!m_isForWebGL2)
            addError(GCGLErrorCode::InvalidOperation);
        return;
    }
    GL_Enable(cap);
}

void GraphicsContextGLANGLE::enableVertexAttribArray(GCGLuint index)
{
    if (!makeContextCurrent())
        return;

    GL_EnableVertexAttribArray(index);
}

void GraphicsContextGLANGLE::finish()
{
    if (!makeContextCurrent())
        return;

    GL_Finish();
}

void GraphicsContextGLANGLE::flush()
{
    if (!makeContextCurrent())
        return;

    GL_Flush();
}

void GraphicsContextGLANGLE::framebufferRenderbuffer(GCGLenum target, GCGLenum attachment, GCGLenum renderbuffertarget, PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    GL_FramebufferRenderbuffer(target, attachment, renderbuffertarget, buffer);
}

void GraphicsContextGLANGLE::framebufferTexture2D(GCGLenum target, GCGLenum attachment, GCGLenum textarget, PlatformGLObject texture, GCGLint level)
{
    if (!makeContextCurrent())
        return;

    GL_FramebufferTexture2D(target, attachment, textarget, texture, level);
    invalidateKnownTextureContent(m_state.currentBoundTexture());
}

void GraphicsContextGLANGLE::frontFace(GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    GL_FrontFace(mode);
}

void GraphicsContextGLANGLE::generateMipmap(GCGLenum target)
{
    if (!makeContextCurrent())
        return;

    GL_GenerateMipmap(target);
}

Vector<GCGLAttribActiveInfo> GraphicsContextGLANGLE::activeAttribs(PlatformGLObject program)
{
    if (!makeContextCurrent())
        return { };
    GLint maxLength = 0;
    GL_GetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxLength);
    if (!maxLength)
        return { };
    GLsizei activeAttribs = 0;
    GL_GetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &activeAttribs);
    std::string name;
    Vector<GCGLAttribActiveInfo> result;
    result.reserveCapacity(activeAttribs);
    for (GLsizei index = 0; index < activeAttribs; ++index) {
        name.resize(maxLength); // GL_ACTIVE_ATTRIBUTE_MAX_LENGTH includes nul termination.
        GLsizei length = 0;
        GLint size = 0;
        GLenum type = 0;
        GL_GetActiveAttrib(program, index, maxLength, &length, &size, &type, name.data());
        if (length < 1 || size != 1) {
            ASSERT_NOT_REACHED();
            return { };
        }
        name.resize(length);
        result.append(GCGLAttribActiveInfo { name.data(), type, GL_GetAttribLocation(program, name.data()) });
    }
    return result;
}

Vector<GCGLUniformActiveInfo> GraphicsContextGLANGLE::activeUniforms(PlatformGLObject program)
{
    if (!makeContextCurrent())
        return { };

    GLint maxLength = 0;
    GL_GetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);
    if (!maxLength)
        return { };
    GLsizei activeUniforms = 0;
    GL_GetProgramiv(program, GL_ACTIVE_UNIFORMS, &activeUniforms);
    std::string name;
    Vector<GCGLUniformActiveInfo> result;
    result.reserveCapacity(activeUniforms);
    for (GLuint index = 0; index < static_cast<GLuint>(activeUniforms); ++index) {
        name.resize(maxLength); // GL_ACTIVE_UNIFORM_MAX_LENGTH includes nul termination.
        GCGLUniformActiveInfo info;
        GLsizei length = 0;
        GLint size = 0;
        GL_GetActiveUniform(program, index, maxLength, &length, &size, &info.type, name.data());
        if (length < 1 || size < 1) {
            ASSERT_NOT_REACHED();
            return { };
        }
        name.resize(length);
        info.name = name;
        info.locations.append(GL_GetUniformLocation(program, name.data()));
        if (m_isForWebGL2) {
            GL_GetActiveUniformsiv(program, 1, &index, GL_UNIFORM_BLOCK_INDEX, &info.blockIndex);
            GL_GetActiveUniformsiv(program, 1, &index, GL_UNIFORM_OFFSET, &info.offset);
            GL_GetActiveUniformsiv(program, 1, &index, GL_UNIFORM_ARRAY_STRIDE, &info.arrayStride);
            GL_GetActiveUniformsiv(program, 1, &index, GL_UNIFORM_MATRIX_STRIDE, &info.matrixStride);
            GL_GetActiveUniformsiv(program, 1, &index, GL_UNIFORM_IS_ROW_MAJOR, &info.isRowMajor);
        }
        if (size > 1) {
            if (!name.ends_with("[0]")) {
                ASSERT_NOT_REACHED();
                return { };
            }
            name.resize(name.length() - 3);
            for (GLint arrayIndex = 1; arrayIndex < size; ++arrayIndex) {
                auto elementName = (std::ostringstream() << name << '[' << arrayIndex << ']').str();
                info.locations.append(GL_GetUniformLocation(program, elementName.data()));
            }
        }
        result.append(WTFMove(info));
    }
    return result;
}

bool GraphicsContextGLANGLE::updateErrors()
{
    if (!makeContextCurrent())
        return false;

    bool movedAnError = false;

    // Set an arbitrary limit of 100 here to avoid creating a hang if
    // a problem driver has a bug that causes it to never clear the error.
    // Otherwise, we would just loop until we got NO_ERROR.
    for (unsigned i = 0; i < 100; ++i) {
        GCGLenum error = GL_GetError();
        if (error == NO_ERROR)
            break;
        addError(enumToErrorCode(error));
        movedAnError = true;
    }

    return movedAnError;
}

GCGLErrorCodeSet GraphicsContextGLANGLE::getErrors()
{
    if (!makeContextCurrent())
        return GCGLErrorCode::InvalidOperation;

    updateErrors();
    return std::exchange(m_errors, { });
}

CString GraphicsContextGLANGLE::getString(GCGLenum name)
{
    if (!makeContextCurrent())
        return { };
    return CString { byteCast<char>(GL_GetString(name)) };
}

void GraphicsContextGLANGLE::hint(GCGLenum target, GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    GL_Hint(target, mode);
}

GCGLboolean GraphicsContextGLANGLE::isBuffer(PlatformGLObject buffer)
{
    if (!buffer)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsBuffer(buffer);
}

GCGLboolean GraphicsContextGLANGLE::isEnabled(GCGLenum cap)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsEnabled(cap);
}

GCGLboolean GraphicsContextGLANGLE::isFramebuffer(PlatformGLObject framebuffer)
{
    if (!framebuffer)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsFramebuffer(framebuffer);
}

GCGLboolean GraphicsContextGLANGLE::isProgram(PlatformGLObject program)
{
    if (!program)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsProgram(program);
}

GCGLboolean GraphicsContextGLANGLE::isRenderbuffer(PlatformGLObject renderbuffer)
{
    if (!renderbuffer)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsRenderbuffer(renderbuffer);
}

GCGLboolean GraphicsContextGLANGLE::isShader(PlatformGLObject shader)
{
    if (!shader)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsShader(shader);
}

GCGLboolean GraphicsContextGLANGLE::isTexture(PlatformGLObject texture)
{
    if (!texture)
        return GL_FALSE;

    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsTexture(texture);
}

void GraphicsContextGLANGLE::lineWidth(GCGLfloat width)
{
    if (!makeContextCurrent())
        return;

    GL_LineWidth(width);
}

void GraphicsContextGLANGLE::linkProgram(PlatformGLObject program)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return;

    GL_LinkProgram(program);
}

void GraphicsContextGLANGLE::pixelStorei(GCGLenum pname, GCGLint param)
{
    if (!makeContextCurrent())
        return;

    GL_PixelStorei(pname, param);
}

void GraphicsContextGLANGLE::polygonOffset(GCGLfloat factor, GCGLfloat units)
{
    if (!makeContextCurrent())
        return;

    GL_PolygonOffset(factor, units);
}

void GraphicsContextGLANGLE::sampleCoverage(GCGLclampf value, GCGLboolean invert)
{
    if (!makeContextCurrent())
        return;

    GL_SampleCoverage(value, invert);
}

void GraphicsContextGLANGLE::scissor(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    GL_Scissor(x, y, width, height);
}

void GraphicsContextGLANGLE::shaderSource(PlatformGLObject shader, const CString& source)
{
    ASSERT(shader);

    if (!makeContextCurrent())
        return;

    const char* sources = source.data();
    int lengths = source.length();
    GL_ShaderSource(shader, 1, &sources, &lengths);
}

void GraphicsContextGLANGLE::stencilFunc(GCGLenum func, GCGLint ref, GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    GL_StencilFunc(func, ref, mask);
}

void GraphicsContextGLANGLE::stencilFuncSeparate(GCGLenum face, GCGLenum func, GCGLint ref, GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    GL_StencilFuncSeparate(face, func, ref, mask);
}

void GraphicsContextGLANGLE::stencilMask(GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    GL_StencilMask(mask);
}

void GraphicsContextGLANGLE::stencilMaskSeparate(GCGLenum face, GCGLuint mask)
{
    if (!makeContextCurrent())
        return;

    GL_StencilMaskSeparate(face, mask);
}

void GraphicsContextGLANGLE::stencilOp(GCGLenum fail, GCGLenum zfail, GCGLenum zpass)
{
    if (!makeContextCurrent())
        return;

    GL_StencilOp(fail, zfail, zpass);
}

void GraphicsContextGLANGLE::stencilOpSeparate(GCGLenum face, GCGLenum fail, GCGLenum zfail, GCGLenum zpass)
{
    if (!makeContextCurrent())
        return;

    GL_StencilOpSeparate(face, fail, zfail, zpass);
}

void GraphicsContextGLANGLE::texParameterf(GCGLenum target, GCGLenum pname, GCGLfloat value)
{
    if (!makeContextCurrent())
        return;

    GL_TexParameterf(target, pname, value);
}

void GraphicsContextGLANGLE::texParameteri(GCGLenum target, GCGLenum pname, GCGLint value)
{
    if (!makeContextCurrent())
        return;

    GL_TexParameteri(target, pname, value);
}

void GraphicsContextGLANGLE::uniform1f(GCGLint location, GCGLfloat v0)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform1f(location, v0);
}

void GraphicsContextGLANGLE::uniform1fv(GCGLint location, std::span<const GCGLfloat> array)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform1fv(location, array.size(), array.data());
}

void GraphicsContextGLANGLE::uniform2f(GCGLint location, GCGLfloat v0, GCGLfloat v1)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform2f(location, v0, v1);
}

void GraphicsContextGLANGLE::uniform2fv(GCGLint location, std::span<const GCGLfloat> array)
{
    ASSERT(!(array.size() % 2));
    if (!makeContextCurrent())
        return;

    GL_Uniform2fv(location, array.size() / 2, array.data());
}

void GraphicsContextGLANGLE::uniform3f(GCGLint location, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform3f(location, v0, v1, v2);
}

void GraphicsContextGLANGLE::uniform3fv(GCGLint location, std::span<const GCGLfloat> array)
{
    ASSERT(!(array.size() % 3));
    if (!makeContextCurrent())
        return;

    GL_Uniform3fv(location, array.size() / 3, array.data());
}

void GraphicsContextGLANGLE::uniform4f(GCGLint location, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2, GCGLfloat v3)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform4f(location, v0, v1, v2, v3);
}

void GraphicsContextGLANGLE::uniform4fv(GCGLint location, std::span<const GCGLfloat> array)
{
    ASSERT(!(array.size() % 4));
    if (!makeContextCurrent())
        return;

    GL_Uniform4fv(location, array.size() / 4, array.data());
}

void GraphicsContextGLANGLE::uniform1i(GCGLint location, GCGLint v0)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform1i(location, v0);
}

void GraphicsContextGLANGLE::uniform1iv(GCGLint location, std::span<const GCGLint> array)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform1iv(location, array.size(), array.data());
}

void GraphicsContextGLANGLE::uniform2i(GCGLint location, GCGLint v0, GCGLint v1)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform2i(location, v0, v1);
}

void GraphicsContextGLANGLE::uniform2iv(GCGLint location, std::span<const GCGLint> array)
{
    ASSERT(!(array.size() % 2));
    if (!makeContextCurrent())
        return;

    GL_Uniform2iv(location, array.size() / 2, array.data());
}

void GraphicsContextGLANGLE::uniform3i(GCGLint location, GCGLint v0, GCGLint v1, GCGLint v2)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform3i(location, v0, v1, v2);
}

void GraphicsContextGLANGLE::uniform3iv(GCGLint location, std::span<const GCGLint> array)
{
    ASSERT(!(array.size() % 3));
    if (!makeContextCurrent())
        return;

    GL_Uniform3iv(location, array.size() / 3, array.data());
}

void GraphicsContextGLANGLE::uniform4i(GCGLint location, GCGLint v0, GCGLint v1, GCGLint v2, GCGLint v3)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform4i(location, v0, v1, v2, v3);
}

void GraphicsContextGLANGLE::uniform4iv(GCGLint location, std::span<const GCGLint> array)
{
    ASSERT(!(array.size() % 4));
    if (!makeContextCurrent())
        return;

    GL_Uniform4iv(location, array.size() / 4, array.data());
}

void GraphicsContextGLANGLE::uniformMatrix2fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> array)
{
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix2fv(location, array.size() / 4, transpose, array.data());
}

void GraphicsContextGLANGLE::uniformMatrix3fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> array)
{
    ASSERT(!(array.size() % 9));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix3fv(location, array.size() / 9, transpose, array.data());
}

void GraphicsContextGLANGLE::uniformMatrix4fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> array)
{
    ASSERT(!(array.size() % 16));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix4fv(location, array.size() / 16, transpose, array.data());
}

void GraphicsContextGLANGLE::useProgram(PlatformGLObject program)
{
    if (!makeContextCurrent())
        return;

    GL_UseProgram(program);
}

void GraphicsContextGLANGLE::validateProgram(PlatformGLObject program)
{
    ASSERT(program);

    if (!makeContextCurrent())
        return;

    GL_ValidateProgram(program);
}

void GraphicsContextGLANGLE::vertexAttrib1f(GCGLuint index, GCGLfloat v0)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib1f(index, v0);
}

void GraphicsContextGLANGLE::vertexAttrib1fv(GCGLuint index, std::span<const GCGLfloat, 1> array)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib1fv(index, array.data());
}

void GraphicsContextGLANGLE::vertexAttrib2f(GCGLuint index, GCGLfloat v0, GCGLfloat v1)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib2f(index, v0, v1);
}

void GraphicsContextGLANGLE::vertexAttrib2fv(GCGLuint index, std::span<const GCGLfloat, 2> array)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib2fv(index, array.data());
}

void GraphicsContextGLANGLE::vertexAttrib3f(GCGLuint index, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib3f(index, v0, v1, v2);
}

void GraphicsContextGLANGLE::vertexAttrib3fv(GCGLuint index, std::span<const GCGLfloat, 3> array)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib3fv(index, array.data());
}

void GraphicsContextGLANGLE::vertexAttrib4f(GCGLuint index, GCGLfloat v0, GCGLfloat v1, GCGLfloat v2, GCGLfloat v3)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib4f(index, v0, v1, v2, v3);
}

void GraphicsContextGLANGLE::vertexAttrib4fv(GCGLuint index, std::span<const GCGLfloat, 4> array)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttrib4fv(index, array.data());
}

void GraphicsContextGLANGLE::vertexAttribPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLboolean normalized, GCGLsizei stride, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttribPointer(index, size, type, normalized, stride, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContextGLANGLE::vertexAttribIPointer(GCGLuint index, GCGLint size, GCGLenum type, GCGLsizei stride, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttribIPointer(index, size, type, stride, reinterpret_cast<GLvoid*>(static_cast<intptr_t>(offset)));
}

void GraphicsContextGLANGLE::viewport(GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    GL_Viewport(x, y, width, height);
}

PlatformGLObject GraphicsContextGLANGLE::createVertexArray()
{
    if (!makeContextCurrent())
        return 0;

    GLuint array = 0;
    if (m_isForWebGL2)
        GL_GenVertexArrays(1, &array);
    else
        GL_GenVertexArraysOES(1, &array);
    return array;
}

void GraphicsContextGLANGLE::deleteVertexArray(PlatformGLObject array)
{
    if (!array)
        return;
    if (!makeContextCurrent())
        return;
    if (m_isForWebGL2)
        GL_DeleteVertexArrays(1, &array);
    else
        GL_DeleteVertexArraysOES(1, &array);
}

GCGLboolean GraphicsContextGLANGLE::isVertexArray(PlatformGLObject array)
{
    if (!array)
        return GL_FALSE;
    if (!makeContextCurrent())
        return GL_FALSE;

    if (m_isForWebGL2)
        return GL_IsVertexArray(array);
    return GL_IsVertexArrayOES(array);
}

void GraphicsContextGLANGLE::bindVertexArray(PlatformGLObject array)
{
    if (!makeContextCurrent())
        return;
    if (m_isForWebGL2)
        GL_BindVertexArray(array);
    else
        GL_BindVertexArrayOES(array);
}

void GraphicsContextGLANGLE::getBooleanv(GCGLenum pname, std::span<GCGLboolean> value)
{
    if (!makeContextCurrent())
        return;

    GL_GetBooleanvRobustANGLE(pname, value.size(), nullptr, value.data());
}

GCGLint GraphicsContextGLANGLE::getBufferParameteri(GCGLenum target, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetBufferParameterivRobustANGLE(target, pname, 1, nullptr, &value);
    return value;
}

void GraphicsContextGLANGLE::getFloatv(GCGLenum pname, std::span<GCGLfloat> value)
{
    if (!makeContextCurrent())
        return;

    GL_GetFloatvRobustANGLE(pname, value.size(), nullptr, value.data());
}

GCGLint64 GraphicsContextGLANGLE::getInteger64(GCGLenum pname)
{
    GCGLint64 value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetInteger64vRobustANGLE(pname, 1, nullptr, &value);
    return value;
}

GCGLint64 GraphicsContextGLANGLE::getInteger64i(GCGLenum pname, GCGLuint index)
{
    GCGLint64 value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetInteger64i_vRobustANGLE(pname, index, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLANGLE::getFramebufferAttachmentParameteri(GCGLenum target, GCGLenum attachment, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    if (attachment == DEPTH_STENCIL_ATTACHMENT)
        attachment = DEPTH_ATTACHMENT; // Or STENCIL_ATTACHMENT, either works.
    GL_GetFramebufferAttachmentParameterivRobustANGLE(target, attachment, pname, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLANGLE::getProgrami(PlatformGLObject program, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetProgramivRobustANGLE(program, pname, 1, nullptr, &value);
    return value;
}

CString GraphicsContextGLANGLE::getProgramInfoLog(PlatformGLObject program)
{
    if (!makeContextCurrent())
        return { };
    GLint maxLength = 0;
    GL_GetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
    if (!maxLength)
        return "";
    Vector<char, 64> buffer(maxLength); // GL_INFO_LOG_LENGTH includes nul termination.
    GLsizei length = 0;
    GL_GetProgramInfoLog(program, maxLength, &length, buffer.mutableSpan().data());
    ASSERT(length == maxLength - 1);
    return buffer.subspan(0, length);
}

GCGLint GraphicsContextGLANGLE::getRenderbufferParameteri(GCGLenum target, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetRenderbufferParameterivRobustANGLE(target, pname, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLANGLE::getShaderi(PlatformGLObject shader, GCGLenum pname)
{
    ASSERT(shader);
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetShaderivRobustANGLE(shader, pname, 1, nullptr, &value);
    return value;
}

CString GraphicsContextGLANGLE::getShaderInfoLog(PlatformGLObject shader)
{
    if (!makeContextCurrent())
        return { };
    GLint maxLength = 0;
    GL_GetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
    if (!maxLength)
        return "";
    Vector<char, 64> buffer(maxLength); // GL_INFO_LOG_LENGTH includes nul termination.
    GLsizei length = 0;
    GL_GetShaderInfoLog(shader, maxLength, &length, buffer.mutableSpan().data());
    ASSERT(length == maxLength - 1);
    return buffer.subspan(0, length);
}

GCGLfloat GraphicsContextGLANGLE::getTexParameterf(GCGLenum target, GCGLenum pname)
{
    GCGLfloat value = 0.f;
    if (!makeContextCurrent())
        return value;
    GL_GetTexParameterfvRobustANGLE(target, pname, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLANGLE::getTexParameteri(GCGLenum target, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetTexParameterivRobustANGLE(target, pname, 1, nullptr, &value);
    return value;
}

void GraphicsContextGLANGLE::getUniformfv(PlatformGLObject program, GCGLint location, std::span<GCGLfloat> value)
{
    if (!makeContextCurrent())
        return;
    // FIXME: Bug in ANGLE bufSize validation for uniforms. See https://bugs.webkit.org/show_bug.cgi?id=219069.
    auto bufSize = value.size() * sizeof(value[0]);
    GL_GetUniformfvRobustANGLE(program, location, bufSize, nullptr, value.data());
}

void GraphicsContextGLANGLE::getUniformiv(PlatformGLObject program, GCGLint location, std::span<GCGLint> value)
{
    if (!makeContextCurrent())
        return;
    // FIXME: Bug in ANGLE bufSize validation for uniforms. See https://bugs.webkit.org/show_bug.cgi?id=219069.
    auto bufSize = value.size() * sizeof(value[0]);
    GL_GetUniformivRobustANGLE(program, location, bufSize, nullptr, value.data());
}

void GraphicsContextGLANGLE::getUniformuiv(PlatformGLObject program, GCGLint location, std::span<GCGLuint> value)
{
    if (!makeContextCurrent())
        return;
    // FIXME: Bug in ANGLE bufSize validation for uniforms. See https://bugs.webkit.org/show_bug.cgi?id=219069.
    auto bufSize = value.size() * sizeof(value[0]);
    GL_GetUniformuivRobustANGLE(program, location, bufSize, nullptr, value.data());
}

GCGLsizeiptr GraphicsContextGLANGLE::getVertexAttribOffset(GCGLuint index, GCGLenum pname)
{
    if (!makeContextCurrent())
        return 0;

    GLvoid* pointer = 0;
    GL_GetVertexAttribPointervRobustANGLE(index, pname, 1, nullptr, &pointer);
    return static_cast<GCGLsizeiptr>(reinterpret_cast<intptr_t>(pointer));
}

PlatformGLObject GraphicsContextGLANGLE::createBuffer()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    GL_GenBuffers(1, &o);
    return o;
}

PlatformGLObject GraphicsContextGLANGLE::createFramebuffer()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    GL_GenFramebuffers(1, &o);
    return o;
}

PlatformGLObject GraphicsContextGLANGLE::createProgram()
{
    if (!makeContextCurrent())
        return 0;

    return GL_CreateProgram();
}

PlatformGLObject GraphicsContextGLANGLE::createRenderbuffer()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    GL_GenRenderbuffers(1, &o);
    return o;
}

PlatformGLObject GraphicsContextGLANGLE::createShader(GCGLenum type)
{
    if (!makeContextCurrent())
        return 0;

    return GL_CreateShader((type == FRAGMENT_SHADER) ? GL_FRAGMENT_SHADER : GL_VERTEX_SHADER);
}

PlatformGLObject GraphicsContextGLANGLE::createTexture()
{
    if (!makeContextCurrent())
        return 0;

    GLuint o = 0;
    GL_GenTextures(1, &o);
    invalidateKnownTextureContent(o);
    return o;
}

void GraphicsContextGLANGLE::deleteBuffer(PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteBuffers(1, &buffer);
}

void GraphicsContextGLANGLE::deleteFramebuffer(PlatformGLObject framebuffer)
{
    if (!makeContextCurrent())
        return;

    // Make sure the framebuffer is not going to be used for drawing
    // operations after it gets deleted.
    if (m_isForWebGL2) {
        if (framebuffer == m_state.boundDrawFBO)
            bindFramebuffer(DRAW_FRAMEBUFFER, 0);
        if (framebuffer == m_state.boundReadFBO)
            bindFramebuffer(READ_FRAMEBUFFER, 0);
    } else if (framebuffer == m_state.boundDrawFBO)
        bindFramebuffer(FRAMEBUFFER, 0);
    GL_DeleteFramebuffers(1, &framebuffer);
}

void GraphicsContextGLANGLE::deleteProgram(PlatformGLObject program)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteProgram(program);
}

void GraphicsContextGLANGLE::deleteRenderbuffer(PlatformGLObject renderbuffer)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteRenderbuffers(1, &renderbuffer);
}

void GraphicsContextGLANGLE::deleteShader(PlatformGLObject shader)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteShader(shader);
}

void GraphicsContextGLANGLE::deleteTexture(PlatformGLObject texture)
{
    if (!makeContextCurrent())
        return;

    m_state.boundTextureMap.removeIf([texture] (auto& keyValue) {
        return keyValue.value.first == texture;
    });
    GL_DeleteTextures(1, &texture);
    invalidateKnownTextureContent(texture);
}

void GraphicsContextGLANGLE::drawArraysInstanced(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei primcount)
{
    if (!makeContextCurrent())
        return;
    prepareForDrawingBufferWriteIfBound();
    if (m_isForWebGL2)
        GL_DrawArraysInstanced(mode, first, count, primcount);
    else
        GL_DrawArraysInstancedANGLE(mode, first, count, primcount);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::drawElementsInstanced(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset, GCGLsizei primcount)
{
    if (!makeContextCurrent())
        return;
    prepareForDrawingBufferWriteIfBound();
    if (m_isForWebGL2)
        GL_DrawElementsInstanced(mode, count, type, reinterpret_cast<void*>(offset), primcount);
    else
        GL_DrawElementsInstancedANGLE(mode, count, type, reinterpret_cast<void*>(offset), primcount);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::vertexAttribDivisor(GCGLuint index, GCGLuint divisor)
{
    if (!makeContextCurrent())
        return;

    if (m_isForWebGL2)
        GL_VertexAttribDivisor(index, divisor);
    else
        GL_VertexAttribDivisorANGLE(index, divisor);
}

GCGLuint GraphicsContextGLANGLE::getUniformBlockIndex(PlatformGLObject program, const CString& uniformBlockName)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return GL_INVALID_INDEX;

    return GL_GetUniformBlockIndex(program, uniformBlockName.data());
}

CString GraphicsContextGLANGLE::getActiveUniformBlockName(PlatformGLObject program, GCGLuint uniformBlockIndex)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return { };
    GLint maxLength = 0;
    GL_GetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH, &maxLength);
    // maxLength == 0 is ok, trying to access uniformBlockIndex in below call sets the consistent error.
    Vector<char, 64> buffer(maxLength); // GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH includes nul termination.
    GLsizei length = 0;
    GL_GetActiveUniformBlockName(program, uniformBlockIndex, maxLength, &length, buffer.mutableSpan().data());
    if (!length)
        return { };
    return buffer.subspan(0, length);
}

void GraphicsContextGLANGLE::uniformBlockBinding(PlatformGLObject program, GCGLuint uniformBlockIndex, GCGLuint uniformBlockBinding)
{
    ASSERT(program);
    if (!makeContextCurrent())
        return;

    GL_UniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
}

// Query Functions

PlatformGLObject GraphicsContextGLANGLE::createQuery()
{
    if (!makeContextCurrent())
        return 0;

    GLuint name = 0;
    GL_GenQueries(1, &name);
    return name;
}

void GraphicsContextGLANGLE::beginQuery(GCGLenum target, PlatformGLObject query)
{
    if (!makeContextCurrent())
        return;

    GL_BeginQuery(target, query);
}

void GraphicsContextGLANGLE::endQuery(GCGLenum target)
{
    if (!makeContextCurrent())
        return;

    GL_EndQuery(target);
}

GCGLuint GraphicsContextGLANGLE::getQueryObjectui(GCGLuint id, GCGLenum pname)
{
    GCGLuint value = 0;
    if (!makeContextCurrent())
        return value;
    GL_GetQueryObjectuivRobustANGLE(id, pname, 1, nullptr, &value);
    return value;
}

// Transform Feedback Functions

PlatformGLObject GraphicsContextGLANGLE::createTransformFeedback()
{
    if (!makeContextCurrent())
        return 0;

    GLuint name = 0;
    GL_GenTransformFeedbacks(1, &name);
    return name;
}

void GraphicsContextGLANGLE::deleteTransformFeedback(PlatformGLObject transformFeedback)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteTransformFeedbacks(1, &transformFeedback);
}

GCGLboolean GraphicsContextGLANGLE::isTransformFeedback(PlatformGLObject transformFeedback)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsTransformFeedback(transformFeedback);
}

void GraphicsContextGLANGLE::bindTransformFeedback(GCGLenum target, PlatformGLObject transformFeedback)
{
    if (!makeContextCurrent())
        return;

    GL_BindTransformFeedback(target, transformFeedback);
}

void GraphicsContextGLANGLE::beginTransformFeedback(GCGLenum primitiveMode)
{
    if (!makeContextCurrent())
        return;

    GL_BeginTransformFeedback(primitiveMode);
}

void GraphicsContextGLANGLE::endTransformFeedback()
{
    if (!makeContextCurrent())
        return;

    GL_EndTransformFeedback();
}

void GraphicsContextGLANGLE::transformFeedbackVaryings(PlatformGLObject program, const Vector<CString>& varyings, GCGLenum bufferMode)
{
    if (!makeContextCurrent())
        return;
    Vector<const char*> pointersToVaryings = varyings.map([](const CString& varying) {
        return varying.data();
    });
    GL_TransformFeedbackVaryings(program, pointersToVaryings.size(), pointersToVaryings.span().data(), bufferMode);
}

std::optional<GCGLTransformFeedbackActiveInfo> GraphicsContextGLANGLE::getTransformFeedbackVarying(PlatformGLObject program, GCGLuint index)
{
    if (!makeContextCurrent())
        return std::nullopt;
    GLsizei maxLength = 0;
    GL_GetProgramiv(program, GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH, &maxLength);
    // maxLength == 0 is ok, trying to access index in below call sets the consistent error.
    Vector<char, 64> buffer(maxLength); // GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH includes nul termination.
    GLsizei length = 0;
    GLsizei size = 0;
    GLenum type = 0;
    GL_GetTransformFeedbackVarying(program, index, maxLength, &length, &size, &type, buffer.mutableSpan().data());
    if (!length)
        return std::nullopt;
    return GCGLTransformFeedbackActiveInfo { buffer.subspan(0, length), type, size };
}

void GraphicsContextGLANGLE::bindBufferBase(GCGLenum target, GCGLuint index, PlatformGLObject buffer)
{
    if (!makeContextCurrent())
        return;

    GL_BindBufferBase(target, index, buffer);
}

void GraphicsContextGLANGLE::blitFramebuffer(GCGLint srcX0, GCGLint srcY0, GCGLint srcX1, GCGLint srcY1, GCGLint dstX0, GCGLint dstY0, GCGLint dstX1, GCGLint dstY1, GCGLbitfield mask, GCGLenum filter)
{
    if (!makeContextCurrent())
        return;
    prepareForDrawingBufferWriteIfBound();
    if (m_isForWebGL2)
        GL_BlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    else if (isExtensionEnabledImpl("GL_NV_framebuffer_blit"_s))
        GL_BlitFramebufferNV(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    else
        GL_BlitFramebufferANGLE(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::framebufferTextureLayer(GCGLenum target, GCGLenum attachment, PlatformGLObject texture, GCGLint level, GCGLint layer)
{
    if (!makeContextCurrent())
        return;

    GL_FramebufferTextureLayer(target, attachment, texture, level, layer);
}

void GraphicsContextGLANGLE::invalidateFramebuffer(GCGLenum target, std::span<const GCGLenum> attachments)
{
    if (!makeContextCurrent())
        return;
    prepareForDrawingBufferWriteIfBound();
    GL_InvalidateFramebuffer(target, attachments.size(), attachments.data());
}

void GraphicsContextGLANGLE::invalidateSubFramebuffer(GCGLenum target, std::span<const GCGLenum> attachments, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;
    prepareForDrawingBufferWriteIfBound();
    GL_InvalidateSubFramebuffer(target, attachments.size(), attachments.data(), x, y, width, height);
}

void GraphicsContextGLANGLE::readBuffer(GCGLenum src)
{
    if (!makeContextCurrent())
        return;

    GL_ReadBuffer(src);
}

void GraphicsContextGLANGLE::copyTexSubImage3D(GCGLenum target, GCGLint level, GCGLint xoffset, GCGLint yoffset, GCGLint zoffset, GCGLint x, GCGLint y, GCGLsizei width, GCGLsizei height)
{
    if (!makeContextCurrent())
        return;

    auto attrs = contextAttributes();
    GCGLenum framebufferTarget = m_isForWebGL2 ? GraphicsContextGL::READ_FRAMEBUFFER : GraphicsContextGL::FRAMEBUFFER;

    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        GL_BindFramebuffer(framebufferTarget, m_fbo);
    }
    GL_CopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
    if (attrs.antialias && m_state.boundReadFBO == m_multisampleFBO)
        GL_BindFramebuffer(framebufferTarget, m_multisampleFBO);
}

GCGLint GraphicsContextGLANGLE::getFragDataLocation(PlatformGLObject program, const CString& name)
{
    if (!makeContextCurrent())
        return -1;

    return GL_GetFragDataLocation(program, name.data());
}

void GraphicsContextGLANGLE::uniform1ui(GCGLint location, GCGLuint v0)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform1ui(location, v0);
}

void GraphicsContextGLANGLE::uniform2ui(GCGLint location, GCGLuint v0, GCGLuint v1)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform2ui(location, v0, v1);
}

void GraphicsContextGLANGLE::uniform3ui(GCGLint location, GCGLuint v0, GCGLuint v1, GCGLuint v2)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform3ui(location, v0, v1, v2);
}

void GraphicsContextGLANGLE::uniform4ui(GCGLint location, GCGLuint v0, GCGLuint v1, GCGLuint v2, GCGLuint v3)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform4ui(location, v0, v1, v2, v3);
}

void GraphicsContextGLANGLE::uniform1uiv(GCGLint location, std::span<const GCGLuint> data)
{
    if (!makeContextCurrent())
        return;

    GL_Uniform1uiv(location, data.size(), data.data());
}

void GraphicsContextGLANGLE::uniform2uiv(GCGLint location, std::span<const GCGLuint> data)
{
    ASSERT(!(data.size() % 2));
    if (!makeContextCurrent())
        return;

    GL_Uniform2uiv(location, data.size() / 2, data.data());
}

void GraphicsContextGLANGLE::uniform3uiv(GCGLint location, std::span<const GCGLuint> data)
{
    ASSERT(!(data.size() % 3));
    if (!makeContextCurrent())
        return;

    GL_Uniform3uiv(location, data.size() / 3, data.data());
}

void GraphicsContextGLANGLE::uniform4uiv(GCGLint location, std::span<const GCGLuint> data)
{
    ASSERT(!(data.size() % 4));
    if (!makeContextCurrent())
        return;

    GL_Uniform4uiv(location, data.size() / 4, data.data());
}

void GraphicsContextGLANGLE::uniformMatrix2x3fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> data)
{
    ASSERT(!(data.size() % 6));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix2x3fv(location, data.size() / 6, transpose, data.data());
}

void GraphicsContextGLANGLE::uniformMatrix3x2fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> data)
{
    ASSERT(!(data.size() % 6));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix3x2fv(location, data.size() / 6, transpose, data.data());
}

void GraphicsContextGLANGLE::uniformMatrix2x4fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> data)
{
    ASSERT(!(data.size() % 8));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix2x4fv(location, data.size() / 8, transpose, data.data());
}

void GraphicsContextGLANGLE::uniformMatrix4x2fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> data)
{
    ASSERT(!(data.size() % 8));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix4x2fv(location, data.size() / 8, transpose, data.data());
}

void GraphicsContextGLANGLE::uniformMatrix3x4fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> data)
{
    ASSERT(!(data.size() % 12));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix3x4fv(location, data.size() / 12, transpose, data.data());
}

void GraphicsContextGLANGLE::uniformMatrix4x3fv(GCGLint location, GCGLboolean transpose, std::span<const GCGLfloat> data)
{
    ASSERT(!(data.size() % 12));
    if (!makeContextCurrent())
        return;

    GL_UniformMatrix4x3fv(location, data.size() / 12, transpose, data.data());
}

void GraphicsContextGLANGLE::vertexAttribI4i(GCGLuint index, GCGLint x, GCGLint y, GCGLint z, GCGLint w)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttribI4i(index, x, y, z, w);
}

void GraphicsContextGLANGLE::vertexAttribI4iv(GCGLuint index, std::span<const GCGLint, 4> values)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttribI4iv(index, values.data());
}

void GraphicsContextGLANGLE::vertexAttribI4ui(GCGLuint index, GCGLuint x, GCGLuint y, GCGLuint z, GCGLuint w)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttribI4ui(index, x, y, z, w);
}

void GraphicsContextGLANGLE::vertexAttribI4uiv(GCGLuint index, std::span<const GCGLuint, 4> values)
{
    if (!makeContextCurrent())
        return;

    GL_VertexAttribI4uiv(index, values.data());
}

void GraphicsContextGLANGLE::drawRangeElements(GCGLenum mode, GCGLuint start, GCGLuint end, GCGLsizei count, GCGLenum type, GCGLintptr offset)
{
    if (!makeContextCurrent())
        return;
    prepareForDrawingBufferWriteIfBound();
    GL_DrawRangeElements(mode, start, end, count, type, reinterpret_cast<void*>(offset));
    checkGPUStatus();
}

void GraphicsContextGLANGLE::drawBuffers(std::span<const GCGLenum> bufs)
{
    if (!makeContextCurrent())
        return;
    GL_DrawBuffers(bufs.size(), bufs.data());
}

void GraphicsContextGLANGLE::clearBufferiv(GCGLenum buffer, GCGLint drawbuffer, std::span<const GCGLint> values)
{
    if (!makeContextCurrent())
        return;
    if (!validateClearBufferv(buffer, values.size()))
        return;
    prepareForDrawingBufferWriteIfBound();
    GL_ClearBufferiv(buffer, drawbuffer, values.data());
    checkGPUStatus();
}

void GraphicsContextGLANGLE::clearBufferuiv(GCGLenum buffer, GCGLint drawbuffer, std::span<const GCGLuint> values)
{
    if (!makeContextCurrent())
        return;
    if (!validateClearBufferv(buffer, values.size()))
        return;
    prepareForDrawingBufferWriteIfBound();
    GL_ClearBufferuiv(buffer, drawbuffer, values.data());
    checkGPUStatus();
}

void GraphicsContextGLANGLE::clearBufferfv(GCGLenum buffer, GCGLint drawbuffer, std::span<const GCGLfloat> values)
{
    if (!makeContextCurrent())
        return;
    if (!validateClearBufferv(buffer, values.size()))
        return;
    prepareForDrawingBufferWriteIfBound();
    GL_ClearBufferfv(buffer, drawbuffer, values.data());
    checkGPUStatus();
}

void GraphicsContextGLANGLE::clearBufferfi(GCGLenum buffer, GCGLint drawbuffer, GCGLfloat depth, GCGLint stencil)
{
    if (!makeContextCurrent())
        return;

    GL_ClearBufferfi(buffer, drawbuffer, depth, stencil);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::deleteQuery(PlatformGLObject query)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteQueries(1, &query);
}

GCGLboolean GraphicsContextGLANGLE::isQuery(PlatformGLObject query)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsQuery(query);
}

GCGLint GraphicsContextGLANGLE::getQuery(GCGLenum target, GCGLenum pname)
{
    if (!makeContextCurrent())
        return 0;

    GLint value = 0;
    GL_GetQueryiv(target, pname, &value);
    return value;
}

PlatformGLObject GraphicsContextGLANGLE::createSampler()
{
    if (!makeContextCurrent())
        return 0;

    GLuint name = 0;
    GL_GenSamplers(1, &name);
    return name;
}

void GraphicsContextGLANGLE::deleteSampler(PlatformGLObject sampler)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteSamplers(1, &sampler);
}

GCGLboolean GraphicsContextGLANGLE::isSampler(PlatformGLObject sampler)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsSampler(sampler);
}

void GraphicsContextGLANGLE::bindSampler(GCGLuint unit, PlatformGLObject sampler)
{
    if (!makeContextCurrent())
        return;

    GL_BindSampler(unit, sampler);
}

void GraphicsContextGLANGLE::samplerParameteri(PlatformGLObject sampler, GCGLenum pname, GCGLint param)
{
    if (!makeContextCurrent())
        return;

    GL_SamplerParameteri(sampler, pname, param);
}

void GraphicsContextGLANGLE::samplerParameterf(PlatformGLObject sampler, GCGLenum pname, GCGLfloat param)
{
    if (!makeContextCurrent())
        return;

    GL_SamplerParameterf(sampler, pname, param);
}

GCGLfloat GraphicsContextGLANGLE::getSamplerParameterf(PlatformGLObject sampler, GCGLenum pname)
{
    GCGLfloat value = 0.f;
    if (!makeContextCurrent())
        return value;

    GL_GetSamplerParameterfvRobustANGLE(sampler, pname, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLANGLE::getSamplerParameteri(PlatformGLObject sampler, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;

    GL_GetSamplerParameterivRobustANGLE(sampler, pname, 1, nullptr, &value);
    return value;
}

GCGLsync GraphicsContextGLANGLE::fenceSync(GCGLenum condition, GCGLbitfield flags)
{
    if (!makeContextCurrent())
        return 0;

    return GL_FenceSync(condition, flags);
}

GCGLboolean GraphicsContextGLANGLE::isSync(GCGLsync sync)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsSync(static_cast<GLsync>(sync));
}

void GraphicsContextGLANGLE::deleteSync(GCGLsync sync)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteSync(static_cast<GLsync>(sync));
}

GCGLenum GraphicsContextGLANGLE::clientWaitSync(GCGLsync sync, GCGLbitfield flags, GCGLuint64 timeout)
{
    if (!makeContextCurrent())
        return GL_WAIT_FAILED;

    return GL_ClientWaitSync(static_cast<GLsync>(sync), flags, timeout);
}

void GraphicsContextGLANGLE::waitSync(GCGLsync sync, GCGLbitfield flags, GCGLint64 timeout)
{
    if (!makeContextCurrent())
        return;

    GL_WaitSync(static_cast<GLsync>(sync), flags, timeout);
}

GCGLint GraphicsContextGLANGLE::getSynci(GCGLsync sync, GCGLenum pname)
{
    GCGLint value = 0;
    if (!makeContextCurrent())
        return value;

    GL_GetSynciv(static_cast<GLsync>(sync), pname, 1, nullptr, &value);
    return value;
}

void GraphicsContextGLANGLE::pauseTransformFeedback()
{
    if (!makeContextCurrent())
        return;

    GL_PauseTransformFeedback();
}

void GraphicsContextGLANGLE::resumeTransformFeedback()
{
    if (!makeContextCurrent())
        return;

    GL_ResumeTransformFeedback();
}

void GraphicsContextGLANGLE::bindBufferRange(GCGLenum target, GCGLuint index, PlatformGLObject buffer, GCGLintptr offset, GCGLsizeiptr size)
{
    if (!makeContextCurrent())
        return;

    GL_BindBufferRange(target, index, buffer, offset, size);
}

void GraphicsContextGLANGLE::getActiveUniformBlockiv(PlatformGLObject program, GCGLuint uniformBlockIndex, GCGLenum pname, std::span<GCGLint> params)
{
    if (!makeContextCurrent())
        return;
    GL_GetActiveUniformBlockivRobustANGLE(program, uniformBlockIndex, pname, params.size(), nullptr, params.data());
}

#if ENABLE(WEBXR)
GCGLExternalImage GraphicsContextGLANGLE::createExternalImage(ExternalImageSource&&, GCGLenum, GCGLint)
{
    notImplemented();
    return { };
}

void GraphicsContextGLANGLE::bindExternalImage(GCGLenum, GCGLExternalImage)
{
    notImplemented();
}

void GraphicsContextGLANGLE::deleteExternalImage(GCGLExternalImage image)
{
    if (!image) [[unlikely]]
        return;
    auto eglImage = m_eglImages.take(image);
    if (!eglImage) [[unlikely]] {
        addError(GCGLErrorCode::InvalidOperation);
        return;
    }
    bool result = EGL_DestroyImageKHR(platformDisplay(), eglImage);
    ASSERT(result);
    if (!result) [[unlikely]]
        addError(GCGLErrorCode::InvalidOperation);
}

GCGLExternalSync GraphicsContextGLANGLE::createExternalSync(ExternalSyncSource&&)
{
    notImplemented();
    return { };
}

void GraphicsContextGLANGLE::deleteExternalSync(GCGLExternalSync sync)
{
    if (!sync) [[unlikely]]
        return;
    EGLSync eglSync = m_eglSyncs.take(sync);
    if (!eglSync) [[unlikely]] {
        addError(GCGLErrorCode::InvalidOperation);
        return;
    }
    bool result = EGL_DestroySync(platformDisplay(), eglSync);
    ASSERT(result);
    if (!result) [[unlikely]]
        addError(GCGLErrorCode::InvalidOperation);
}

#endif

void GraphicsContextGLANGLE::multiDrawArraysANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLint, const GCGLsizei> firstsAndCounts)
{
    if (!makeContextCurrent())
        return;
    prepareForDrawingBufferWriteIfBound();
    GL_MultiDrawArraysANGLE(mode, firstsAndCounts.data<0>(), firstsAndCounts.data<1>(), firstsAndCounts.bufSize);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::multiDrawArraysInstancedANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLint, const GCGLsizei, const GCGLsizei> firstsCountsAndInstanceCounts)
{
    if (!makeContextCurrent())
        return;
    prepareForDrawingBufferWriteIfBound();
    GL_MultiDrawArraysInstancedANGLE(mode, firstsCountsAndInstanceCounts.data<0>(), firstsCountsAndInstanceCounts.data<1>(), firstsCountsAndInstanceCounts.data<2>(), firstsCountsAndInstanceCounts.bufSize);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::multiDrawElementsANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLsizei, const GCGLsizei> countsAndOffsets, GCGLenum type)
{
    if (!makeContextCurrent())
        return;

    prepareForDrawingBufferWriteIfBound();
    GL_MultiDrawElementsANGLE(mode, countsAndOffsets.data<0>(), type, asPointers(countsAndOffsets.span<1>()).span().data(), countsAndOffsets.bufSize);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::multiDrawElementsInstancedANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLsizei, const GCGLsizei, const GCGLsizei> countsOffsetsAndInstanceCounts, GCGLenum type)
{
    if (!makeContextCurrent())
        return;

    prepareForDrawingBufferWriteIfBound();
    GL_MultiDrawElementsInstancedANGLE(mode, countsOffsetsAndInstanceCounts.data<0>(), type, asPointers(countsOffsetsAndInstanceCounts.span<1>()).span().data(), countsOffsetsAndInstanceCounts.data<2>(), countsOffsetsAndInstanceCounts.bufSize);
    checkGPUStatus();
}

bool GraphicsContextGLANGLE::enableExtension(GCGLExtension extension)
{
    if (m_knownActiveExtensions.contains(extension))
        return true;
    if (!m_requestableExtensions.contains(extension))
        return false;
    auto name = extensionName(extension);
    if (name.isEmpty()) {
        ASSERT_NOT_REACHED();
        return false;
    }
    m_knownActiveExtensions.add(extension);
    if (!makeContextCurrent())
        return true;
    bool success = enableExtensionsImpl({ name });
    ASSERT_UNUSED(success, success);
    return true;
}

bool GraphicsContextGLANGLE::enableExtensionsImpl(std::initializer_list<ASCIILiteral> names)
{
    Vector<ASCIILiteral> requests;
    requests.reserveCapacity(names.size());
    for (auto name : names) {
        if (m_extensions.contains(name))
            continue;
        if (!m_allRequestableExtensions.contains(name))
            return false;
        if (m_allEnabledRequestableExtensions.contains(name))
            continue;
        requests.append(name);
    }
    for (auto name : requests) {
        GL_RequestExtensionANGLE(name.span().data());
        m_allEnabledRequestableExtensions.add(name);
    }
    return true;
}

bool GraphicsContextGLANGLE::supportsExtensionImpl(ASCIILiteral name) const
{
    return m_extensions.contains(name);
}

bool GraphicsContextGLANGLE::isExtensionEnabledImpl(ASCIILiteral name) const
{
    return m_extensions.contains(name) || m_allEnabledRequestableExtensions.contains(name);
}

CString GraphicsContextGLANGLE::getTranslatedShaderSourceANGLE(PlatformGLObject shader)
{
    if (!makeContextCurrent())
        return { };
    GLint maxLength = 0;
    GL_GetShaderivRobustANGLE(shader, GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE, 1, nullptr, &maxLength);
    if (!maxLength)
        return "";
    Vector<char, 64> buffer(maxLength); // GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE includes nul termination.
    GLsizei length = 0;
    GL_GetTranslatedShaderSourceANGLE(shader, maxLength, &length, buffer.mutableSpan().data());
    ASSERT(length == maxLength - 1);
    return buffer.subspan(0, length);
}

void GraphicsContextGLANGLE::drawBuffersEXT(std::span<const GCGLenum> bufs)
{
    if (!makeContextCurrent())
        return;

    GL_DrawBuffersEXT(bufs.size(), bufs.data());
}

PlatformGLObject GraphicsContextGLANGLE::createQueryEXT()
{
    if (!makeContextCurrent())
        return 0;

    GLuint name = 0;
    GL_GenQueriesEXT(1, &name);
    return name;
}

void GraphicsContextGLANGLE::deleteQueryEXT(PlatformGLObject query)
{
    if (!makeContextCurrent())
        return;

    GL_DeleteQueriesEXT(1, &query);
}

GCGLboolean GraphicsContextGLANGLE::isQueryEXT(PlatformGLObject query)
{
    if (!makeContextCurrent())
        return GL_FALSE;

    return GL_IsQueryEXT(query);
}

void GraphicsContextGLANGLE::beginQueryEXT(GCGLenum target, PlatformGLObject query)
{
    if (!makeContextCurrent())
        return;

    GL_BeginQueryEXT(target, query);
}

void GraphicsContextGLANGLE::endQueryEXT(GCGLenum target)
{
    if (!makeContextCurrent())
        return;

    GL_EndQueryEXT(target);
}

void GraphicsContextGLANGLE::queryCounterEXT(PlatformGLObject query, GCGLenum target)
{
    if (!makeContextCurrent())
        return;

    GL_QueryCounterEXT(query, target);
}

GCGLint GraphicsContextGLANGLE::getQueryiEXT(GCGLenum target, GCGLenum pname)
{
    if (!makeContextCurrent())
        return 0;

    GLint value = 0;
    GL_GetQueryivRobustANGLE(target, pname, 1, nullptr, &value);
    return value;
}

GCGLint GraphicsContextGLANGLE::getQueryObjectiEXT(PlatformGLObject query, GCGLenum pname)
{
    if (!makeContextCurrent())
        return 0;

    GLint value = 0;
    GL_GetQueryObjectivRobustANGLE(query, pname, 1, nullptr, &value);
    return value;
}

GCGLuint64 GraphicsContextGLANGLE::getQueryObjectui64EXT(PlatformGLObject query, GCGLenum pname)
{
    if (!makeContextCurrent())
        return 0;

    GLuint64 value = 0;
    GL_GetQueryObjectui64vRobustANGLE(query, pname, 1, nullptr, &value);
    return value;
}

GCGLint64 GraphicsContextGLANGLE::getInteger64EXT(GCGLenum pname)
{
    if (!makeContextCurrent())
        return 0;

    GCGLint64 value = 0;
    GL_GetInteger64vRobustANGLE(pname, 1, nullptr, &value);
    return value;
}

void GraphicsContextGLANGLE::enableiOES(GCGLenum target, GCGLuint index)
{
    if (!makeContextCurrent())
        return;

    GL_EnableiOES(target, index);
}

void GraphicsContextGLANGLE::disableiOES(GCGLenum target, GCGLuint index)
{
    if (!makeContextCurrent())
        return;

    GL_DisableiOES(target, index);
}

void GraphicsContextGLANGLE::blendEquationiOES(GCGLuint buf, GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    GL_BlendEquationiOES(buf, mode);
}

void GraphicsContextGLANGLE::blendEquationSeparateiOES(GCGLuint buf, GCGLenum modeRGB, GCGLenum modeAlpha)
{
    if (!makeContextCurrent())
        return;

    GL_BlendEquationSeparateiOES(buf, modeRGB, modeAlpha);
}

void GraphicsContextGLANGLE::blendFunciOES(GCGLuint buf, GCGLenum src, GCGLenum dst)
{
    if (!makeContextCurrent())
        return;

    GL_BlendFunciOES(buf, src, dst);
}

void GraphicsContextGLANGLE::blendFuncSeparateiOES(GCGLuint buf, GCGLenum srcRGB, GCGLenum dstRGB, GCGLenum srcAlpha, GCGLenum dstAlpha)
{
    if (!makeContextCurrent())
        return;

    GL_BlendFuncSeparateiOES(buf, srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void GraphicsContextGLANGLE::colorMaskiOES(GCGLuint buf, GCGLboolean red, GCGLboolean green, GCGLboolean blue, GCGLboolean alpha)
{
    if (!makeContextCurrent())
        return;

    GL_ColorMaskiOES(buf, red, green, blue, alpha);
}

void GraphicsContextGLANGLE::drawArraysInstancedBaseInstanceANGLE(GCGLenum mode, GCGLint first, GCGLsizei count, GCGLsizei instanceCount, GCGLuint baseInstance)
{
    if (!makeContextCurrent())
        return;
    prepareForDrawingBufferWriteIfBound();
    GL_DrawArraysInstancedBaseInstanceANGLE(mode, first, count, instanceCount, baseInstance);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::drawElementsInstancedBaseVertexBaseInstanceANGLE(GCGLenum mode, GCGLsizei count, GCGLenum type, GCGLintptr offset, GCGLsizei instanceCount, GCGLint baseVertex, GCGLuint baseInstance)
{
    if (!makeContextCurrent())
        return;
    prepareForDrawingBufferWriteIfBound();
    GL_DrawElementsInstancedBaseVertexBaseInstanceANGLE(mode, count, type, reinterpret_cast<void*>(offset), instanceCount, baseVertex, baseInstance);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::multiDrawArraysInstancedBaseInstanceANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLint, const GCGLsizei, const GCGLsizei, const GCGLuint> firstsCountsInstanceCountsAndBaseInstances)
{
    if (!makeContextCurrent())
        return;
    prepareForDrawingBufferWriteIfBound();
    GL_MultiDrawArraysInstancedBaseInstanceANGLE(mode, firstsCountsInstanceCountsAndBaseInstances.data<0>(), firstsCountsInstanceCountsAndBaseInstances.data<1>(), firstsCountsInstanceCountsAndBaseInstances.data<2>(), firstsCountsInstanceCountsAndBaseInstances.data<3>(), firstsCountsInstanceCountsAndBaseInstances.bufSize);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::multiDrawElementsInstancedBaseVertexBaseInstanceANGLE(GCGLenum mode, GCGLSpanTuple<const GCGLsizei, const GCGLsizei, const GCGLsizei, const GCGLint, const GCGLuint> countsOffsetsInstanceCountsBaseVerticesAndBaseInstances, GCGLenum type)
{
    if (!makeContextCurrent())
        return;

    prepareForDrawingBufferWriteIfBound();
    GL_MultiDrawElementsInstancedBaseVertexBaseInstanceANGLE(mode, countsOffsetsInstanceCountsBaseVerticesAndBaseInstances.data<0>(), type, asPointers(countsOffsetsInstanceCountsBaseVerticesAndBaseInstances.span<1>()).span().data(), countsOffsetsInstanceCountsBaseVerticesAndBaseInstances.data<2>(), countsOffsetsInstanceCountsBaseVerticesAndBaseInstances.data<3>(), countsOffsetsInstanceCountsBaseVerticesAndBaseInstances.data<4>(), countsOffsetsInstanceCountsBaseVerticesAndBaseInstances.bufSize);
    checkGPUStatus();
}

void GraphicsContextGLANGLE::clipControlEXT(GCGLenum origin, GCGLenum depth)
{
    if (!makeContextCurrent())
        return;

    GL_ClipControlEXT(origin, depth);
}

void GraphicsContextGLANGLE::provokingVertexANGLE(GCGLenum provokeMode)
{
    if (!makeContextCurrent())
        return;

    GL_ProvokingVertexANGLE(provokeMode);
}

void GraphicsContextGLANGLE::polygonModeANGLE(GCGLenum face, GCGLenum mode)
{
    if (!makeContextCurrent())
        return;

    GL_PolygonModeANGLE(face, mode);
}

void GraphicsContextGLANGLE::polygonOffsetClampEXT(GCGLfloat factor, GCGLfloat units, GCGLfloat clamp)
{
    if (!makeContextCurrent())
        return;

    GL_PolygonOffsetClampEXT(factor, units, clamp);
}

void GraphicsContextGLANGLE::simulateEventForTesting(SimulatedEventForTesting event)
{
    if (event == SimulatedEventForTesting::GPUStatusFailure || event == SimulatedEventForTesting::DisplayBufferAllocationFailure) {
        m_failNextStatusCheck = true;
        return;
    }
}

RefPtr<NativeImage> GraphicsContextGLANGLE::copyNativeImageYFlipped(SurfaceBuffer source)
{
    if (!makeContextCurrent())
        return nullptr;
    if (getInternalFramebufferSize().isEmpty())
        return nullptr;
    RefPtr<PixelBuffer> pixelBuffer;
    if (source == SurfaceBuffer::DrawingBuffer)
        pixelBuffer = readRenderingResults();
    else
        pixelBuffer = readCompositedResults();
    if (!pixelBuffer)
        return nullptr;
    return createNativeImageFromPixelBuffer(contextAttributes(), pixelBuffer.releaseNonNull());
}

RefPtr<PixelBuffer> GraphicsContextGLANGLE::drawingBufferToPixelBuffer(FlipY flipY)
{
    // Reading premultiplied alpha would involve unpremultiplying, which is lossy.
    if (contextAttributes().premultipliedAlpha)
        return nullptr;
    auto results = readRenderingResultsForPainting();
    if (flipY == FlipY::Yes && results && !results->size().isEmpty()) {
        ASSERT(results->format().pixelFormat == PixelFormat::RGBA8 || results->format().pixelFormat == PixelFormat::BGRA8);
        // FIXME: Make PixelBufferConversions support negative rowBytes and in-place conversions.
        size_t rowStride = results->size().width() * 4;
        auto rowBuffer = MallocSpan<uint8_t>::malloc(rowStride);
        for (auto bytes = results->bytes(); bytes.size() >= rowStride * 2; bytes = bytes.subspan(rowStride, bytes.size() - 2 * rowStride)) {
            auto top = bytes.first(rowStride);
            auto bottom = bytes.last(rowStride);
            memcpySpan(rowBuffer.mutableSpan(), bottom);
            memcpySpan(bottom, top);
            memcpySpan(top, rowBuffer.span());
        }
    }
    return results;
}

RefPtr<PixelBuffer> GraphicsContextGLANGLE::readRenderingResultsForPainting()
{
    if (!makeContextCurrent())
        return nullptr;
    if (getInternalFramebufferSize().isEmpty())
        return nullptr;
    return readRenderingResults();
}

void GraphicsContextGLANGLE::addError(GCGLErrorCode errorCode)
{
    m_errors.add(errorCode);
}

EnumSet<GCGLExtension> GraphicsContextGLANGLE::knownActiveExtensions() const
{
    return m_knownActiveExtensions;
}

EnumSet<GCGLExtension> GraphicsContextGLANGLE::requestableExtensions() const
{
    return m_requestableExtensions;
}

void GraphicsContextGLANGLE::invalidateKnownTextureContent(GCGLuint)
{
}

GCGLenum GraphicsContextGLANGLE::adjustWebGL1TextureInternalFormat(GCGLenum internalformat, GCGLenum format, GCGLenum type)
{
    // The implementation of WEBGL_color_buffer_float for WebGL 1.0 / ES 2.0 requires a sized
    // internal format. Adjust it if necessary at this lowest level.
    if (type == GL_FLOAT) {
        if (m_knownActiveExtensions.contains(GCGLExtension::CHROMIUM_color_buffer_float_rgba) && format == GL_RGBA && internalformat == GL_RGBA)
            return GL_RGBA32F;
        if (m_knownActiveExtensions.contains(GCGLExtension::CHROMIUM_color_buffer_float_rgb) && format == GL_RGB && internalformat == GL_RGB)
            return GL_RGB32F;
    }
    return internalformat;
}

void GraphicsContextGLANGLE::setPackParameters(GCGLint alignment, GCGLint rowLength, GCGLboolean reverseRowOrder)
{
    if (m_packAlignment != alignment) {
        GL_PixelStorei(GL_PACK_ALIGNMENT, alignment);
        m_packAlignment = alignment;
    }
    if (m_packRowLength != rowLength) {
        GL_PixelStorei(GL_PACK_ROW_LENGTH, rowLength);
        m_packRowLength = rowLength;
    }
    if (m_packReverseRowOrder != reverseRowOrder) {
        GL_PixelStorei(GL_PACK_REVERSE_ROW_ORDER_ANGLE, reverseRowOrder);
        m_packReverseRowOrder = reverseRowOrder;
    }
}

bool GraphicsContextGLANGLE::validateClearBufferv(GCGLenum buffer, size_t valuesSize)
{
    // ClearBuffersfv, iv, uiv are missing ANGLE Robust* entry-point. Make the call act similar way as other
    // calls that validate buffer sizes by validating it here.
    switch (buffer) {
    case GraphicsContextGL::COLOR:
        if (valuesSize == 4)
            return true;
        break;
    case GraphicsContextGL::DEPTH:
    case GraphicsContextGL::STENCIL:
        if (valuesSize == 1)
            return true;
        break;
    default:
        break;
    }
    addError(GCGLErrorCode::InvalidOperation);
    return false;
}

void GraphicsContextGLANGLE::prepareForDrawingBufferWriteIfBound()
{
    if (m_state.boundDrawFBO != m_fbo)
        return;
    prepareForDrawingBufferWrite();
}

void GraphicsContextGLANGLE::prepareForDrawingBufferWrite()
{
}

}

#endif // ENABLE(WEBGL)
