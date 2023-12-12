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

#include "config.h"
#include "WebXRHand.h"

#if ENABLE(WEBXR) && ENABLE(WEBXR_HANDS)

#include "WebXRInputSource.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(WebXRHand);

Ref<WebXRHand> WebXRHand::create(const WebXRInputSource& inputSource)
{
    return adoptRef(*new WebXRHand(inputSource));
}

WebXRHand::WebXRHand(const WebXRInputSource& inputSource)
    : m_inputSource(inputSource)
{
    RefPtr session = this->session();
    RefPtr document = session ? downcast<Document>(session->scriptExecutionContext()) : nullptr;
    if (!document)
        return;

    size_t jointCount = static_cast<size_t>(XRHandJoint::Count);
    m_joints = Vector<Ref<WebXRJointSpace>>(jointCount, [&](size_t i) {
        return WebXRJointSpace::create(*document, *this, static_cast<XRHandJoint>(i));
    });
}

WebXRHand::~WebXRHand() = default;

RefPtr<WebXRJointSpace> WebXRHand::get(const XRHandJoint& key)
{
    size_t jointIndex = static_cast<size_t>(key);
    if (jointIndex >= m_joints.size())
        return nullptr;

    return m_joints[jointIndex].ptr();
}

WebXRHand::Iterator::Iterator(WebXRHand& hand)
    : m_hand(hand)
{
}

std::optional<KeyValuePair<XRHandJoint, RefPtr<WebXRJointSpace>>> WebXRHand::Iterator::next()
{
    if (m_index >= m_hand->m_joints.size())
        return std::nullopt;

    size_t index = m_index++;
    return KeyValuePair<XRHandJoint, RefPtr<WebXRJointSpace>> { static_cast<XRHandJoint>(index), m_hand->m_joints[index].ptr() };
}

WebXRSession* WebXRHand::session()
{
    if (!m_inputSource)
        return nullptr;

    return m_inputSource->session();
}

void WebXRHand::updateFromInputSource(const PlatformXR::FrameData::InputSource& inputSource)
{
    if (!inputSource.handJoints) {
        m_hasMissingPoses = true;
        return;
    }

    auto& handJoints = *(inputSource.handJoints);
    if (handJoints.size() != m_joints.size()) {
        m_hasMissingPoses = true;
        return;
    }

    bool hasMissingPoses = false;
    for (size_t i = 0; i < handJoints.size(); ++i) {
        if (!handJoints[i])
            hasMissingPoses = true;

        m_joints[i]->updateFromJoint(handJoints[i]);
    }
    m_hasMissingPoses = hasMissingPoses;
}

}

#endif
