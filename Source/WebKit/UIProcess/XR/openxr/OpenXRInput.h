/*
 * Copyright (C) 2025-2026 Igalia, S.L.
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

#if ENABLE(WEBXR) && USE(OPENXR)

#include "OpenXRInputMappings.h"
#include "OpenXRUtils.h"

#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebKit {

class OpenXRInputSource;

class OpenXRInput {
    WTF_MAKE_TZONE_ALLOCATED(OpenXRInput);
    WTF_MAKE_NONCOPYABLE(OpenXRInput);
public:
    static std::unique_ptr<OpenXRInput> create(XrInstance, XrSession, OpenXRSystemProperties&&);
    ~OpenXRInput();

    Vector<PlatformXR::FrameData::InputSource> collectInputSources(const XrFrameState&, XrSpace) const;
    void updateInteractionProfile();

    const Vector<UniqueRef<OpenXRInputSource>>& inputSources() LIFETIME_BOUND { return m_inputSources; }

private:
    OpenXRInput(XrInstance, XrSession);
    XrResult initialize(OpenXRSystemProperties&&);

    XrInstance m_instance { XR_NULL_HANDLE };
    XrSession m_session { XR_NULL_HANDLE };
    Vector<UniqueRef<OpenXRInputSource>> m_inputSources;
    PlatformXR::InputSourceHandle m_handleIndex { 0 };
};

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
