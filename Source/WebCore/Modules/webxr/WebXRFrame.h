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

#include "DOMHighResTimeStamp.h"
#include "ExceptionOr.h"
#include "PlatformXR.h"
#include <JavaScriptCore/Float32Array.h>
#include <wtf/IsoMalloc.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Document;
class WebXRJointPose;
class WebXRJointSpace;
class WebXRPose;
class WebXRReferenceSpace;
class WebXRSession;
class WebXRSpace;
class WebXRViewerPose;

class WebXRFrame : public RefCounted<WebXRFrame> {
    WTF_MAKE_ISO_ALLOCATED(WebXRFrame);
public:
    enum class IsAnimationFrame : bool { No, Yes };
    static Ref<WebXRFrame> create(WebXRSession&, IsAnimationFrame);
    ~WebXRFrame();

    const WebXRSession& session() const { return m_session.get(); }

    ExceptionOr<RefPtr<WebXRViewerPose>> getViewerPose(const Document&, const WebXRReferenceSpace&);
    ExceptionOr<RefPtr<WebXRPose>> getPose(const Document&, const WebXRSpace&, const WebXRSpace&);

#if ENABLE(WEBXR_HANDS)
    ExceptionOr<RefPtr<WebXRJointPose>> getJointPose(const Document&, const WebXRJointSpace&, const WebXRSpace&);
    ExceptionOr<bool> fillJointRadii(const Vector<RefPtr<WebXRJointSpace>>&, Float32Array&);
    ExceptionOr<bool> fillPoses(const Document&, const Vector<RefPtr<WebXRSpace>>&, const WebXRSpace&, Float32Array&);
#endif

    void setTime(DOMHighResTimeStamp time) { m_time = time; }

    void setActive(bool active) { m_active = active; }
    bool isActive() const { return m_active; }
    bool isAnimationFrame() const { return m_isAnimationFrame; }

    static TransformationMatrix matrixFromPose(const PlatformXR::FrameData::Pose&);

private:
    WebXRFrame(WebXRSession&, IsAnimationFrame);

    bool isOutsideNativeBoundsOfBoundedReferenceSpace(const WebXRSpace&, const WebXRSpace&) const;
    bool isLocalReferenceSpace(const WebXRSpace&) const;
    bool mustPosesBeLimited(const WebXRSpace&, const WebXRSpace&) const;

    struct PopulatedPose;
    ExceptionOr<std::optional<PopulatedPose>> populatePose(const Document&, const WebXRSpace&, const WebXRSpace&);

    bool m_active { false };
    bool m_isAnimationFrame { false };
    DOMHighResTimeStamp m_time { 0 };
    Ref<WebXRSession> m_session;
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
