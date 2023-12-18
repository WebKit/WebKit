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

#include "PlatformXR.h"
#include "WebXRInputSource.h"
#include "WebXRJointSpace.h"
#include "WebXRSession.h"
#include "XRHandJoint.h"
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class WebXRHand : public RefCounted<WebXRHand>, public CanMakeWeakPtr<WebXRHand> {
    WTF_MAKE_ISO_ALLOCATED(WebXRHand);
public:

    static Ref<WebXRHand> create(const WebXRInputSource&);
    ~WebXRHand();

    unsigned size() const { return m_joints.size(); }

    RefPtr<WebXRJointSpace> get(const XRHandJoint& key);

    class Iterator {
    public:
        explicit Iterator(WebXRHand&);
        std::optional<KeyValuePair<XRHandJoint, RefPtr<WebXRJointSpace>>> next();

    private:
        Ref<WebXRHand> m_hand;
        size_t m_index { 0 };
    };
    Iterator createIterator(ScriptExecutionContext*) { return Iterator(*this); }

    // For GC reachability.
    WebXRSession* session();

    bool hasMissingPoses() const { return m_hasMissingPoses; }
    void updateFromInputSource(const PlatformXR::FrameData::InputSource&);

private:
    WebXRHand(const WebXRInputSource&);

    FixedVector<Ref<WebXRJointSpace>> m_joints;
    bool m_hasMissingPoses { true };
    WeakPtr<WebXRInputSource> m_inputSource;
};

}
#endif
