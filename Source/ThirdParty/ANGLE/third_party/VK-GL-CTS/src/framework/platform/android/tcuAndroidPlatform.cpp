/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Android EGL and Vulkan platforms.
 *//*--------------------------------------------------------------------*/

#include "tcuAndroidPlatform.hpp"
#include "tcuAndroidUtil.hpp"
#include "gluRenderContext.hpp"
#include "egluNativeDisplay.hpp"
#include "egluNativeWindow.hpp"
#include "egluGLContextFactory.hpp"
#include "egluUtil.hpp"
#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"
#include "tcuFunctionLibrary.hpp"
#include "vkWsiPlatform.hpp"

// Assume no call translation is needed
#include <android/native_window.h>
#if DE_ANDROID_API >= 24
#include <media/NdkImageReader.h>
#include <media/NdkImage.h>
#include <mutex>
#include <atomic>
#endif
#if DE_ANDROID_API >= 26
#include <android/hardware_buffer.h>
#endif
struct egl_native_pixmap_t;
DE_STATIC_ASSERT(sizeof(eglw::EGLNativeDisplayType) == sizeof(void *));
DE_STATIC_ASSERT(sizeof(eglw::EGLNativePixmapType) == sizeof(struct egl_native_pixmap_t *));
DE_STATIC_ASSERT(sizeof(eglw::EGLNativeWindowType) == sizeof(ANativeWindow *));

namespace tcu
{
namespace Android
{

using namespace eglw;

static const eglu::NativeDisplay::Capability DISPLAY_CAPABILITIES = eglu::NativeDisplay::CAPABILITY_GET_DISPLAY_LEGACY;
static const eglu::NativeWindow::Capability WINDOW_CAPABILITIES   = (eglu::NativeWindow::Capability)(
    eglu::NativeWindow::CAPABILITY_CREATE_SURFACE_LEGACY | eglu::NativeWindow::CAPABILITY_CREATE_SURFACE_PLATFORM |
    eglu::NativeWindow::CAPABILITY_CREATE_SURFACE_PLATFORM_EXTENSION | eglu::NativeWindow::CAPABILITY_SET_SURFACE_SIZE |
    eglu::NativeWindow::CAPABILITY_GET_SCREEN_SIZE);

class NativeDisplay : public eglu::NativeDisplay
{
public:
    NativeDisplay(void) : eglu::NativeDisplay(DISPLAY_CAPABILITIES), m_library("libEGL.so")
    {
    }
    virtual ~NativeDisplay(void)
    {
    }

    virtual EGLNativeDisplayType getLegacyNative(void)
    {
        return EGL_DEFAULT_DISPLAY;
    }
    virtual const eglw::Library &getLibrary(void) const
    {
        return m_library;
    }

private:
    eglw::DefaultLibrary m_library;
};

class NativeDisplayFactory : public eglu::NativeDisplayFactory
{
public:
    NativeDisplayFactory(WindowRegistry &windowRegistry);
    ~NativeDisplayFactory(void)
    {
    }

    virtual eglu::NativeDisplay *createDisplay(const EGLAttrib *attribList) const;
};

class NativeWindow : public eglu::NativeWindow
{
public:
    NativeWindow(Window *window, int width, int height, int32_t format);
    virtual ~NativeWindow(void);

    virtual EGLNativeWindowType getLegacyNative(void)
    {
        return m_window->getNativeWindow();
    }
    virtual EGLNativeWindowType getPlatformExtension(void)
    {
        return m_window->getNativeWindow();
    }
    virtual EGLNativeWindowType getPlatformNative(void)
    {
        return m_window->getNativeWindow();
    }
    IVec2 getScreenSize(void) const
    {
        return m_window->getSize();
    }

    void setSurfaceSize(IVec2 size);

    virtual void processEvents(void);

private:
    Window *m_window;
    int32_t m_format;
};

class NativeWindowFactory : public eglu::NativeWindowFactory
{
public:
    NativeWindowFactory(WindowRegistry &windowRegistry);
    ~NativeWindowFactory(void);

