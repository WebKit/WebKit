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

#include "config.h"

#if ENABLE(WEBXR) && USE(OPENXR)
#include "OpenXRSwapchain.h"

using namespace WebCore;

namespace PlatformXR {

std::unique_ptr<OpenXRSwapchain> OpenXRSwapchain::create(XrInstance instance, XrSession session, const XrSwapchainCreateInfo& info)
{
    ASSERT(session != XR_NULL_HANDLE);
    ASSERT(info.faceCount == 1);

    XrSwapchain swapchain { XR_NULL_HANDLE };
    auto result = xrCreateSwapchain(session, &info, &swapchain);
    RETURN_IF_FAILED(result, "xrEnumerateInstanceExtensionProperties", instance, nullptr);
    ASSERT(swapchain != XR_NULL_HANDLE);

    uint32_t imageCount;
    result = xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr);
    RETURN_IF_FAILED(result, "xrEnumerateSwapchainImages", instance, nullptr);
    if (!imageCount) {
        LOG(XR, "xrEnumerateSwapchainImages(): no images\n");
        return nullptr;
    }

    Vector<XrSwapchainImageOpenGLKHR> imageBuffers(imageCount, [] {
        return createStructure<XrSwapchainImageOpenGLKHR, XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR>();
    }());

    Vector<XrSwapchainImageBaseHeader*> imageHeaders = imageBuffers.map([](auto& image) mutable {
        return (XrSwapchainImageBaseHeader*) &image;
    });

    // Get images from an XrSwapchain
    result = xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount, imageHeaders[0]);
    RETURN_IF_FAILED(result, "xrEnumerateSwapchainImages with imageCount", instance, nullptr);

    return std::unique_ptr<OpenXRSwapchain>(new OpenXRSwapchain(instance, swapchain, info, WTFMove(imageBuffers)));
}

OpenXRSwapchain::OpenXRSwapchain(XrInstance instance, XrSwapchain swapchain, const XrSwapchainCreateInfo& info, Vector<XrSwapchainImageOpenGLKHR>&& imageBuffers)
    : m_instance(instance)
    , m_swapchain(swapchain)
    , m_createInfo(info)
    , m_imageBuffers(WTFMove(imageBuffers))
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
#if LOG_DISABLED
    UNUSED_VARIABLE(m_instance);
#endif

    RELEASE_ASSERT_WITH_MESSAGE(!m_acquiredTexture , "Expected no acquired images. ReleaseImage not called?");

    auto acquireInfo = createStructure<XrSwapchainImageAcquireInfo, XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO>();
    uint32_t swapchainImageIndex = 0;
    auto result = xrAcquireSwapchainImage(m_swapchain, &acquireInfo, &swapchainImageIndex);
    RETURN_IF_FAILED(result, "xrAcquireSwapchainImage", m_instance, std::nullopt);

    RELEASE_ASSERT(swapchainImageIndex < m_imageBuffers.size());

    auto waitInfo = createStructure<XrSwapchainImageWaitInfo, XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO>();
    waitInfo.timeout = XR_INFINITE_DURATION;
    result = xrWaitSwapchainImage(m_swapchain, &waitInfo);
    RETURN_IF_FAILED(result, "xrWaitSwapchainImage", m_instance, std::nullopt);

    m_acquiredTexture = m_imageBuffers[swapchainImageIndex].image;

    return m_acquiredTexture;
}

void OpenXRSwapchain::releaseImage()
{
    RELEASE_ASSERT_WITH_MESSAGE(m_acquiredTexture, "Expected a valid acquired image. AcquireImage not called?");

    auto releaseInfo = createStructure<XrSwapchainImageReleaseInfo, XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO>();
    auto result = xrReleaseSwapchainImage(m_swapchain, &releaseInfo);
    LOG_IF_FAILED(result, "xrReleaseSwapchainImage", m_instance);

    m_acquiredTexture = 0;
}

} // namespace PlatformXR

#endif // ENABLE(WEBXR) && USE(OPENXR)
