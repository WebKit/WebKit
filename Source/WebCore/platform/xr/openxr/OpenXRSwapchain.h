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

#pragma once

#if ENABLE(WEBXR) && USE(OPENXR)

#include "GraphicsContextGL.h"
#include "OpenXRUtils.h"

#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace PlatformXR {

class OpenXRSwapchain {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(OpenXRSwapchain);
public:
    static std::unique_ptr<OpenXRSwapchain> create(XrInstance, XrSession, const XrSwapchainCreateInfo&);
    ~OpenXRSwapchain();

    std::optional<PlatformGLObject> acquireImage();
    void releaseImage();
    XrSwapchain swapchain() const { return m_swapchain; }
    int32_t width() const { return m_createInfo.width; }
    int32_t height() const { return m_createInfo.height; }

private:
    OpenXRSwapchain(XrInstance, XrSwapchain, const XrSwapchainCreateInfo&, Vector<XrSwapchainImageOpenGLKHR>&&);

    XrInstance m_instance;
    XrSwapchain m_swapchain;
    XrSwapchainCreateInfo m_createInfo;
    Vector<XrSwapchainImageOpenGLKHR> m_imageBuffers;
    PlatformGLObject m_acquiredTexture { 0 };
};


} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(OPENXR)