    virtual eglu::NativeWindow *createWindow(eglu::NativeDisplay *nativeDisplay,
                                             const eglu::WindowParams &params) const;
    virtual eglu::NativeWindow *createWindow(eglu::NativeDisplay *nativeDisplay, EGLDisplay display, EGLConfig config,
                                             const EGLAttrib *attribList, const eglu::WindowParams &params) const;

private:
    virtual eglu::NativeWindow *createWindow(const eglu::WindowParams &params, int32_t format) const;

    WindowRegistry &m_windowRegistry;
};

#if DE_ANDROID_API >= 24
struct ImageQueue
{
    std::mutex lock;
    std::atomic<bool> closing{false};
    AImageReader_ImageListener listener;

    ImageQueue()
    {
        listener.context          = this;
        listener.onImageAvailable = onImageAvailable;
    }

    ~ImageQueue()
    {
    }

    static void onImageAvailable(void *context, AImageReader *reader)
    {
        ImageQueue *queue = reinterpret_cast<ImageQueue *>(context);

        std::unique_lock<std::mutex> guard(queue->lock, std::try_to_lock);
        if (!guard.owns_lock() || queue->closing.load(std::memory_order_acquire))
            return;

        AImage *image = nullptr;

#if DE_ANDROID_API >= 26
        int fenceFd = -1;
        while (AImageReader_acquireNextImageAsync(reader, &image, &fenceFd) == AMEDIA_OK && image != nullptr)
        {
            AImage_deleteAsync(image, fenceFd);
        }
#else
        while (AImageReader_acquireNextImage(reader, &image) == AMEDIA_OK && image != nullptr)
        {
            AImage_delete(image);
        }
#endif
    }
};

static ANativeWindow *acquireImageReaderWindow(int width, int height, int32_t format, AImageReader **outReader,
                                               ImageQueue **outQueue)
{
    AImageReader *reader = nullptr;
#if DE_ANDROID_API >= 26
    uint64_t usage        = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_COMPOSER_OVERLAY;
    media_status_t status = AImageReader_newWithUsage(width, height, format, usage, 4, &reader);
#else
    media_status_t status = AImageReader_new(width, height, format, 4, &reader);
#endif
    if (status != AMEDIA_OK || !reader)
        throw ResourceError("Failed to create AImageReader", nullptr, __FILE__, __LINE__);

    *outQueue = new ImageQueue();
    AImageReader_setImageListener(reader, &(*outQueue)->listener);

    ANativeWindow *nativeWindow = nullptr;
    status                      = AImageReader_getWindow(reader, &nativeWindow);
    if (status != AMEDIA_OK || !nativeWindow)
    {
        AImageReader_setImageListener(reader, nullptr);
        delete *outQueue;
        *outQueue = nullptr;
        AImageReader_delete(reader);
        throw ResourceError("Failed to get window from AImageReader", nullptr, __FILE__, __LINE__);
    }

    *outReader = reader;
    return nativeWindow;
}

class ImageReaderNativeWindow : public eglu::NativeWindow
{
public:
    ImageReaderNativeWindow(AImageReader *reader, ImageQueue *queue, ANativeWindow *window, int width, int height)
        : eglu::NativeWindow(WINDOW_CAPABILITIES)
        , m_reader(reader)
        , m_queue(queue)
        , m_window(window)
        , m_size(width, height)
    {
    }

    virtual ~ImageReaderNativeWindow(void)
    {
        if (m_reader)
        {
            if (m_queue)
            {
                m_queue->closing.store(true, std::memory_order_release);
                std::lock_guard<std::mutex> guard(m_queue->lock);
                AImageReader_setImageListener(m_reader, nullptr);
            }
            AImageReader_delete(m_reader);
        }
        if (m_queue)
            delete m_queue;
    }

