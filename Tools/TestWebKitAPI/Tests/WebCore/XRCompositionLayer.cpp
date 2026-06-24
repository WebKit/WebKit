/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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

#if ENABLE(WEBXR_LAYERS)

#include <WebCore/TransformationMatrix.h>
#include <WebCore/XRCompositionLayerPose.h>

namespace TestWebKitAPI {

// A 'local-floor' reference space's nativeOrigin places the floor at y = -height in local space, matching the default WebXRReferenceSpace::floorOriginTransform() fallback.
static constexpr double DefaultUserHeight = 1.7;

TEST(XRCompositionLayer, LocalFloorSpaceTranslatesLayerToFloor)
{
    // Regression guard for https://bugs.webkit.org/show_bug.cgi?id=314634, a layer placed at the origin of a 'local-floor' reference space
    // must land at y = -DefaultUserHeight in local space.
    WebCore::TransformationMatrix floorOrigin;
    floorOrigin.translate3d(0.0, -DefaultUserHeight, 0.0);

    auto pose = WebCore::computeXRCompositionLayerPose(floorOrigin, nullptr);
    ASSERT_TRUE(pose.has_value());
    EXPECT_FLOAT_EQ(0.0f, pose->position.x());
    EXPECT_FLOAT_EQ(static_cast<float>(-DefaultUserHeight), pose->position.y());
    EXPECT_FLOAT_EQ(0.0f, pose->position.z());
}

TEST(XRCompositionLayer, ComposesSpaceAndLayerTranslationsInTheRightOrder)
{
    // A floor-anchored space combined with a layer one meter above the floor should land at y = -DefaultUserHeight + 1, not at -DefaultUserHeight - 1.
    // This would catch a multiply-order bug independently from the sign-flip above.
    WebCore::TransformationMatrix floorOrigin;
    floorOrigin.translate3d(0.0, -DefaultUserHeight, 0.0);

    WebCore::TransformationMatrix oneMeterHeight;
    oneMeterHeight.translate3d(0.0, 1.0, 0.0);

    auto pose = WebCore::computeXRCompositionLayerPose(floorOrigin, &oneMeterHeight);
    ASSERT_TRUE(pose.has_value());
    EXPECT_FLOAT_EQ(static_cast<float>(-DefaultUserHeight + 1.0), pose->position.y());
}

} // namespace TestWebKitAPI

#endif // ENABLE(WEBXR_LAYERS)
