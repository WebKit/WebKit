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

#include "config.h"
#include "GBMBufferSwapchain.h"

#if USE(LIBGBM)

#include "DMABufColorSpace.h"
#include "GBMDevice.h"
#include <gbm.h>

namespace WebCore {

GBMBufferSwapchain::GBMBufferSwapchain(BufferSwapchainSize size)
{
    ASSERT(unsigned(size) <= c_maxBuffers);
    m_array.size = unsigned(size);
}

GBMBufferSwapchain::~GBMBufferSwapchain() = default;

RefPtr<GBMBufferSwapchain::Buffer> GBMBufferSwapchain::getBuffer(const BufferDescription& description)
{
    auto& device = GBMDevice::singleton();

    // If the description of the requested buffers has changed, update the description to the new one and wreck the existing buffers.
    // This should handle changes in format or dimension of the buffers.
    if (description.format.fourcc != m_array.description.format.fourcc || description.width != m_array.description.width || description.height != m_array.description.height || description.flags != m_array.description.flags) {
        m_array.description = description;
        m_array.object = { };
    }

    // Swapchain was asked to provide a buffer. The buffer array is traversed to find one.
    for (unsigned i = 0; i < m_array.size; ++i) {
        if (!m_array.object[i]) {
            // If no buffer was spawned yet at this location, we do that, and return it.
            auto buffer = adoptRef(*new Buffer(m_handleGenerator++, description));
            m_array.object[i] = buffer.copyRef();

            // Fill out the buffer's description and plane information for known and supported formats.
            switch (description.format.fourcc) {
            case DMABufFormat::FourCC::XRGB8888:
            case DMABufFormat::FourCC::XBGR8888:
            case DMABufFormat::FourCC::RGBX8888:
            case DMABufFormat::FourCC::BGRX8888:
            case DMABufFormat::FourCC::ARGB8888:
            case DMABufFormat::FourCC::ABGR8888:
            case DMABufFormat::FourCC::RGBA8888:
            case DMABufFormat::FourCC::BGRA8888:
            case DMABufFormat::FourCC::I420:
            case DMABufFormat::FourCC::YV12:
            case DMABufFormat::FourCC::A420:
            case DMABufFormat::FourCC::NV12:
            case DMABufFormat::FourCC::NV21:
            case DMABufFormat::FourCC::YUY2:
            case DMABufFormat::FourCC::YVYU:
            case DMABufFormat::FourCC::UYVY:
            case DMABufFormat::FourCC::VYUY:
            case DMABufFormat::FourCC::VUYA:
            case DMABufFormat::FourCC::AYUV:
            case DMABufFormat::FourCC::Y444:
            case DMABufFormat::FourCC::Y41B:
            case DMABufFormat::FourCC::Y42B:
                buffer->m_description.format.numPlanes = description.format.numPlanes;
                for (unsigned i = 0; i < buffer->m_description.format.numPlanes; ++i) {
                    buffer->m_planes[i].fourcc = description.format.planes[i].fourcc;
                    buffer->m_planes[i].width = description.format.planeWidth(i, description.width);
                    buffer->m_planes[i].height = description.format.planeHeight(i, description.height);
                }
                break;
            default:
                return nullptr;
            }

            uint32_t boFlags = 0;
            if (description.flags & BufferDescription::LinearStorage)
                boFlags |= GBM_BO_USE_LINEAR;

            // For each plane, we spawn a gbm_bo object of the appropriate size and format.
            // TODO: GBM_BO_USE_LINEAR will be needed when transferring memory into the bo (e.g. copying
            // over the software-decoded video data), but might not be required for backing e.g. ANGLE rendering.
            for (unsigned i = 0; i < buffer->m_description.format.numPlanes; ++i) {
                auto& plane = buffer->m_planes[i];
                plane.bo = gbm_bo_create(device.device(), plane.width, plane.height, uint32_t(plane.fourcc), boFlags);
                plane.stride = gbm_bo_get_stride(plane.bo);
            }

            // Lock the buffer and return it.
            buffer->m_state.locked = true;
            return buffer;
        }

        // There is already an existing buffer at this location. If marked as locked, update its state by reading
        // the release flag. If still locked after that, we have to continue over to the next candidate.
        {
            auto& buffer = m_array.object[i];
            if (buffer->m_state.locked)
                buffer->m_state.locked = !buffer->m_state.releaseFlag.released();

            if (buffer->m_state.locked)
                continue;
        }

        // This buffer was unlocked, so it can be used. Lock and return it.
        auto buffer = m_array.object[i].copyRef();
        buffer->m_state.locked = true;

        // Swap out the located buffer to the end of the buffer queue. When the next buffer is requested from this
        // swapchain, the previously-used buffers will be traversed and tested for release first, meaning there's a
        // higher chance we will be able to reuse them, without the penalty of having to traverse to deep into the
        // swapchain array.
        for (++i; i < m_array.size && !!m_array.object[i]; ++i)
            std::swap(m_array.object[i - 1], m_array.object[i]);

        return buffer;
    }

    return nullptr;
}

GBMBufferSwapchain::Buffer::Buffer(uint32_t handle, const BufferDescription& description)
    : m_handle(handle)
    , m_description(description)
{
    m_state.releaseFlag = DMABufReleaseFlag(DMABufReleaseFlag::Initialize);
}

GBMBufferSwapchain::Buffer::~Buffer() = default;

DMABufObject GBMBufferSwapchain::Buffer::createDMABufObject(uintptr_t handle) const
{
    DMABufObject object(handle);
    object.format = m_description.format;
    object.width = m_description.width;
    object.height = m_description.height;
    object.releaseFlag = m_state.releaseFlag.dup();

    for (unsigned i = 0; i < m_description.format.numPlanes; ++i) {
        object.fd[i] = UnixFileDescriptor { gbm_bo_get_fd(m_planes[i].bo), UnixFileDescriptor::Adopt };
        object.offset[i] = 0;
        object.stride[i] = m_planes[i].stride;
        object.modifier[i] = gbm_bo_get_modifier(m_planes[i].bo);
    }

    return object;
}

GBMBufferSwapchain::Buffer::PlaneData::~PlaneData()
{
    if (bo)
        gbm_bo_destroy(bo);
}

} // namespace WebCore

#endif // USE(LIBGBM)
