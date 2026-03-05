/*
 * Copyright (C) 2024, 2025, 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MemoryMappedGPUBuffer.h"

#if USE(GBM)
#include "DRMDeviceManager.h"
#include "GBMDevice.h"
#include "GBMVersioning.h"
#include "IntRect.h"
#include "Logging.h"
#include "PlatformDisplay.h"
#include "VivanteSuperTiledTextureInlines.h"
#include <epoxy/egl.h>
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <wtf/SafeStrerror.h>
#include <wtf/StdLibExtras.h>

#if USE(LIBDRM)
#include <drm_fourcc.h>
#include <xf86drm.h>
#endif

namespace WebCore {

MemoryMappedGPUBuffer::MemoryMappedGPUBuffer(const IntSize& size, OptionSet<BufferFlag> flags)
    : m_size(size)
    , m_flags(flags)
{
    ASSERT(m_flags.contains(BufferFlag::ForceLinear) || m_flags.contains(BufferFlag::ForceVivanteSuperTiled));
}

MemoryMappedGPUBuffer::~MemoryMappedGPUBuffer()
{
    unmapIfNeeded();
}

std::unique_ptr<MemoryMappedGPUBuffer> MemoryMappedGPUBuffer::create(const IntSize& size, OptionSet<BufferFlag> flags)
{
    auto& manager = WebCore::DRMDeviceManager::singleton();
    ASSERT(manager.isInitialized());

    auto gbmDevice = manager.mainGBMDevice(WebCore::DRMDeviceManager::NodeType::Render);
    if (!gbmDevice) {
        LOG_ERROR("MemoryMappedGPUBuffer::create(), failed to get GBM render device node");
        return nullptr;
    }

    bool preferBGRA = flags.contains(BufferFlag::UseBGRALayout);
    const auto preferredDMABufFormat = FourCC(preferBGRA ? DRM_FORMAT_ARGB8888 : DRM_FORMAT_ABGR8888);

    auto negotiateBufferFormat = [&]() -> std::optional<GLDisplay::BufferFormat> {
        const auto& supportedFormats = PlatformDisplay::sharedDisplay().bufferFormats();
        for (const auto& format : supportedFormats) {
            if (format.fourcc != preferredDMABufFormat)
                continue;

            if (flags.contains(BufferFlag::ForceLinear)) {
                if (format.modifiers.contains(DRM_FORMAT_MOD_LINEAR)) {
                    // If a linear buffer is requested - only allow a single modifier.
                    auto useFormat = format;
                    useFormat.modifiers = { DRM_FORMAT_MOD_LINEAR };
                    return useFormat;
                }
            } else if (flags.contains(BufferFlag::ForceVivanteSuperTiled)) {
                if (format.modifiers.contains(DRM_FORMAT_MOD_VIVANTE_SUPER_TILED)) {
                    // If a Vivante super-tiled buffer is requested - only allow a single modifier.
                    auto useFormat = format;
                    useFormat.modifiers = { DRM_FORMAT_MOD_VIVANTE_SUPER_TILED };
                    return useFormat;
                }
            } else
                return format;
        }

        return std::nullopt;
    };

    auto bufferFormat = negotiateBufferFormat();

    if (flags.contains(BufferFlag::ForceLinear) && (!bufferFormat.has_value() || !bufferFormat->modifiers.contains(DRM_FORMAT_MOD_LINEAR))) {
        WTFLogAlways("ERROR: ForceLinear flag set but DRM_FORMAT_MOD_LINEAR not supported by the negotiated buffer format. Aborting ..."); // NOLINT
        CRASH();
    }

    if (flags.contains(BufferFlag::ForceVivanteSuperTiled) && (!bufferFormat.has_value() || !bufferFormat->modifiers.contains(DRM_FORMAT_MOD_VIVANTE_SUPER_TILED))) {
        WTFLogAlways("ERROR: ForceVivanteSuperTiled flag set but DRM_FORMAT_MOD_VIVANTE_SUPER_TILED not supported by the negotiated buffer format. Aborting ..."); // NOLINT
        CRASH();
    }

    if (!bufferFormat.has_value()) {
        LOG_ERROR("MemoryMappedGPUBuffer::create(), failed to negotiate buffer format");
        return nullptr;
    }

    auto buffer = std::unique_ptr<MemoryMappedGPUBuffer>(new MemoryMappedGPUBuffer(size, flags));
    auto* bo = buffer->allocate(gbmDevice->device(), bufferFormat.value());
    if (!bo) {
        LOG_ERROR("MemoryMappedGPUBuffer::create(), failed to create GBM buffer of size %dx%d: %s", size.width(), size.height(), safeStrerror(errno).data());
        return nullptr;
    }

    if (!buffer->createDMABufFromGBMBufferObject(bo)) {
        LOG_ERROR("MemoryMappedGPUBuffer::create(), failed to create dma-buf from GBM buffer object");
        gbm_bo_destroy(bo);
        return nullptr;
    }

    gbm_bo_destroy(bo);
    return buffer;
}

struct gbm_bo* MemoryMappedGPUBuffer::allocate(struct gbm_device* device, const GLDisplay::BufferFormat& bufferFormat)
{
    auto allocateSize = m_size;
    if (m_flags.contains(BufferFlag::ForceVivanteSuperTiled))
        allocateSize = VivanteSuperTiledTexture::alignToSuperTileIntSize(m_size);

    struct gbm_bo* bo = nullptr;
    m_modifier = DRM_FORMAT_MOD_INVALID;
    if (!bufferFormat.modifiers.isEmpty())
        bo = gbm_bo_create_with_modifiers2(device, allocateSize.width(), allocateSize.height(), bufferFormat.fourcc.value, bufferFormat.modifiers.span().data(), bufferFormat.modifiers.size(), GBM_BO_USE_RENDERING);

    if (m_flags.contains(BufferFlag::ForceVivanteSuperTiled) && !bo) {
        WTFLogAlways("ERROR: ForceVivanteSuperTiled flag set but GBM couldn't allocate the buffer using gbm_bo_create_with_modifiers2. Aborting ..."); // NOLINT
        CRASH();
    }

    if (bo) {
        m_modifier = gbm_bo_get_modifier(bo);
    } else {
        bo = gbm_bo_create(device, m_size.width(), m_size.height(), bufferFormat.fourcc.value, GBM_BO_USE_LINEAR);
        m_modifier = DRM_FORMAT_MOD_INVALID;
    }

    if (!bo || gbm_bo_get_plane_count(bo) <= 0)
        return nullptr;

    m_allocatedSize = IntSize(gbm_bo_get_width(bo), gbm_bo_get_height(bo));
    return bo;
}

bool MemoryMappedGPUBuffer::isLinear() const
{
    return m_modifier == DRM_FORMAT_MOD_INVALID || m_modifier == DRM_FORMAT_MOD_LINEAR;
}

bool MemoryMappedGPUBuffer::isVivanteSuperTiled() const
{
    return m_modifier == DRM_FORMAT_MOD_VIVANTE_SUPER_TILED;
}

bool MemoryMappedGPUBuffer::createDMABufFromGBMBufferObject(struct gbm_bo* bo)
{
    Vector<UnixFileDescriptor> fds;
    Vector<uint32_t> offsets;
    Vector<uint32_t> strides;

    auto format = gbm_bo_get_format(bo);
    auto planeCount = gbm_bo_get_plane_count(bo);

    for (int i = 0; i < planeCount; ++i) {
        if (auto fd = exportGBMBufferObjectAsDMABuf(bo, i))
            fds.append(WTF::move(fd));
        else
            return false;
        offsets.append(gbm_bo_get_offset(bo, i));
        strides.append(gbm_bo_get_stride_for_plane(bo, i));
    }

    ASSERT(!m_dmaBuf);
    m_dmaBuf = DMABufBuffer::create(m_size, format, WTF::move(fds), WTF::move(offsets), WTF::move(strides), m_modifier);
    return true;
}

int MemoryMappedGPUBuffer::primaryPlaneDmaBufFD() const
{
    ASSERT(m_dmaBuf);

    auto& fds = m_dmaBuf->attributes().fds;
    ASSERT(!fds.isEmpty());

    auto fd = fds[0].value();
    ASSERT(fd >= 0);

    return fd;
}

uint32_t MemoryMappedGPUBuffer::primaryPlaneDmaBufStride() const
{
    ASSERT(m_dmaBuf);

    auto& strides = m_dmaBuf->attributes().strides;
    ASSERT(!strides.isEmpty());

    auto stride = strides[0];
    ASSERT(stride > 0);
    return stride;
}

bool MemoryMappedGPUBuffer::mapIfNeeded()
{
    if (isMapped())
        return true;

    ASSERT(isLinear() || isVivanteSuperTiled());
    m_mappedLength = primaryPlaneDmaBufStride() * m_allocatedSize.height();
    m_mappedData = mmap(nullptr, m_mappedLength, PROT_READ | PROT_WRITE, MAP_SHARED, primaryPlaneDmaBufFD(), 0);
    if (m_mappedData == MAP_FAILED) {
        m_mappedLength = 0;
        m_mappedData = nullptr;
        return false;
    }

    return true;
}

void MemoryMappedGPUBuffer::unmapIfNeeded()
{
    if (!isMapped())
        return;

    munmap(m_mappedData, m_mappedLength);
    m_mappedData = nullptr;
    m_mappedLength = 0;
}

EGLImage MemoryMappedGPUBuffer::createEGLImageFromDMABuf()
{
    ASSERT(m_dmaBuf);

    const auto& attributes = m_dmaBuf->attributes();
    auto planeCount = attributes.fds.size();

    Vector<EGLAttrib> eglAttributes {
        EGL_WIDTH, static_cast<EGLAttrib>(m_allocatedSize.width()),
        EGL_HEIGHT, static_cast<EGLAttrib>(m_allocatedSize.height()),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(attributes.fourcc)
    };

    static constexpr std::array planeAttributeNames = {
        std::array { EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT },
        std::array { EGL_DMA_BUF_PLANE1_FD_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT },
        std::array { EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT },
        std::array { EGL_DMA_BUF_PLANE3_FD_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT },
    };

    for (size_t i = 0; i < planeCount; ++i) {
        const auto& names = planeAttributeNames[i];
        std::array<EGLAttrib, 6> planeAttrs {
            names[0], static_cast<EGLAttrib>(attributes.fds[i].value()),
            names[1], static_cast<EGLAttrib>(attributes.offsets[i]),
            names[2], static_cast<EGLAttrib>(attributes.strides[i])
        };
        eglAttributes.append(std::span<const EGLAttrib> { planeAttrs });

        if (m_modifier != DRM_FORMAT_MOD_INVALID) {
            std::array<EGLAttrib, 4> modifierAttrs {
                names[3], static_cast<EGLAttrib>(m_modifier >> 32),
                names[4], static_cast<EGLAttrib>(m_modifier & 0xffffffff)
            };
            eglAttributes.append(std::span<const EGLAttrib> { modifierAttrs });
        }
    }

    eglAttributes.append(EGL_NONE);

    auto& display = WebCore::PlatformDisplay::sharedDisplay();
    auto eglImage = display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, eglAttributes);
    if (!eglImage)
        LOG_ERROR("MemoryMappedGPUBuffer::createEGLImageFromDMABuf(), failed to export GBM buffer as EGLImage");

    return eglImage;
}

UnixFileDescriptor MemoryMappedGPUBuffer::exportGBMBufferObjectAsDMABuf(struct gbm_bo* bo, unsigned planeIndex)
{
    auto handle = gbm_bo_get_handle_for_plane(bo, planeIndex);
    if (handle.s32 == -1) {
        LOG_ERROR("MemoryMappedGPUBuffer::exportGBMBufferObjectAsDMABuf(), failed to obtain gbm handle for plane %u", planeIndex);
        return { };
    }

    int fd = 0;
    int ret = drmPrimeHandleToFD(gbm_device_get_fd(gbm_bo_get_device(bo)), handle.u32, DRM_CLOEXEC | DRM_RDWR, &fd);
    if (ret < 0) {
        LOG_ERROR("MemoryMappedGPUBuffer::exportGBMBufferObjectAsDMABuf(), failed to export dma-buf for plane %u", planeIndex);
        return { };
    }

    return UnixFileDescriptor { fd, UnixFileDescriptor::Adopt };
}

void MemoryMappedGPUBuffer::updateContents(AccessScope& scope, const void* srcData, const IntRect& targetRect, unsigned bytesPerLine)
{
    ASSERT_UNUSED(scope, &scope.buffer() == this);
    ASSERT(scope.mode() == AccessScope::Mode::Write);
    ASSERT(isMapped());

    if (isLinear()) {
        updateContentsInLinearFormat(srcData, targetRect, bytesPerLine);
        return;
    }

    ASSERT(isVivanteSuperTiled());
    updateContentsInVivanteSuperTiledFormat(srcData, targetRect, bytesPerLine);
}

void MemoryMappedGPUBuffer::updateContentsInLinearFormat(const void* srcData, const IntRect& targetRect, unsigned bytesPerLine)
{
    const uint32_t dstPitch = primaryPlaneDmaBufStride() / 4;
    const size_t dstOffset = targetRect.y() * dstPitch + targetRect.x();
    const uint32_t srcPitch = bytesPerLine / 4;

    auto dstBufferSpan = unsafeMakeSpan<uint32_t>(static_cast<uint32_t*>(m_mappedData), m_mappedLength / sizeof(uint32_t));
    auto dstPixelSpan = dstBufferSpan.subspan(dstOffset);

    const auto srcPixelSpan = unsafeMakeSpan<const uint32_t>(static_cast<const uint32_t*>(srcData), targetRect.height() * srcPitch);

    if (srcPitch == dstPitch) {
        memcpySpan(dstPixelSpan, srcPixelSpan);
        return;
    }

    for (int32_t y = 0; y < targetRect.height(); ++y)
        memcpySpan(dstPixelSpan.subspan(y * dstPitch, dstPitch - targetRect.x()), srcPixelSpan.subspan(y * srcPitch, srcPitch));
}

void MemoryMappedGPUBuffer::updateContentsInVivanteSuperTiledFormat(const void* srcData, const IntRect& targetRect, unsigned bytesPerLine)
{
    auto dstBufferSpan = unsafeMakeSpan<uint32_t>(static_cast<uint32_t*>(m_mappedData), m_mappedLength / sizeof(uint32_t));

    const unsigned srcPitch = bytesPerLine / 4;
    const auto srcPixelSpan = unsafeMakeSpan<const uint32_t>(static_cast<const uint32_t*>(srcData), targetRect.height() * srcPitch);

    VivanteSuperTiledTexture texture(dstBufferSpan, primaryPlaneDmaBufStride());

    // Write line by line, accounting for source pitch which may differ from target width.
    unsigned dstX = targetRect.x();
    unsigned dstY = targetRect.y();
    unsigned width = targetRect.width();
    unsigned height = targetRect.height();

    for (unsigned y = 0; y < height; ++y)
        texture.writeLine(dstX, dstY + y, width, srcPixelSpan.subspan(y * srcPitch, srcPitch));
}

std::span<uint32_t> MemoryMappedGPUBuffer::mappedDataSpan(AccessScope& scope) const
{
    ASSERT_UNUSED(scope, &scope.buffer() == this);
    ASSERT(isMapped());
    ASSERT(isLinear() || isVivanteSuperTiled());
    return unsafeMakeSpan<uint32_t>(static_cast<uint32_t*>(m_mappedData), m_mappedLength / sizeof(uint32_t));
}

MemoryMappedGPUBuffer::AccessScope::AccessScope(MemoryMappedGPUBuffer& buffer, AccessScope::Mode mode)
    : m_buffer(buffer)
    , m_mode(mode)
{
    ASSERT(m_buffer.isMapped());
    m_buffer.performDMABufSyncSystemCall({ DMABufSyncFlag::Start, m_mode == AccessScope::Mode::Read ? DMABufSyncFlag::Read : DMABufSyncFlag::Write });
}

MemoryMappedGPUBuffer::AccessScope::~AccessScope()
{
    m_buffer.performDMABufSyncSystemCall({ DMABufSyncFlag::End, m_mode == AccessScope::Mode::Read ? DMABufSyncFlag::Read : DMABufSyncFlag::Write });
}

std::unique_ptr<MemoryMappedGPUBuffer::AccessScope> MemoryMappedGPUBuffer::AccessScope::create(MemoryMappedGPUBuffer& buffer, AccessScope::Mode mode)
{
    if (!buffer.mapIfNeeded())
        return nullptr;
    return std::unique_ptr<AccessScope>(new AccessScope(buffer, mode));
}

bool MemoryMappedGPUBuffer::performDMABufSyncSystemCall(OptionSet<DMABufSyncFlag> flags)
{
    constexpr unsigned maxRetries = 10;

    struct dma_buf_sync sync;
    zeroBytes(sync);

    auto mapFlag = [&](auto ourFlag, auto theirFlag) {
        if (flags.contains(ourFlag))
            sync.flags |= theirFlag;
    };

    mapFlag(DMABufSyncFlag::Start, DMA_BUF_SYNC_START);
    mapFlag(DMABufSyncFlag::End, DMA_BUF_SYNC_END);
    mapFlag(DMABufSyncFlag::Read, DMA_BUF_SYNC_READ);
    mapFlag(DMABufSyncFlag::Write, DMA_BUF_SYNC_WRITE);

    auto fd = primaryPlaneDmaBufFD();

    unsigned counter = 0;
    int result;
    do {
        result = ioctl(fd, DMA_BUF_IOCTL_SYNC, &sync);
    } while (result == -1 && (errno == EAGAIN || errno == EINTR) && (counter++) < maxRetries);

    if (result < 0) {
        LOG_ERROR("MemoryMappedGPUBuffer::performDMABufSyncSystemCall(), DMA_BUF_SYNC_IOCTL failed - may result in visual artifacts.");
        return false;
    }

    return true;
}

} // namespace WebCore

#endif // USE(GBM)
