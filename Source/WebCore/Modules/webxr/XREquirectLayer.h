/*
 * Copyright (C) 2024 Apple, Inc. All rights reserved.
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

#if ENABLE(WEBXR_LAYERS)

#include "XRCompositionLayer.h"

namespace WebCore {

class WebXRRigidTransform;
class WebXRSpace;

// https://immersive-web.github.io/layers/#xrequirectlayertype
class XREquirectLayer : public XRCompositionLayer {
public:
    const WebXRSpace& space() const { RELEASE_ASSERT_NOT_REACHED(); }
    [[noreturn]] void setSpace(WebXRSpace&) { RELEASE_ASSERT_NOT_REACHED(); }
    const WebXRRigidTransform& transform() const { RELEASE_ASSERT_NOT_REACHED(); }
    [[noreturn]] void setTransform(WebXRRigidTransform&) { RELEASE_ASSERT_NOT_REACHED(); }

    float radius() const { RELEASE_ASSERT_NOT_REACHED(); }
    [[noreturn]] void setRadius(float) { RELEASE_ASSERT_NOT_REACHED(); }
    float centralHorizontalAngle() const { RELEASE_ASSERT_NOT_REACHED(); }
    [[noreturn]] void setCentralHorizontalAngle(float) { RELEASE_ASSERT_NOT_REACHED(); }
    float upperVerticalAngle() const { RELEASE_ASSERT_NOT_REACHED(); }
    [[noreturn]] void setUpperVerticalAngle(float) { RELEASE_ASSERT_NOT_REACHED(); }
    float lowerVerticalAngle() const { RELEASE_ASSERT_NOT_REACHED(); }
    [[noreturn]] void setLowerVerticalAngle(float) { RELEASE_ASSERT_NOT_REACHED(); }
};

} // namespace WebCore

#endif // ENABLE(WEBXR_LAYERS)
