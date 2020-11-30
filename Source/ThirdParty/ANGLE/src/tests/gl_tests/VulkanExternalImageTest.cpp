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

// List of VkFormat/internalformat combinations Chrome uses.
// This is compiled from the maps in
// components/viz/common/resources/resource_format_utils.cc.
const struct ImageFormatPair
{
    VkFormat vkFormat;
    GLenum internalFormat;
    const char *requiredExtension;
} kChromeFormats[] = {
    {VK_FORMAT_R8G8B8A8_UNORM, GL_RGBA8_OES},                    // RGBA_8888
    {VK_FORMAT_B8G8R8A8_UNORM, GL_BGRA8_EXT},                    // BGRA_8888
    {VK_FORMAT_R4G4B4A4_UNORM_PACK16, GL_RGBA4},                 // RGBA_4444
    {VK_FORMAT_R16G16B16A16_SFLOAT, GL_RGBA16F_EXT},             // RGBA_F16
    {VK_FORMAT_R8_UNORM, GL_R8_EXT},                             // RED_8
    {VK_FORMAT_R5G6B5_UNORM_PACK16, GL_RGB565},                  // RGB_565
    {VK_FORMAT_R16_UNORM, GL_R16_EXT, "GL_EXT_texture_norm16"},  // R16_EXT
    {VK_FORMAT_A2B10G10R10_UNORM_PACK32, GL_RGB10_A2_EXT},       // RGBA_1010102
    {VK_FORMAT_R8_UNORM, GL_ALPHA8_EXT},                         // ALPHA_8
    {VK_FORMAT_R8_UNORM, GL_LUMINANCE8_EXT},                     // LUMINANCE_8
    {VK_FORMAT_R8G8_UNORM, GL_RG8_EXT},                          // RG_88

    // TODO(spang): Chrome could use GL_RGBA8_OES here if we can solve a couple
    // of validation comformance issues (see crbug.com/1058521). Or, we can add
    // a new internalformat that's unambiguously R8G8B8X8 in ANGLE and use that.
    {VK_FORMAT_R8G8B8A8_UNORM, GL_RGB8_OES},  // RGBX_8888
};

struct OpaqueFdTraits
{
    using Handle = int;
    static Handle InvalidHandle() { return kInvalidFd; }

    static const char *MemoryObjectExtension() { return "GL_EXT_memory_object_fd"; }
    static const char *SemaphoreExtension() { return "GL_EXT_semaphore_fd"; }

    static bool CanCreateSemaphore(const VulkanExternalHelper &helper)
    {
        return helper.canCreateSemaphoreOpaqueFd();
    }

    static VkResult CreateSemaphore(VulkanExternalHelper *helper, VkSemaphore *semaphore)
    {
        return helper->createSemaphoreOpaqueFd(semaphore);
    }

    static VkResult ExportSemaphore(VulkanExternalHelper *helper,
                                    VkSemaphore semaphore,
                                    Handle *handle)
    {
        return helper->exportSemaphoreOpaqueFd(semaphore, handle);
    }

    static void ImportSemaphore(GLuint semaphore, Handle handle)
    {
        glImportSemaphoreFdEXT(semaphore, GL_HANDLE_TYPE_OPAQUE_FD_EXT, handle);
    }

    static bool CanCreateImage(const VulkanExternalHelper &helper,
                               VkFormat format,
                               VkImageType type,
                               VkImageTiling tiling)
    {
        return helper.canCreateImageOpaqueFd(format, type, tiling);
    }

    static VkResult CreateImage2D(VulkanExternalHelper *helper,
                                  VkFormat format,
                                  VkExtent3D extent,
                                  VkImage *imageOut,
                                  VkDeviceMemory *deviceMemoryOut,
                                  VkDeviceSize *deviceMemorySizeOut)
    {
        return helper->createImage2DOpaqueFd(format, extent, imageOut, deviceMemoryOut,
                                             deviceMemorySizeOut);
    }

