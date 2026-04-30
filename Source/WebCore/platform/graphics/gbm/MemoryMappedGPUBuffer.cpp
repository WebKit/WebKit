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
#include <unistd.h>
#include <wtf/SafeStrerror.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>

#if USE(LIBDRM)
#include <drm_fourcc.h>
#endif

namespace WebCore {

// How CPU-side access to the BO is performed. The GPU-sampling export is always
// gbm_bo_get_fd_for_plane() so Mesa attaches its implicit-sync fence; only the
// CPU-mapping side varies.
enum class CPUMappingStrategy : uint8_t {
    Unsupported,
    // mmap() the FD returned by gbm_bo_get_fd_for_plane(). Available on Mesa builds
    // where that FD is RDWR-capable.
    DmaBufFDMmap,
    // gbm_bo_map() / gbm_bo_unmap(). Driver-native CPU mapping that does not touch
    // the kernel dma-buf cache, so the GPU-sampling export's mode flags don't
    // constrain it. Used as a fallback on Mesa builds where the dma-buf FD is not
    // PROT_WRITE-mappable (e.g. Mesa 25.02 RPi5).
    GBMBoMap,
};

static ASCIILiteral strategyName(CPUMappingStrategy strategy)
{
    switch (strategy) {
    case CPUMappingStrategy::DmaBufFDMmap:
        return "mmap(gbm_bo_get_fd_for_plane)"_s;
    case CPUMappingStrategy::GBMBoMap:
        return "gbm_bo_map()"_s;
    case CPUMappingStrategy::Unsupported:
        return "Unavailable"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

MemoryMappedGPUBuffer::MemoryMappedGPUBuffer(const IntSize& size, OptionSet<BufferFlag> flags)
    : m_size(size)
    , m_flags(flags)
{
    ASSERT(m_flags.contains(BufferFlag::ForceLinear) || m_flags.contains(BufferFlag::ForceVivanteSuperTiled));
}

MemoryMappedGPUBuffer::~MemoryMappedGPUBuffer()
{
    unmapIfNeeded();

    if (m_bo)
        gbm_bo_destroy(m_bo);
}

static bool probeReadWriteMappability(int fd, size_t length)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0 || (flags & O_ACCMODE) != O_RDWR)
        return false;

    void* mapped = mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED)
        return false;

    munmap(mapped, length);
    return true;
}

static CPUMappingStrategy runCapabilityProbe()
{
    // The probe result is cached for the rest of the session; if we
    // ran before DRM init, we'd remember Unsupported and silently
    // disable the MemoryMappedGPUBuffer path on hardware where it works.
    // Crash instead of hiding the ordering bug, if that happens.
    auto& manager = WebCore::DRMDeviceManager::singleton();
    RELEASE_ASSERT_WITH_MESSAGE(manager.isInitialized(), "MemoryMappedGPUBuffer capability probe ran before DRMDeviceManager initialization");

    auto gbmDevice = manager.mainGBMDevice(WebCore::DRMDeviceManager::NodeType::Render);
    if (!gbmDevice) {
        RELEASE_LOG_ERROR(GraphicsBuffer, "MemoryMappedGPUBuffer capability probe: no GBM render device node");
        return CPUMappingStrategy::Unsupported;
    }

    // mmap capability depends on the kernel dma-buf subsystem and the GBM backend, not on
    // buffer size, format, or modifier -- so the single-plane linear probe generalizes.
    static constexpr int probeSize = 16;
    auto createProbeBO = [&]() -> struct gbm_bo* {
        auto* bo = gbm_bo_create(gbmDevice->device(), probeSize, probeSize, DRM_FORMAT_ARGB8888, GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);
        if (!bo)
            RELEASE_LOG_ERROR(GraphicsBuffer, "MemoryMappedGPUBuffer capability probe: gbm_bo_create failed: %s", safeStrerror(errno).data());
        return bo;
    };

    auto strategy = CPUMappingStrategy::Unsupported;

    if (auto* bo = createProbeBO()) {
        UnixFileDescriptor fd { gbm_bo_get_fd_for_plane(bo, 0), UnixFileDescriptor::Adopt };
        if (fd && probeReadWriteMappability(fd.value(), gbm_bo_get_stride_for_plane(bo, 0) * probeSize))
            strategy = CPUMappingStrategy::DmaBufFDMmap;
        gbm_bo_destroy(bo);
    }

    if (strategy == CPUMappingStrategy::Unsupported) {
        if (auto* bo = createProbeBO()) {
            uint32_t stride = 0;
            void* mapData = nullptr;
            void* mapped = gbm_bo_map(bo, 0, 0, probeSize, probeSize, GBM_BO_TRANSFER_READ_WRITE, &stride, &mapData);
            if (mapped && mapped != MAP_FAILED) {
                gbm_bo_unmap(bo, mapData);
                strategy = CPUMappingStrategy::GBMBoMap;
            }
            gbm_bo_destroy(bo);
        }
    }

    RELEASE_LOG(GraphicsBuffer, "MemoryMappedGPUBuffer capability probe: strategy=%s", strategyName(strategy).characters());
    return strategy;
}

