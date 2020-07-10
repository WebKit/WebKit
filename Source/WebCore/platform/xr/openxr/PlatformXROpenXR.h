/*
 * Copyright (C) 2020 Igalia, S.L.
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

#if ENABLE(WEBXR)
#include "PlatformXR.h"

#include <wtf/HashMap.h>

#if USE_OPENXR
#include <openxr/openxr.h>

namespace PlatformXR {

// https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#system
// A system represents a collection of related devices in the runtime, often made up of several individual
// hardware components working together to enable XR experiences.
//
// WebXR talks about XR devices, is a physical unit of hardware that can present imagery to the user, so
// there is not direct correspondence between an OpenXR system and a WebXR device because the system API
// is an abstraction for a collection of devices while the WebXR device is mostly one physical unit,
// usually an HMD or a phone/tablet.
//
// It's important also not to try to associate OpenXR system with WebXR's XRSystem because they're totally
// different concepts. The system in OpenXR was defined above as a collection of related devices. In WebXR,
// the XRSystem is basically the entry point for the WebXR API available via the Navigator object.
class OpenXRDevice final : public Device {
public:
    OpenXRDevice(XrSystemId, XrInstance);
    XrSystemId xrSystemId() const { return m_systemId; }
private:
    void collectSupportedSessionModes();
    void enumerateConfigurationViews();

    WebCore::IntSize recommendedResolution(SessionMode) final;

    using ViewConfigurationPropertiesMap = HashMap<XrViewConfigurationType, XrViewConfigurationProperties, IntHash<XrViewConfigurationType>, WTF::StrongEnumHashTraits<XrViewConfigurationType>>;
    ViewConfigurationPropertiesMap m_viewConfigurationProperties;
    using ViewConfigurationViewsMap = HashMap<XrViewConfigurationType, Vector<XrViewConfigurationView>, IntHash<XrViewConfigurationType>, WTF::StrongEnumHashTraits<XrViewConfigurationType>>;
    ViewConfigurationViewsMap m_configurationViews;

    XrSystemId m_systemId;
    XrInstance m_instance;
};

} // namespace PlatformXR

#endif // USE_OPENXR
#endif // ENABLE(WEBXR)
