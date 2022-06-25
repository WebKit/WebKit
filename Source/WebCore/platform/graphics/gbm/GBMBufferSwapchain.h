/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022 Igalia S.L.
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

#if USE(LIBGBM)

#include "DMABufFormat.h"
#include "DMABufObject.h"
#include "DMABufReleaseFlag.h"
#include <array>
#include <cstdint>
#include <wtf/Noncopyable.h>
#include <wtf/Nonmovable.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

struct gbm_bo;

namespace WebCore {

class GBMBufferSwapchain : public ThreadSafeRefCounted<GBMBufferSwapchain> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // The size should be adjusted to the use-case.
    // For cyclical rendering (e.g. WebGL), Four should be ideal since we'll rely on
    // something like vsync to maintain a constant framerate and subsequent buffer churn.
    // There are other sporadic producers like software-based media decoders that will
    // be requesting and filling in buffers in bursts but still have them displayed on
    // a cyclical basis. Eight should be enough to cover that.
    // In any case, the swapchain will do whatever it can to reuse released buffers
    // and not unnecessarily spawn new ones, meaning the limit shouldn't be reached.
    // If it is, getBuffer() will return a null object and the producer has to
    // handle that somehow.
    enum class BufferSwapchainSize : unsigned {
        Four = 4,
        Eight = 8
    };

    GBMBufferSwapchain(BufferSwapchainSize);
    ~GBMBufferSwapchain();

    struct BufferDescription {
        enum Flags : uint32_t {
            NoFlags = 0,
            LinearStorage = 1 << 0,
        };

        DMABufFormat format { };
        uint32_t width { 0 };
        uint32_t height  { 0 };
        uint32_t flags { NoFlags };
    };

    class Buffer : public ThreadSafeRefCounted<Buffer> {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        Buffer(uint32_t, const BufferDescription&);
        ~Buffer();

        DMABufObject createDMABufObject(uintptr_t) const;

        struct PlaneData {
            WTF_MAKE_NONCOPYABLE(PlaneData);
            WTF_MAKE_NONMOVABLE(PlaneData);

            PlaneData() = default;
            ~PlaneData();

            DMABufFormat::FourCC fourcc { DMABufFormat::FourCC::Invalid };
            uint32_t width { 0 };
            uint32_t height { 0 };
            uint32_t stride { 0 };
            struct gbm_bo* bo { nullptr };
        };

        uint32_t handle() const { return m_handle; }

        unsigned numPlanes() const { return m_description.format.numPlanes; }
        const PlaneData& planeData(unsigned index) const
        {
            ASSERT(index < DMABufFormat::c_maxPlanes);
            return m_planes[index];
        }

    private:
        friend class GBMBufferSwapchain;

        uint32_t m_handle { 0 };
        struct {
            bool locked { false };
            DMABufReleaseFlag releaseFlag;
        } m_state;

        BufferDescription m_description;
        std::array<PlaneData, DMABufFormat::c_maxPlanes> m_planes;
    };

    RefPtr<Buffer> getBuffer(const BufferDescription&);

private:
    static constexpr unsigned c_maxBuffers = 8;

    uint32_t m_handleGenerator { 0 };
    struct {
        BufferDescription description;
        unsigned size { 0 };
        std::array<RefPtr<Buffer>, c_maxBuffers> object { };
    } m_array;
};

} // namespace WebCore

#endif // USE(LIBGBM)
