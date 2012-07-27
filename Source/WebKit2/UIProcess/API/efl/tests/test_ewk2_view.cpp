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

static void onFormAboutToBeSubmitted(void* userData, Evas_Object*, void* eventInfo)
{
    Ewk_Form_Submission_Request* request = static_cast<Ewk_Form_Submission_Request*>(eventInfo);
    bool* handled = static_cast<bool*>(userData);

    ASSERT_TRUE(request);

    Eina_List* fieldNames = ewk_form_submission_request_field_names_get(request);
    ASSERT_TRUE(fieldNames);
    ASSERT_EQ(eina_list_count(fieldNames), 3);
    void* data;
    EINA_LIST_FREE(fieldNames, data)
        eina_stringshare_del(static_cast<char*>(data));

    const char* value1 = ewk_form_submission_request_field_value_get(request, "text1");
    ASSERT_STREQ(value1, "value1");
    eina_stringshare_del(value1);
    const char* value2 = ewk_form_submission_request_field_value_get(request, "text2");
    ASSERT_STREQ(value2, "value2");
    eina_stringshare_del(value2);
    const char* password = ewk_form_submission_request_field_value_get(request, "password");
    ASSERT_STREQ(password, "secret");
    eina_stringshare_del(password);

    *handled = true;
}

TEST_F(EWK2UnitTestBase, ewk_view_form_submission_request)
{
    const char* formHTML =
        "<html><head><script type='text/javascript'>function submitForm() { document.getElementById('myform').submit(); }</script></head>"
        "<body onload='submitForm()'>"
        " <form id='myform' action='#'>"
        "  <input type='text' name='text1' value='value1'>"
        "  <input type='text' name='text2' value='value2'>"
        "  <input type='password' name='password' value='secret'>"
        "  <textarea cols='5' rows='5' name='textarea'>Text</textarea>"
        "  <input type='hidden' name='hidden1' value='hidden1'>"
        "  <input type='submit' value='Submit'>"
        " </form>"
        "</body></html>";

    ewk_view_html_string_load(webView(), formHTML, "file:///", 0);
    bool handled = false;
    evas_object_smart_callback_add(webView(), "form,submission,request", onFormAboutToBeSubmitted, &handled);
    while (!handled)
        ecore_main_loop_iterate();
    ASSERT_TRUE(handled);
    evas_object_smart_callback_del(webView(), "form,submission,request", onFormAboutToBeSubmitted);
}