    static VkResult ExportMemory(VulkanExternalHelper *helper,
                                 VkDeviceMemory deviceMemory,
                                 Handle *handle)
    {
        return helper->exportMemoryOpaqueFd(deviceMemory, handle);
    }

    static void ImportMemory(GLuint memoryObject, GLuint64 size, Handle handle)
    {
        glImportMemoryFdEXT(memoryObject, size, GL_HANDLE_TYPE_OPAQUE_FD_EXT, handle);
    }
};

struct FuchsiaTraits
{
    using Handle = zx_handle_t;

    static Handle InvalidHandle() { return ZX_HANDLE_INVALID; }

    static const char *MemoryObjectExtension() { return "GL_ANGLE_memory_object_fuchsia"; }
    static const char *SemaphoreExtension() { return "GL_ANGLE_semaphore_fuchsia"; }

    static bool CanCreateSemaphore(const VulkanExternalHelper &helper)
    {
        return helper.canCreateSemaphoreZirconEvent();
    }

    static VkResult CreateSemaphore(VulkanExternalHelper *helper, VkSemaphore *semaphore)
    {
        return helper->createSemaphoreZirconEvent(semaphore);
    }

    static VkResult ExportSemaphore(VulkanExternalHelper *helper,
                                    VkSemaphore semaphore,
                                    Handle *handle)
    {
        return helper->exportSemaphoreZirconEvent(semaphore, handle);
    }

    static void ImportSemaphore(GLuint semaphore, Handle handle)
    {
        glImportSemaphoreZirconHandleANGLE(semaphore, GL_HANDLE_TYPE_ZIRCON_EVENT_ANGLE, handle);
    }

    static bool CanCreateImage(const VulkanExternalHelper &helper,
                               VkFormat format,
                               VkImageType type,
                               VkImageTiling tiling)
    {
        return helper.canCreateImageZirconVmo(format, type, tiling);
    }

    static VkResult CreateImage2D(VulkanExternalHelper *helper,
                                  VkFormat format,
                                  VkExtent3D extent,
                                  VkImage *imageOut,
                                  VkDeviceMemory *deviceMemoryOut,
                                  VkDeviceSize *deviceMemorySizeOut)
    {
        return helper->createImage2DZirconVmo(format, extent, imageOut, deviceMemoryOut,
                                              deviceMemorySizeOut);
    }

    static VkResult ExportMemory(VulkanExternalHelper *helper,
                                 VkDeviceMemory deviceMemory,
                                 Handle *handle)
    {
        return helper->exportMemoryZirconVmo(deviceMemory, handle);
    }

    static void ImportMemory(GLuint memoryObject, GLuint64 size, Handle handle)
    {
        glImportMemoryZirconHandleANGLE(memoryObject, size, GL_HANDLE_TYPE_ZIRCON_VMO_ANGLE,
                                        handle);
    }
};

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

template <typename Traits>
void RunShouldImportMemoryTest(bool isSwiftshader, bool enableDebugLayers)
{
    ASSERT(EnsureGLExtensionEnabled(Traits::MemoryObjectExtension()));

    VulkanExternalHelper helper;
    helper.initialize(isSwiftshader, enableDebugLayers);

    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    ANGLE_SKIP_TEST_IF(
        !Traits::CanCreateImage(helper, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL));

    VkImage image                 = VK_NULL_HANDLE;
    VkDeviceMemory deviceMemory   = VK_NULL_HANDLE;
    VkDeviceSize deviceMemorySize = 0;

    VkExtent3D extent = {1, 1, 1};
    VkResult result =
        Traits::CreateImage2D(&helper, format, extent, &image, &deviceMemory, &deviceMemorySize);
    EXPECT_EQ(result, VK_SUCCESS);

    typename Traits::Handle memoryHandle = Traits::InvalidHandle();
    result = Traits::ExportMemory(&helper, deviceMemory, &memoryHandle);
    EXPECT_EQ(result, VK_SUCCESS);
    EXPECT_NE(memoryHandle, Traits::InvalidHandle());

    {
        GLMemoryObject memoryObject;
        GLint dedicatedMemory = GL_TRUE;
        glMemoryObjectParameterivEXT(memoryObject, GL_DEDICATED_MEMORY_OBJECT_EXT,
                                     &dedicatedMemory);
        Traits::ImportMemory(memoryObject, deviceMemorySize, memoryHandle);

        // Test that after calling glImportMemoryFdEXT, the parameters of the memory object cannot
        // be changed
        dedicatedMemory = GL_FALSE;
        glMemoryObjectParameterivEXT(memoryObject, GL_DEDICATED_MEMORY_OBJECT_EXT,
                                     &dedicatedMemory);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }

    EXPECT_GL_NO_ERROR();

    vkDestroyImage(helper.getDevice(), image, nullptr);
    vkFreeMemory(helper.getDevice(), deviceMemory, nullptr);
}

