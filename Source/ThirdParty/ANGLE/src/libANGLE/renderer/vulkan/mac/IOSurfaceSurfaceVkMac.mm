//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IOSurfaceSurfaceVkMac.mm:
//    Implements methods from IOSurfaceSurfaceVkMac.
//

#include "libANGLE/renderer/vulkan/mac/IOSurfaceSurfaceVkMac.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/DisplayVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/TextureVk.h"

#include <IOSurface/IOSurface.h>

namespace rx
{

namespace
{

struct IOSurfaceFormatInfo
{
    GLenum internalFormat;
    GLenum type;

    size_t componentBytes;

    GLenum nativeSizedInternalFormat;
};

// clang-format off
constexpr std::array<IOSurfaceFormatInfo, 7> kIOSurfaceFormats = {{
    {GL_RED,      GL_UNSIGNED_BYTE,                1, GL_R8       },
    {GL_R16UI,    GL_UNSIGNED_SHORT,               2, GL_R16UI    },
    {GL_RG,       GL_UNSIGNED_BYTE,                2, GL_RG8      },
    {GL_RGB,      GL_UNSIGNED_BYTE,                4, GL_BGRA8_EXT},
    {GL_BGRA_EXT, GL_UNSIGNED_BYTE,                4, GL_BGRA8_EXT},
    {GL_RGB10_A2, GL_UNSIGNED_INT_2_10_10_10_REV,  4, GL_BGR10_A2_ANGLEX },
    {GL_RGBA,     GL_HALF_FLOAT,                   8, GL_RGBA16F  },
}};
// clang-format on

int FindIOSurfaceFormatIndex(GLenum internalFormat, GLenum type)
{
    for (int i = 0; i < static_cast<int>(kIOSurfaceFormats.size()); ++i)
    {
        const auto &formatInfo = kIOSurfaceFormats[i];
        if (formatInfo.internalFormat == internalFormat && formatInfo.type == type)
        {
            return i;
        }
    }
    return -1;
}

}  // anonymous namespace

IOSurfaceSurfaceVkMac::IOSurfaceSurfaceVkMac(const egl::SurfaceState &state,
                                             EGLClientBuffer buffer,
                                             const egl::AttributeMap &attribs)
    : OffscreenSurfaceVk(state), mIOSurface(nullptr), mPlane(0), mFormatIndex(-1)
{
    // Keep reference to the IOSurface so it doesn't get deleted while the pbuffer exists.
    mIOSurface = reinterpret_cast<IOSurfaceRef>(buffer);
    CFRetain(mIOSurface);

    // Extract attribs useful for the call to CGLTexImageIOSurface2D
    mWidth  = static_cast<int>(attribs.get(EGL_WIDTH));
    mHeight = static_cast<int>(attribs.get(EGL_HEIGHT));
    mPlane  = static_cast<int>(attribs.get(EGL_IOSURFACE_PLANE_ANGLE));

    EGLAttrib internalFormat = attribs.get(EGL_TEXTURE_INTERNAL_FORMAT_ANGLE);
    EGLAttrib type           = attribs.get(EGL_TEXTURE_TYPE_ANGLE);
    mFormatIndex =
        FindIOSurfaceFormatIndex(static_cast<GLenum>(internalFormat), static_cast<GLenum>(type));
    ASSERT(mFormatIndex >= 0);
}

IOSurfaceSurfaceVkMac::~IOSurfaceSurfaceVkMac()
{
    if (mIOSurface != nullptr)
    {
        CFRelease(mIOSurface);
        mIOSurface = nullptr;
    }
}

egl::Error IOSurfaceSurfaceVkMac::initialize(const egl::Display *display)
{
    DisplayVk *displayVk = vk::GetImpl(display);
    angle::Result result = initializeImpl(displayVk);
    return angle::ToEGL(result, displayVk, EGL_BAD_SURFACE);
}

angle::Result IOSurfaceSurfaceVkMac::initializeImpl(DisplayVk *displayVk)
{
    RendererVk *renderer      = displayVk->getRenderer();
    const egl::Config *config = mState.config;

    GLint samples = 1;
    if (config->sampleBuffers && config->samples > 1)
    {
        samples = config->samples;
    }
    ANGLE_VK_CHECK(displayVk, samples > 0, VK_ERROR_INITIALIZATION_FAILED);

    // Swiftshader will use the raw pointer to the buffer referenced by the IOSurfaceRef
    ANGLE_TRY(mColorAttachment.initializeWithExternalMemory(
        displayVk, mWidth, mHeight,
        renderer->getFormat(kIOSurfaceFormats[mFormatIndex].nativeSizedInternalFormat), samples,
        IOSurfaceGetBaseAddressOfPlane(mIOSurface, mPlane)));
    mColorRenderTarget.init(&mColorAttachment.image, &mColorAttachment.imageViews, 0, 0);

    return angle::Result::Continue;
}

egl::Error IOSurfaceSurfaceVkMac::unMakeCurrent(const gl::Context *context)
{
    ASSERT(context != nullptr);
    ContextVk *contextVk = vk::GetImpl(context);
    DisplayVk *displayVk = vk::GetImpl(context->getDisplay());
    angle::Result result = contextVk->flushImpl(nullptr);
    return angle::ToEGL(result, displayVk, EGL_BAD_SURFACE);
}

egl::Error IOSurfaceSurfaceVkMac::bindTexImage(const gl::Context *context,
                                               gl::Texture *texture,
                                               EGLint buffer)
{
    IOSurfaceLock(mIOSurface, 0, nullptr);

    return egl::NoError();
}

egl::Error IOSurfaceSurfaceVkMac::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    ASSERT(context != nullptr);
    ContextVk *contextVk = vk::GetImpl(context);
    DisplayVk *displayVk = vk::GetImpl(context->getDisplay());
    angle::Result result = contextVk->finishImpl();

