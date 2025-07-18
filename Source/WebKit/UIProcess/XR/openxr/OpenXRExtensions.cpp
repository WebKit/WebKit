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

#include "config.h"
#include "OpenXRExtensions.h"

#if ENABLE(WEBXR) && USE(OPENXR)

#include "OpenXRUtils.h"
#if USE(LIBEPOXY)
#define __GBM__ 1
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#endif
#include <openxr/openxr_platform.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

std::unique_ptr<OpenXRExtensions> OpenXRExtensions::create()
{
    uint32_t extensionCount;
    XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);

    if (XR_FAILED(result) || !extensionCount) {
        LOG(XR, "xrEnumerateInstanceExtensionProperties(): no extensions\n");
        return nullptr;
    }

    Vector<XrExtensionProperties> extensions(extensionCount, [] {
        return createOpenXRStruct<XrExtensionProperties, XR_TYPE_EXTENSION_PROPERTIES>();
    }());

    result = xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensions.mutableSpan().data());
    if (XR_FAILED(result)) {
        LOG(XR, "xrEnumerateInstanceExtensionProperties() failed: %d\n", result);
        return nullptr;
    }

    return makeUnique<OpenXRExtensions>(WTFMove(extensions));
}

OpenXRExtensions::OpenXRExtensions(Vector<XrExtensionProperties>&& extensions)
    : m_extensions(WTFMove(extensions))
    , m_methods(makeUnique<OpenXRExtensionMethods>())
{
}

// Destructor must be explicitly defined here because at this point OpenXRExtensionMethods is already defined.
// If we don't do this, the compiler will try to generate the default destructor for this class the first time
// it finds it which might be too early, in the sense that the struct is not defined yet and thus it will fail.
OpenXRExtensions::~OpenXRExtensions() = default;

bool OpenXRExtensions::loadMethods(XrInstance instance)
{
#if defined(XR_USE_PLATFORM_EGL)
    m_methods->getProcAddressFunc = eglGetProcAddress;
    if (!m_methods->getProcAddressFunc) {
        LOG(XR, "Failed to load eglGetProcAddress");
        return false;
    }
#endif
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    xrGetInstanceProcAddr(instance, "xrGetOpenGLESGraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&m_methods->xrGetOpenGLESGraphicsRequirementsKHR));
    if (!m_methods->xrGetOpenGLESGraphicsRequirementsKHR) {
        LOG(XR, "Failed to load xrGetOpenGLESGraphicsRequirementsKHR");
        return false;
    }
#endif
    return true;
}

bool OpenXRExtensions::isExtensionSupported(std::span<const char> name) const
{
    auto position = m_extensions.findIf([name](auto& property) {
        return equalSpans(unsafeSpan(property.extensionName), name);
    });
    return position != notFound;
}

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