// glImportMemoryFdEXT must be able to import a valid opaque fd.
TEST_P(VulkanExternalImageTest, ShouldImportMemoryOpaqueFd)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_memory_object_fd"));
    RunShouldImportMemoryTest<OpaqueFdTraits>(isSwiftshader(), enableDebugLayers());
}

// glImportMemoryZirconHandleANGLE must be able to import a valid vmo.
TEST_P(VulkanExternalImageTest, ShouldImportMemoryZirconVmo)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_ANGLE_memory_object_fuchsia"));
    RunShouldImportMemoryTest<FuchsiaTraits>(isSwiftshader(), enableDebugLayers());
}

template <typename Traits>
void RunShouldImportSemaphoreTest(bool isSwiftshader, bool enableDebugLayers)
{
    ASSERT(EnsureGLExtensionEnabled(Traits::SemaphoreExtension()));

    VulkanExternalHelper helper;
    helper.initialize(isSwiftshader, enableDebugLayers);

    ANGLE_SKIP_TEST_IF(!Traits::CanCreateSemaphore(helper));

    VkSemaphore vkSemaphore = VK_NULL_HANDLE;
    VkResult result         = helper.createSemaphoreOpaqueFd(&vkSemaphore);
    EXPECT_EQ(result, VK_SUCCESS);

    typename Traits::Handle semaphoreHandle = Traits::InvalidHandle();
    result = Traits::ExportSemaphore(&helper, vkSemaphore, &semaphoreHandle);
    EXPECT_EQ(result, VK_SUCCESS);
    EXPECT_NE(semaphoreHandle, Traits::InvalidHandle());

    {
        GLSemaphore glSemaphore;
        Traits::ImportSemaphore(glSemaphore, semaphoreHandle);
    }

    EXPECT_GL_NO_ERROR();

    vkDestroySemaphore(helper.getDevice(), vkSemaphore, nullptr);
}

// glImportSemaphoreFdEXT must be able to import a valid opaque fd.
TEST_P(VulkanExternalImageTest, ShouldImportSemaphoreOpaqueFd)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_semaphore_fd"));
    RunShouldImportSemaphoreTest<OpaqueFdTraits>(isSwiftshader(), enableDebugLayers());
}

// glImportSemaphoreZirconHandleANGLE must be able to import a valid handle.
TEST_P(VulkanExternalImageTest, ShouldImportSemaphoreZirconEvent)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_ANGLE_semaphore_fuchsia"));
    RunShouldImportSemaphoreTest<FuchsiaTraits>(isSwiftshader(), enableDebugLayers());
}

