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

#include "WPEWaylandPlatformTest.h"
#include <wpe/wayland/wpe-wayland.h>

namespace TestWebKitAPI {

static const char* display = g_getenv("WAYLAND_DISPLAY");

#define connectOrSkipIfNotUnderWayland() \
    { \
        if (!display || !*display) { \
            g_test_skip("Not running under Wayland"); \
            return; \
        } \
        GUniqueOutPtr<GError> error; \
        g_assert_true(wpe_display_connect(test->display(), &error.outPtr())); \
        g_assert_no_error(error.get()); \
    }

#define connectOrSkipIfNotUnderTestingWeston() \
    { \
        if (!display || !g_str_has_prefix(display, "WKTesting-weston-")) { \
            g_test_skip("Not running under testing Weston"); \
            return; \
        } \
        GUniqueOutPtr<GError> error; \
        g_assert_true(wpe_display_connect(test->display(), &error.outPtr())); \
        g_assert_no_error(error.get()); \
    }

static void testDisplayWaylandConnect(WPEWaylandPlatformTest* test, gconstpointer)
{
    connectOrSkipIfNotUnderWayland();

    g_assert_nonnull(wpe_display_wayland_get_wl_display(WPE_DISPLAY_WAYLAND(test->display())));

    // Can't connect twice.
    GUniqueOutPtr<GError> error;
    g_assert_false(wpe_display_connect(test->display(), &error.outPtr()));
    g_assert_error(error.get(), WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED);

    // Connect to invalid display fails.
    GRefPtr<WPEDisplay> display = adoptGRef(wpe_display_wayland_new());
    g_assert_true(WPE_IS_DISPLAY_WAYLAND(display.get()));
    g_assert_false(wpe_display_wayland_connect(WPE_DISPLAY_WAYLAND(display.get()), "invalid", &error.outPtr()));
    g_assert_error(error.get(), WPE_DISPLAY_ERROR, WPE_DISPLAY_ERROR_CONNECTION_FAILED);

    // Connect to the default display using wpe_display_wayland_connect().
    g_assert_true(wpe_display_wayland_connect(WPE_DISPLAY_WAYLAND(display.get()), nullptr, &error.outPtr()));
    g_assert_no_error(error.get());
    g_assert_nonnull(wpe_display_wayland_get_wl_display(WPE_DISPLAY_WAYLAND(display.get())));
}

static void testDisplayWaylandKeymap(WPEWaylandPlatformTest* test, gconstpointer)
{
    connectOrSkipIfNotUnderWayland();

    auto* keymap = wpe_display_get_keymap(test->display());
    g_assert_true(WPE_IS_KEYMAP_XKB(keymap));
    test->assertObjectIsDeletedWhenTestFinishes(keymap);
}

static void testDisplayWaylandScreens(WPEWaylandPlatformTest* test, gconstpointer)
{
    connectOrSkipIfNotUnderTestingWeston();

    g_assert_cmpuint(wpe_display_get_n_screens(test->display()), ==, 1);
    auto* screen = wpe_display_get_screen(test->display(), 0);
    g_assert_true(WPE_IS_SCREEN_WAYLAND(screen));
    test->assertObjectIsDeletedWhenTestFinishes(screen);
    g_assert_nonnull(wpe_screen_wayland_get_wl_output(WPE_SCREEN_WAYLAND(screen)));
    g_assert_cmpuint(wpe_screen_get_id(screen), >, 0);
    g_assert_cmpint(wpe_screen_get_x(screen), ==, 0);
    g_assert_cmpint(wpe_screen_get_y(screen), ==, 0);
    g_assert_cmpint(wpe_screen_get_width(screen), ==, 1024);
    g_assert_cmpint(wpe_screen_get_height(screen), ==, 768);
    g_assert_cmpfloat(wpe_screen_get_scale(screen), ==, 1.);
    g_assert_cmpint(wpe_screen_get_refresh_rate(screen), ==, 60000);

    g_assert_null(wpe_display_get_screen(test->display(), 1));
}

static void testDisplayWaylandAvailableInputDevices(WPEWaylandPlatformTest* test, gconstpointer)
{
    connectOrSkipIfNotUnderTestingWeston();

    auto devices = wpe_display_get_available_input_devices(test->display());
    g_assert_true(devices & WPE_AVAILABLE_INPUT_DEVICE_MOUSE);
    g_assert_true(devices & WPE_AVAILABLE_INPUT_DEVICE_KEYBOARD);
    g_assert_false(devices & WPE_AVAILABLE_INPUT_DEVICE_TOUCHSCREEN);
}

static void testDisplayWaylandCreateView(WPEWaylandPlatformTest* test, gconstpointer)
{
    connectOrSkipIfNotUnderWayland();

    GRefPtr<WPEView> view1 = adoptGRef(wpe_view_new(test->display()));
    g_assert_true(WPE_IS_VIEW_WAYLAND(view1.get()));
    test->assertObjectIsDeletedWhenTestFinishes(view1.get());
    g_assert_true(wpe_view_get_display(view1.get()) == test->display());
    auto* toplevel = wpe_view_get_toplevel(view1.get());
    g_assert_true(WPE_IS_TOPLEVEL_WAYLAND(toplevel));
    test->assertObjectIsDeletedWhenTestFinishes(toplevel);
    g_assert_cmpuint(wpe_toplevel_get_max_views(toplevel), ==, 1);

    auto* settings = wpe_display_get_settings(test->display());
    GUniqueOutPtr<GError> error;
    wpe_settings_set_boolean(settings, WPE_SETTING_CREATE_VIEWS_WITH_A_TOPLEVEL, FALSE, WPE_SETTINGS_SOURCE_APPLICATION, &error.outPtr());
    g_assert_no_error(error.get());
    GRefPtr<WPEView> view2 = adoptGRef(wpe_view_new(test->display()));
    g_assert_true(WPE_IS_VIEW_WAYLAND(view2.get()));
    test->assertObjectIsDeletedWhenTestFinishes(view2.get());
    g_assert_true(wpe_view_get_display(view2.get()) == test->display());
    g_assert_null(wpe_view_get_toplevel(view2.get()));
}

void beforeAll()
{
    WPEWaylandPlatformTest::add("DisplayWayland", "connect", testDisplayWaylandConnect);
    WPEWaylandPlatformTest::add("DisplayWayland", "keymap", testDisplayWaylandKeymap);
    WPEWaylandPlatformTest::add("DisplayWayland", "screens", testDisplayWaylandScreens);
    WPEWaylandPlatformTest::add("DisplayWayland", "available-input-devices", testDisplayWaylandAvailableInputDevices);
    WPEWaylandPlatformTest::add("DisplayWayland", "create-view", testDisplayWaylandCreateView);
}

void afterAll()
{
}

} // namespace TestWebKitAPI
