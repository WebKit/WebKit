/*
 * Copyright (C) 2021 Apple, Inc. All rights reserved.
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

#if ENABLE(WEBXR) && ENABLE(WEBXR_HANDS)

#include "Document.h"
#include "PlatformXR.h"
#include "WebXRSpace.h"
#include "XRHandJoint.h"
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class WebXRHand;
class WebXRSession;

class WebXRJointSpace final: public RefCounted<WebXRJointSpace>, public WebXRSpace {
    WTF_MAKE_ISO_ALLOCATED(WebXRJointSpace);
public:
    static Ref<WebXRJointSpace> create(Document&, WebXRHand&, XRHandJoint, std::optional<PlatformXR::FrameData::InputSourceHandJoint>&& = std::nullopt);
    virtual ~WebXRJointSpace();

    using RefCounted<WebXRJointSpace>::ref;
    using RefCounted<WebXRJointSpace>::deref;

    XRHandJoint jointName() const { return m_jointName; }

    float radius() const { return m_joint ? m_joint->radius : 0; }
    void updateFromJoint(const std::optional<PlatformXR::FrameData::InputSourceHandJoint>&);
    bool handHasMissingPoses() const;

    WebXRSession* session() const final;

private:
    WebXRJointSpace(Document&, WebXRHand&, XRHandJoint, std::optional<PlatformXR::FrameData::InputSourceHandJoint>&&);

    std::optional<TransformationMatrix> nativeOrigin() const final;

    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    bool isJointSpace() const final { return true; }

    WeakPtr<WebXRHand> m_hand;
    XRHandJoint m_jointName { XRHandJoint::Wrist };
    std::optional<PlatformXR::FrameData::InputSourceHandJoint> m_joint;
};

}

SPECIALIZE_TYPE_TRAITS_WEBXRSPACE(WebXRJointSpace, isJointSpace())

#endif