template <typename Traits>
void RunShouldClearTest(bool isSwiftshader, bool enableDebugLayers)
{
    ASSERT(EnsureGLExtensionEnabled(Traits::MemoryObjectExtension()));

    VulkanExternalHelper helper;
    helper.initialize(isSwiftshader, enableDebugLayers);

    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    ANGLE_SKIP_TEST_IF(
        !Traits::CanCreateImage(helper, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL));

    VkImage image                 = VK_NULL_HANDLE;
    VkDeviceMemory deviceMemory   = VK_NULL_HANDLE;
    VkDeviceSize deviceMemorySize = 0;

    VkExtent3D extent = {1, 1, 1};
    VkResult result =
        Traits::CreateImage2D(&helper, format, extent, &image, &deviceMemory, &deviceMemorySize);
    EXPECT_EQ(result, VK_SUCCESS);

    typename Traits::Handle memoryHandle = Traits::InvalidHandle();
    result = Traits::ExportMemory(&helper, deviceMemory, &memoryHandle);
    EXPECT_EQ(result, VK_SUCCESS);
    EXPECT_NE(memoryHandle, Traits::InvalidHandle());

    {
        GLMemoryObject memoryObject;
        GLint dedicatedMemory = GL_TRUE;
        glMemoryObjectParameterivEXT(memoryObject, GL_DEDICATED_MEMORY_OBJECT_EXT,
                                     &dedicatedMemory);
        Traits::ImportMemory(memoryObject, deviceMemorySize, memoryHandle);

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

// Test creating and clearing a simple RGBA8 texture in a opaque fd.
TEST_P(VulkanExternalImageTest, ShouldClearOpaqueFdRGBA8)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_memory_object_fd"));
    // http://anglebug.com/4630
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGL() && (IsPixel2() || IsPixel2XL()));
    RunShouldClearTest<OpaqueFdTraits>(isSwiftshader(), enableDebugLayers());
}

// Test creating and clearing a simple RGBA8 texture in a zircon vmo.
TEST_P(VulkanExternalImageTest, ShouldClearZirconVmoRGBA8)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_ANGLE_memory_object_fuchsia"));
    RunShouldClearTest<FuchsiaTraits>(isSwiftshader(), enableDebugLayers());
}

template <typename Traits>
void RunTextureFormatCompatChromiumTest(bool isSwiftshader, bool enableDebugLayers)
{
    ASSERT(EnsureGLExtensionEnabled(Traits::MemoryObjectExtension()));

    VulkanExternalHelper helper;
    helper.initialize(isSwiftshader, enableDebugLayers);
    for (const ImageFormatPair &format : kChromeFormats)
    {
        if (!Traits::CanCreateImage(helper, format.vkFormat, VK_IMAGE_TYPE_2D,
                                    VK_IMAGE_TILING_OPTIMAL))
        {
            continue;
        }

        if (format.requiredExtension && !IsGLExtensionEnabled(format.requiredExtension))
        {
            continue;
        }

        VkImage image                 = VK_NULL_HANDLE;
        VkDeviceMemory deviceMemory   = VK_NULL_HANDLE;
        VkDeviceSize deviceMemorySize = 0;

        VkExtent3D extent = {113, 211, 1};
        VkResult result   = Traits::CreateImage2D(&helper, format.vkFormat, extent, &image,
                                                &deviceMemory, &deviceMemorySize);
        EXPECT_EQ(result, VK_SUCCESS);

        typename Traits::Handle memoryHandle = Traits::InvalidHandle();
        result = Traits::ExportMemory(&helper, deviceMemory, &memoryHandle);
        EXPECT_EQ(result, VK_SUCCESS);
        EXPECT_NE(memoryHandle, Traits::InvalidHandle());

        {
            GLMemoryObject memoryObject;
            GLint dedicatedMemory = GL_TRUE;
            glMemoryObjectParameterivEXT(memoryObject, GL_DEDICATED_MEMORY_OBJECT_EXT,
                                         &dedicatedMemory);
            Traits::ImportMemory(memoryObject, deviceMemorySize, memoryHandle);

            GLTexture texture;
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexStorageMem2DEXT(GL_TEXTURE_2D, 1, format.internalFormat, extent.width,
                                 extent.height, memoryObject, 0);
        }

        EXPECT_GL_NO_ERROR();

        vkDestroyImage(helper.getDevice(), image, nullptr);
        vkFreeMemory(helper.getDevice(), deviceMemory, nullptr);
    }
}

// Test all format combinations used by Chrome import successfully (opaque fd).
TEST_P(VulkanExternalImageTest, TextureFormatCompatChromiumFd)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_memory_object_fd"));
    RunTextureFormatCompatChromiumTest<OpaqueFdTraits>(isSwiftshader(), enableDebugLayers());
}

