/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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

#if ENABLE(WEBXR)

#include "WebXRSpace.h"
#include "XRHandedness.h"
#include "XRTargetRayMode.h"
#include <wtf/IsoMalloc.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class WebXRInputSpace : public RefCounted<WebXRInputSpace>, public WebXRSpace {
    WTF_MAKE_ISO_ALLOCATED(WebXRInputSpace);
public:
    static Ref<WebXRInputSpace> create(Document&, WebXRSession&, const PlatformXR::FrameData::InputSourcePose&);
    virtual ~WebXRInputSpace();

    using RefCounted<WebXRInputSpace>::ref;
    using RefCounted<WebXRInputSpace>::deref;

    std::optional<bool> isPositionEmulated() const final { return m_pose.isPositionEmulated; }
    void setPose(const PlatformXR::FrameData::InputSourcePose& pose) { m_pose = pose; }

private:
    WebXRInputSpace(Document&, WebXRSession&, const PlatformXR::FrameData::InputSourcePose&);
    WebXRSession* session() const final { return m_session.get(); }
    std::optional<TransformationMatrix> nativeOrigin() const final;

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    WeakPtr<WebXRSession> m_session;
    PlatformXR::FrameData::InputSourcePose m_pose;
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
