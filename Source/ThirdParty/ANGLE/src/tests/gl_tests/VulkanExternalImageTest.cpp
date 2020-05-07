//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VulkanExternalImageTest.cpp : Tests of images allocated externally using Vulkan.

#include "test_utils/ANGLETest.h"

#include "common/debug.h"
#include "test_utils/VulkanExternalHelper.h"
#include "test_utils/gl_raii.h"

namespace angle
{

namespace
{

constexpr int kInvalidFd = -1;

VkFormat ChooseAnyImageFormat(const VulkanExternalHelper &helper)
{
    static constexpr VkFormat kFormats[] = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
    };

    for (VkFormat format : kFormats)
    {
        if (helper.canCreateImageOpaqueFd(format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL))
        {
            return format;
        }
    }

    return VK_FORMAT_UNDEFINED;
}

}  // namespace

class VulkanExternalImageTest : public ANGLETest
{
  protected:
    VulkanExternalImageTest()
    {
        setWindowWidth(1);
        setWindowHeight(1);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// glImportMemoryFdEXT must be able to import a valid opaque fd.
TEST_P(VulkanExternalImageTest, ShouldImportMemoryOpaqueFd)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(isSwiftshader());
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_memory_object_fd"));

    VulkanExternalHelper helper;
    helper.initialize();

    VkFormat format = ChooseAnyImageFormat(helper);
    ANGLE_SKIP_TEST_IF(format == VK_FORMAT_UNDEFINED);

    VkImage image                 = VK_NULL_HANDLE;
    VkDeviceMemory deviceMemory   = VK_NULL_HANDLE;
    VkDeviceSize deviceMemorySize = 0;

    VkExtent3D extent = {1, 1, 1};
    VkResult result =
        helper.createImage2DOpaqueFd(format, extent, &image, &deviceMemory, &deviceMemorySize);
    EXPECT_EQ(result, VK_SUCCESS);

    int fd = kInvalidFd;
    result = helper.exportMemoryOpaqueFd(deviceMemory, &fd);
    EXPECT_EQ(result, VK_SUCCESS);
    EXPECT_NE(fd, kInvalidFd);

    {
        GLMemoryObject memoryObject;
        glImportMemoryFdEXT(memoryObject, deviceMemorySize, GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);
    }

    EXPECT_GL_NO_ERROR();

    vkDestroyImage(helper.getDevice(), image, nullptr);
    vkFreeMemory(helper.getDevice(), deviceMemory, nullptr);
}

// glImportSemaphoreFdEXT must be able to import a valid opaque fd.
TEST_P(VulkanExternalImageTest, ShouldImportSemaphoreOpaqueFd)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(isSwiftshader());
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_semaphore_fd"));

    VulkanExternalHelper helper;
    helper.initialize();

    ANGLE_SKIP_TEST_IF(!helper.canCreateSemaphoreOpaqueFd());

    VkSemaphore vkSemaphore = VK_NULL_HANDLE;
    VkResult result         = helper.createSemaphoreOpaqueFd(&vkSemaphore);
    EXPECT_EQ(result, VK_SUCCESS);

    int fd = kInvalidFd;
    result = helper.exportSemaphoreOpaqueFd(vkSemaphore, &fd);
    EXPECT_EQ(result, VK_SUCCESS);
    EXPECT_NE(fd, kInvalidFd);

    {
        GLSemaphore glSemaphore;
        glImportSemaphoreFdEXT(glSemaphore, GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);
    }

    EXPECT_GL_NO_ERROR();

    vkDestroySemaphore(helper.getDevice(), vkSemaphore, nullptr);
}

// Test creating and clearing a simple RGBA8 texture in a opaque fd.
TEST_P(VulkanExternalImageTest, ShouldClearOpaqueFdRGBA8)
{
    // http://anglebug.com/4229
    ANGLE_SKIP_TEST_IF(IsVulkan());
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_memory_object_fd"));

    VulkanExternalHelper helper;
    helper.initialize();

    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    ANGLE_SKIP_TEST_IF(
        !helper.canCreateImageOpaqueFd(format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL));

    VkImage image                 = VK_NULL_HANDLE;
    VkDeviceMemory deviceMemory   = VK_NULL_HANDLE;
    VkDeviceSize deviceMemorySize = 0;

    VkExtent3D extent = {1, 1, 1};
    VkResult result =
        helper.createImage2DOpaqueFd(format, extent, &image, &deviceMemory, &deviceMemorySize);
    EXPECT_EQ(result, VK_SUCCESS);

    int fd = kInvalidFd;
    result = helper.exportMemoryOpaqueFd(deviceMemory, &fd);
    EXPECT_EQ(result, VK_SUCCESS);
    EXPECT_NE(fd, kInvalidFd);

    {
        GLMemoryObject memoryObject;
        glImportMemoryFdEXT(memoryObject, deviceMemorySize, GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);

        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexStorageMem2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1, memoryObject, 0);

        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

        glClearColor(0.5f, 0.5f, 0.5f, 0.5f);
        glClear(GL_COLOR_BUFFER_BIT);

        EXPECT_PIXEL_NEAR(0, 0, 128, 128, 128, 128, 1.0);
    }

    EXPECT_GL_NO_ERROR();

    vkDestroyImage(helper.getDevice(), image, nullptr);
    vkFreeMemory(helper.getDevice(), deviceMemory, nullptr);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(VulkanExternalImageTest);

}  // namespace angle
