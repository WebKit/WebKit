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

#include "GLContext.h"
#include <wtf/text/StringView.h>

#if USE(LIBEPOXY)
#include <epoxy/egl.h>
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#if USE(GBM)
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
#endif

namespace WebCore {

std::unique_ptr<GLDisplay> GLDisplay::create(EGLDisplay eglDisplay)
{
    if (eglDisplay == EGL_NO_DISPLAY)
        return nullptr;

    if (eglInitialize(eglDisplay, nullptr, nullptr) == EGL_FALSE)
        return nullptr;

    return makeUnique<GLDisplay>(eglDisplay);
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
    m_extensions.EXT_image_dma_buf_import = findExtension("EGL_EXT_image_dma_buf_import"_s);
    m_extensions.EXT_image_dma_buf_import_modifiers = findExtension("EGL_EXT_image_dma_buf_import_modifiers"_s);
    m_extensions.MESA_image_dma_buf_export = findExtension("EGL_MESA_image_dma_buf_export"_s);
}

void GLDisplay::terminate()
{
    if (m_display == EGL_NO_DISPLAY)
        return;

    eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglTerminate(m_display);
    m_display = EGL_NO_DISPLAY;
}

bool GLDisplay::checkVersion(int major, int minor) const
{
    return (m_version.major > major) || ((m_version.major == major) && (m_version.minor >= minor));
}

EGLImage GLDisplay::createImage(EGLContext context, EGLenum target, EGLClientBuffer clientBuffer, const Vector<EGLAttrib>& attributes) const
{
    if (m_display == EGL_NO_DISPLAY)
        return EGL_NO_IMAGE;

    if (checkVersion(1, 5)) {
        static PFNEGLCREATEIMAGEPROC s_eglCreateImage = reinterpret_cast<PFNEGLCREATEIMAGEPROC>(eglGetProcAddress("eglCreateImage"));
        if (s_eglCreateImage)
            return s_eglCreateImage(m_display, context, target, clientBuffer, attributes.isEmpty() ? nullptr : attributes.data());
        return EGL_NO_IMAGE;
    }

    if (!m_extensions.KHR_image_base)
        return EGL_NO_IMAGE;

    Vector<EGLint> intAttributes = attributes.map<Vector<EGLint>>([] (EGLAttrib value) {
        return value;
    });
    static PFNEGLCREATEIMAGEKHRPROC s_eglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
    if (s_eglCreateImageKHR)
        return s_eglCreateImageKHR(m_display, context, target, clientBuffer, intAttributes.isEmpty() ? nullptr : intAttributes.data());
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
const Vector<GLDisplay::DMABufFormat>& GLDisplay::dmabufFormats()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [this] {
        if (m_display == EGL_NO_DISPLAY)
            return;

        if (!m_extensions.EXT_image_dma_buf_import)
            return;

        static PFNEGLQUERYDMABUFFORMATSEXTPROC s_eglQueryDmaBufFormatsEXT = reinterpret_cast<PFNEGLQUERYDMABUFFORMATSEXTPROC>(eglGetProcAddress("eglQueryDmaBufFormatsEXT"));
        if (!s_eglQueryDmaBufFormatsEXT)
            return;

        EGLint formatsCount;
        if (!s_eglQueryDmaBufFormatsEXT(m_display, 0, nullptr, &formatsCount) || !formatsCount)
            return;

        Vector<EGLint> formats(formatsCount);
        if (!s_eglQueryDmaBufFormatsEXT(m_display, formatsCount, reinterpret_cast<EGLint*>(formats.data()), &formatsCount))
            return;

        static PFNEGLQUERYDMABUFMODIFIERSEXTPROC s_eglQueryDmaBufModifiersEXT = m_extensions.EXT_image_dma_buf_import_modifiers ?
            reinterpret_cast<PFNEGLQUERYDMABUFMODIFIERSEXTPROC>(eglGetProcAddress("eglQueryDmaBufModifiersEXT")) : nullptr;

        // For now we only support formats that can be created with a single GBM buffer for all planes.
        static const Vector<EGLint> s_supportedFormats = {
            DRM_FORMAT_XRGB8888, DRM_FORMAT_RGBX8888, DRM_FORMAT_XBGR8888, DRM_FORMAT_BGRX8888,
            DRM_FORMAT_ARGB8888, DRM_FORMAT_RGBA8888, DRM_FORMAT_ABGR8888, DRM_FORMAT_BGRA8888,
            DRM_FORMAT_RGB565,
            DRM_FORMAT_XRGB2101010, DRM_FORMAT_XBGR2101010, DRM_FORMAT_ARGB2101010, DRM_FORMAT_ABGR2101010,
            DRM_FORMAT_XRGB16161616F, DRM_FORMAT_XBGR16161616F, DRM_FORMAT_ARGB16161616F, DRM_FORMAT_ABGR16161616F
        };

        m_dmabufFormats = WTF::compactMap(s_supportedFormats, [&](auto format) -> std::optional<DMABufFormat> {
            if (!formats.contains(format))
                return std::nullopt;

            Vector<uint64_t, 1> dmabufModifiers = { DRM_FORMAT_MOD_INVALID };
            if (s_eglQueryDmaBufModifiersEXT) {
                EGLint modifiersCount;
                if (s_eglQueryDmaBufModifiersEXT(m_display, format, 0, nullptr, nullptr, &modifiersCount) && modifiersCount) {
                    Vector<EGLuint64KHR> modifiers(modifiersCount);
                    if (s_eglQueryDmaBufModifiersEXT(m_display, format, modifiersCount, reinterpret_cast<EGLuint64KHR*>(modifiers.data()), nullptr, &modifiersCount)) {
                        dmabufModifiers.grow(modifiersCount);
                        for (int i = 0; i < modifiersCount; ++i)
                            dmabufModifiers[i] = modifiers[i];
                    }
                }
            }
            return DMABufFormat { static_cast<uint32_t>(format), WTFMove(dmabufModifiers) };
        });
    });
    return m_dmabufFormats;
}
#endif // USE(GBM)

} // namespace WebCore
