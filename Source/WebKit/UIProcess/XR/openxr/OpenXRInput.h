/*
 * Copyright (C) 2025 Igalia, S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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
