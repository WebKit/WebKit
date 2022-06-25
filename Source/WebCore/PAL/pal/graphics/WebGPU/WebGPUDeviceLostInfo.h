/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "WebGPUDeviceLostReason.h"
#include <optional>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace PAL::WebGPU {

class DeviceLostInfo final : public RefCounted<DeviceLostInfo> {
public:
    using Reason = std::optional<DeviceLostReason>;

    static Ref<DeviceLostInfo> create(Reason&& reason, String&& message)
    {
        return adoptRef(*new DeviceLostInfo(WTFMove(reason), WTFMove(message)));
    }

    const Reason& reason() const { return m_reason; }
    const String& message() const { return m_message; }

protected:
    DeviceLostInfo(Reason&& reason, String&& message)
        : m_reason(WTFMove(reason))
        , m_message(WTFMove(message))
    {
    }

private:
    DeviceLostInfo(const DeviceLostInfo&) = delete;
    DeviceLostInfo(DeviceLostInfo&&) = delete;
    DeviceLostInfo& operator=(const DeviceLostInfo&) = delete;
    DeviceLostInfo& operator=(DeviceLostInfo&&) = delete;

    Reason m_reason;
    String m_message;
};

} // namespace PAL::WebGPU
