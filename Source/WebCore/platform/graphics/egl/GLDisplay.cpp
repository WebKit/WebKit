/*
 * Copyright (C) 2024 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "config.h"
#include "GLDisplay.h"

#include "FourCC.h"
#include "GLContext.h"
#include "Logging.h"
#include <array>
#include <wtf/HashMap.h>
#include <wtf/Hasher.h>
#include <wtf/ListHashSet.h>
#include <wtf/Locker.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringView.h>
#include <wtf/text/UTF8CStringView.h>

#if USE(LIBEPOXY)
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#if OS(ANDROID)
#include "BufferFormatAndroid.h"
#include <android/hardware_buffer.h>
#include <drm/drm_fourcc.h>
#elif USE(GBM)
#include <drm_fourcc.h>
#endif

#if !USE(LIBEPOXY)
typedef EGLImage (EGLAPIENTRYP PFNEGLCREATEIMAGEPROC) (EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLAttrib*);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEPROC) (EGLDisplay, EGLImage);
#ifndef EGL_KHR_image_base
#define EGL_KHR_image_base 1
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEKHRPROC) (EGLDisplay, EGLImage);
typedef EGLImageKHR (EGLAPIENTRYP PFNEGLCREATEIMAGEKHRPROC) (EGLDisplay, EGLContext, EGLenum target, EGLClientBuffer, const EGLint* attribList);
typedef EGLBoolean (EGLAPIENTRYP PFNEGLDESTROYIMAGEKHRPROC) (EGLDisplay, EGLImageKHR);
#endif
#ifndef EGL_ANDROID_blob_cache
#define EGL_ANDROID_blob_cache 1
typedef khronos_ssize_t EGLsizeiANDROID;
typedef void (*EGLSetBlobFuncANDROID) (const void* key, EGLsizeiANDROID keySize, const void* value, EGLsizeiANDROID valueSize);
typedef EGLsizeiANDROID (*EGLGetBlobFuncANDROID) (const void* key, EGLsizeiANDROID keySize, void* value, EGLsizeiANDROID valueSize);
typedef void (EGLAPIENTRYP PFNEGLSETBLOBCACHEFUNCSANDROIDPROC) (EGLDisplay, EGLSetBlobFuncANDROID, EGLGetBlobFuncANDROID);
#endif
#endif

// Epoxy does not yet define these macros as of version 1.5.10
#ifndef EGL_DEVICE_TYPE_EXT
#define EGL_DEVICE_TYPE_EXT 0x3590
#endif

#ifndef EGL_DEVICE_TYPE_CPU_EXT
#define EGL_DEVICE_TYPE_CPU_EXT 0x3594
#endif

#ifndef EGL_RENDERER_EXT
#define EGL_RENDERER_EXT 0x335F
#endif

namespace WebCore {

static bool s_shouldUseInMemoryShaderCache { false };

class InMemoryShaderCache {
    WTF_MAKE_NONCOPYABLE(InMemoryShaderCache);
    friend NeverDestroyed<InMemoryShaderCache>;
public:
    static InMemoryShaderCache& singleton()
    {
        static NeverDestroyed<InMemoryShaderCache> cache;
        return cache.get();
    }

    void set(std::span<const uint8_t> key, std::span<const uint8_t> value)
    {
        if (value.size() > maximumValueSize)
            return;

        Locker locker { m_lock };
        auto addResult = m_entries.add(Vector<uint8_t>(key), Vector<uint8_t> { });
        m_size -= addResult.iterator->value.size();
        addResult.iterator->value = Vector<uint8_t>(value);
        m_size += value.size();
        m_keysByAge.appendOrMoveToLast(addResult.iterator->key);

        while (m_size > maximumSize)
            m_size -= m_entries.take(m_keysByAge.takeFirst()).size();
    }

    size_t get(std::span<const uint8_t> key, std::span<uint8_t> value)
    {
        Locker locker { m_lock };
        auto it = m_entries.find(Vector<uint8_t>(key));
        if (it == m_entries.end())
            return 0;

        m_keysByAge.appendOrMoveToLast(it->key);
        if (it->value.size() <= value.size())
            memcpySpan(value, it->value.span());
        return it->value.size();
    }

private:
    InMemoryShaderCache() = default;

    static constexpr size_t maximumValueSize = 64 * KB;
    static constexpr size_t maximumSize = 8 * MB;

    Lock m_lock;
    HashMap<Vector<uint8_t>, Vector<uint8_t>> m_entries WTF_GUARDED_BY_LOCK(m_lock);
    ListHashSet<Vector<uint8_t>> m_keysByAge WTF_GUARDED_BY_LOCK(m_lock);
    size_t m_size WTF_GUARDED_BY_LOCK(m_lock) { 0 };
};

static void setShaderBlob(const void* key, EGLsizeiANDROID keySize, const void* value, EGLsizeiANDROID valueSize)
{
    if (keySize <= 0 || valueSize <= 0)
        return;

    InMemoryShaderCache::singleton().set(unsafeMakeSpan(static_cast<const uint8_t*>(key), keySize), unsafeMakeSpan(static_cast<const uint8_t*>(value), valueSize));
}

static EGLsizeiANDROID getShaderBlob(const void* key, EGLsizeiANDROID keySize, void* value, EGLsizeiANDROID valueSize)
{
    if (keySize <= 0 || valueSize < 0)
        return 0;

    return InMemoryShaderCache::singleton().get(unsafeMakeSpan(static_cast<const uint8_t*>(key), keySize), unsafeMakeSpan(static_cast<uint8_t*>(value), valueSize));
}

void GLDisplay::setShouldUseInMemoryShaderCache(bool shouldUse)
{
    s_shouldUseInMemoryShaderCache = shouldUse;
}

RefPtr<GLDisplay> GLDisplay::create(EGLDisplay eglDisplay)
{
#if !USE(GRAPHICS_LAYER_WC)
    ASSERT(isMainThread());
#endif
    if (eglDisplay == EGL_NO_DISPLAY)
        return nullptr;

    if (eglInitialize(eglDisplay, nullptr, nullptr) == EGL_FALSE)
        return nullptr;

    return adoptRef(new GLDisplay(eglDisplay));
}

GLDisplay::GLDisplay(EGLDisplay eglDisplay)
    : m_display(eglDisplay)
{
    EGLint majorVersion, minorVersion;
    eglInitialize(m_display, &majorVersion, &minorVersion);
    m_version.major = majorVersion;
    m_version.minor = minorVersion;

    const char* extensionsString = eglQueryString(m_display, EGL_EXTENSIONS);
    auto displayExtensions = StringView::fromLatin1(extensionsString).split(' ');
    auto findExtension = [&](auto extensionName) {
        return std::any_of(displayExtensions.begin(), displayExtensions.end(), [&](auto extensionEntry) {
            return extensionEntry == extensionName;
        });
    };

    m_extensions.KHR_image_base = findExtension("EGL_KHR_image_base"_s);
    m_extensions.KHR_surfaceless_context = findExtension("EGL_KHR_surfaceless_context"_s);
    m_extensions.KHR_fence_sync = findExtension("EGL_KHR_fence_sync"_s);
    m_extensions.KHR_wait_sync = findExtension("EGL_KHR_wait_sync"_s);
    m_extensions.ANDROID_native_fence_sync = findExtension("EGL_ANDROID_native_fence_sync"_s);
    m_extensions.ANDROID_blob_cache = findExtension("EGL_ANDROID_blob_cache"_s);
    m_extensions.EXT_image_dma_buf_import = findExtension("EGL_EXT_image_dma_buf_import"_s);
    m_extensions.EXT_image_dma_buf_import_modifiers = findExtension("EGL_EXT_image_dma_buf_import_modifiers"_s);
    m_extensions.MESA_image_dma_buf_export = findExtension("EGL_MESA_image_dma_buf_export"_s);

    if (s_shouldUseInMemoryShaderCache && m_extensions.ANDROID_blob_cache) {
        static PFNEGLSETBLOBCACHEFUNCSANDROIDPROC s_eglSetBlobCacheFuncsANDROID = reinterpret_cast<PFNEGLSETBLOBCACHEFUNCSANDROIDPROC>(eglGetProcAddress("eglSetBlobCacheFuncsANDROID"));
        if (s_eglSetBlobCacheFuncsANDROID)
            s_eglSetBlobCacheFuncsANDROID(m_display, setShaderBlob, getShaderBlob);
    }

#if OS(ANDROID)
    m_extensions.ANDROID_get_native_client_buffer = findExtension("EGL_ANDROID_get_native_client_buffer"_s);
    m_extensions.ANDROID_image_native_buffer = findExtension("EGL_ANDROID_image_native_buffer"_s);
#endif
}

void GLDisplay::terminate()
{
    Locker locker { m_terminationLock };
    auto display = m_display.exchange(EGL_NO_DISPLAY);
    if (display == EGL_NO_DISPLAY)
        return;

    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(display);
}

bool GLDisplay::runIfNotTerminated(NOESCAPE const Function<void(EGLDisplay)>& function)
{
    Locker locker { m_terminationLock };
    auto display = m_display.load();
    if (display == EGL_NO_DISPLAY)
        return false;

    function(display);
    return true;
}

bool GLDisplay::checkVersion(int major, int minor) const
{
    return (m_version.major > major) || ((m_version.major == major) && (m_version.minor >= minor));
}

#if USE(LIBEPOXY)
bool GLDisplay::isSoftwareRendered() const
{
    if (m_display == EGL_NO_DISPLAY)
        return false;

    static const auto containsSoftwareRendererName = [](const UTF8CStringView& rendererName) {
        static constexpr ASCIILiteral substringsToCheck[] = {
            "llvmpipe"_s,
            "swrast"_s,
        };

        for (const auto& substring : substringsToCheck) {
            if (contains(rendererName.span(), substring))
                return true;
        }

        return false;
    };

    if (GLContext::isExtensionSupported(eglQueryString(nullptr, EGL_EXTENSIONS), "EGL_EXT_device_query")) {
        EGLDeviceEXT eglDevice;
        if (eglQueryDisplayAttribEXT(m_display, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&eglDevice))) {
            const char* deviceExtensionsString = eglQueryDeviceStringEXT(eglDevice, EGL_EXTENSIONS);
            if (GLContext::isExtensionSupported(deviceExtensionsString, "EGL_EXT_device_type")) {
                EGLAttrib deviceType;
                if (eglQueryDeviceAttribEXT(eglDevice, EGL_DEVICE_TYPE_EXT, &deviceType))
                    return deviceType == EGL_DEVICE_TYPE_CPU_EXT;
            }

            if (GLContext::isExtensionSupported(deviceExtensionsString, "EGL_MESA_device_software"))
                return true;

            if (GLContext::isExtensionSupported(deviceExtensionsString, "EGL_EXT_device_query_name")) {
                if (const char* rendererName = eglQueryDeviceStringEXT(eglDevice, EGL_RENDERER_EXT))
                    return containsSoftwareRendererName(UTF8CStringView::unsafeFromUTF8(rendererName));
            }
        }
    }

    if (const char* rendererName = reinterpret_cast<const char*>(glGetString(GL_RENDERER)))
        return containsSoftwareRendererName(UTF8CStringView::unsafeFromUTF8(rendererName));

    return false;
}
#endif // USE(LIBEPOXY)

EGLImage GLDisplay::createImage(EGLContext context, EGLenum target, EGLClientBuffer clientBuffer, const Vector<EGLAttrib>& attributes) const
{
    if (m_display == EGL_NO_DISPLAY)
        return EGL_NO_IMAGE;

    if (checkVersion(1, 5)) {
        static PFNEGLCREATEIMAGEPROC s_eglCreateImage = reinterpret_cast<PFNEGLCREATEIMAGEPROC>(eglGetProcAddress("eglCreateImage"));
        if (s_eglCreateImage)
            return s_eglCreateImage(m_display, context, target, clientBuffer, attributes.isEmpty() ? nullptr : attributes.span().data());
        return EGL_NO_IMAGE;
    }

    if (!m_extensions.KHR_image_base)
        return EGL_NO_IMAGE;

    Vector<EGLint> intAttributes = attributes.map<Vector<EGLint>>([] (EGLAttrib value) {
        return value;
    });
    static PFNEGLCREATEIMAGEKHRPROC s_eglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
    if (s_eglCreateImageKHR)
        return s_eglCreateImageKHR(m_display, context, target, clientBuffer, intAttributes.isEmpty() ? nullptr : intAttributes.span().data());
    return EGL_NO_IMAGE_KHR;
}

bool GLDisplay::destroyImage(EGLImage image) const
{
    if (m_display == EGL_NO_DISPLAY)
        return false;

    if (checkVersion(1, 5)) {
        static PFNEGLDESTROYIMAGEPROC s_eglDestroyImage = reinterpret_cast<PFNEGLDESTROYIMAGEPROC>(eglGetProcAddress("eglDestroyImage"));
        if (s_eglDestroyImage)
            return s_eglDestroyImage(m_display, image);
        return false;
    }

    if (!m_extensions.KHR_image_base)
        return false;

    static PFNEGLDESTROYIMAGEKHRPROC s_eglDestroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    if (s_eglDestroyImageKHR)
        return s_eglDestroyImageKHR(m_display, image);
    return false;
}

#if USE(GBM)
static Vector<GLDisplay::BufferFormat> queryDMABufFormats(EGLDisplay eglDisplay, const Vector<FourCC>& supportedFormats, bool supportModifiers)
{
    static PFNEGLQUERYDMABUFFORMATSEXTPROC s_eglQueryDmaBufFormatsEXT = reinterpret_cast<PFNEGLQUERYDMABUFFORMATSEXTPROC>(eglGetProcAddress("eglQueryDmaBufFormatsEXT"));
    if (!s_eglQueryDmaBufFormatsEXT)
        return { };

    EGLint formatsCount;
    if (!s_eglQueryDmaBufFormatsEXT(eglDisplay, 0, nullptr, &formatsCount) || !formatsCount)
        return { };

    Vector<EGLint> formats(formatsCount);
    if (!s_eglQueryDmaBufFormatsEXT(eglDisplay, formatsCount, reinterpret_cast<EGLint*>(formats.mutableSpan().data()), &formatsCount))
        return { };

    static PFNEGLQUERYDMABUFMODIFIERSEXTPROC s_eglQueryDmaBufModifiersEXT = reinterpret_cast<PFNEGLQUERYDMABUFMODIFIERSEXTPROC>(eglGetProcAddress("eglQueryDmaBufModifiersEXT"));

    return WTF::compactMap(supportedFormats, [&](FourCC format) -> std::optional<GLDisplay::BufferFormat> {
        if (!formats.contains(format))
            return std::nullopt;

        Vector<uint64_t, 1> dmabufModifiers = { DRM_FORMAT_MOD_INVALID };
        if (supportModifiers && s_eglQueryDmaBufModifiersEXT) {
            EGLint modifiersCount;
            if (s_eglQueryDmaBufModifiersEXT(eglDisplay, format.value, 0, nullptr, nullptr, &modifiersCount) && modifiersCount) {
                Vector<EGLuint64KHR> modifiers(modifiersCount);
                if (s_eglQueryDmaBufModifiersEXT(eglDisplay, format.value, modifiersCount, reinterpret_cast<EGLuint64KHR*>(modifiers.mutableSpan().data()), nullptr, &modifiersCount)) {
                    dmabufModifiers.grow(modifiersCount);
                    for (int i = 0; i < modifiersCount; ++i)
                        dmabufModifiers[i] = modifiers[i];
                }
            }
        }
        return GLDisplay::BufferFormat { format, WTF::move(dmabufModifiers) };
    });
}

const Vector<GLDisplay::BufferFormat>& GLDisplay::bufferFormats()
{
    Locker locker { m_bufferFormatsLock };
    if (!m_bufferFormatsInitialized) {
        if (m_display != EGL_NO_DISPLAY && m_extensions.EXT_image_dma_buf_import) {
            // For now we only support formats that can be created with a single GBM buffer for all planes.
            static const Vector<FourCC> s_supportedFormats = {
                DRM_FORMAT_XRGB8888, DRM_FORMAT_RGBX8888, DRM_FORMAT_XBGR8888, DRM_FORMAT_BGRX8888,
                DRM_FORMAT_ARGB8888, DRM_FORMAT_RGBA8888, DRM_FORMAT_ABGR8888, DRM_FORMAT_BGRA8888,
                DRM_FORMAT_RGB565,
                DRM_FORMAT_XRGB2101010, DRM_FORMAT_XBGR2101010, DRM_FORMAT_ARGB2101010, DRM_FORMAT_ABGR2101010,
                DRM_FORMAT_XRGB16161616F, DRM_FORMAT_XBGR16161616F, DRM_FORMAT_ARGB16161616F, DRM_FORMAT_ABGR16161616F
            };
            m_bufferFormats = queryDMABufFormats(m_display, s_supportedFormats, m_extensions.EXT_image_dma_buf_import_modifiers);
        }
        m_bufferFormatsInitialized = true;
    }
    return m_bufferFormats;
}

#if USE(GSTREAMER)
const Vector<GLDisplay::BufferFormat>& GLDisplay::bufferFormatsForVideo()
{
    Locker locker { m_bufferFormatsForVideoLock };
    if (!m_bufferFormatsForVideoInitialized) {
        if (m_display != EGL_NO_DISPLAY && m_extensions.EXT_image_dma_buf_import) {
            // Formats supported by the texture mapper.
            // FIXME: add support for YUY2, YVYU, UYVY, VYUY, AYUV.
            static const Vector<FourCC> s_supportedFormats = {
                DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888, DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888,
                DRM_FORMAT_YUV420, DRM_FORMAT_YVU420, DRM_FORMAT_NV12, DRM_FORMAT_NV21,
                DRM_FORMAT_YUV444, DRM_FORMAT_YUV411, DRM_FORMAT_YUV422, DRM_FORMAT_P010
            };
            m_bufferFormatsForVideo = queryDMABufFormats(m_display, s_supportedFormats, m_extensions.EXT_image_dma_buf_import_modifiers);
        }
        m_bufferFormatsForVideoInitialized = true;
    }
    return m_bufferFormatsForVideo;
}
#endif
#endif // USE(GBM)

#if OS(ANDROID)
const Vector<GLDisplay::BufferFormat>& GLDisplay::bufferFormats()
{
    static LazyNeverDestroyed<Vector<GLDisplay::BufferFormat>> formats;
    static std::once_flag flag;
    std::call_once(flag, []() {
        formats.construct();

        // This list includes those formats supported by AHardwareBuffer which are suitable for rendering content, sorted by preference. See:
        // https://android.googlesource.com/platform/frameworks/native/+/4f463a6b1de9198963dc6aff74154a504ba3f8f6/libs/nativewindow/include/android/hardware_buffer.h#66
        static constexpr auto drmFormats = WTF::toArray<FourCC>({
            DRM_FORMAT_RGBA8888,
            DRM_FORMAT_RGBX8888,
            DRM_FORMAT_RGB565,
            DRM_FORMAT_RGBA1010102,
            DRM_FORMAT_RGB888,
        });

        // The usage flags match the common set used in the usageToAHardwareBufferUsage() helper function
        // in AcceleratedSurface.cpp, under the assumption that the additional flag hinting that buffers
        // may be mapped, and the scanout flags can be added to formats determined here to be supported.
        //
        // The width, height, and layers count cannot be zero, otherwise AHardwareBuffer_isSupported()
        // will always fail. Ideally the check would be done with the actual size needed for an allocation
        // but that would require additional plumbing and does not seem to be needed at the moment.
        AHardwareBuffer_Desc description { };
        description.usage = AHARDWAREBUFFER_USAGE_GPU_FRAMEBUFFER | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
        description.width = description.height = description.layers = 1;

        for (const auto& drmFormat : drmFormats) {
            const auto ahbFormat = toAHardwareBufferFormat(drmFormat);
            RELEASE_ASSERT(ahbFormat);
            description.format = ahbFormat.value();
            if (AHardwareBuffer_isSupported(&description)) {
                RELEASE_LOG_DEBUG(GraphicsBuffer, "AHB: Adding supported DRM format '%s'", drmFormat.string().data());
                formats->append(GLDisplay::BufferFormat(drmFormat.value));
            } else
                RELEASE_LOG_DEBUG(GraphicsBuffer, "AHB: Skipping unsupported DRM format '%s'", drmFormat.string().data());
        }
        RELEASE_LOG_DEBUG(GraphicsBuffer, "AHB: There are %zu supported formats", formats->size());
    });

    return formats.get();
}
#endif // OS(ANDROID)

} // namespace WebCore
