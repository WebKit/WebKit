/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if USE(AVFOUNDATION)

#include "GPUVideoEncoderVTB.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class GPUVideoEncoderVTBH264 final : public GPUVideoEncoderVTB {
    WTF_MAKE_TZONE_ALLOCATED(GPUVideoEncoderVTBH264);
public:
    static Ref<GPUVideoEncoderVTBH264> create(CreationInfo&& info, const Vector<std::pair<String, String>>& parameters, GPUVideoEncoderCallback&& encoderCallback, GPUVideoEncoderDescriptionCallback&& descriptionCallback, GPUVideoEncoderErrorCallback&& errorCallback)
    {
        return adoptRef(*new GPUVideoEncoderVTBH264(WTF::move(info), parameters, WTF::move(encoderCallback), WTF::move(descriptionCallback), WTF::move(errorCallback)));
    }

    ~GPUVideoEncoderVTBH264() = default;

private:
    GPUVideoEncoderVTBH264(CreationInfo&&, const Vector<std::pair<String, String>>&, GPUVideoEncoderCallback&&, GPUVideoEncoderDescriptionCallback&&, GPUVideoEncoderErrorCallback&&);

    bool convertAndNotify(RetainPtr<CMSampleBufferRef>&&, GPUVideoEncoderFrameInfo&&, const PlatformVideoColorSpace&) final;
    void configureAdditionalProperties() final;

    const RetainPtr<CFStringRef> m_profileLevel;
};

}

#endif // USE(AVFOUNDATION)
