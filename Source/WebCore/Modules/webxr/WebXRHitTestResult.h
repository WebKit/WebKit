/*
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
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

#if ENABLE(WEBXR_HIT_TEST)

#include "ExceptionOr.h"
#include "PlatformXR.h"
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class WebXRFrame;
class WebXRPose;
class WebXRSpace;

class WebXRHitTestResult : public RefCounted<WebXRHitTestResult> {
    WTF_MAKE_TZONE_ALLOCATED(WebXRHitTestResult);
public:
    static Ref<WebXRHitTestResult> create(WebXRFrame&, WebXRSpace&, const PlatformXR::FrameData::HitTestResult&);
    ~WebXRHitTestResult();
    ExceptionOr<RefPtr<WebXRPose>> getPose(Document&, const WebXRSpace& baseSpace);

private:
    WebXRHitTestResult(WebXRFrame&, WebXRSpace&, const PlatformXR::FrameData::HitTestResult&);

    const Ref<WebXRFrame> m_frame;
    Ref<WebXRSpace> m_space;
    PlatformXR::FrameData::HitTestResult m_result;
};

} // namespace WebCore

#endif // ENABLE(WEBXR_HIT_TEST)
