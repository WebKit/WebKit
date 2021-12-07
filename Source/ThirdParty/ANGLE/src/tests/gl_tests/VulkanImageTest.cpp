//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VulkanImageTest.cpp : Tests of EGL_ANGLE_vulkan_image & GL_ANGLE_vulkan_image extensions.

#include "test_utils/ANGLETest.h"

#include "common/debug.h"
#include "test_utils/VulkanHelper.h"
#include "test_utils/gl_raii.h"

namespace angle
{

constexpr GLuint kWidth  = 64u;
constexpr GLuint kHeight = 64u;
constexpr GLuint kWhite  = 0xffffffff;
constexpr GLuint kRed    = 0xff0000ff;

using VulkanImageTest = ANGLETest;

TEST_P(VulkanImageTest, DeviceVulkan)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    EXPECT_TRUE(IsEGLClientExtensionEnabled("EGL_EXT_device_query"));
    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    EGLAttrib result = 0;
    EXPECT_EGL_TRUE(eglQueryDisplayAttribEXT(display, EGL_DEVICE_EXT, &result));

    EGLDeviceEXT device = reinterpret_cast<EGLDeviceEXT>(result);
    EXPECT_NE(EGL_NO_DEVICE_EXT, device);
    EXPECT_TRUE(IsEGLDeviceExtensionEnabled(device, "EGL_ANGLE_device_vulkan"));

    EXPECT_EGL_TRUE(eglQueryDeviceAttribEXT(device, EGL_VULKAN_INSTANCE_ANGLE, &result));
    VkInstance instance = reinterpret_cast<VkInstance>(result);
    EXPECT_NE(instance, static_cast<VkInstance>(VK_NULL_HANDLE));

    EXPECT_EGL_TRUE(eglQueryDeviceAttribEXT(device, EGL_VULKAN_PHYSICAL_DEVICE_ANGLE, &result));
    VkPhysicalDevice physical_device = reinterpret_cast<VkPhysicalDevice>(result);
    EXPECT_NE(physical_device, static_cast<VkPhysicalDevice>(VK_NULL_HANDLE));

    EXPECT_EGL_TRUE(eglQueryDeviceAttribEXT(device, EGL_VULKAN_DEVICE_ANGLE, &result));
    VkDevice vk_device = reinterpret_cast<VkDevice>(result);
    EXPECT_NE(vk_device, static_cast<VkDevice>(VK_NULL_HANDLE));

    EXPECT_EGL_TRUE(eglQueryDeviceAttribEXT(device, EGL_VULKAN_QUEUE_ANGLE, &result));
    VkQueue queue = reinterpret_cast<VkQueue>(result);
    EXPECT_NE(queue, static_cast<VkQueue>(VK_NULL_HANDLE));

    EXPECT_EGL_TRUE(eglQueryDeviceAttribEXT(device, EGL_VULKAN_QUEUE_FAMILIY_INDEX_ANGLE, &result));

    {
        EXPECT_EGL_TRUE(
            eglQueryDeviceAttribEXT(device, EGL_VULKAN_DEVICE_EXTENSIONS_ANGLE, &result));
        const char *const *extensions = reinterpret_cast<const char *const *>(result);
        EXPECT_NE(extensions, nullptr);
        int extension_count = 0;
        while (extensions[extension_count])
        {
            extension_count++;
        }
        EXPECT_NE(extension_count, 0);
    }

    {
        EXPECT_EGL_TRUE(
            eglQueryDeviceAttribEXT(device, EGL_VULKAN_INSTANCE_EXTENSIONS_ANGLE, &result));
        const char *const *extensions = reinterpret_cast<const char *const *>(result);
        EXPECT_NE(extensions, nullptr);
        int extension_count = 0;
        while (extensions[extension_count])
        {
            extension_count++;
        }
        EXPECT_NE(extension_count, 0);
    }

    EXPECT_EGL_TRUE(eglQueryDeviceAttribEXT(device, EGL_VULKAN_FEATURES_ANGLE, &result));
    const VkPhysicalDeviceFeatures2KHR *features =
        reinterpret_cast<const VkPhysicalDeviceFeatures2KHR *>(result);
    EXPECT_NE(features, nullptr);
    EXPECT_EQ(features->sType, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);

