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

#include "WebXRRigidTransform.h"
#include "XRCompositionLayer.h"

namespace WebCore {

class XRProjectionLayer : public XRCompositionLayer {
    WTF_MAKE_ISO_ALLOCATED(XRProjectionLayer);
public:
    virtual ~XRProjectionLayer();

    uint32_t textureWidth() const { RELEASE_ASSERT_NOT_REACHED(); }
    uint32_t textureHeight() const { RELEASE_ASSERT_NOT_REACHED(); }
    uint32_t textureArrayLength() const { RELEASE_ASSERT_NOT_REACHED(); }

    bool ignoreDepthValues() const { RELEASE_ASSERT_NOT_REACHED(); }
    std::optional<float> fixedFoveation() const { RELEASE_ASSERT_NOT_REACHED(); }
    [[noreturn]] void setFixedFoveation(std::optional<float>) { RELEASE_ASSERT_NOT_REACHED(); }
    WebXRRigidTransform* deltaPose() const { RELEASE_ASSERT_NOT_REACHED(); }
    [[noreturn]] void setDeltaPose(WebXRRigidTransform*) { RELEASE_ASSERT_NOT_REACHED(); }

    // WebXRLayer
    void startFrame(const PlatformXR::FrameData&) final;
    PlatformXR::Device::Layer endFrame() final;
};

} // namespace WebCore

#endif