    virtual eglw::EGLNativeWindowType getLegacyNative(void)
    {
        return m_window;
    }
    virtual void *getPlatformExtension(void)
    {
        return m_window;
    }
    virtual void *getPlatformNative(void)
    {
        return m_window;
    }
    tcu::IVec2 getScreenSize(void) const
    {
        return m_size;
    }
    void setSurfaceSize(tcu::IVec2 size)
    {
        int32_t format = 0; // 0 means keep the existing format
        ANativeWindow_setBuffersGeometry(m_window, size.x(), size.y(), format);
        m_size = size;
    }
    virtual void processEvents(void)
    {
    }

private:
    AImageReader *m_reader;
    ImageQueue *m_queue;
    ANativeWindow *m_window;
    tcu::IVec2 m_size;
};
#endif

// NativeWindow

NativeWindow::NativeWindow(Window *window, int width, int height, int32_t format)
    : eglu::NativeWindow(WINDOW_CAPABILITIES)
    , m_window(window)
    , m_format(format)
{
    // Set up buffers.
    setSurfaceSize(IVec2(width, height));
}

NativeWindow::~NativeWindow(void)
{
    m_window->release();
}

void NativeWindow::processEvents(void)
{
    if (m_window->isPendingDestroy())
        throw eglu::WindowDestroyedError("Window has been destroyed");
}

void NativeWindow::setSurfaceSize(tcu::IVec2 size)
{
    m_window->setBuffersGeometry(size.x() != eglu::WindowParams::SIZE_DONT_CARE ? size.x() : 0,
                                 size.y() != eglu::WindowParams::SIZE_DONT_CARE ? size.y() : 0, m_format);
}

// NativeWindowFactory

NativeWindowFactory::NativeWindowFactory(WindowRegistry &windowRegistry)
    : eglu::NativeWindowFactory("default", "Default display", WINDOW_CAPABILITIES)
    , m_windowRegistry(windowRegistry)
{
}

NativeWindowFactory::~NativeWindowFactory(void)
{
}

eglu::NativeWindow *NativeWindowFactory::createWindow(eglu::NativeDisplay *nativeDisplay,
                                                      const eglu::WindowParams &params) const
{
    DE_UNREF(nativeDisplay);
    return createWindow(params, WINDOW_FORMAT_RGBA_8888);
}

eglu::NativeWindow *NativeWindowFactory::createWindow(eglu::NativeDisplay *nativeDisplay, EGLDisplay display,
                                                      EGLConfig config, const EGLAttrib *attribList,
                                                      const eglu::WindowParams &params) const
{
    const int32_t format =
        (int32_t)eglu::getConfigAttribInt(nativeDisplay->getLibrary(), display, config, EGL_NATIVE_VISUAL_ID);
    DE_UNREF(nativeDisplay && attribList);
    return createWindow(params, format);
}

eglu::NativeWindow *NativeWindowFactory::createWindow(const eglu::WindowParams &params, int32_t format) const
{
    Window *window = m_windowRegistry.tryAcquireWindow();

    if (window)
    {
        return new NativeWindow(window, params.width, params.height, format);
    }
    else
    {
#if DE_ANDROID_API >= 24
        int width  = params.width != eglu::WindowParams::SIZE_DONT_CARE ? params.width : 256;
        int height = params.height != eglu::WindowParams::SIZE_DONT_CARE ? params.height : 256;
        width      = width > 0 ? width : 256;
        height     = height > 0 ? height : 256;

        AImageReader *reader = nullptr;
        ImageQueue *queue    = nullptr;
        // Always use AIMAGE_FORMAT_RGBA_8888: the AImageReader is only used as a
        // surface handle provider, and AIMAGE_FORMAT_* constants are not
        // interchangeable with ANativeWindow_LegacyFormat values.
        ANativeWindow *nativeWindow = acquireImageReaderWindow(width, height, AIMAGE_FORMAT_RGBA_8888, &reader, &queue);

        return new ImageReaderNativeWindow(reader, queue, nativeWindow, width, height);
#else
        throw ResourceError("Native window is not available", nullptr, __FILE__, __LINE__);
#endif
    }
}

// NativeDisplayFactory

NativeDisplayFactory::NativeDisplayFactory(WindowRegistry &windowRegistry)
    : eglu::NativeDisplayFactory("default", "Default display", DISPLAY_CAPABILITIES)
{
    m_nativeWindowRegistry.registerFactory(new NativeWindowFactory(windowRegistry));
}

eglu::NativeDisplay *NativeDisplayFactory::createDisplay(const EGLAttrib *attribList) const
{
    DE_UNREF(attribList);
    return new NativeDisplay();
}

// Vulkan

class VulkanLibrary : public vk::Library
{
public:
    VulkanLibrary(const char *libraryPath)
        : m_library(libraryPath != nullptr ? libraryPath : "libvulkan.so")
        , m_driver(m_library)
    {
    }