// Test all format combinations used by Chrome import successfully (fuchsia).
TEST_P(VulkanExternalImageTest, TextureFormatCompatChromiumZirconVmo)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_ANGLE_memory_object_fuchsia"));
    RunTextureFormatCompatChromiumTest<FuchsiaTraits>(isSwiftshader(), enableDebugLayers());
}

template <typename Traits>
void RunShouldClearWithSemaphoresTest(bool isSwiftshader, bool enableDebugLayers)
{
    ASSERT(EnsureGLExtensionEnabled(Traits::MemoryObjectExtension()));
    ASSERT(EnsureGLExtensionEnabled(Traits::SemaphoreExtension()));

    VulkanExternalHelper helper;
    helper.initialize(isSwiftshader, enableDebugLayers);

    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    ANGLE_SKIP_TEST_IF(
        !Traits::CanCreateImage(helper, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL));
    ANGLE_SKIP_TEST_IF(!Traits::CanCreateSemaphore(helper));

    VkSemaphore vkAcquireSemaphore = VK_NULL_HANDLE;
    VkResult result                = Traits::CreateSemaphore(&helper, &vkAcquireSemaphore);
    EXPECT_EQ(result, VK_SUCCESS);
    EXPECT_TRUE(vkAcquireSemaphore != VK_NULL_HANDLE);

    VkSemaphore vkReleaseSemaphore = VK_NULL_HANDLE;
    result                         = Traits::CreateSemaphore(&helper, &vkReleaseSemaphore);
    EXPECT_EQ(result, VK_SUCCESS);
    EXPECT_TRUE(vkReleaseSemaphore != VK_NULL_HANDLE);

    typename Traits::Handle acquireSemaphoreHandle = Traits::InvalidHandle();
    result = Traits::ExportSemaphore(&helper, vkAcquireSemaphore, &acquireSemaphoreHandle);
    EXPECT_EQ(result, VK_SUCCESS);
    EXPECT_NE(acquireSemaphoreHandle, Traits::InvalidHandle());

    typename Traits::Handle releaseSemaphoreHandle = Traits::InvalidHandle();
    result = Traits::ExportSemaphore(&helper, vkReleaseSemaphore, &releaseSemaphoreHandle);
    EXPECT_EQ(result, VK_SUCCESS);
    EXPECT_NE(releaseSemaphoreHandle, Traits::InvalidHandle());

    VkImage image                 = VK_NULL_HANDLE;
    VkDeviceMemory deviceMemory   = VK_NULL_HANDLE;
    VkDeviceSize deviceMemorySize = 0;

    VkExtent3D extent = {1, 1, 1};
    result =
        Traits::CreateImage2D(&helper, format, extent, &image, &deviceMemory, &deviceMemorySize);
    EXPECT_EQ(result, VK_SUCCESS);

    typename Traits::Handle memoryHandle = Traits::InvalidHandle();
    result = Traits::ExportMemory(&helper, deviceMemory, &memoryHandle);
    EXPECT_EQ(result, VK_SUCCESS);
    EXPECT_NE(memoryHandle, Traits::InvalidHandle());

    {
        GLMemoryObject memoryObject;
        GLint dedicatedMemory = GL_TRUE;
        glMemoryObjectParameterivEXT(memoryObject, GL_DEDICATED_MEMORY_OBJECT_EXT,
                                     &dedicatedMemory);
        Traits::ImportMemory(memoryObject, deviceMemorySize, memoryHandle);

        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexStorageMem2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1, memoryObject, 0);

        GLSemaphore glAcquireSemaphore;
        Traits::ImportSemaphore(glAcquireSemaphore, acquireSemaphoreHandle);

        helper.releaseImageAndSignalSemaphore(image, VK_IMAGE_LAYOUT_UNDEFINED,
                                              VK_IMAGE_LAYOUT_GENERAL, vkAcquireSemaphore);

        const GLuint barrierTextures[] = {
            texture,
        };
        constexpr uint32_t textureBarriersCount = std::extent<decltype(barrierTextures)>();
        const GLenum textureSrcLayouts[]        = {
            GL_LAYOUT_GENERAL_EXT,
        };
        constexpr uint32_t textureSrcLayoutsCount = std::extent<decltype(textureSrcLayouts)>();
        static_assert(textureBarriersCount == textureSrcLayoutsCount,
                      "barrierTextures and textureSrcLayouts must be the same length");
        glWaitSemaphoreEXT(glAcquireSemaphore, 0, nullptr, textureBarriersCount, barrierTextures,
                           textureSrcLayouts);

        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

        glClearColor(0.5f, 0.5f, 0.5f, 0.5f);
        glClear(GL_COLOR_BUFFER_BIT);

        GLSemaphore glReleaseSemaphore;
        Traits::ImportSemaphore(glReleaseSemaphore, releaseSemaphoreHandle);

        const GLenum textureDstLayouts[] = {
            GL_LAYOUT_TRANSFER_SRC_EXT,
        };
        constexpr uint32_t textureDstLayoutsCount = std::extent<decltype(textureSrcLayouts)>();
        static_assert(textureBarriersCount == textureDstLayoutsCount,
                      "barrierTextures and textureDstLayouts must be the same length");
        glSignalSemaphoreEXT(glReleaseSemaphore, 0, nullptr, textureBarriersCount, barrierTextures,
                             textureDstLayouts);

        helper.waitSemaphoreAndAcquireImage(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                            vkReleaseSemaphore);
        uint8_t pixels[4];
        VkOffset3D offset = {};
        VkExtent3D extent = {1, 1, 1};
        helper.readPixels(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, format, offset, extent,
                          pixels, sizeof(pixels));

        EXPECT_NEAR(0x80, pixels[0], 1);
        EXPECT_NEAR(0x80, pixels[1], 1);
        EXPECT_NEAR(0x80, pixels[2], 1);
        EXPECT_NEAR(0x80, pixels[3], 1);
    }

    EXPECT_GL_NO_ERROR();

    vkDeviceWaitIdle(helper.getDevice());
    vkDestroyImage(helper.getDevice(), image, nullptr);
    vkDestroySemaphore(helper.getDevice(), vkAcquireSemaphore, nullptr);
    vkDestroySemaphore(helper.getDevice(), vkReleaseSemaphore, nullptr);
    vkFreeMemory(helper.getDevice(), deviceMemory, nullptr);
}

