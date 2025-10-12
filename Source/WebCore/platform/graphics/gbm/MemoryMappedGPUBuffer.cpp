/*
 * Copyright (C) 2024, 2025 Igalia S.L.
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
#include <epoxy/egl.h>
#include <fcntl.h>
#include <linux/dma-buf.h>
#include <sys/ioctl.h>
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
}

MemoryMappedGPUBuffer::~MemoryMappedGPUBuffer()
{
    unmapIfNeeded();

    if (m_bo)
        gbm_bo_destroy(m_bo);
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

    constexpr FourCC preferredDMABufFormat = DRM_FORMAT_ABGR8888;

    auto negotiateBufferFormat = [&]() -> std::optional<GLDisplay::BufferFormat> {
        const auto& supportedFormats = PlatformDisplay::sharedDisplay().bufferFormats();
        for (const auto& format : supportedFormats) {
            if (format.fourcc != preferredDMABufFormat)
                continue;

            if (!flags.contains(BufferFlag::ForceLinear))
                return format;

            if (format.modifiers.contains(DRM_FORMAT_MOD_LINEAR)) {
                // If a linear buffer is requested - only allow a single modifier.
                auto useFormat = format;
                useFormat.modifiers = { DRM_FORMAT_MOD_LINEAR };
                return useFormat;
            }
        }

        return std::nullopt;
    };

    auto bufferFormat = negotiateBufferFormat();

    if (flags.contains(BufferFlag::ForceLinear) && (!bufferFormat.has_value() || !bufferFormat->modifiers.contains(DRM_FORMAT_MOD_LINEAR))) {
        WTFLogAlways("ERROR: ForceLinear flag set but DRM_FORMAT_MOD_LINEAR not supported by the negotiated buffer format. Aborting ..."); // NOLINT
        CRASH();
    }

    if (!bufferFormat.has_value()) {
        LOG_ERROR("MemoryMappedGPUBuffer::create(), failed to negotiate buffer format");
        return nullptr;
    }

    auto buffer = std::unique_ptr<MemoryMappedGPUBuffer>(new MemoryMappedGPUBuffer(size, flags));
    if (!buffer->allocate(gbmDevice->device(), bufferFormat.value())) {
        LOG_ERROR("MemoryMappedGPUBuffer::create(), failed to create GBM buffer of size %dx%d: %s", size.width(), size.height(), safeStrerror(errno).data());
        return nullptr;
    }

    if (!buffer->createDMABufFromGBMBufferObject()) {
        LOG_ERROR("MemoryMappedGPUBuffer::create(), failed to create dma-buf from GBM buffer object");
        return nullptr;
    }

    return buffer;
}

bool MemoryMappedGPUBuffer::allocate(struct gbm_device* device, const GLDisplay::BufferFormat& bufferFormat)
{
    m_modifier = DRM_FORMAT_MOD_INVALID;
    if (!bufferFormat.modifiers.isEmpty())
        m_bo = gbm_bo_create_with_modifiers2(device, m_size.width(), m_size.height(), bufferFormat.fourcc.value, bufferFormat.modifiers.span().data(), bufferFormat.modifiers.size(), GBM_BO_USE_RENDERING);

    if (m_bo)
        m_modifier = gbm_bo_get_modifier(m_bo);
    else {
        m_bo = gbm_bo_create(device, m_size.width(), m_size.height(), bufferFormat.fourcc.value, GBM_BO_USE_LINEAR);
        m_modifier = DRM_FORMAT_MOD_INVALID;
    }

    if (!m_bo || gbm_bo_get_plane_count(m_bo) <= 0)
        return false;

    return true;
}

bool MemoryMappedGPUBuffer::isLinear() const
{
    ASSERT(m_bo);
    return gbm_bo_get_plane_count(m_bo) == 1 && (m_modifier == DRM_FORMAT_MOD_INVALID || m_modifier == DRM_FORMAT_MOD_LINEAR);
}

bool MemoryMappedGPUBuffer::createDMABufFromGBMBufferObject()
{
    ASSERT(m_eglAttributes.isEmpty());

    Vector<UnixFileDescriptor> fds;
    Vector<uint32_t> offsets;
    Vector<uint32_t> strides;

    auto format = gbm_bo_get_format(m_bo);

    m_eglAttributes = {
        EGL_WIDTH, static_cast<EGLAttrib>(gbm_bo_get_width(m_bo)),
        EGL_HEIGHT, static_cast<EGLAttrib>(gbm_bo_get_height(m_bo)),
        EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLAttrib>(format)
    };

#define ADD_PLANE_ATTRIBUTES(planeIndex) { \
    if (auto fd = exportGBMBufferObjectAsDMABuf(planeIndex)) \
        fds.append(WTFMove(fd)); \
    else \
        return false; \
    offsets.append(gbm_bo_get_offset(m_bo, planeIndex)); \
    strides.append(gbm_bo_get_stride_for_plane(m_bo, planeIndex)); \
    std::array<EGLint, 6> planeAttributes { \
        EGL_DMA_BUF_PLANE##planeIndex##_FD_EXT, static_cast<EGLint>(fds.last().value()), \
        EGL_DMA_BUF_PLANE##planeIndex##_OFFSET_EXT, static_cast<EGLint>(offsets.last()), \
        EGL_DMA_BUF_PLANE##planeIndex##_PITCH_EXT, static_cast<EGLint>(strides.last()) \
    }; \
    m_eglAttributes.append(std::span<const EGLint> { planeAttributes }); \
    if (m_modifier != DRM_FORMAT_MOD_INVALID) { \
        std::array<EGLint, 4> modifierAttributes { \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_HI_EXT, static_cast<EGLint>(m_modifier >> 32), \
            EGL_DMA_BUF_PLANE##planeIndex##_MODIFIER_LO_EXT, static_cast<EGLint>(m_modifier & 0xffffffff) \
        }; \
        m_eglAttributes.append(std::span<const EGLint> { modifierAttributes }); \
    } \
    }

    auto planeCount = gbm_bo_get_plane_count(m_bo);
    if (planeCount > 0)
        ADD_PLANE_ATTRIBUTES(0);
    if (planeCount > 1)
        ADD_PLANE_ATTRIBUTES(1);
    if (planeCount > 2)
        ADD_PLANE_ATTRIBUTES(2);
    if (planeCount > 3)
        ADD_PLANE_ATTRIBUTES(3);

#undef ADD_PLANE_ATTRIBS

    m_eglAttributes.append(EGL_NONE);

    ASSERT(!m_dmaBuf);
    m_dmaBuf = DMABufBuffer::create(m_size, format, WTFMove(fds), WTFMove(offsets), WTFMove(strides), m_modifier);
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

    ASSERT(isLinear());
    m_mappedLength = primaryPlaneDmaBufStride() * m_size.height();
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
    ASSERT(!m_eglAttributes.isEmpty());

    auto& display = WebCore::PlatformDisplay::sharedDisplay();
    auto eglImage = display.createEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, m_eglAttributes);
    if (!eglImage)
        LOG_ERROR("MemoryMappedGPUBuffer::createEGLImageFromDMABuf(), failed to export GBM buffer as EGLImage");

    return eglImage;
}

UnixFileDescriptor MemoryMappedGPUBuffer::exportGBMBufferObjectAsDMABuf(unsigned planeIndex)
{
    auto handle = gbm_bo_get_handle_for_plane(m_bo, planeIndex);
    if (handle.s32 == -1) {
        LOG_ERROR("MemoryMappedGPUBuffer::exportGBMBufferObjectAsDMABuf(), failed to obtain gbm handle for plane %u", planeIndex);
        return { };
    }

    int fd = 0;
    int ret = drmPrimeHandleToFD(gbm_device_get_fd(gbm_bo_get_device(m_bo)), handle.u32, DRM_CLOEXEC | DRM_RDWR, &fd);
    if (ret < 0) {
        LOG_ERROR("MemoryMappedGPUBuffer::exportGBMBufferObjectAsDMABuf(), failed to export dma-buf for plane %u", planeIndex);
        return { };
    }

    return UnixFileDescriptor { fd, UnixFileDescriptor::Adopt };
}

void MemoryMappedGPUBuffer::updateContents(AccessScope& scope, const MemoryMappedGPUBuffer& srcBuffer, const IntRect& targetRect)
{
    ASSERT_UNUSED(scope, &scope.buffer() == this);
    ASSERT(scope.mode() == AccessScope::Mode::Write);
    ASSERT(srcBuffer.isLinear());
    updateContents(scope, srcBuffer.m_mappedData, targetRect, srcBuffer.primaryPlaneDmaBufStride());
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
void MemoryMappedGPUBuffer::updateContents(AccessScope& scope, const void* srcData, const IntRect& targetRect, unsigned bytesPerLine)
{
    ASSERT_UNUSED(scope, &scope.buffer() == this);
    ASSERT(scope.mode() == AccessScope::Mode::Write);
    ASSERT(isMapped());
    ASSERT(isLinear());

    uint32_t* dstPixels = static_cast<uint32_t*>(m_mappedData);
    const uint32_t dstPitch = primaryPlaneDmaBufStride() / 4;
    const auto dstOffset = targetRect.y() * dstPitch + targetRect.x();

    const uint32_t* srcPixels = static_cast<const uint32_t*>(srcData);
    const uint32_t srcPitch = bytesPerLine / 4;

    const auto srcPixelSpan = unsafeMakeSpan<const uint32_t>(srcPixels, targetRect.height() * srcPitch);
    auto dstPixelSpan = unsafeMakeSpan<uint32_t>(dstPixels + dstOffset, m_size.height() * dstPitch - dstOffset);

    if (srcPitch == dstPitch) {
        memcpySpan(dstPixelSpan, srcPixelSpan);
        return;
    }

    for (int32_t y = 0; y < targetRect.height(); ++y)
        memcpySpan(dstPixelSpan.subspan(y * dstPitch, dstPitch - targetRect.x()), srcPixelSpan.subspan(y * srcPitch, srcPitch));
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

std::span<uint32_t> MemoryMappedGPUBuffer::mappedDataSpan(AccessScope& scope) const
{
    ASSERT_UNUSED(scope, &scope.buffer() == this);
    ASSERT(isMapped());
    ASSERT(isLinear());
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    return { static_cast<uint32_t*>(m_mappedData), primaryPlaneDmaBufStride() / 4 };
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
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
