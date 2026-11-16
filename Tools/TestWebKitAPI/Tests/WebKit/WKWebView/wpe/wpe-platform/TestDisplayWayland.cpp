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
#include <sys/eventfd.h>
#include <unistd.h>
#include <wayland-server.h>
#include <wpe/wayland/wpe-wayland.h>
#include <wtf/Lock.h>
#include <wtf/Threading.h>

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

static void testDisplayWaylandEGLDisplay(WPEWaylandPlatformTest* test, gconstpointer)
{
    connectOrSkipIfNotUnderWayland();

    // The EGL display must be terminated before the Wayland display is disconnected.
    GRefPtr<WPEDisplay> display = adoptGRef(wpe_display_wayland_new());
    GUniqueOutPtr<GError> error;
    g_assert_true(wpe_display_wayland_connect(WPE_DISPLAY_WAYLAND(display.get()), nullptr, &error.outPtr()));
    g_assert_no_error(error.get());
    if (!wpe_display_get_egl_display(display.get(), &error.outPtr()))
        g_test_skip("EGL is not available");
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

class TestCompositor {
public:
    TestCompositor()
        : m_display(wl_display_create())
        , m_eventFD(eventfd(0, EFD_CLOEXEC))
    {
        m_socketName = wl_display_add_socket_auto(m_display);
        wl_display_init_shm(m_display);
        wl_global_create(m_display, &wl_compositor_interface, 4, const_cast<struct wl_interface*>(&wl_compositor_interface), bind);
        wl_global_create(m_display, &wl_data_device_manager_interface, 3, const_cast<struct wl_interface*>(&wl_data_device_manager_interface), bind);
        wl_event_loop_add_fd(wl_display_get_event_loop(m_display), m_eventFD, WL_EVENT_READABLE, [](int fd, uint32_t, void* data) -> int {
            uint64_t value;
            if (read(fd, &value, sizeof(value)) != sizeof(value))
                return 0;

            auto& compositor = *static_cast<TestCompositor*>(data);
            Vector<Function<void()>> tasks;
            {
                Locker locker { compositor.m_tasksLock };
                tasks = WTF::move(compositor.m_tasks);
            }
            for (auto& task : tasks)
                task();
            return 0;
        }, this);
        m_thread = Thread::create("TestCompositor"_s, [this] {
            wl_display_run(m_display);
        });
    }

    ~TestCompositor()
    {
        runInCompositorThread([this] {
            wl_display_terminate(m_display);
        });
        m_thread->waitForCompletion();
        wl_display_destroy_clients(m_display);
        wl_display_destroy(m_display);
        close(m_eventFD);
    }

    const char* socketName() const { return m_socketName; }

    void addSeat(uint32_t capabilities)
    {
        runInCompositorThread([this, capabilities] {
            m_seats.append(wl_global_create(m_display, &wl_seat_interface, 5, GUINT_TO_POINTER(capabilities), [](struct wl_client* client, void* data, uint32_t version, uint32_t id) {
                wl_seat_send_capabilities(createResource(client, &wl_seat_interface, version, id), GPOINTER_TO_UINT(data));
            }));
        });
    }

    void removeFirstSeat()
    {
        runInCompositorThread([this] {
            wl_global_destroy(m_seats[0]);
            m_seats.removeAt(0);
        });
    }

private:
    void runInCompositorThread(Function<void()>&& task)
    {
        {
            Locker locker { m_tasksLock };
            m_tasks.append(WTF::move(task));
        }
        uint64_t value = 1;
        if (write(m_eventFD, &value, sizeof(value)) != sizeof(value))
            g_assert_not_reached();
    }

    static struct wl_resource* createResource(struct wl_client* client, const struct wl_interface* interface, uint32_t version, uint32_t id)
    {
        auto* resource = wl_resource_create(client, interface, version, id);
        wl_resource_set_dispatcher(resource, dispatch, nullptr, nullptr, nullptr);
        return resource;
    }

    static void bind(struct wl_client* client, void* data, uint32_t version, uint32_t id)
    {
        createResource(client, static_cast<const struct wl_interface*>(data), version, id);
    }

    static int dispatch(const void*, void* target, uint32_t, const struct wl_message* message, union wl_argument* arguments)
    {
        auto* resource = static_cast<struct wl_resource*>(target);
        if (!g_strcmp0(message->name, "destroy") || !g_strcmp0(message->name, "release")) {
            wl_resource_destroy(resource);
            return 0;
        }

        unsigned argument = 0;
        for (const char* signature = message->signature; *signature; ++signature) {
            if (g_ascii_isdigit(*signature) || *signature == '?')
                continue;
            if (*signature == 'n')
                createResource(wl_resource_get_client(resource), message->types[argument], wl_resource_get_version(resource), arguments[argument].n);
            ++argument;
        }
        return 0;
    }

    struct wl_display* m_display { nullptr };
    Vector<struct wl_global*> m_seats;
    const char* m_socketName { nullptr };
    int m_eventFD { -1 };
    RefPtr<Thread> m_thread;
    Lock m_tasksLock;
    Vector<Function<void()>> m_tasks WTF_GUARDED_BY_LOCK(m_tasksLock);
};

static void waitForAvailableInputDevices(WPEDisplay* display, WPEAvailableInputDevices expected)
{
    bool timedOut = false;
    auto timeoutID = g_timeout_add_seconds(5, [](gpointer data) -> gboolean {
        *static_cast<bool*>(data) = true;
        return G_SOURCE_REMOVE;
    }, &timedOut);
    while (wpe_display_get_available_input_devices(display) != expected && !timedOut)
        g_main_context_iteration(nullptr, TRUE);
    if (!timedOut)
        g_source_remove(timeoutID);
    g_assert_cmpuint(wpe_display_get_available_input_devices(display), ==, expected);
}

static void testDisplayWaylandSeatAddedAndRemoved(WPEWaylandPlatformTest* test, gconstpointer)
{
    TestCompositor compositor;
    if (!compositor.socketName()) {
        g_test_skip("Could not create a Wayland socket");
        return;
    }

    GUniqueOutPtr<GError> error;
    g_assert_true(wpe_display_wayland_connect(WPE_DISPLAY_WAYLAND(test->display()), compositor.socketName(), &error.outPtr()));
    g_assert_no_error(error.get());
    g_assert_cmpuint(wpe_display_get_available_input_devices(test->display()), ==, WPE_AVAILABLE_INPUT_DEVICE_NONE);
    g_assert_true(WPE_IS_CLIPBOARD_WAYLAND(wpe_display_get_clipboard(test->display())));

    compositor.addSeat(WL_SEAT_CAPABILITY_TOUCH);
    waitForAvailableInputDevices(test->display(), WPE_AVAILABLE_INPUT_DEVICE_TOUCHSCREEN);

    compositor.removeFirstSeat();
    waitForAvailableInputDevices(test->display(), WPE_AVAILABLE_INPUT_DEVICE_NONE);

    compositor.addSeat(WL_SEAT_CAPABILITY_TOUCH);
    waitForAvailableInputDevices(test->display(), WPE_AVAILABLE_INPUT_DEVICE_TOUCHSCREEN);

    compositor.addSeat(WL_SEAT_CAPABILITY_POINTER);
    compositor.removeFirstSeat();
    waitForAvailableInputDevices(test->display(), WPE_AVAILABLE_INPUT_DEVICE_MOUSE);
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
    WPEWaylandPlatformTest::add("DisplayWayland", "egl-display", testDisplayWaylandEGLDisplay);
    WPEWaylandPlatformTest::add("DisplayWayland", "keymap", testDisplayWaylandKeymap);
    WPEWaylandPlatformTest::add("DisplayWayland", "screens", testDisplayWaylandScreens);
    WPEWaylandPlatformTest::add("DisplayWayland", "available-input-devices", testDisplayWaylandAvailableInputDevices);
    WPEWaylandPlatformTest::add("DisplayWayland", "create-view", testDisplayWaylandCreateView);
    WPEWaylandPlatformTest::add("DisplayWayland", "seat-added-and-removed", testDisplayWaylandSeatAddedAndRemoved);
}

void afterAll()
{
}

} // namespace TestWebKitAPI
