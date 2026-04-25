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

#pragma once

#if USE(GBM)
#include "DMABufBuffer.h"
#include "GLDisplay.h"
#include "IntSize.h"
#include <wtf/OptionSet.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/unix/UnixFileDescriptor.h>

struct gbm_bo;
struct gbm_device;
typedef void* EGLImage;

namespace WebCore {

class IntRect;

// Use MemoryMappedGPUBuffer to create a OpenGL texture, that's baked by a dma-buf.
class MemoryMappedGPUBuffer {
    WTF_MAKE_NONCOPYABLE(MemoryMappedGPUBuffer);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(MemoryMappedGPUBuffer);
public:
    ~MemoryMappedGPUBuffer();

    enum class BufferFlag : uint8_t {
        ForceLinear = 1 << 0,
        ForceVivanteSuperTiled = 1 << 1,
        UseBGRALayout = 1 << 2
    };

    // On some non-Mesa stacks gbm_bo allocation and dma-buf export succeed but mmap
    // still fails; the probe verifies all three once per session before committing
    // to this path. Gates only MemoryMappedGPUBuffer::create() -- callers that only
    // need a GPUSampling FD do not depend on this flag.
    static bool isSupported();

    enum class FDExportPurpose : uint8_t {
        // FD will be imported as an EGLImage and sampled/rendered by GL. Always uses
        // gbm_bo_get_fd_for_plane() so Mesa observes the export and attaches its
        // implicit-sync fence; bypassing gbm here leads to EINVAL in gallium's fence
        // wait when the dma-buf is re-imported as EGLImage (Mesa >= 25).
        GPUSampling,
        // FD will be mmap'd with PROT_READ | PROT_WRITE. Uses the probe-selected
        // strategy that produced a writable mapping on this system. Not suitable
        // for EGLImage imports -- the RDWR fallback bypasses gbm.
        CPUMapping,
    };

    static int exportFDForPlane(struct gbm_bo*, int plane, FDExportPurpose);

    static ASCIILiteral exportStrategyDescription();

    // Will only return a MemoryMappedGPUBuffer, if gbm_bo allocation + mapping to userland + EGLImage creation succeeded.
    static std::unique_ptr<MemoryMappedGPUBuffer> create(const IntSize&, OptionSet<BufferFlag>);

    // Returns the actual allocated buffer size, which may be larger than size()
    // due to GPU alignment requirements (e.g. tiled formats).
    const IntSize& allocatedSize() const LIFETIME_BOUND { return m_allocatedSize; }

    const IntSize& size() const LIFETIME_BOUND { return m_size; }
    const OptionSet<BufferFlag>& flags() const LIFETIME_BOUND { return m_flags; }

    // Map dma-buf into memory, if not yet mapped.
    bool mapIfNeeded();
    void unmapIfNeeded();

    // Export gbm_bo buffer as dma-buf and wrap in EGLImage.
    EGLImage createEGLImageFromDMABuf();

    // Update the underlying data of the dma-buf, as often as desired.
    // You need to obtain an AccessScope, fencing the write operation.
    class AccessScope;
    void updateContents(AccessScope&, const void* srcData, const IntRect& targetRect, unsigned bytesPerLine);

    // You need to obtain an AccessScope, fencing the read operation.
    std::span<uint32_t> mappedDataSpan(AccessScope&) const;

    class AccessScope {
        WTF_MAKE_NONCOPYABLE(AccessScope);
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(AccessScope);
    public:
        ~AccessScope();

        enum class Mode : bool {
            Read,
            Write
        };

        static std::unique_ptr<AccessScope> create(MemoryMappedGPUBuffer&, Mode);

        const Mode& mode() const LIFETIME_BOUND { return m_mode; }
        const MemoryMappedGPUBuffer& buffer() const { return m_buffer; }

    private:
        AccessScope(MemoryMappedGPUBuffer&, Mode);

        MemoryMappedGPUBuffer& m_buffer;
        Mode m_mode { Mode::Read };
    };

    bool isMapped() const { return !!m_mappedData; }
    bool isLinear() const;
    bool isVivanteSuperTiled() const;

private:
    MemoryMappedGPUBuffer(const IntSize&, OptionSet<BufferFlag>);

    enum class DMABufSyncFlag : uint8_t {
        Start = 1 << 0,
        End   = 1 << 1,
        Read  = 1 << 2,
        Write = 1 << 3
    };

    bool performDMABufSyncSystemCall(OptionSet<DMABufSyncFlag> flags);

    struct gbm_bo* allocate(struct gbm_device*, const GLDisplay::BufferFormat&);
    bool createDMABufFromGBMBufferObject(struct gbm_bo*);
    bool exportFDForMappingFromGBMBufferObject(struct gbm_bo*);

    void updateContentsInLinearFormat(const void* srcData, const IntRect& targetRect, unsigned bytesPerLine);
    void updateContentsInVivanteSuperTiledFormat(const void* srcData, const IntRect& targetRect, unsigned bytesPerLine);

    uint32_t primaryPlaneDmaBufStride() const;

    IntSize m_size;
    IntSize m_allocatedSize;
    OptionSet<BufferFlag> m_flags;
    uint64_t m_modifier { 0 };
    RefPtr<DMABufBuffer> m_dmaBuf;

    // Distinct from the GPU-sampling FDs in m_dmaBuf: those must come from
    // gbm_bo_get_fd_for_plane() to stay in gbm's bookkeeping. m_exportedFDForMapping
    // is exported via the probe-selected strategy so it is guaranteed RDWR-mmappable
    // on this system.
    UnixFileDescriptor m_exportedFDForMapping;

    void* m_mappedData { nullptr };
    size_t m_mappedLength { 0 };
};

inline std::unique_ptr<MemoryMappedGPUBuffer::AccessScope> makeGPUBufferReadScope(MemoryMappedGPUBuffer& buffer)
{
    return MemoryMappedGPUBuffer::AccessScope::create(buffer, MemoryMappedGPUBuffer::AccessScope::Mode::Read);
}

inline std::unique_ptr<MemoryMappedGPUBuffer::AccessScope> makeGPUBufferWriteScope(MemoryMappedGPUBuffer& buffer)
{
    return MemoryMappedGPUBuffer::AccessScope::create(buffer, MemoryMappedGPUBuffer::AccessScope::Mode::Write);
}

} // namespace WebCore

#endif // USE(GBM)
