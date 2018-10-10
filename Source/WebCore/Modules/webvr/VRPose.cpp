/*
 * Copyright (C) 2017 Igalia S.L. All rights reserved.
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
#include "VRPose.h"

namespace WebCore {

static RefPtr<Float32Array> optionalFloat3ToJSCArray(const std::optional<VRPlatformTrackingInfo::Float3>& data)
{
    if (!data)
        return nullptr;

    return Float32Array::create(data->data, 3);
}

RefPtr<Float32Array> VRPose::position() const
{
    if (!m_trackingInfo.position)
        return nullptr;

    auto& position = *m_trackingInfo.position;
    float positionData[3] = { position.x(), position.y(), position.z() };
    return Float32Array::create(positionData, 3);
}

RefPtr<Float32Array> VRPose::linearVelocity() const
{
    return optionalFloat3ToJSCArray(m_trackingInfo.linearVelocity);
}

RefPtr<Float32Array> VRPose::linearAcceleration() const
{
    return optionalFloat3ToJSCArray(m_trackingInfo.linearAcceleration);
}

RefPtr<Float32Array> VRPose::orientation() const
{
    if (!m_trackingInfo.orientation)
        return nullptr;

    auto& orientation = *m_trackingInfo.orientation;
    float orientationData[4] = { orientation.x, orientation.y, orientation.z, orientation.w };
    return Float32Array::create(orientationData, 4);
}

RefPtr<Float32Array> VRPose::angularVelocity() const
{
    return optionalFloat3ToJSCArray(m_trackingInfo.angularVelocity);
}

RefPtr<Float32Array> VRPose::angularAcceleration() const
{
    return optionalFloat3ToJSCArray(m_trackingInfo.angularAcceleration);
}

} // namespace WebCore