    const vk::PlatformInterface &getPlatformInterface(void) const
    {
        return m_driver;
    }

    const tcu::FunctionLibrary &getFunctionLibrary(void) const
    {
        return m_library;
    }

private:
    const tcu::DynamicFunctionLibrary m_library;
    const vk::PlatformDriver m_driver;
};

DE_STATIC_ASSERT(sizeof(vk::pt::AndroidNativeWindowPtr) == sizeof(ANativeWindow *));

class VulkanWindow : public vk::wsi::AndroidWindowInterface
{
public:
    VulkanWindow(tcu::Android::Window &window)
        : vk::wsi::AndroidWindowInterface(vk::pt::AndroidNativeWindowPtr(window.getNativeWindow()))
        , m_window(window)
    {
    }

    void setVisible(bool visible)
    {
        DE_UNREF(visible);
    }

    void resize(const UVec2 &newSize)
    {
        DE_UNREF(newSize);
    }

    void setMinimized(bool minimized)
    {
        DE_UNREF(minimized);
        TCU_THROW(NotSupportedError, "Minimized on Android is not implemented");
    }

    ~VulkanWindow(void)
    {
        m_window.release();
    }

private:
    tcu::Android::Window &m_window;
};

#if DE_ANDROID_API >= 24
class ImageReaderVulkanWindow : public vk::wsi::AndroidWindowInterface
{
public:
    ImageReaderVulkanWindow(AImageReader *reader, ImageQueue *queue, ANativeWindow *window)
        : vk::wsi::AndroidWindowInterface(vk::pt::AndroidNativeWindowPtr(window))
        , m_reader(reader)
        , m_queue(queue)
    {
    }

    void setVisible(bool visible)
    {
        DE_UNREF(visible);
    }

    void resize(const UVec2 &newSize)
    {
        DE_UNREF(newSize);
    }

    void setMinimized(bool minimized)
    {
        DE_UNREF(minimized);
        TCU_THROW(NotSupportedError, "Minimized on Android is not implemented");
    }

    ~ImageReaderVulkanWindow(void)
    {
        if (m_reader)
        {
            if (m_queue)
            {
                m_queue->closing.store(true, std::memory_order_release);
                std::lock_guard<std::mutex> guard(m_queue->lock);
                AImageReader_setImageListener(m_reader, nullptr);
            }
            AImageReader_delete(m_reader);
        }
        if (m_queue)
            delete m_queue;
    }

private:
    AImageReader *m_reader;
    ImageQueue *m_queue;
};
#endif

class VulkanDisplay : public vk::wsi::Display
{
public:
    VulkanDisplay(WindowRegistry &windowRegistry) : m_windowRegistry(windowRegistry)
    {
    }