static CPUMappingStrategy cachedCPUMappingStrategy()
{
    static const CPUMappingStrategy strategy = runCapabilityProbe();
    return strategy;
}

bool MemoryMappedGPUBuffer::isSupported()
{
    return cachedCPUMappingStrategy() != CPUMappingStrategy::Unsupported;
}

ASCIILiteral MemoryMappedGPUBuffer::exportStrategyDescription()
{
    return strategyName(cachedCPUMappingStrategy());
}

std::unique_ptr<MemoryMappedGPUBuffer> MemoryMappedGPUBuffer::create(const IntSize& size, OptionSet<BufferFlag> flags)
{
    if (!isSupported())
        return nullptr;

    // Vivante super-tiled writes need a stable linear mapping at the BO's natural
    // stride. gbm_bo_map() is allowed to return a staging buffer with its own stride
    // (typically linearized for tiled BOs), which would silently corrupt the
    // VivanteSuperTiledTexture writer. Only the dma-buf-mmap path is safe here.
    RELEASE_ASSERT_WITH_MESSAGE(!flags.contains(BufferFlag::ForceVivanteSuperTiled) || cachedCPUMappingStrategy() == CPUMappingStrategy::DmaBufFDMmap,
        "ForceVivanteSuperTiled requires the dma-buf-mmap CPU-mapping strategy; gbm_bo_map() may linearize tiled BOs.");

    auto& manager = WebCore::DRMDeviceManager::singleton();
    ASSERT(manager.isInitialized());

    auto gbmDevice = manager.mainGBMDevice(WebCore::DRMDeviceManager::NodeType::Render);
    if (!gbmDevice) {
        RELEASE_LOG_ERROR(GraphicsBuffer, "MemoryMappedGPUBuffer::create(), failed to get GBM render device node");
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
                // If a linear buffer is requested - only allow a single modifier.
                auto useFormat = format;
                if (format.modifiers.contains(DRM_FORMAT_MOD_LINEAR))
                    useFormat.modifiers = { DRM_FORMAT_MOD_LINEAR };
                else
                    useFormat.modifiers = { };
                return useFormat;
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

    if (!bufferFormat.has_value()) {
        WTFLogAlways("ERROR: Could not negotiate a suitable buffer format. Aborting ..."); // NOLINT
        CRASH();
    }

    if (flags.contains(BufferFlag::ForceVivanteSuperTiled) && (!bufferFormat.has_value() || !bufferFormat->modifiers.contains(DRM_FORMAT_MOD_VIVANTE_SUPER_TILED))) {
        WTFLogAlways("ERROR: ForceVivanteSuperTiled flag set but DRM_FORMAT_MOD_VIVANTE_SUPER_TILED not supported by the negotiated buffer format. Aborting ..."); // NOLINT
        CRASH();
    }

    if (!bufferFormat.has_value()) {
        RELEASE_LOG_ERROR(GraphicsBuffer, "MemoryMappedGPUBuffer::create(), failed to negotiate buffer format");
        return nullptr;
    }

    auto buffer = std::unique_ptr<MemoryMappedGPUBuffer>(new MemoryMappedGPUBuffer(size, flags));
    auto* bo = buffer->allocate(gbmDevice->device(), bufferFormat.value());
    if (!bo) {
        RELEASE_LOG_ERROR(GraphicsBuffer, "MemoryMappedGPUBuffer::create(), failed to create GBM buffer of size %dx%d: %s", size.width(), size.height(), safeStrerror(errno).data());
        return nullptr;
    }

    // Single prime export, via gbm so Mesa observes it and attaches its implicit-sync
    // dma_resv fence. Routing this through libdrm directly leaves V3D Gallium without
    // the fence and pre-draw waits then return EINVAL (or hang waiting for a fence
    // that never signals).
    if (!buffer->createDMABufFromGBMBufferObject(bo)) {
        RELEASE_LOG_ERROR(GraphicsBuffer, "MemoryMappedGPUBuffer::create(), failed to create dma-buf from GBM buffer object");
        gbm_bo_destroy(bo);
        return nullptr;
    }

    buffer->m_bo = bo;
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
    auto attributes = DMABufBufferAttributes::fromGBMBufferObject(bo);
    if (!attributes)
        return false;

    attributes->modifier = m_modifier;

    ASSERT(!m_dmaBuf);
    m_dmaBuf = DMABufBuffer::create(WTF::move(*attributes));
    return true;
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

int MemoryMappedGPUBuffer::primaryPlaneDmaBufFD() const
{
    ASSERT(m_dmaBuf);

    auto& fds = m_dmaBuf->attributes().fds;
    ASSERT(!fds.isEmpty());

    auto fd = fds[0].value();
    ASSERT(fd >= 0);
    return fd;
}

bool MemoryMappedGPUBuffer::mapIfNeeded()
{
    if (isMapped())
        return true;

    ASSERT(isLinear() || isVivanteSuperTiled());
    m_mappedLength = primaryPlaneDmaBufStride() * m_allocatedSize.height();

    switch (cachedCPUMappingStrategy()) {
    case CPUMappingStrategy::DmaBufFDMmap:
        m_mappedData = mmap(nullptr, m_mappedLength, PROT_READ | PROT_WRITE, MAP_SHARED, primaryPlaneDmaBufFD(), 0);
        if (m_mappedData == MAP_FAILED) {
            m_mappedLength = 0;
            m_mappedData = nullptr;
            return false;
        }
        return true;
    case CPUMappingStrategy::GBMBoMap: {
        ASSERT(m_bo);
        uint32_t stride = 0;
        void* mapData = nullptr;
        void* mapped = gbm_bo_map(m_bo, 0, 0, m_allocatedSize.width(), m_allocatedSize.height(), GBM_BO_TRANSFER_READ_WRITE, &stride, &mapData);
        if (!mapped || mapped == MAP_FAILED) {
            m_mappedLength = 0;
            return false;
        }
        m_mappedData = mapped;
        m_gbmBoMapData = mapData;
        return true;
    }
    case CPUMappingStrategy::Unsupported:
        RELEASE_ASSERT_NOT_REACHED();
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void MemoryMappedGPUBuffer::unmapIfNeeded()
{
    if (!isMapped())
        return;

    switch (cachedCPUMappingStrategy()) {
    case CPUMappingStrategy::DmaBufFDMmap:
        munmap(m_mappedData, m_mappedLength);
        break;
    case CPUMappingStrategy::GBMBoMap:
        ASSERT(m_bo);
        gbm_bo_unmap(m_bo, m_gbmBoMapData);
        m_gbmBoMapData = nullptr;
        break;
    case CPUMappingStrategy::Unsupported:
        RELEASE_ASSERT_NOT_REACHED();
    }

    m_mappedData = nullptr;
    m_mappedLength = 0;
}

EGLImage MemoryMappedGPUBuffer::createEGLImageFromDMABuf()
{
    ASSERT(m_dmaBuf);

    auto& display = WebCore::PlatformDisplay::sharedDisplay();
    auto eglImage = m_dmaBuf->createEGLImage(display.glDisplay());
    if (!eglImage)
        RELEASE_LOG_ERROR(GraphicsBuffer, "MemoryMappedGPUBuffer::createEGLImageFromDMABuf(), failed to export GBM buffer as EGLImage");

    return eglImage;
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
        RELEASE_LOG_ERROR(GraphicsBuffer, "MemoryMappedGPUBuffer::performDMABufSyncSystemCall(), DMA_BUF_SYNC_IOCTL failed - may result in visual artifacts.");
        return false;
    }

    return true;
}

} // namespace WebCore

#endif // USE(GBM)
