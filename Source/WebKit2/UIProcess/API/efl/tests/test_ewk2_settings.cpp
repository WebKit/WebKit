/*
 * Copyright (C) 2012 Samsung Electronics
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

#include "UnitTestUtils/EWK2UnitTestBase.h"
#include "UnitTestUtils/EWK2UnitTestEnvironment.h"
#include <EWebKit2.h>
#include <Eina.h>

using namespace EWK2UnitTest;

extern EWK2UnitTestEnvironment* environment;

TEST_F(EWK2UnitTestBase, ewk_settings_fullscreen_enabled)
{
    Ewk_Settings* settings = ewk_view_settings_get(webView());

#if ENABLE(FULLSCREEN_API)
    ASSERT_TRUE(ewk_settings_fullscreen_enabled_get(settings));

    ASSERT_TRUE(ewk_settings_fullscreen_enabled_set(settings, EINA_TRUE));
    ASSERT_TRUE(ewk_settings_fullscreen_enabled_get(settings));

    ASSERT_TRUE(ewk_settings_fullscreen_enabled_set(settings, EINA_FALSE));
    ASSERT_FALSE(ewk_settings_fullscreen_enabled_get(settings));
#else
    ASSERT_FALSE(ewk_settings_fullscreen_enabled_get(settings));

    ASSERT_FALSE(ewk_settings_fullscreen_enabled_set(settings, EINA_TRUE));
    ASSERT_FALSE(ewk_settings_fullscreen_enabled_get(settings));

    ASSERT_FALSE(ewk_settings_fullscreen_enabled_set(settings, EINA_FALSE));
    ASSERT_FALSE(ewk_settings_fullscreen_enabled_get(settings));
#endif
}

TEST_F(EWK2UnitTestBase, ewk_settings_javascript_enabled)
{
    Ewk_Settings* settings = ewk_view_settings_get(webView());

    ASSERT_TRUE(ewk_settings_javascript_enabled_set(settings, EINA_TRUE));
    ASSERT_TRUE(ewk_settings_javascript_enabled_get(settings));

    ASSERT_TRUE(ewk_settings_javascript_enabled_set(settings, 2));
    ASSERT_TRUE(ewk_settings_javascript_enabled_get(settings));

    ASSERT_TRUE(ewk_settings_javascript_enabled_set(settings, EINA_FALSE));
    ASSERT_FALSE(ewk_settings_javascript_enabled_get(settings));
}

TEST_F(EWK2UnitTestBase, ewk_settings_loads_images_automatically)
{
    Ewk_Settings* settings = ewk_view_settings_get(webView());

    ASSERT_TRUE(ewk_settings_loads_images_automatically_set(settings, EINA_TRUE));
    ASSERT_TRUE(ewk_settings_loads_images_automatically_get(settings));

    ASSERT_TRUE(ewk_settings_loads_images_automatically_set(settings, 2));
    ASSERT_TRUE(ewk_settings_loads_images_automatically_get(settings));

    ASSERT_TRUE(ewk_settings_loads_images_automatically_set(settings, EINA_FALSE));
    ASSERT_FALSE(ewk_settings_loads_images_automatically_get(settings));
}

TEST_F(EWK2UnitTestBase, ewk_settings_developer_extras_enabled)
{
    Ewk_Settings* settings = ewk_view_settings_get(webView());

    ASSERT_TRUE(ewk_settings_developer_extras_enabled_set(settings, EINA_TRUE));
    ASSERT_TRUE(ewk_settings_developer_extras_enabled_get(settings));

    ASSERT_TRUE(ewk_settings_developer_extras_enabled_set(settings, 2));
    ASSERT_TRUE(ewk_settings_developer_extras_enabled_get(settings));

    ASSERT_TRUE(ewk_settings_developer_extras_enabled_set(settings, EINA_FALSE));
    ASSERT_FALSE(ewk_settings_developer_extras_enabled_get(settings));
}

TEST_F(EWK2UnitTestBase, ewk_settings_file_access_from_file_urls_allowed)
{
    CString testURL = environment->urlForResource("local_file_access.html");
    Ewk_Settings* settings = ewk_view_settings_get(webView());

    ASSERT_FALSE(ewk_settings_file_access_from_file_urls_allowed_get(settings));

    ASSERT_TRUE(ewk_settings_file_access_from_file_urls_allowed_set(settings, true));
    ASSERT_TRUE(ewk_settings_file_access_from_file_urls_allowed_get(settings));

    // Check that file access from file:// URLs is allowed.
    ewk_view_uri_set(webView(), testURL.data());
    ASSERT_TRUE(waitUntilTitleChangedTo("Frame loaded"));

    ASSERT_TRUE(ewk_settings_file_access_from_file_urls_allowed_set(settings, false));
    ASSERT_FALSE(ewk_settings_file_access_from_file_urls_allowed_get(settings));

    // Check that file access from file:// URLs is NOT allowed.
    ewk_view_uri_set(webView(), testURL.data());
    ASSERT_TRUE(waitUntilTitleChangedTo("Frame NOT loaded"));
}
