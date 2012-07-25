/*
    Copyright (C) 2012 Samsung Electronics
    Copyright (C) 2012 Intel Corporation. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; if not, write to the Free Software Foundation,
    Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "config.h"

#include "UnitTestUtils/EWK2UnitTestBase.h"
#include "UnitTestUtils/EWK2UnitTestEnvironment.h"
#include <EWebKit2.h>
#include <Ecore.h>
#include <gtest/gtest.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>

using namespace EWK2UnitTest;

extern EWK2UnitTestEnvironment* environment;

static void onLoadFinishedForRedirection(void* userData, Evas_Object*, void*)
{
    int* countLoadFinished = static_cast<int*>(userData);
    (*countLoadFinished)--;
}

TEST_F(EWK2UnitTestBase, ewk_view_uri_get)
{
    loadUrlSync(environment->defaultTestPageUrl());
    EXPECT_STREQ(ewk_view_uri_get(webView()), environment->defaultTestPageUrl());

    int countLoadFinished = 2;
    evas_object_smart_callback_add(webView(), "load,finished", onLoadFinishedForRedirection, &countLoadFinished);
    ewk_view_uri_set(webView(), environment->urlForResource("redirect_uri_to_default.html").data());
    while (countLoadFinished)
        ecore_main_loop_iterate();
    evas_object_smart_callback_del(webView(), "load,finished", onLoadFinishedForRedirection);
    EXPECT_STREQ(ewk_view_uri_get(webView()), environment->defaultTestPageUrl());
}

TEST_F(EWK2UnitTestBase, ewk_view_setting_encoding_custom)
{
    ASSERT_FALSE(ewk_view_setting_encoding_custom_get(webView()));
    ASSERT_TRUE(ewk_view_setting_encoding_custom_set(webView(), "UTF-8"));
    ASSERT_STREQ(ewk_view_setting_encoding_custom_get(webView()), "UTF-8");
    // Set the default charset.
    ASSERT_TRUE(ewk_view_setting_encoding_custom_set(webView(), 0));
    ASSERT_FALSE(ewk_view_setting_encoding_custom_get(webView()));
}