    IOSurfaceUnlock(mIOSurface, 0, nullptr);

    return angle::ToEGL(result, displayVk, EGL_BAD_SURFACE);
}

// static
bool IOSurfaceSurfaceVkMac::ValidateAttributes(const DisplayVk *displayVk,
                                               EGLClientBuffer buffer,
                                               const egl::AttributeMap &attribs)
{
    ASSERT(displayVk != nullptr);
    RendererVk *renderer = displayVk->getRenderer();

    IOSurfaceRef ioSurface = reinterpret_cast<IOSurfaceRef>(buffer);

    // The plane must exist for this IOSurface. IOSurfaceGetPlaneCount can return 0 for non-planar
    // ioSurfaces but we will treat non-planar like it is a single plane.
    size_t surfacePlaneCount = std::max(size_t(1), IOSurfaceGetPlaneCount(ioSurface));
    EGLAttrib plane          = attribs.get(EGL_IOSURFACE_PLANE_ANGLE);
    if (plane < 0 || static_cast<size_t>(plane) >= surfacePlaneCount)
    {
        return false;
    }

    // The width height specified must be at least (1, 1) and at most the plane size
    EGLAttrib width  = attribs.get(EGL_WIDTH);
    EGLAttrib height = attribs.get(EGL_HEIGHT);
    if (width <= 0 || static_cast<size_t>(width) > IOSurfaceGetWidthOfPlane(ioSurface, plane) ||
        height <= 0 || static_cast<size_t>(height) > IOSurfaceGetHeightOfPlane(ioSurface, plane))
    {
        return false;
    }

    // Find this IOSurface format
    EGLAttrib internalFormat = attribs.get(EGL_TEXTURE_INTERNAL_FORMAT_ANGLE);
    EGLAttrib type           = attribs.get(EGL_TEXTURE_TYPE_ANGLE);

    int formatIndex =
        FindIOSurfaceFormatIndex(static_cast<GLenum>(internalFormat), static_cast<GLenum>(type));

    if (formatIndex < 0)
    {
        return false;
    }

    // Check that the format matches this IOSurface plane
    if (IOSurfaceGetBytesPerElementOfPlane(ioSurface, plane) !=
        kIOSurfaceFormats[formatIndex].componentBytes)
    {
        return false;
    }

    void *pointer = IOSurfaceGetBaseAddressOfPlane(ioSurface, plane);
    VkMemoryHostPointerPropertiesEXT memoryHostPointerProperties = {};
    memoryHostPointerProperties.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT;
    vkGetMemoryHostPointerPropertiesEXT(
        renderer->getDevice(), VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_MAPPED_FOREIGN_MEMORY_BIT_EXT,
        pointer, &memoryHostPointerProperties);

    bool hostVisible =
        memoryHostPointerProperties.memoryTypeBits & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    if (!hostVisible)
    {
        return false;
    }

    VkDeviceSize alignment = renderer->getMinImportedHostPointerAlignment();
    if (reinterpret_cast<size_t>(pointer) % alignment != 0)
    {
        return false;
    }

    return true;
}

}  // namespace rx
