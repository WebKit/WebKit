/*
 * Copyright (C) 2026 Igalia, S.L.
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

#include "WPEHeadlessPlatformTest.h"
#include <wpe/headless/wpe-headless.h>

namespace TestWebKitAPI {

static void testDisplayHeadlessCreateView(WPEHeadlessPlatformTest* test, gconstpointer)
{
    GRefPtr<WPEView> view = adoptGRef(wpe_view_new(test->display()));
    g_assert_true(WPE_IS_VIEW_HEADLESS(view.get()));
    test->assertObjectIsDeletedWhenTestFinishes(view.get());
    g_assert_true(wpe_view_get_display(view.get()) == test->display());
    auto* toplevel = wpe_view_get_toplevel(view.get());
    g_assert_true(WPE_IS_TOPLEVEL_HEADLESS(toplevel));
    test->assertObjectIsDeletedWhenTestFinishes(toplevel);
    g_assert_true(wpe_toplevel_get_state(toplevel) & WPE_TOPLEVEL_STATE_ACTIVE);
}

static void testDisplayHeadlessViewFocus(WPEHeadlessPlatformTest* test, gconstpointer)
{
    // A view created with a toplevel is mapped and focused, since there's no seat to deliver focus.
    GRefPtr<WPEView> view = adoptGRef(wpe_view_new(test->display()));
    test->assertObjectIsDeletedWhenTestFinishes(view.get());
    g_assert_nonnull(wpe_view_get_toplevel(view.get()));
    g_assert_true(wpe_view_get_mapped(view.get()));
    g_assert_true(wpe_view_get_has_focus(view.get()));

    // Removing the view from its toplevel unmaps it and removes the focus.
    wpe_view_set_toplevel(view.get(), nullptr);
    g_assert_false(wpe_view_get_mapped(view.get()));
    g_assert_false(wpe_view_get_has_focus(view.get()));

    // Adding it to a toplevel again maps and focuses it.
    GRefPtr<WPEToplevel> toplevel = adoptGRef(wpe_display_create_toplevel(test->display(), 1));
    test->assertObjectIsDeletedWhenTestFinishes(toplevel.get());
    wpe_view_set_toplevel(view.get(), toplevel.get());
    g_assert_true(wpe_view_get_mapped(view.get()));
    g_assert_true(wpe_view_get_has_focus(view.get()));

    // A view created without a toplevel is not focused until it's added to one.
    auto* settings = wpe_display_get_settings(test->display());
    GUniqueOutPtr<GError> error;
    wpe_settings_set_boolean(settings, WPE_SETTING_CREATE_VIEWS_WITH_A_TOPLEVEL, FALSE, WPE_SETTINGS_SOURCE_APPLICATION, &error.outPtr());
    g_assert_no_error(error.get());
    GRefPtr<WPEView> view2 = adoptGRef(wpe_view_new(test->display()));
    test->assertObjectIsDeletedWhenTestFinishes(view2.get());
    g_assert_null(wpe_view_get_toplevel(view2.get()));
    g_assert_false(wpe_view_get_mapped(view2.get()));
    g_assert_false(wpe_view_get_has_focus(view2.get()));

    GRefPtr<WPEToplevel> toplevel2 = adoptGRef(wpe_display_create_toplevel(test->display(), 1));
    test->assertObjectIsDeletedWhenTestFinishes(toplevel2.get());
    wpe_view_set_toplevel(view2.get(), toplevel2.get());
    g_assert_true(wpe_view_get_mapped(view2.get()));
    g_assert_true(wpe_view_get_has_focus(view2.get()));
}

void beforeAll()
{
    WPEHeadlessPlatformTest::add("DisplayHeadless", "create-view", testDisplayHeadlessCreateView);
    WPEHeadlessPlatformTest::add("DisplayHeadless", "view-focus", testDisplayHeadlessViewFocus);
}

void afterAll()
{
}

} // namespace TestWebKitAPI