// Test creating and clearing RGBA8 texture in opaque fd with acquire/release.
TEST_P(VulkanExternalImageTest, ShouldClearOpaqueFdWithSemaphores)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_memory_object_fd"));
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_semaphore_fd"));
    RunShouldClearWithSemaphoresTest<OpaqueFdTraits>(isSwiftshader(), enableDebugLayers());
}

// Test creating and clearing RGBA8 texture in zircon vmo with acquire/release.
TEST_P(VulkanExternalImageTest, ShouldClearZirconVmoWithSemaphores)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_ANGLE_memory_object_fuchsia"));
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_ANGLE_semaphore_fuchsia"));
    RunShouldClearWithSemaphoresTest<FuchsiaTraits>(isSwiftshader(), enableDebugLayers());
}

// Support for Zircon handle types is mandatory on Fuchsia.
TEST_P(VulkanExternalImageTest, ShouldSupportExternalHandlesFuchsia)
{
    ANGLE_SKIP_TEST_IF(!IsFuchsia());
    EXPECT_TRUE(EnsureGLExtensionEnabled("GL_ANGLE_memory_object_fuchsia"));
    EXPECT_TRUE(EnsureGLExtensionEnabled("GL_ANGLE_semaphore_fuchsia"));
    VulkanExternalHelper helper;
    helper.initialize(isSwiftshader(), enableDebugLayers());
    EXPECT_TRUE(helper.canCreateSemaphoreZirconEvent());
    EXPECT_TRUE(helper.canCreateImageZirconVmo(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D,
                                               VK_IMAGE_TILING_OPTIMAL));
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(VulkanExternalImageTest);

}  // namespace angle