    EXPECT_EGL_TRUE(eglQueryDeviceAttribEXT(device, EGL_VULKAN_GET_INSTANCE_PROC_ADDR, &result));
    PFN_vkGetInstanceProcAddr get_instance_proc_addr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(result);
    EXPECT_NE(get_instance_proc_addr, nullptr);
}

TEST_P(VulkanImageTest, ExportVKImage)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    EXPECT_TRUE(IsEGLDisplayExtensionEnabled(display, "EGL_ANGLE_vulkan_image"));
    EXPECT_TRUE(IsGLExtensionEnabled("GL_ANGLE_vulkan_image"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    EXPECT_GL_NO_ERROR();

    EGLContext context   = window->getContext();
    EGLImageKHR eglImage = eglCreateImageKHR(
        display, context, EGL_GL_TEXTURE_2D_KHR,
        reinterpret_cast<EGLClientBuffer>(static_cast<uintptr_t>(texture)), nullptr);
    EXPECT_NE(eglImage, EGL_NO_IMAGE_KHR);

    VkImage vkImage        = VK_NULL_HANDLE;
    VkImageCreateInfo info = {};
    EXPECT_EGL_TRUE(eglExportVkImageANGLE(display, eglImage, &vkImage, &info));
    EXPECT_NE(vkImage, static_cast<VkImage>(VK_NULL_HANDLE));
    EXPECT_EQ(info.sType, VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
    EXPECT_EQ(info.pNext, nullptr);
    EXPECT_EQ(info.imageType, VK_IMAGE_TYPE_2D);
    EXPECT_EQ(info.format, VK_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(info.extent.width, kWidth);
    EXPECT_EQ(info.extent.height, kHeight);
    EXPECT_EQ(info.extent.depth, 1u);
    EXPECT_EQ(info.queueFamilyIndexCount, 0u);
    EXPECT_EQ(info.pQueueFamilyIndices, nullptr);
    EXPECT_EQ(info.initialLayout, VK_IMAGE_LAYOUT_UNDEFINED);

    EXPECT_EGL_TRUE(eglDestroyImageKHR(display, eglImage));
}

TEST_P(VulkanImageTest, PixelTestTexImage2D)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    VulkanHelper helper;
    helper.initializeFromANGLE();

    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    EXPECT_TRUE(IsEGLDisplayExtensionEnabled(display, "EGL_ANGLE_vulkan_image"));
    EXPECT_TRUE(IsGLExtensionEnabled("GL_ANGLE_vulkan_image"));

    constexpr GLuint kColor = 0xafbfcfdf;

    GLTexture texture;

    {
        glBindTexture(GL_TEXTURE_2D, texture);
        std::vector<GLuint> pixels(kWidth * kHeight, kColor);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    EGLContext context   = window->getContext();
    EGLImageKHR eglImage = eglCreateImageKHR(
        display, context, EGL_GL_TEXTURE_2D_KHR,
        reinterpret_cast<EGLClientBuffer>(static_cast<uintptr_t>(texture)), nullptr);
    EXPECT_NE(eglImage, EGL_NO_IMAGE_KHR);

    VkImage vkImage        = VK_NULL_HANDLE;
    VkImageCreateInfo info = {};
    EXPECT_EGL_TRUE(eglExportVkImageANGLE(display, eglImage, &vkImage, &info));
    EXPECT_NE(vkImage, static_cast<VkImage>(VK_NULL_HANDLE));

    GLuint textures[1] = {texture};
    GLenum layouts[1]  = {GL_NONE};
    glReleaseTexturesANGLE(1, textures, layouts);
    EXPECT_EQ(layouts[0], static_cast<GLenum>(GL_LAYOUT_TRANSFER_DST_EXT));

    {
        std::vector<GLuint> pixels(kWidth * kHeight);
        helper.readPixels(vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, info.format, {},
                          info.extent, pixels.data(), pixels.size() * sizeof(GLuint));
        EXPECT_EQ(pixels, std::vector<GLuint>(kWidth * kHeight, kColor));
    }

    layouts[0] = GL_LAYOUT_TRANSFER_SRC_EXT;
    glAcquireTexturesANGLE(1, textures, layouts);

    EXPECT_GL_NO_ERROR();
    EXPECT_EGL_TRUE(eglDestroyImageKHR(display, eglImage));
}

TEST_P(VulkanImageTest, PixelTestClear)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    VulkanHelper helper;
    helper.initializeFromANGLE();

    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    EGLContext context   = window->getContext();
    EGLImageKHR eglImage = eglCreateImageKHR(
        display, context, EGL_GL_TEXTURE_2D_KHR,
        reinterpret_cast<EGLClientBuffer>(static_cast<uintptr_t>(texture)), nullptr);
    EXPECT_NE(eglImage, EGL_NO_IMAGE_KHR);

    VkImage vkImage        = VK_NULL_HANDLE;
    VkImageCreateInfo info = {};
    EXPECT_EGL_TRUE(eglExportVkImageANGLE(display, eglImage, &vkImage, &info));
    EXPECT_NE(vkImage, static_cast<VkImage>(VK_NULL_HANDLE));

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    glViewport(0, 0, kWidth, kHeight);
    // clear framebuffer with white color.
    glClearColor(1.f, 1.f, 1.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    GLuint textures[1] = {texture};
    GLenum layouts[1]  = {GL_NONE};
    glReleaseTexturesANGLE(1, textures, layouts);
    EXPECT_EQ(layouts[0], static_cast<GLenum>(GL_LAYOUT_TRANSFER_DST_EXT));

    std::vector<GLuint> pixels(kWidth * kHeight);
    helper.readPixels(vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, info.format, {}, info.extent,
                      pixels.data(), pixels.size() * sizeof(GLuint));
    EXPECT_EQ(pixels, std::vector<GLuint>(kWidth * kHeight, kWhite));

    layouts[0] = GL_LAYOUT_TRANSFER_SRC_EXT;
    glAcquireTexturesANGLE(1, textures, layouts);

    // clear framebuffer with red color.
    glClearColor(1.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    glReleaseTexturesANGLE(1, textures, layouts);
    EXPECT_EQ(layouts[0], static_cast<GLenum>(GL_LAYOUT_TRANSFER_DST_EXT));

    helper.readPixels(vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, info.format, {}, info.extent,
                      pixels.data(), pixels.size() * sizeof(GLuint));
    EXPECT_EQ(pixels, std::vector<GLuint>(kWidth * kHeight, kRed));

    layouts[0] = GL_LAYOUT_TRANSFER_SRC_EXT;
    glAcquireTexturesANGLE(1, textures, layouts);

    EXPECT_GL_NO_ERROR();
    EXPECT_EGL_TRUE(eglDestroyImageKHR(display, eglImage));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

TEST_P(VulkanImageTest, PixelTestDrawQuad)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    VulkanHelper helper;
    helper.initializeFromANGLE();

    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    EGLContext context   = window->getContext();
    EGLImageKHR eglImage = eglCreateImageKHR(
        display, context, EGL_GL_TEXTURE_2D_KHR,
        reinterpret_cast<EGLClientBuffer>(static_cast<uintptr_t>(texture)), nullptr);
    EXPECT_NE(eglImage, EGL_NO_IMAGE_KHR);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    glViewport(0, 0, kWidth, kHeight);
    // clear framebuffer with black color.
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // draw red quad
    ANGLE_GL_PROGRAM(drawRed, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    drawQuad(drawRed, essl1_shaders::PositionAttrib(), 0.5f);

    GLuint textures[1] = {texture};
    GLenum layouts[1]  = {GL_NONE};
    glReleaseTexturesANGLE(1, textures, layouts);
    EXPECT_EQ(layouts[0], static_cast<GLenum>(GL_LAYOUT_COLOR_ATTACHMENT_EXT));

    VkImage vkImage        = VK_NULL_HANDLE;
    VkImageCreateInfo info = {};
    EXPECT_EGL_TRUE(eglExportVkImageANGLE(display, eglImage, &vkImage, &info));
    EXPECT_NE(vkImage, static_cast<VkImage>(VK_NULL_HANDLE));

    std::vector<GLuint> pixels(kWidth * kHeight);
    helper.readPixels(vkImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, info.format, {},
                      info.extent, pixels.data(), pixels.size() * sizeof(GLuint));
    EXPECT_EQ(pixels, std::vector<GLuint>(kWidth * kHeight, kRed));

    layouts[0] = GL_LAYOUT_TRANSFER_SRC_EXT;
    glAcquireTexturesANGLE(1, textures, layouts);

    EXPECT_GL_NO_ERROR();
    EXPECT_EGL_TRUE(eglDestroyImageKHR(display, eglImage));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(VulkanImageTest);

}  // namespace angle