    vk::wsi::Window *createWindow(const Maybe<UVec2> &initialSize) const
    {
        Window *const window = m_windowRegistry.tryAcquireWindow();

        if (window)
        {
            try
            {
                if (initialSize)
                    window->setBuffersGeometry((int)initialSize->x(), (int)initialSize->y(), WINDOW_FORMAT_RGBA_8888);

                return new VulkanWindow(*window);
            }
            catch (...)
            {
                window->release();
                throw;
            }
        }
        else
        {
#if DE_ANDROID_API >= 24
            uint32_t width  = initialSize ? initialSize->x() : 256;
            uint32_t height = initialSize ? initialSize->y() : 256;
            width           = width > 0 ? width : 256;
            height          = height > 0 ? height : 256;

            AImageReader *reader = nullptr;
            ImageQueue *queue    = nullptr;
            ANativeWindow *nativeWindow =
                acquireImageReaderWindow(width, height, AIMAGE_FORMAT_RGBA_8888, &reader, &queue);

            return new ImageReaderVulkanWindow(reader, queue, nativeWindow);
#else
            TCU_THROW(ResourceError, "Native window is not available");
#endif
        }
    }

private:
    WindowRegistry &m_windowRegistry;
};

static size_t getTotalSystemMemory(ANativeActivity *activity)
{
    const size_t MiB = (size_t)(1 << 20);
    // Use relatively high fallback size to encourage CDD-compliant behavior
    const size_t fallbackSize = (sizeof(void *) == sizeof(uint64_t)) ? 2048 * MiB : 1024 * MiB;

    if (activity)
    {
        try
        {
            const size_t totalMemory = getTotalAndroidSystemMemory(activity);
            print("Device has %.2f MiB of system memory\n",
                  static_cast<double>(totalMemory) / static_cast<double>(MiB));
            return totalMemory;
        }
        catch (const std::exception &e)
        {
            print("WARNING: Failed to determine system memory size required by CDD: %s\n", e.what());
            print("WARNING: Using fall-back size of %.2f MiB\n", double(fallbackSize) / double(MiB));
        }
    }

    return fallbackSize;
}

// Platform

Platform::Platform(NativeActivity &activity)
    : m_activity(activity)
    , m_totalSystemMemory(getTotalSystemMemory(activity.getNativeActivity()))
{
    m_nativeDisplayFactoryRegistry.registerFactory(new NativeDisplayFactory(m_windowRegistry));
    m_contextFactoryRegistry.registerFactory(new eglu::GLContextFactory(m_nativeDisplayFactoryRegistry));
}

Platform::~Platform(void)
{
}

bool Platform::processEvents(void)
{
    m_windowRegistry.garbageCollect();
    return true;
}

vk::Library *Platform::createLibrary(const char *libraryPath) const
{
    return new VulkanLibrary(libraryPath);
}

void Platform::describePlatform(std::ostream &dst) const
{
    tcu::Android::describePlatform(m_activity.getNativeActivity(), dst);
}

void Platform::getMemoryLimits(tcu::PlatformMemoryLimits &limits) const
{
    // Worst-case estimates
    const size_t MiB          = (size_t)(1 << 20);
    const size_t baseMemUsage = 400 * MiB;

#if (DE_PTR_SIZE == 4)
    // Some tests, such as:
    //
    // dEQP-VK.api.object_management.max_concurrent.*
    // dEQP-VK.memory.allocation.random.*
    //
    // when run in succession, can lead to system memory fragmentation. It depends on the allocator, and on some 32-bit
    // systems can lead to out of memory errors. As a workaround, we use a smaller amount of memory on 32-bit systems,
    // as this typically avoids out of memory errors caused by fragmentation.
    const double safeUsageRatio = 0.1;
#else
    const double safeUsageRatio = 0.25;
#endif

    limits.totalSystemMemory =
        de::max((size_t)(double(int64_t(m_totalSystemMemory) - int64_t(baseMemUsage)) * safeUsageRatio), 16 * MiB);

    // Assume UMA architecture
    limits.totalDeviceLocalMemory = 0;

    // Reasonable worst-case estimates
    limits.deviceMemoryAllocationGranularity = 64 * 1024;
    limits.devicePageSize                    = 4096;
    limits.devicePageTableEntrySize          = 8;
    limits.devicePageTableHierarchyLevels    = 3;
}

vk::wsi::Display *Platform::createWsiDisplay(vk::wsi::Type wsiType) const
{
    if (wsiType == vk::wsi::TYPE_ANDROID)
        return new VulkanDisplay(const_cast<WindowRegistry &>(m_windowRegistry));
    else
        TCU_THROW(NotSupportedError, "WSI type not supported on Android");
}

bool Platform::hasDisplay(vk::wsi::Type wsiType) const
{
    if (wsiType == vk::wsi::TYPE_ANDROID)
        return true;

    return false;
}

void Platform::setCustomScreenOrientation(bool enable) const
{
    CustomOrientation::instance().set(enable);
}

void Platform::requestPixelCopy(const char *filename) const
{
    PixelCopy(m_activity.getNativeActivity(), filename);
}

void Platform::rotateScreen(int rotation) const
{
    ANativeActivity *activity = m_activity.getNativeActivity();
    setRequestedOrientation(activity, tcu::Android::mapScreenRotation((tcu::ScreenRotation)rotation));
}

} // namespace Android
} // namespace tcu

tcu::Platform *createPlatform(void)
{
    tcu::Android::NativeActivity activity(NULL);
    return new tcu::Android::Platform(activity);
}
