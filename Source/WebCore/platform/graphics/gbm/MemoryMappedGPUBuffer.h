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

#pragma once

#if USE(GBM)
#include "DMABufBuffer.h"
#include "GLDisplay.h"
#include "IntSize.h"
#include <wtf/OptionSet.h>
#include <wtf/unix/UnixFileDescriptor.h>

struct gbm_bo;
struct gbm_device;
typedef void* EGLImage;
typedef intptr_t EGLAttrib;

namespace WebCore {

class IntRect;

// Use MemoryMappedGPUBuffer to create a OpenGL texture, that's baked by a dma-buf.
class MemoryMappedGPUBuffer {
    WTF_MAKE_NONCOPYABLE(MemoryMappedGPUBuffer);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED();
public:
    ~MemoryMappedGPUBuffer();

    enum class BufferFlag : uint8_t {
        ForceLinear = 1 << 0
    };

    // Will only return a MemoryMappedGPUBuffer, if gbm_bo allocation + mapping to userland + EGLImage creation succeeded.
    static std::unique_ptr<MemoryMappedGPUBuffer> create(const IntSize&, OptionSet<BufferFlag>);

    const IntSize& size() const { return m_size; }
    const OptionSet<BufferFlag>& flags() const { return m_flags; }

    // Map dma-buf into memory, if not yet mapped.
    bool mapIfNeeded();
    void unmapIfNeeded();

    // Export gbm_bo buffer as dma-buf and wrap in EGLImage.
    EGLImage createEGLImageFromDMABuf();

    // Update the underlying data of the dma-buf, as often as desired.
    // You need to obtain an AccessScope, fencing the write operation.
    class AccessScope;
    void updateContents(AccessScope&, const void* srcData, const IntRect& targetRect, unsigned bytesPerLine);
    void updateContents(AccessScope&, const MemoryMappedGPUBuffer& srcBuffer, const IntRect& targetRect);

    // You need to obtain an AccessScope, fencing the read operation.
    std::span<uint32_t> mappedDataSpan(AccessScope&) const;

    class AccessScope {
        WTF_MAKE_NONCOPYABLE(AccessScope);
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED();
    public:
        ~AccessScope();

        enum class Mode : bool {
            Read,
            Write
        };

        static std::unique_ptr<AccessScope> create(MemoryMappedGPUBuffer&, Mode);

        const Mode& mode() const { return m_mode; }
        const MemoryMappedGPUBuffer& buffer() const { return m_buffer; }

    private:
        AccessScope(MemoryMappedGPUBuffer&, Mode);

        MemoryMappedGPUBuffer& m_buffer;
        Mode m_mode { Mode::Read };
    };

    bool isMapped() const { return !!m_mappedData; }
    bool isLinear() const;

private:
    MemoryMappedGPUBuffer(const IntSize&, OptionSet<BufferFlag>);

    enum class DMABufSyncFlag : uint8_t {
        Start = 1 << 0,
        End   = 1 << 1,
        Read  = 1 << 2,
        Write = 1 << 3
    };

    bool performDMABufSyncSystemCall(OptionSet<DMABufSyncFlag> flags);
    bool allocate(struct gbm_device*, const GLDisplay::BufferFormat&);
    bool createDMABufFromGBMBufferObject();
    UnixFileDescriptor exportGBMBufferObjectAsDMABuf(unsigned planeIndex);

    int primaryPlaneDmaBufFD() const;
    uint32_t primaryPlaneDmaBufStride() const;

    IntSize m_size;
    OptionSet<BufferFlag> m_flags;
    struct gbm_bo* m_bo { nullptr };
    uint64_t m_modifier { 0 };
    Vector<EGLAttrib> m_eglAttributes;
    RefPtr<DMABufBuffer> m_dmaBuf;

    void* m_mappedData { nullptr };
    uint32_t m_mappedLength { 0 };
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
