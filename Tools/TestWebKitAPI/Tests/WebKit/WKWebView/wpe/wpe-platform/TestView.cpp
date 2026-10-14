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
#include <wpe/GRefPtrWPE.h>

namespace TestWebKitAPI {

class WPEMockViewTest : public WPEMockPlatformTest {
public:
    WPE_PLATFORM_TEST_FIXTURE(WPEMockViewTest);

    WPEMockViewTest()
        : m_view(adoptGRef(wpe_view_new(m_display.get())))
    {
        assertObjectIsDeletedWhenTestFinishes(m_view.get());
        g_assert_true(wpe_view_get_display(m_view.get()) == display());
    }

    WPEView* view() const { return m_view.get(); }

private:
    GRefPtr<WPEView> m_view;
};

static void testViewToplevel(WPEMockViewTest* test, gconstpointer)
{
    auto* toplevel = wpe_view_get_toplevel(test->view());
    g_assert_true(WPE_IS_TOPLEVEL_MOCK(toplevel));
    test->assertObjectIsDeletedWhenTestFinishes(toplevel);
    g_assert_true(wpe_toplevel_get_display(toplevel) == wpe_view_get_display(test->view()));
}

static void testViewSize(WPEMockViewTest* test, gconstpointer)
{
    g_assert_cmpint(wpe_view_get_width(test->view()), ==, 1024);
    g_assert_cmpint(wpe_view_get_height(test->view()), ==, 768);
    auto* toplevel = wpe_view_get_toplevel(test->view());
    g_assert_true(WPE_IS_TOPLEVEL_MOCK(toplevel));
    test->assertObjectIsDeletedWhenTestFinishes(toplevel);
    int width, height;
    wpe_toplevel_get_size(toplevel, &width, &height);
    g_assert_cmpint(wpe_view_get_width(test->view()), ==, width);
    g_assert_cmpint(wpe_view_get_height(test->view()), ==, height);

    gboolean viewResized = FALSE;
    auto viewResizedID = g_signal_connect(test->view(), "resized", G_CALLBACK(+[](WPEView*, gboolean* viewResized) {
        *viewResized = TRUE;
    }), &viewResized);

    g_assert_true(wpe_toplevel_resize(toplevel, 800, 600));
    g_assert_true(viewResized);
    g_assert_cmpint(wpe_view_get_width(test->view()), ==, 800);
    g_assert_cmpint(wpe_view_get_height(test->view()), ==, 600);
    wpe_toplevel_get_size(toplevel, &width, &height);
    g_assert_cmpint(wpe_view_get_width(test->view()), ==, width);
    g_assert_cmpint(wpe_view_get_height(test->view()), ==, height);
    g_signal_handler_disconnect(test->view(), viewResizedID);
}

static void testViewScale(WPEMockViewTest* test, gconstpointer)
{
    g_assert_cmpfloat(wpe_view_get_scale(test->view()), ==, 1.);
    auto* toplevel = wpe_view_get_toplevel(test->view());
    g_assert_true(WPE_IS_TOPLEVEL_MOCK(toplevel));
    test->assertObjectIsDeletedWhenTestFinishes(toplevel);
    g_assert_cmpfloat(wpe_view_get_scale(test->view()), ==, wpe_toplevel_get_scale(toplevel));

    wpeDisplayMockAddSecondaryScreen(WPE_DISPLAY_MOCK(test->display()));
    gboolean viewScaleChanged = FALSE;
    auto viewScaleChangedID = g_signal_connect(test->view(), "notify::scale", G_CALLBACK(+[](WPEView*, GParamSpec*, gboolean* viewScaleChanged) {
        *viewScaleChanged = TRUE;
    }), &viewScaleChanged);
    wpeToplevelMockSwitchToScreen(WPE_TOPLEVEL_MOCK(toplevel), 1);
    g_assert_true(viewScaleChanged);
    g_assert_cmpfloat(wpe_view_get_scale(test->view()), ==, 2.);
    g_signal_handler_disconnect(test->view(), viewScaleChangedID);
}

static void testViewToplevelState(WPEMockViewTest* test, gconstpointer)
{
    auto state = wpe_view_get_toplevel_state(test->view());
    g_assert_cmpuint(state, ==, 0);
    auto* toplevel = wpe_view_get_toplevel(test->view());
    g_assert_true(WPE_IS_TOPLEVEL_MOCK(toplevel));
    test->assertObjectIsDeletedWhenTestFinishes(toplevel);
    g_assert_cmpuint(wpe_view_get_toplevel_state(test->view()), ==, wpe_toplevel_get_state(toplevel));

    gboolean viewStateChanged = FALSE;
    auto viewStateChangedID = g_signal_connect(test->view(), "notify::toplevel-state", G_CALLBACK(+[](WPEView*, GParamSpec*, gboolean* viewStateChanged) {
        *viewStateChanged = TRUE;
    }), &viewStateChanged);

    wpeToplevelMockSetActive(WPE_TOPLEVEL_MOCK(toplevel), TRUE);
    g_assert_true(viewStateChanged);
    state = wpe_view_get_toplevel_state(test->view());
    g_assert_true(state & WPE_TOPLEVEL_STATE_ACTIVE);
    g_assert_false(state & WPE_TOPLEVEL_STATE_FULLSCREEN);
    g_assert_false(state & WPE_TOPLEVEL_STATE_MAXIMIZED);
    g_assert_cmpuint(state, ==, wpe_toplevel_get_state(toplevel));

    gboolean viewResized = FALSE;
    auto viewResizedID = g_signal_connect(test->view(), "resized", G_CALLBACK(+[](WPEView*, gboolean* viewResized) {
        *viewResized = TRUE;
    }), &viewResized);

    viewStateChanged = FALSE;
    g_assert_true(wpe_toplevel_fullscreen(toplevel));
    g_assert_true(viewStateChanged);
    state = wpe_view_get_toplevel_state(test->view());
    g_assert_true(state & WPE_TOPLEVEL_STATE_ACTIVE);
    g_assert_true(state & WPE_TOPLEVEL_STATE_FULLSCREEN);
    g_assert_false(state & WPE_TOPLEVEL_STATE_MAXIMIZED);
    g_assert_cmpuint(state, ==, wpe_toplevel_get_state(toplevel));
    g_assert_true(viewResized);
    g_assert_cmpint(wpe_view_get_width(test->view()), ==, 1920);
    g_assert_cmpint(wpe_view_get_height(test->view()), ==, 1080);

    viewStateChanged = FALSE;
    viewResized = FALSE;
    g_assert_true(wpe_toplevel_unfullscreen(toplevel));
    g_assert_true(viewStateChanged);
    state = wpe_view_get_toplevel_state(test->view());
    g_assert_true(state & WPE_TOPLEVEL_STATE_ACTIVE);
    g_assert_false(state & WPE_TOPLEVEL_STATE_FULLSCREEN);
    g_assert_false(state & WPE_TOPLEVEL_STATE_MAXIMIZED);
    g_assert_cmpuint(state, ==, wpe_toplevel_get_state(toplevel));
    g_assert_true(viewResized);
    g_assert_cmpint(wpe_view_get_width(test->view()), ==, 1024);
    g_assert_cmpint(wpe_view_get_height(test->view()), ==, 768);

    viewStateChanged = FALSE;
    g_assert_true(wpe_toplevel_maximize(toplevel));
    g_assert_true(viewStateChanged);
    state = wpe_view_get_toplevel_state(test->view());
    g_assert_true(state & WPE_TOPLEVEL_STATE_ACTIVE);
    g_assert_false(state & WPE_TOPLEVEL_STATE_FULLSCREEN);
    g_assert_true(state & WPE_TOPLEVEL_STATE_MAXIMIZED);
    g_assert_cmpuint(state, ==, wpe_toplevel_get_state(toplevel));
    g_assert_true(viewResized);
    g_assert_cmpint(wpe_view_get_width(test->view()), ==, 1920);
    g_assert_cmpint(wpe_view_get_height(test->view()), ==, 1040);

    viewStateChanged = FALSE;
    viewResized = FALSE;
    g_assert_true(wpe_toplevel_unmaximize(toplevel));
    g_assert_true(viewStateChanged);
    state = wpe_view_get_toplevel_state(test->view());
    g_assert_true(state & WPE_TOPLEVEL_STATE_ACTIVE);
    g_assert_false(state & WPE_TOPLEVEL_STATE_FULLSCREEN);
    g_assert_false(state & WPE_TOPLEVEL_STATE_MAXIMIZED);
    g_assert_cmpuint(state, ==, wpe_toplevel_get_state(toplevel));
    g_assert_true(viewResized);
    g_assert_cmpint(wpe_view_get_width(test->view()), ==, 1024);
    g_assert_cmpint(wpe_view_get_height(test->view()), ==, 768);

    g_signal_handler_disconnect(test->view(), viewStateChangedID);
    g_signal_handler_disconnect(test->view(), viewResizedID);
}

static GRefPtr<WPEEvent> createKeyEvent(WPEView* view, WPEEventType type)
{
    return adoptGRef(wpe_event_keyboard_new(type, view, WPE_INPUT_SOURCE_KEYBOARD, 0, WPE_MODIFIER_KEYBOARD_CONTROL, 41, WPE_KEY_f));
}

struct ProcessedResult {
    unsigned count { 0 };
    WPEEvent* event { nullptr };
    gboolean handled { FALSE };
    const char* userData { nullptr };
};

static void testViewKeyboardEventProcessed(WPEMockViewTest* test, gconstpointer)
{
    ProcessedResult result;
    auto signalID = g_signal_connect(test->view(), "event-processed", G_CALLBACK(+[](WPEView*, WPEEvent* event, gboolean handled, ProcessedResult* result) {
        result->count++;
        result->event = event;
        result->handled = handled;
        result->userData = static_cast<const char*>(wpe_event_get_user_data(event));
    }), &result);

    auto event = createKeyEvent(test->view(), WPE_EVENT_KEYBOARD_KEY_DOWN);

    // A platform implementation recognizes the event it sent by the data it
    // attached to it, so that has to survive the round trip.
    static const char* platformData = "platform-data";
    wpe_event_set_user_data(event.get(), const_cast<char*>(platformData), nullptr);

    wpe_view_event_processed(test->view(), event.get(), FALSE);
    g_assert_cmpuint(result.count, ==, 1);
    g_assert_true(result.event == event.get());
    g_assert_false(result.handled);
    g_assert_cmpstr(result.userData, ==, platformData);
    g_assert_cmpuint(wpe_event_get_event_type(result.event), ==, WPE_EVENT_KEYBOARD_KEY_DOWN);
    g_assert_cmpuint(wpe_event_keyboard_get_keyval(result.event), ==, WPE_KEY_f);
    g_assert_true(wpe_event_get_view(result.event) == test->view());

    // Every processed event is reported, so a consumer can always tell a key
    // the web content took from one it never got to decide on.
    wpe_view_event_processed(test->view(), event.get(), TRUE);
    g_assert_cmpuint(result.count, ==, 2);
    g_assert_true(result.event == event.get());
    g_assert_true(result.handled);

    auto keyUpEvent = createKeyEvent(test->view(), WPE_EVENT_KEYBOARD_KEY_UP);
    wpe_view_event_processed(test->view(), keyUpEvent.get(), FALSE);
    g_assert_cmpuint(result.count, ==, 3);
    g_assert_true(result.event == keyUpEvent.get());
    g_assert_null(result.userData);

    g_signal_handler_disconnect(test->view(), signalID);

    // Reporting a result nobody listens for is not an error.
    wpe_view_event_processed(test->view(), event.get(), TRUE);
    g_assert_cmpuint(result.count, ==, 3);
}

struct EventCounts {
    unsigned event { 0 };
    unsigned processed { 0 };
};

static void testViewKeyboardEventProcessedIsSeparateFromEvent(WPEMockViewTest* test, gconstpointer)
{
    EventCounts counts;
    auto eventID = g_signal_connect(test->view(), "event", G_CALLBACK(+[](WPEView*, WPEEvent*, EventCounts* counts) -> gboolean {
        counts->event++;
        return TRUE;
    }), &counts);
    auto processedID = g_signal_connect(test->view(), "event-processed", G_CALLBACK(+[](WPEView*, WPEEvent*, gboolean, EventCounts* counts) {
        counts->processed++;
    }), &counts);

    auto event = createKeyEvent(test->view(), WPE_EVENT_KEYBOARD_KEY_DOWN);

    // Delivering an event says nothing about what became of it, which is the
    // whole reason the second signal exists.
    wpe_view_event(test->view(), event.get());
    g_assert_cmpuint(counts.event, ==, 1);
    g_assert_cmpuint(counts.processed, ==, 0);

    wpe_view_event_processed(test->view(), event.get(), FALSE);
    g_assert_cmpuint(counts.event, ==, 1);
    g_assert_cmpuint(counts.processed, ==, 1);

    g_signal_handler_disconnect(test->view(), eventID);
    g_signal_handler_disconnect(test->view(), processedID);
}

static void testViewEventProcessedScroll(WPEMockViewTest* test, gconstpointer)
{
    ProcessedResult result;
    auto signalID = g_signal_connect(test->view(), "event-processed", G_CALLBACK(+[](WPEView*, WPEEvent* event, gboolean handled, ProcessedResult* result) {
        result->count++;
        result->event = event;
        result->handled = handled;
    }), &result);

    GRefPtr<WPEEvent> event = adoptGRef(wpe_event_scroll_new(test->view(), WPE_INPUT_SOURCE_MOUSE, 0, WPE_MODIFIER_KEYBOARD_CONTROL,
        0., -3., FALSE, FALSE, 10., 20.));

    wpe_view_event_processed(test->view(), event.get(), FALSE);
    g_assert_cmpuint(result.count, ==, 1);
    g_assert_true(result.event == event.get());
    g_assert_false(result.handled);
    g_assert_cmpuint(wpe_event_get_event_type(result.event), ==, WPE_EVENT_SCROLL);
    g_assert_true(wpe_event_get_modifiers(result.event) & WPE_MODIFIER_KEYBOARD_CONTROL);

    // The deltas have to survive, since what to do with an unused scroll
    // depends on them.
    double deltaX, deltaY;
    wpe_event_scroll_get_deltas(result.event, &deltaX, &deltaY);
    g_assert_cmpfloat(deltaX, ==, 0.);
    g_assert_cmpfloat(deltaY, ==, -3.);

    wpe_view_event_processed(test->view(), event.get(), TRUE);
    g_assert_cmpuint(result.count, ==, 2);
    g_assert_true(result.handled);

    g_signal_handler_disconnect(test->view(), signalID);
}

void beforeAll()
{
    WPEMockViewTest::add("View", "toplevel", testViewToplevel);
    WPEMockViewTest::add("View", "size", testViewSize);
    WPEMockViewTest::add("View", "scale", testViewScale);
    WPEMockViewTest::add("View", "toplevel-state", testViewToplevelState);
    WPEMockViewTest::add("View", "event-processed", testViewKeyboardEventProcessed);
    WPEMockViewTest::add("View", "event-processed-is-separate-from-event", testViewKeyboardEventProcessedIsSeparateFromEvent);
    WPEMockViewTest::add("View", "event-processed-scroll", testViewEventProcessedScroll);
}

void afterAll()
{
}

} // namespace TestWebKitAPI
