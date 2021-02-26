/*
 * Copyright (C) 2021 Igalia, S.L.
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

#include "config.h"
#include "OpenXRExtensions.h"

#if ENABLE(WEBXR) && USE(OPENXR)

using namespace WebCore;

namespace PlatformXR {

std::unique_ptr<OpenXRExtensions> OpenXRExtensions::create()
{
    uint32_t extensionCount { 0 };
    XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);

    if (XR_FAILED(result) || !extensionCount) {
        LOG(XR, "xrEnumerateInstanceExtensionProperties(): no extensions\n");
        return nullptr;
    }

    Vector<XrExtensionProperties> extensions(extensionCount, [] {
        return createStructure<XrExtensionProperties, XR_TYPE_EXTENSION_PROPERTIES>();
    }());

    result = xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensions.data());
    if (XR_FAILED(result)) {
        LOG(XR, "xrEnumerateInstanceExtensionProperties() failed: %d\n", result);
        return nullptr;
    }

    return makeUnique<OpenXRExtensions>(WTFMove(extensions));
}

OpenXRExtensions::OpenXRExtensions(Vector<XrExtensionProperties>&& extensions)
    : m_extensions(WTFMove(extensions))
{
}

void OpenXRExtensions::loadMethods(XrInstance instance)
{
#if USE(EGL)
    m_methods.getProcAddressFunc = eglGetProcAddress;
#endif
    xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&m_methods.xrGetOpenGLGraphicsRequirementsKHR));
}

bool OpenXRExtensions::isExtensionSupported(const char* name) const
{
    auto position = m_extensions.findMatching([name](auto& property) {
        return !strcmp(property.extensionName, name);
    });
    return position != notFound;
}

} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(OPENXR)
