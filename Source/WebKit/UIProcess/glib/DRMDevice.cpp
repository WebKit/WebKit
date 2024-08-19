/*
 * Copyright (C) 2024 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DRMDevice.h"

#include <WebCore/GLContext.h>
#include <WebCore/PlatformDisplay.h>
#include <epoxy/egl.h>
#include <mutex>
#include <wtf/Function.h>
#include <wtf/NeverDestroyed.h>

#if PLATFORM(GTK)
#include "Display.h"
#endif

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
#include <wpe/wpe-platform.h>
#endif

#if USE(LIBDRM)
#include <xf86drm.h>
#endif

#ifndef EGL_DRM_RENDER_NODE_FILE_EXT
#define EGL_DRM_RENDER_NODE_FILE_EXT 0x3377
#endif

namespace WebKit {

static EGLDeviceEXT eglDisplayDevice(EGLDisplay eglDisplay)
{
    if (!WebCore::GLContext::isExtensionSupported(eglQueryString(nullptr, EGL_EXTENSIONS), "EGL_EXT_device_query"))
        return nullptr;

    EGLDeviceEXT eglDevice;
    if (eglQueryDisplayAttribEXT(eglDisplay, EGL_DEVICE_EXT, reinterpret_cast<EGLAttrib*>(&eglDevice)))
        return eglDevice;

    return nullptr;
}

#if USE(LIBDRM)
static void drmForeachDevice(Function<bool(drmDevice*)>&& functor)
{
    drmDevicePtr devices[64];
    memset(devices, 0, sizeof(devices));

    int numDevices = drmGetDevices2(0, devices, std::size(devices));
    if (numDevices <= 0)
        return;

    for (int i = 0; i < numDevices; ++i) {
        if (!functor(devices[i]))
            break;
    }
    drmFreeDevices(devices, numDevices);
}

static String drmFirstRenderNode()
{
    String renderNodeDeviceFile;
    drmForeachDevice([&](drmDevice* device) {
        if (!(device->available_nodes & (1 << DRM_NODE_RENDER)))
            return true;

        renderNodeDeviceFile = String::fromUTF8(device->nodes[DRM_NODE_RENDER]);
        return false;
    });
    return renderNodeDeviceFile;
}

static String drmRenderNodeFromPrimaryDeviceFile(const String& primaryDeviceFile)
{
    if (primaryDeviceFile.isEmpty())
        return drmFirstRenderNode();

    String renderNodeDeviceFile;
    drmForeachDevice([&](drmDevice* device) {
        if (!(device->available_nodes & (1 << DRM_NODE_PRIMARY | 1 << DRM_NODE_RENDER)))
            return true;

        if (String::fromUTF8(device->nodes[DRM_NODE_PRIMARY]) == primaryDeviceFile) {
            renderNodeDeviceFile = String::fromUTF8(device->nodes[DRM_NODE_RENDER]);
            return false;
        }

        return true;
    });
    // If we fail to find a render node for the device file, just use the device file as render node.
    return !renderNodeDeviceFile.isEmpty() ? renderNodeDeviceFile : primaryDeviceFile;
}
#endif

static String drmRenderNodeForEGLDisplay(EGLDisplay eglDisplay)
{
    if (EGLDeviceEXT device = eglDisplayDevice(eglDisplay)) {
        if (WebCore::GLContext::isExtensionSupported(eglQueryDeviceStringEXT(device, EGL_EXTENSIONS), "EGL_EXT_device_drm_render_node"))
            return String::fromUTF8(eglQueryDeviceStringEXT(device, EGL_DRM_RENDER_NODE_FILE_EXT));

#if USE(LIBDRM)
        // If EGL_EXT_device_drm_render_node is not present, try to get the render node using DRM API.
        return drmRenderNodeFromPrimaryDeviceFile(drmPrimaryDevice());
#endif
    }

#if USE(LIBDRM)
    // If EGLDevice is not available, just get the first render node returned by DRM.
    return drmFirstRenderNode();
#else
    return { };
#endif
}

static String drmPrimaryDeviceForEGLDisplay(EGLDisplay eglDisplay)
{
    EGLDeviceEXT device = eglDisplayDevice(eglDisplay);
    if (!device)
        return { };

    if (!WebCore::GLContext::isExtensionSupported(eglQueryDeviceStringEXT(device, EGL_EXTENSIONS), "EGL_EXT_device_drm"))
        return { };

    return String::fromUTF8(eglQueryDeviceStringEXT(device, EGL_DRM_DEVICE_FILE_EXT));
}

static EGLDisplay currentEGLDisplay()
{
#if PLATFORM(GTK)
    if (auto* glDisplay = Display::singleton().glDisplay())
        return glDisplay->eglDisplay();
#endif

    auto eglDisplay = eglGetCurrentDisplay();
    if (eglDisplay != EGL_NO_DISPLAY)
        return eglDisplay;

    return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

const String& drmPrimaryDevice()
{
    static LazyNeverDestroyed<String> primaryDevice;
    static std::once_flag once;
    std::call_once(once, [] {
#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
        if (g_type_class_peek(WPE_TYPE_DISPLAY)) {
            primaryDevice.construct(String::fromUTF8(wpe_display_get_drm_device(wpe_display_get_primary())));
            return;
        }
#endif

        auto eglDisplay = currentEGLDisplay();
        if (eglDisplay != EGL_NO_DISPLAY) {
            primaryDevice.construct(drmPrimaryDeviceForEGLDisplay(eglDisplay));
            return;
        }

        primaryDevice.construct();
    });
    return primaryDevice.get();
}

const String& drmRenderNodeDevice()
{
    static LazyNeverDestroyed<String> renderNodeDevice;
    static std::once_flag once;
    std::call_once(once, [] {
#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
        if (g_type_class_peek(WPE_TYPE_DISPLAY)) {
            renderNodeDevice.construct(String::fromUTF8(wpe_display_get_drm_render_node(wpe_display_get_primary())));
            return;
        }
#endif

        const char* envDeviceFile = getenv("WEBKIT_WEB_RENDER_DEVICE_FILE");
        if (envDeviceFile && *envDeviceFile) {
            renderNodeDevice.construct(String::fromUTF8(envDeviceFile));
            return;
        }

        auto eglDisplay = currentEGLDisplay();
        if (eglDisplay != EGL_NO_DISPLAY) {
            renderNodeDevice.construct(drmRenderNodeForEGLDisplay(eglDisplay));
            return;
        }

        renderNodeDevice.construct();
    });
    return renderNodeDevice.get();
}

} // namespace WebKit
