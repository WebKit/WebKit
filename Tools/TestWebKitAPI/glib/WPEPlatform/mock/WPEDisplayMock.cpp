/*
 * Copyright (C) 2025 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEDisplayMock.h"

#include "WPEScreenMock.h"
#include "WPEToplevelMock.h"
#include "WPEViewMock.h"
#include <gio/gio.h>
#include <gmodule.h>
#include <mutex>

#if USE(LIBDRM)
#include <drm_fourcc.h>
#endif

struct _WPEDisplayMock {
    WPEDisplay parent;

    gboolean isConnected;
    gboolean useFakeDMABufFormats;
    gboolean useExplicitSync;
    WPEDRMDevice* fakeDRMDevice;
    WPEDRMDevice* fakeDisplayDevice;

    WPEScreen* mainScreen;
    WPEScreen* secondaryScreen;

    unsigned inputDevices;
};

G_DEFINE_DYNAMIC_TYPE(WPEDisplayMock, wpe_display_mock, WPE_TYPE_DISPLAY)

static void wpeDisplayMockConstructed(GObject* object)
{
    G_OBJECT_CLASS(wpe_display_mock_parent_class)->constructed(object);

    auto* mock = WPE_DISPLAY_MOCK(object);
    mock->mainScreen = WPE_SCREEN(g_object_new(WPE_TYPE_SCREEN_MOCK, "id", 1, "x", 0, "y", 0, "width", 800, "height", 600, "refresh-rate", 60000, nullptr));
}

static void wpeDisplayMockDispose(GObject* object)
{
    auto* mock = WPE_DISPLAY_MOCK(object);
    g_clear_object(&mock->mainScreen);
    g_clear_object(&mock->secondaryScreen);
    g_clear_pointer(&mock->fakeDRMDevice, wpe_drm_device_unref);
    g_clear_pointer(&mock->fakeDisplayDevice, wpe_drm_device_unref);

    G_OBJECT_CLASS(wpe_display_mock_parent_class)->dispose(object);
}

static gboolean wpeDisplayMockConnect(WPEDisplay* display, GError** error)
{
    auto* mock = WPE_DISPLAY_MOCK(display);
    if (mock->isConnected) {
        g_set_error_literal(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED, "Mock display is already connected");
        return FALSE;
    }
    mock->isConnected = TRUE;
    return TRUE;
}

static WPEView* wpeDisplayMockCreateView(WPEDisplay* display)
{
    auto* view = WPE_VIEW(g_object_new(WPE_TYPE_VIEW_MOCK, "display", display, nullptr));

    if (wpe_settings_get_boolean(wpe_display_get_settings(display), WPE_SETTING_CREATE_VIEWS_WITH_A_TOPLEVEL, nullptr)) {
        WPEToplevel* toplevel = wpeToplevelMockNew(WPE_DISPLAY_MOCK(display), 1);
        wpe_view_set_toplevel(view, toplevel);
        g_object_unref(toplevel);
    }

    return view;
}

static WPEInputMethodContext* wpeDisplayMockCreateInputMethodContext(WPEDisplay* display, WPEView*)
{
    return nullptr;
}

static gpointer wpeDisplayMockGetEGLDisplay(WPEDisplay* display, GError** error)
{
    g_set_error_literal(error, WPE_EGL_ERROR, WPE_EGL_ERROR_NOT_AVAILABLE, "Can't get EGL display: no display connection matching mock connection found");
    return nullptr;
}

static WPEKeymap* wpeDisplayMockGetKeymap(WPEDisplay* display)
{
    return nullptr;
}

static WPEBufferDMABufFormats* wpeDisplayMockGetPreferredDMABufFormats(WPEDisplay* display)
{
    auto* mock = WPE_DISPLAY_MOCK(display);
    if (!mock->useFakeDMABufFormats)
        return nullptr;

    auto* builder = wpe_buffer_dma_buf_formats_builder_new(mock->fakeDRMDevice);
    if (!mock->fakeDisplayDevice)
        mock->fakeDisplayDevice = wpe_drm_device_new("/dev/dri/mock1", nullptr);
    wpe_buffer_dma_buf_formats_builder_append_group(builder, mock->fakeDisplayDevice, WPE_BUFFER_DMA_BUF_FORMAT_USAGE_SCANOUT);
#if USE(LIBDRM)
    wpe_buffer_dma_buf_formats_builder_append_format(builder, DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_VIVANTE_SUPER_TILED);
    wpe_buffer_dma_buf_formats_builder_append_format(builder, DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_VIVANTE_TILED);
#endif
    wpe_buffer_dma_buf_formats_builder_append_group(builder, nullptr, WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING);
#if USE(LIBDRM)
    wpe_buffer_dma_buf_formats_builder_append_format(builder, DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR);
    wpe_buffer_dma_buf_formats_builder_append_format(builder, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR);
#endif
    auto* formats = wpe_buffer_dma_buf_formats_builder_end(builder);
    wpe_buffer_dma_buf_formats_builder_unref(builder);

    return formats;
}

static guint wpeDisplayMockGetNScreens(WPEDisplay* display)
{
    auto* mock = WPE_DISPLAY_MOCK(display);
    return mock->secondaryScreen ? 2 : 1;
}

static WPEScreen* wpeDisplayMockGetScreen(WPEDisplay* display, guint index)
{
    auto* mock = WPE_DISPLAY_MOCK(display);
    switch (index) {
    case 0:
        return mock->mainScreen;
    case 1:
        if (mock->secondaryScreen)
            return mock->secondaryScreen;
        break;
    }
    return nullptr;
}

static WPEDRMDevice* wpeDisplayMockGetDRMDevice(WPEDisplay* display)
{
    return WPE_DISPLAY_MOCK(display)->fakeDRMDevice;
}

static gboolean wpeDisplayMockUseExplicitSync(WPEDisplay* display)
{
    return WPE_DISPLAY_MOCK(display)->useExplicitSync;
}

static void wpe_display_mock_class_init(WPEDisplayMockClass* displayMockClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(displayMockClass);
    objectClass->constructed = wpeDisplayMockConstructed;
    objectClass->dispose = wpeDisplayMockDispose;

    WPEDisplayClass* displayClass = WPE_DISPLAY_CLASS(displayMockClass);
    displayClass->connect = wpeDisplayMockConnect;
    displayClass->create_view = wpeDisplayMockCreateView;
    displayClass->create_input_method_context = wpeDisplayMockCreateInputMethodContext;
    displayClass->get_egl_display = wpeDisplayMockGetEGLDisplay;
    displayClass->get_keymap = wpeDisplayMockGetKeymap;
    displayClass->get_preferred_dma_buf_formats = wpeDisplayMockGetPreferredDMABufFormats;
    displayClass->get_n_screens = wpeDisplayMockGetNScreens;
    displayClass->get_screen = wpeDisplayMockGetScreen;
    displayClass->get_drm_device = wpeDisplayMockGetDRMDevice;
    displayClass->use_explicit_sync = wpeDisplayMockUseExplicitSync;
}

static void wpe_display_mock_class_finalize(WPEDisplayMockClass*)
{
}

static void wpe_display_mock_init(WPEDisplayMock* self)
{
}

WPEDisplay* wpeDisplayMockNew()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        wpeDisplayMockRegister(nullptr);
    });
    return WPE_DISPLAY(g_object_new(WPE_TYPE_DISPLAY_MOCK, nullptr));
}

void wpeDisplayMockRegister(GIOModule* ioModule)
{
    wpe_display_mock_register_type(G_TYPE_MODULE(ioModule));
    if (!ioModule)
        g_io_extension_point_register(WPE_DISPLAY_EXTENSION_POINT_NAME);
    g_io_extension_point_implement(WPE_DISPLAY_EXTENSION_POINT_NAME, WPE_TYPE_DISPLAY_MOCK, "wpe-display-mock", G_MAXINT32);
}

void wpeDisplayMockDisconnect(WPEDisplayMock* mock)
{
    mock->isConnected = FALSE;
    GError* error = g_error_new_literal(WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_LOST, "Display disconnected");
    wpe_display_disconnected(WPE_DISPLAY(mock), error);
    g_error_free(error);
}

void wpeDisplayMockUseFakeDRMNodes(WPEDisplayMock* mock, gboolean useFakeDRMNodes)
{
    if (!useFakeDRMNodes) {
        g_clear_pointer(&mock->fakeDRMDevice, wpe_drm_device_unref);
        return;
    }

    if (!mock->fakeDRMDevice)
        mock->fakeDRMDevice = wpe_drm_device_new("/dev/dri/mock0", "/dev/dri/mockD128");
}

void wpeDisplayMockUseFakeDMABufFormats(WPEDisplayMock* mock, gboolean useFakeDMABufFormats)
{
    mock->useFakeDMABufFormats = useFakeDMABufFormats;
}

void wpeDisplayMockSetUseExplicitSync(WPEDisplayMock* mock, gboolean useExplicitSync)
{
    mock->useExplicitSync = useExplicitSync;
}

void wpeDisplayMockSetInitialInputDevices(WPEDisplayMock* mock, WPEAvailableInputDevices devices)
{
    mock->inputDevices = devices;
    wpe_display_set_available_input_devices(WPE_DISPLAY(mock), static_cast<WPEAvailableInputDevices>(mock->inputDevices));
}

void wpeDisplayMockAddInputDevice(WPEDisplayMock* mock, WPEAvailableInputDevices devices)
{
    mock->inputDevices |= devices;
    wpe_display_set_available_input_devices(WPE_DISPLAY(mock), static_cast<WPEAvailableInputDevices>(mock->inputDevices));
}

void wpeDisplayMockRemoveInputDevice(WPEDisplayMock* mock, WPEAvailableInputDevices devices)
{
    mock->inputDevices &= ~devices;
    wpe_display_set_available_input_devices(WPE_DISPLAY(mock), static_cast<WPEAvailableInputDevices>(mock->inputDevices));
}

void wpeDisplayMockAddSecondaryScreen(WPEDisplayMock* mock)
{
    if (mock->secondaryScreen)
        return;

    mock->secondaryScreen = WPE_SCREEN(g_object_new(WPE_TYPE_SCREEN_MOCK, "id", 2, "x", 0, "y", 0, "width", 1024, "height", 768, "scale", 2., "refresh-rate", 120000, nullptr));
    wpe_display_screen_added(WPE_DISPLAY(mock), mock->secondaryScreen);
}

void wpeDisplayMockRemoveSecondaryScreen(WPEDisplayMock* mock)
{
    if (!mock->secondaryScreen)
        return;

    auto* screen = std::exchange(mock->secondaryScreen, nullptr);
    wpe_display_screen_removed(WPE_DISPLAY(mock), screen);
    g_object_unref(screen);
}
