/*
 *  Copyright (C) 2024 Metrological Group B.V.
 *  Copyright (C) 2024 Igalia S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if USE(GSTREAMER) && USE(TEXTURE_MAPPER_DMABUF)
#include "DMABufUtilities.h"

namespace WebCore {

DMABufDestination::DMABufDestination(struct gbm_bo* bo, uint32_t width, uint32_t height)
    : bo(bo)
    , height(height)
{
    map = gbm_bo_map(bo, 0, 0, width, height, GBM_BO_TRANSFER_WRITE, &stride, &mapData);
    if (!map)
        return;

    isValid = true;
    data = reinterpret_cast<uint8_t*>(map);
}

DMABufDestination::~DMABufDestination()
{
    if (isValid)
        gbm_bo_unmap(bo, mapData);
}

void DMABufDestination::copyPlaneData(GstVideoFrame* sourceVideoFrame, unsigned planeIndex)
{
    auto sourceStride = GST_VIDEO_FRAME_PLANE_STRIDE(sourceVideoFrame, planeIndex);
    auto* planeData = reinterpret_cast<uint8_t*>(GST_VIDEO_FRAME_PLANE_DATA(sourceVideoFrame, planeIndex));
    for (uint32_t y = 0; y < height; ++y) {
        auto* destinationData = &data[y * stride];
        auto* sourceData = &planeData[y * sourceStride];
        memcpy(destinationData, sourceData, std::min(static_cast<uint32_t>(sourceStride), stride));
    }
}

bool fillSwapChainBuffer(const RefPtr<GBMBufferSwapchain::Buffer>& buffer, const GRefPtr<GstSample>& sample)
{
    GstMappedFrame sourceFrame(sample, GST_MAP_READ);
    if (!sourceFrame)
        return false;

    auto* sourceVideoFrame = sourceFrame.get();
    for (unsigned i = 0; i < GST_VIDEO_FRAME_N_PLANES(sourceVideoFrame); ++i) {
        auto& planeData = buffer->planeData(i);

        Locker locker { DMABufDestination::mappingLock() };
        DMABufDestination destination(planeData.bo, planeData.width, planeData.height);
        if (destination.isValid)
            destination.copyPlaneData(sourceVideoFrame, i);
    }
    return true;
}

} // namespace WebCore

#endif // USE(GSTREAMER) && USE(TEXTURE_MAPPER_DMABUF)
