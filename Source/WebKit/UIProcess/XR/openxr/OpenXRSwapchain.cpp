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

#if ENABLE(WEBXR) && USE(OPENXR)
#include "OpenXRSwapchain.h"

#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(OpenXRSwapchain);

std::unique_ptr<OpenXRSwapchain> OpenXRSwapchain::create(XrSession session, const XrSwapchainCreateInfo& info, HasAlpha hasAlpha)
{
    ASSERT(session != XR_NULL_HANDLE);
    ASSERT(info.faceCount == 1);

    XrSwapchain swapchain { XR_NULL_HANDLE };
    CHECK_XRCMD(xrCreateSwapchain(session, &info, &swapchain));
    if (swapchain == XR_NULL_HANDLE) {
        LOG(XR, "xrCreateSwapchain() failed: swapchain is null");
        return nullptr;
    }

    uint32_t imageCount;
    CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr));
    if (!imageCount) {
        LOG(XR, "xrEnumerateSwapchainImages(): no images\n");
        return nullptr;
    }

    Vector<XrSwapchainImageOpenGLESKHR> imageBuffers(imageCount, [] {
        return createOpenXRStruct<XrSwapchainImageOpenGLESKHR, XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR>();
    }());

    Vector<XrSwapchainImageBaseHeader*> imageHeaders = imageBuffers.map([](auto& image) {
        return (XrSwapchainImageBaseHeader*) &image;
    });

    // Get images from an XrSwapchain
    CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount, imageHeaders[0]));

    return std::unique_ptr<OpenXRSwapchain>(new OpenXRSwapchain(swapchain, info, WTFMove(imageBuffers), hasAlpha));
}

OpenXRSwapchain::OpenXRSwapchain(XrSwapchain swapchain, const XrSwapchainCreateInfo& info, Vector<XrSwapchainImageOpenGLESKHR>&& imageBuffers, HasAlpha hasAlpha)
    : m_swapchain(swapchain)
    , m_createInfo(info)
    , m_imageBuffers(WTFMove(imageBuffers))
    , m_hasAlpha(hasAlpha)
{
}

OpenXRSwapchain::~OpenXRSwapchain()
{
    if (m_acquiredTexture)
        releaseImage();
    if (m_swapchain != XR_NULL_HANDLE)
        xrDestroySwapchain(m_swapchain);
}

std::optional<PlatformGLObject> OpenXRSwapchain::acquireImage()
{
    RELEASE_ASSERT_WITH_MESSAGE(!m_acquiredTexture , "Expected no acquired images. ReleaseImage not called?");

    auto acquireInfo = createOpenXRStruct<XrSwapchainImageAcquireInfo, XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO>();
    uint32_t swapchainImageIndex = 0;
    CHECK_XRCMD(xrAcquireSwapchainImage(m_swapchain, &acquireInfo, &swapchainImageIndex));
    ASSERT(swapchainImageIndex < m_imageBuffers.size());

    auto waitInfo = createOpenXRStruct<XrSwapchainImageWaitInfo, XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO>();
    waitInfo.timeout = XR_INFINITE_DURATION;
    CHECK_XRCMD(xrWaitSwapchainImage(m_swapchain, &waitInfo));

    m_acquiredTexture = m_imageBuffers[swapchainImageIndex].image;

    return m_acquiredTexture;
}

void OpenXRSwapchain::releaseImage()
{
    RELEASE_ASSERT_WITH_MESSAGE(m_acquiredTexture, "Expected a valid acquired image. AcquireImage not called?");

    auto releaseInfo = createOpenXRStruct<XrSwapchainImageReleaseInfo, XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO>();
    CHECK_XRCMD(xrReleaseSwapchainImage(m_swapchain, &releaseInfo));

    m_acquiredTexture = 0;
}

} // namespace WebKit

#endif // ENABLE(WEBXR) && USE(OPENXR)
