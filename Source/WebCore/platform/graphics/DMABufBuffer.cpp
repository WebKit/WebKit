/*
 * Copyright (C) 2024 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DMABufBuffer.h"

#if USE(GBM)
#include "CoordinatedPlatformLayerBuffer.h"
#include "GBMVersioning.h"
#include "GLDisplay.h"
#include <atomic>
#include <drm_fourcc.h>

#if USE(LIBEPOXY)
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

namespace WebCore {

static uint64_t generateID()
{
    static std::atomic<uint64_t> id;
    return ++id;
}

DMABufBuffer::DMABufBuffer(Attributes&& attributes)
    : m_id(generateID())
    , m_attributes(WTF::move(attributes))
{
}

DMABufBuffer::DMABufBuffer(uint64_t id, Attributes&& attributes)
    : m_id(id)
    , m_attributes(WTF::move(attributes))
{
}

DMABufBuffer::~DMABufBuffer() = default;

void DMABufBuffer::setBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&& buffer)
{
    m_buffer = WTF::move(buffer);
}

std::optional<DMABufBuffer::Attributes> DMABufBuffer::takeAttributes()
{
    if (m_attributes.fds.isEmpty())
        return std::nullopt;

    return DMABufBuffer::Attributes { WTF::move(m_attributes.size), std::exchange(m_attributes.fourcc, 0), WTF::move(m_attributes.fds), WTF::move(m_attributes.offsets), WTF::move(m_attributes.strides), std::exchange(m_attributes.modifier, 0) };
}

std::optional<DMABufBufferAttributes> DMABufBufferAttributes::fromGBMBufferObject(struct gbm_bo* bo, EnableModifiers enableModifiers)
{
    if (!bo)
        return std::nullopt;

    int planeCount = gbm_bo_get_plane_count(bo);
    if (planeCount <= 0)
        return std::nullopt;

    DMABufBufferAttributes attributes;
    attributes.size = { static_cast<int>(gbm_bo_get_width(bo)), static_cast<int>(gbm_bo_get_height(bo)) };
    attributes.fourcc = gbm_bo_get_format(bo);
    attributes.modifier = enableModifiers == EnableModifiers::Yes ? gbm_bo_get_modifier(bo) : DRM_FORMAT_MOD_INVALID;

    for (int i = 0; i < planeCount; ++i) {
        int fd = gbm_bo_get_fd_for_plane(bo, i);
        if (fd < 0) {
            LOG_ERROR("DMABufBufferAttributes::fromGBMBufferObject(), failed to export dma-buf for plane %d", i);
            return std::nullopt;
        }
        attributes.fds.append(UnixFileDescriptor { fd, UnixFileDescriptor::Adopt });
        attributes.offsets.append(gbm_bo_get_offset(bo, i));
        attributes.strides.append(gbm_bo_get_stride_for_plane(bo, i));
    }

    return attributes;
}

EGLImage DMABufBuffer::createEGLImage(GLDisplay& display) const
{
    return createEGLImage(display, m_attributes);
}

static std::optional<Vector<EGLAttrib>> buildEGLAttributesForDMABuf(const DMABufBuffer::Attributes& dmaBufAttributes, DMABufBuffer::Attributes::EnableModifiers enableModifiers)
{
    auto planeCount = dmaBufAttributes.fds.size();
    if (!planeCount || planeCount > DMABufBuffer::Attributes::maxPlaneCountForEGLImage)
        return std::nullopt;

    bool hasModifiers = dmaBufAttributes.modifier != DRM_FORMAT_MOD_INVALID && enableModifiers == DMABufBuffer::Attributes::EnableModifiers::Yes;

    // 6 base attributes + per-plane (6 + optional 4 modifier) + EGL_NONE terminator.
    static constexpr unsigned baseAttributeCount = 6;
    static constexpr unsigned planeAttributeCount = 6;
    static constexpr unsigned modifierAttributeCount = 4;

    Vector<EGLAttrib> eglAttributes;
    eglAttributes.reserveInitialCapacity(baseAttributeCount + planeCount * (planeAttributeCount + (hasModifiers ? modifierAttributeCount : 0)) + 1);

    eglAttributes.appendList<EGLAttrib>({
        EGL_WIDTH, static_cast<EGLAttrib>(dmaBufAttributes.size.width()),
        EGL_HEIGHT, static_cast<EGLAttrib>(dmaBufAttributes.size.height()),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(dmaBufAttributes.fourcc.value)
    });

    static constexpr std::array planeAttributeNames = {
        std::array { EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT },
        std::array { EGL_DMA_BUF_PLANE1_FD_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT },
        std::array { EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT },
        std::array { EGL_DMA_BUF_PLANE3_FD_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT },
    };

    for (size_t i = 0; i < planeCount; ++i) {
        const auto& names = planeAttributeNames[i];
        eglAttributes.appendList<EGLAttrib>({
            names[0], static_cast<EGLAttrib>(dmaBufAttributes.fds[i].value()),
            names[1], static_cast<EGLAttrib>(dmaBufAttributes.offsets[i]),
            names[2], static_cast<EGLAttrib>(dmaBufAttributes.strides[i])
        });

        if (hasModifiers) {
            eglAttributes.appendList<EGLAttrib>({
                names[3], static_cast<EGLAttrib>(dmaBufAttributes.modifier >> 32),
                names[4], static_cast<EGLAttrib>(dmaBufAttributes.modifier & 0xffffffff)
            });
        }
    }

    eglAttributes.append(EGL_NONE);
    return eglAttributes;
}

EGLImage DMABufBuffer::createEGLImage(GLDisplay& display, const Attributes& dmaBufAttributes)
{
    auto enableModifiers = display.extensions().EXT_image_dma_buf_import_modifiers
        ? DMABufBuffer::Attributes::EnableModifiers::Yes : DMABufBuffer::Attributes::EnableModifiers::No;
    auto eglAttributes = buildEGLAttributesForDMABuf(dmaBufAttributes, enableModifiers);
    if (!eglAttributes)
        return EGL_NO_IMAGE;
    return display.createImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, *eglAttributes);
}

std::optional<Vector<EGLint>> DMABufBuffer::buildEGLImageAttributes(const Attributes& dmaBufAttributes, Attributes::EnableModifiers enableModifiers)
{
    auto eglAttributes = buildEGLAttributesForDMABuf(dmaBufAttributes, enableModifiers);
    if (!eglAttributes)
        return std::nullopt;
    return eglAttributes->map<Vector<EGLint>>([](EGLAttrib value) {
        return static_cast<EGLint>(value);
    });
}

} // namespace WebCore

#endif // USE(GBM)
