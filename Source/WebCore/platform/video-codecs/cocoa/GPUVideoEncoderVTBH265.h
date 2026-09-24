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

class GPUVideoEncoderVTBH265 final : public GPUVideoEncoderVTB {
    WTF_MAKE_TZONE_ALLOCATED(GPUVideoEncoderVTBH265);
public:
    static Ref<GPUVideoEncoderVTBH265> create(CreationInfo&& info, GPUVideoEncoderCallback&& encoderCallback, GPUVideoEncoderDescriptionCallback&& descriptionCallback, GPUVideoEncoderErrorCallback&& errorCallback)
    {
        return adoptRef(*new GPUVideoEncoderVTBH265(WTF::move(info), WTF::move(encoderCallback), WTF::move(descriptionCallback), WTF::move(errorCallback)));
    }

    ~GPUVideoEncoderVTBH265() = default;

private:
    GPUVideoEncoderVTBH265(CreationInfo&&, GPUVideoEncoderCallback&&, GPUVideoEncoderDescriptionCallback&&, GPUVideoEncoderErrorCallback&&);

    bool convertAndNotify(RetainPtr<CMSampleBufferRef>&&, GPUVideoEncoderFrameInfo&&, const PlatformVideoColorSpace&) final;
};

}

#endif // USE(AVFOUNDATION)
