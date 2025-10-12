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

    ~WPEMockViewTest()
    {
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

void beforeAll()
{
    WPEMockViewTest::add("View", "toplevel", testViewToplevel);
    WPEMockViewTest::add("View", "size", testViewSize);
    WPEMockViewTest::add("View", "scale", testViewScale);
    WPEMockViewTest::add("View", "toplevel-state", testViewToplevelState);
}

void afterAll()
{
}

} // namespace TestWebKitAPI
