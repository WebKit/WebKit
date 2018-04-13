/*
 * Copyright (C) 2017 Igalia S.L. All rights reserved.
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

#include "VRPlatformDisplay.h"

#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class VRDisplayCapabilities : public RefCounted<VRDisplayCapabilities> {
public:
    static Ref<VRDisplayCapabilities> create(unsigned capabilityFlags)
    {
        return adoptRef(*new VRDisplayCapabilities(capabilityFlags));
    }

    bool hasPosition() const { return m_flags & VRDisplayCapabilityFlagPosition; }
    bool hasOrientation() const { return m_flags & VRDisplayCapabilityFlagOrientation; }
    bool hasExternalDisplay() const { return m_flags & VRDisplayCapabilityFlagExternalDisplay; }
    bool canPresent() const { return m_flags & VRDisplayCapabilityFlagPresent; }
    unsigned maxLayers() const { return canPresent() ? 1 : 0; }

private:
    VRDisplayCapabilities(unsigned capabilityFlags)
        : m_flags(capabilityFlags)
    {
    }

    unsigned m_flags;
};

} // namespace WebCore
