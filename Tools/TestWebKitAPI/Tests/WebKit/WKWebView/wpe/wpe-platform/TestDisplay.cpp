/*
 * Copyright (C) 2025 Igalia, S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "WPEDisplayMock.h"
#include "WPEMockPlatformTest.h"
#include "WPEScreenMock.h"
#include "WPEToplevelMock.h"
#include "WPEViewMock.h"

#if USE(LIBDRM)
#include <drm_fourcc.h>
#endif

namespace TestWebKitAPI {

static void testDisplayConnect(WPEMockPlatformTest* test, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    g_assert_true(wpe_display_connect(test->display(), &error.outPtr()));
    g_assert_no_error(error.get());

    // Can't connect twice.
    g_assert_false(wpe_display_connect(test->display(), &error.outPtr()));
    g_assert_error(error.get(), WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED);
}

static void testDisplayDisconnected(WPEMockPlatformTest* test, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    g_assert_true(wpe_display_connect(test->display(), &error.outPtr()));
    g_assert_no_error(error.get());

    gboolean displayDisconnected = FALSE;
    auto displayDisconnectedID = g_signal_connect(test->display(), "disconnected", G_CALLBACK(+[](WPEDisplay*, GError* error, gboolean* displayDisconnected) {
        *displayDisconnected = TRUE;
        g_assert_error(error, WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_LOST);
    }), &displayDisconnected);

    wpeDisplayMockDisconnect(WPE_DISPLAY_MOCK(test->display()));
    g_assert_true(displayDisconnected);

    g_signal_handler_disconnect(test->display(), displayDisconnectedID);
}

static void testDisplayPrimary(WPEMockPlatformTest* test, gconstpointer)
{
    // The first created display is always the primary.
    g_assert_true(wpe_display_get_primary() == test->display());

    GRefPtr<WPEDisplay> display2 = adoptGRef(wpeDisplayMockNew());
    test->assertObjectIsDeletedWhenTestFinishes(display2.get());
    g_assert_true(wpe_display_get_primary() == test->display());
    wpe_display_set_primary(display2.get());
    g_assert_true(wpe_display_get_primary() == display2.get());

    // If the primary display is destroyed, there's no primary unless explicitly set again.
    display2 = nullptr;
    g_assert_null(wpe_display_get_primary());

    wpe_display_set_primary(test->display());
    g_assert_true(wpe_display_get_primary() == test->display());
}

static void testDisplayKeymap(WPEMockPlatformTest* test, gconstpointer)
{
    // Default XKB keymap is returned when platform doesn't implement it.
    auto* keymap = wpe_display_get_keymap(test->display());
    g_assert_true(WPE_IS_KEYMAP_XKB(keymap));
    test->assertObjectIsDeletedWhenTestFinishes(keymap);
}

static void testDisplayDRMNodes(WPEMockPlatformTest* test, gconstpointer)
{
    g_assert_null(wpe_display_get_drm_device(test->display()));

    wpeDisplayMockUseFakeDRMNodes(WPE_DISPLAY_MOCK(test->display()), TRUE);
    auto* device = wpe_display_get_drm_device(test->display());
    g_assert_nonnull(device);
    g_assert_cmpstr(wpe_drm_device_get_primary_node(device), ==, "/dev/dri/mock0");
    g_assert_cmpstr(wpe_drm_device_get_render_node(device), ==, "/dev/dri/mockD128");
}

static void testDisplayBufferFormats(WPEMockPlatformTest* test, gconstpointer)
{
    g_assert_null(wpe_display_get_preferred_buffer_formats(test->display()));

    wpeDisplayMockUseFakeDRMNodes(WPE_DISPLAY_MOCK(test->display()), TRUE);
    wpeDisplayMockUseFakeBufferFormats(WPE_DISPLAY_MOCK(test->display()), TRUE);
    auto* formats = wpe_display_get_preferred_buffer_formats(test->display());
    g_assert_true(WPE_IS_BUFFER_FORMATS(formats));
    test->assertObjectIsDeletedWhenTestFinishes(formats);

#if USE(LIBDRM)
    auto* device = wpe_buffer_formats_get_device(formats);
    g_assert_nonnull(device);
    g_assert_cmpstr(wpe_drm_device_get_primary_node(device), ==, "/dev/dri/mock0");
    g_assert_cmpstr(wpe_drm_device_get_render_node(device), ==, "/dev/dri/mockD128");

    g_assert_cmpuint(wpe_buffer_formats_get_n_groups(formats), ==, 2);
    g_assert_cmpuint(wpe_buffer_formats_get_group_usage(formats, 0), ==, WPE_BUFFER_FORMAT_USAGE_SCANOUT);
    auto* targetDevice = wpe_buffer_formats_get_group_device(formats, 0);
    g_assert_nonnull(targetDevice);
    g_assert_cmpstr(wpe_drm_device_get_primary_node(targetDevice), ==, "/dev/dri/mock1");
    g_assert_null(wpe_drm_device_get_render_node(targetDevice));
    g_assert_cmpuint(wpe_buffer_formats_get_group_n_formats(formats, 0), ==, 1);
    g_assert_true(wpe_buffer_formats_get_format_fourcc(formats, 0, 0) == DRM_FORMAT_XRGB8888);
    auto* modifiers = wpe_buffer_formats_get_format_modifiers(formats, 0, 0);
    g_assert_cmpuint(modifiers->len, ==, 2);
    guint64* modifier = &g_array_index(modifiers, guint64, 0);
    g_assert_cmpuint(*modifier, ==, DRM_FORMAT_MOD_VIVANTE_SUPER_TILED);
    modifier = &g_array_index(modifiers, guint64, 1);
    g_assert_cmpuint(*modifier, ==, DRM_FORMAT_MOD_VIVANTE_TILED);

    g_assert_cmpuint(wpe_buffer_formats_get_group_usage(formats, 1), ==, WPE_BUFFER_FORMAT_USAGE_RENDERING);
    g_assert_null(wpe_buffer_formats_get_group_device(formats, 1));
    g_assert_cmpuint(wpe_buffer_formats_get_group_n_formats(formats, 1), ==, 2);
    g_assert_true(wpe_buffer_formats_get_format_fourcc(formats, 1, 0) == DRM_FORMAT_XRGB8888);
    modifiers = wpe_buffer_formats_get_format_modifiers(formats, 1, 0);
    g_assert_cmpuint(modifiers->len, ==, 1);
    modifier = &g_array_index(modifiers, guint64, 0);
    g_assert_cmpuint(*modifier, ==, DRM_FORMAT_MOD_LINEAR);
    g_assert_true(wpe_buffer_formats_get_format_fourcc(formats, 1, 1) == DRM_FORMAT_ARGB8888);
    modifiers = wpe_buffer_formats_get_format_modifiers(formats, 1, 1);
    g_assert_cmpuint(modifiers->len, ==, 1);
    modifier = &g_array_index(modifiers, guint64, 0);
    g_assert_cmpuint(*modifier, ==, DRM_FORMAT_MOD_LINEAR);
#endif
}

static void testDisplayExplicitSync(WPEMockPlatformTest* test, gconstpointer)
{
    g_assert_false(wpe_display_use_explicit_sync(test->display()));
    wpeDisplayMockSetUseExplicitSync(WPE_DISPLAY_MOCK(test->display()), TRUE);
    g_assert_true(wpe_display_use_explicit_sync(test->display()));
}

static void testDisplayScreens(WPEMockPlatformTest* test, gconstpointer)
{
    // Mock display has one screen by default.
    g_assert_cmpuint(wpe_display_get_n_screens(test->display()), ==, 1);
    auto* mainScreen = wpe_display_get_screen(test->display(), 0);
    g_assert_true(WPE_IS_SCREEN(mainScreen));
    test->assertObjectIsDeletedWhenTestFinishes(mainScreen);
    g_assert_cmpuint(wpe_screen_get_id(mainScreen), ==, 1);
    g_assert_cmpint(wpe_screen_get_x(mainScreen), ==, 0);
    g_assert_cmpint(wpe_screen_get_y(mainScreen), ==, 0);
    g_assert_cmpint(wpe_screen_get_width(mainScreen), ==, 800);
    g_assert_cmpint(wpe_screen_get_height(mainScreen), ==, 600);
    g_assert_cmpfloat(wpe_screen_get_scale(mainScreen), ==, 1.);
    g_assert_cmpint(wpe_screen_get_refresh_rate(mainScreen), ==, 60000);

    g_assert_null(wpe_display_get_screen(test->display(), 1));

    gboolean screenAdded = FALSE;
    auto screenAddedID = g_signal_connect(test->display(), "screen-added", G_CALLBACK(+[](WPEDisplay*, WPEScreen* screen, gboolean* screenAdded) {
        *screenAdded = TRUE;
        g_assert_cmpuint(wpe_screen_get_id(screen), ==, 2);
    }), &screenAdded);
    wpeDisplayMockAddSecondaryScreen(WPE_DISPLAY_MOCK(test->display()));
    g_assert_true(screenAdded);
    g_assert_cmpuint(wpe_display_get_n_screens(test->display()), ==, 2);
    auto* secondaryScreen = wpe_display_get_screen(test->display(), 1);
    g_assert_true(WPE_IS_SCREEN(secondaryScreen));
    test->assertObjectIsDeletedWhenTestFinishes(secondaryScreen);
    g_assert_cmpuint(wpe_screen_get_id(secondaryScreen), ==, 2);
    g_assert_cmpint(wpe_screen_get_x(secondaryScreen), ==, 0);
    g_assert_cmpint(wpe_screen_get_y(secondaryScreen), ==, 0);
    g_assert_cmpint(wpe_screen_get_width(secondaryScreen), ==, 1024);
    g_assert_cmpint(wpe_screen_get_height(secondaryScreen), ==, 768);
    g_assert_cmpfloat(wpe_screen_get_scale(secondaryScreen), ==, 2.);
    g_assert_cmpint(wpe_screen_get_refresh_rate(secondaryScreen), ==, 120000);

    g_assert_null(wpe_display_get_screen(test->display(), 2));

    gboolean screenRemoved = FALSE;
    auto screenRemovedID = g_signal_connect(test->display(), "screen-removed", G_CALLBACK(+[](WPEDisplay*, WPEScreen* screen, gboolean* screenRemoved) {
        *screenRemoved = TRUE;
        g_assert_cmpuint(wpe_screen_get_id(screen), ==, 2);
        g_assert_true(wpeScreenMockIsInvalid(WPE_SCREEN_MOCK(screen)));
    }), &screenRemoved);
    wpeDisplayMockRemoveSecondaryScreen(WPE_DISPLAY_MOCK(test->display()));
    g_assert_true(screenRemoved);
    g_assert_cmpuint(wpe_display_get_n_screens(test->display()), ==, 1);

    g_signal_handler_disconnect(test->display(), screenAddedID);
    g_signal_handler_disconnect(test->display(), screenRemovedID);
}

class WPEMockAvailableInputDevicesTest : public WPEMockPlatformTest {
public:
    WPE_PLATFORM_TEST_FIXTURE(WPEMockAvailableInputDevicesTest);

    WPEMockAvailableInputDevicesTest()
    {
        wpeDisplayMockSetInitialInputDevices(WPE_DISPLAY_MOCK(display()), static_cast<WPEAvailableInputDevices>(WPE_AVAILABLE_INPUT_DEVICE_MOUSE | WPE_AVAILABLE_INPUT_DEVICE_KEYBOARD));
        g_signal_connect_swapped(display(), "notify::available-input-devices", G_CALLBACK(+[](WPEMockAvailableInputDevicesTest* test) {
            test->m_propertyChanged = true;
        }), this);
    }

    ~WPEMockAvailableInputDevicesTest()
    {
        g_signal_handlers_disconnect_by_data(display(), this);
    }

    bool addDevice(WPEAvailableInputDevices device)
    {
        m_propertyChanged = false;
        wpeDisplayMockAddInputDevice(WPE_DISPLAY_MOCK(display()), device);
        return std::exchange(m_propertyChanged, false);
    }

    bool removeDevice(WPEAvailableInputDevices device)
    {
        m_propertyChanged = false;
        wpeDisplayMockRemoveInputDevice(WPE_DISPLAY_MOCK(display()), device);
        return std::exchange(m_propertyChanged, false);
    }

private:
    bool m_propertyChanged { false };
};

static void testDisplayAvailableInputDevices(WPEMockAvailableInputDevicesTest* test, gconstpointer)
{
    auto devices = wpe_display_get_available_input_devices(test->display());
    g_assert_true(devices & WPE_AVAILABLE_INPUT_DEVICE_MOUSE);
    g_assert_true(devices & WPE_AVAILABLE_INPUT_DEVICE_KEYBOARD);
    g_assert_false(devices & WPE_AVAILABLE_INPUT_DEVICE_TOUCHSCREEN);

    g_assert_true(test->addDevice(WPE_AVAILABLE_INPUT_DEVICE_TOUCHSCREEN));
    devices = wpe_display_get_available_input_devices(test->display());
    g_assert_true(devices & WPE_AVAILABLE_INPUT_DEVICE_MOUSE);
    g_assert_true(devices & WPE_AVAILABLE_INPUT_DEVICE_KEYBOARD);
    g_assert_true(devices & WPE_AVAILABLE_INPUT_DEVICE_TOUCHSCREEN);

    g_assert_false(test->addDevice(WPE_AVAILABLE_INPUT_DEVICE_MOUSE));
    g_assert_false(test->addDevice(WPE_AVAILABLE_INPUT_DEVICE_KEYBOARD));
    g_assert_false(test->addDevice(WPE_AVAILABLE_INPUT_DEVICE_TOUCHSCREEN));

    g_assert_true(test->removeDevice(WPE_AVAILABLE_INPUT_DEVICE_MOUSE));
    devices = wpe_display_get_available_input_devices(test->display());
    g_assert_false(devices & WPE_AVAILABLE_INPUT_DEVICE_MOUSE);
    g_assert_true(devices & WPE_AVAILABLE_INPUT_DEVICE_KEYBOARD);
    g_assert_true(devices & WPE_AVAILABLE_INPUT_DEVICE_TOUCHSCREEN);
    g_assert_false(test->removeDevice(WPE_AVAILABLE_INPUT_DEVICE_MOUSE));

    g_assert_true(test->removeDevice(WPE_AVAILABLE_INPUT_DEVICE_KEYBOARD));
    devices = wpe_display_get_available_input_devices(test->display());
    g_assert_false(devices & WPE_AVAILABLE_INPUT_DEVICE_MOUSE);
    g_assert_false(devices & WPE_AVAILABLE_INPUT_DEVICE_KEYBOARD);
    g_assert_true(devices & WPE_AVAILABLE_INPUT_DEVICE_TOUCHSCREEN);
    g_assert_false(test->removeDevice(WPE_AVAILABLE_INPUT_DEVICE_MOUSE));
    g_assert_false(test->removeDevice(WPE_AVAILABLE_INPUT_DEVICE_KEYBOARD));

    g_assert_true(test->removeDevice(WPE_AVAILABLE_INPUT_DEVICE_TOUCHSCREEN));
    devices = wpe_display_get_available_input_devices(test->display());
    g_assert_false(devices & WPE_AVAILABLE_INPUT_DEVICE_MOUSE);
    g_assert_false(devices & WPE_AVAILABLE_INPUT_DEVICE_KEYBOARD);
    g_assert_false(devices & WPE_AVAILABLE_INPUT_DEVICE_TOUCHSCREEN);
    g_assert_false(test->removeDevice(WPE_AVAILABLE_INPUT_DEVICE_MOUSE));
    g_assert_false(test->removeDevice(WPE_AVAILABLE_INPUT_DEVICE_KEYBOARD));
    g_assert_false(test->removeDevice(WPE_AVAILABLE_INPUT_DEVICE_TOUCHSCREEN));
}

static void testDisplayCreateView(WPEMockPlatformTest* test, gconstpointer)
{
    GRefPtr<WPEView> view1 = adoptGRef(wpe_view_new(test->display()));
    g_assert_true(WPE_IS_VIEW_MOCK(view1.get()));
    test->assertObjectIsDeletedWhenTestFinishes(view1.get());
    g_assert_true(wpe_view_get_display(view1.get()) == test->display());
    auto* toplevel = wpe_view_get_toplevel(view1.get());
    g_assert_true(WPE_IS_TOPLEVEL_MOCK(toplevel));
    test->assertObjectIsDeletedWhenTestFinishes(toplevel);
    g_assert_cmpuint(wpe_toplevel_get_max_views(toplevel), ==, 1);

    auto* settings = wpe_display_get_settings(test->display());
    GUniqueOutPtr<GError> error;
    wpe_settings_set_boolean(settings, WPE_SETTING_CREATE_VIEWS_WITH_A_TOPLEVEL, FALSE, WPE_SETTINGS_SOURCE_APPLICATION, &error.outPtr());
    g_assert_no_error(error.get());
    GRefPtr<WPEView> view2 = adoptGRef(wpe_view_new(test->display()));
    g_assert_true(WPE_IS_VIEW_MOCK(view2.get()));
    test->assertObjectIsDeletedWhenTestFinishes(view2.get());
    g_assert_true(wpe_view_get_display(view2.get()) == test->display());
    g_assert_null(wpe_view_get_toplevel(view2.get()));
}

void beforeAll()
{
    WPEMockPlatformTest::add("Display", "connect", testDisplayConnect);
    WPEMockPlatformTest::add("Display", "disconnected", testDisplayDisconnected);
    WPEMockPlatformTest::add("Display", "primary", testDisplayPrimary);
    WPEMockPlatformTest::add("Display", "keymap", testDisplayKeymap);
    WPEMockPlatformTest::add("Display", "drm-nodes", testDisplayDRMNodes);
    WPEMockPlatformTest::add("Display", "buffer-formats", testDisplayBufferFormats);
    WPEMockPlatformTest::add("Display", "explicit-sync", testDisplayExplicitSync);
    WPEMockPlatformTest::add("Display", "screens", testDisplayScreens);
    WPEMockAvailableInputDevicesTest::add("Display", "available-input-devices", testDisplayAvailableInputDevices);
    WPEMockPlatformTest::add("Display", "create-view", testDisplayCreateView);
}

void afterAll()
{
}

} // namespace TestWebKitAPI
