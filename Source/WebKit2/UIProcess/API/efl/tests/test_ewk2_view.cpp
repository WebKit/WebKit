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
#include "UnitTestUtils/EWK2UnitTestServer.h"
#include <EWebKit2.h>
#include <Ecore.h>
#include <Eina.h>
#include <Evas.h>
#include <gtest/gtest.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using namespace EWK2UnitTest;

extern EWK2UnitTestEnvironment* environment;
bool fullScreenCallbackCalled;

static void onLoadFinishedForRedirection(void* userData, Evas_Object*, void*)
{
    int* countLoadFinished = static_cast<int*>(userData);
    (*countLoadFinished)--;
}

TEST_F(EWK2UnitTestBase, ewk_view_url_get)
{
    ASSERT_TRUE(loadUrlSync(environment->defaultTestPageUrl()));
    EXPECT_STREQ(environment->defaultTestPageUrl(), ewk_view_url_get(webView()));

    int countLoadFinished = 2;
    evas_object_smart_callback_add(webView(), "load,finished", onLoadFinishedForRedirection, &countLoadFinished);
    ewk_view_url_set(webView(), environment->urlForResource("redirect_url_to_default.html").data());
    while (countLoadFinished)
        ecore_main_loop_iterate();
    evas_object_smart_callback_del(webView(), "load,finished", onLoadFinishedForRedirection);
    EXPECT_STREQ(environment->defaultTestPageUrl(), ewk_view_url_get(webView()));
}

TEST_F(EWK2UnitTestBase, ewk_view_device_pixel_ratio)
{
    ASSERT_TRUE(loadUrlSync(environment->defaultTestPageUrl()));

    // Default pixel ratio is 1.0
    ASSERT_FLOAT_EQ(1, ewk_view_device_pixel_ratio_get(webView()));

    ASSERT_TRUE(ewk_view_device_pixel_ratio_set(webView(), 1.2));
    ASSERT_FLOAT_EQ(1.2, ewk_view_device_pixel_ratio_get(webView()));

    ASSERT_TRUE(ewk_view_device_pixel_ratio_set(webView(), 1));
    ASSERT_FLOAT_EQ(1, ewk_view_device_pixel_ratio_get(webView()));
}

TEST_F(EWK2UnitTestBase, ewk_view_html_string_load)
{
    ewk_view_html_string_load(webView(), "<html><head><title>Foo</title></head><body>Bar</body></html>", 0, 0);
    ASSERT_TRUE(waitUntilTitleChangedTo("Foo"));
    ASSERT_STREQ("Foo", ewk_view_title_get(webView()));
    ewk_view_html_string_load(webView(), "<html><head><title>Bar</title></head><body>Foo</body></html>", 0, 0);
    ASSERT_TRUE(waitUntilTitleChangedTo("Bar"));
    ASSERT_STREQ("Bar", ewk_view_title_get(webView()));
}

static void serverCallbackNavigation(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
{
    if (message->method != SOUP_METHOD_GET) {
        soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    soup_message_set_status(message, SOUP_STATUS_OK);

    Eina_Strbuf* body = eina_strbuf_new();
    eina_strbuf_append_printf(body, "<html><title>%s</title><body>%s</body></html>", path + 1, path + 1);
    const size_t bodyLength = eina_strbuf_length_get(body);
    soup_message_body_append(message->response_body, SOUP_MEMORY_TAKE, eina_strbuf_string_steal(body), bodyLength);
    eina_strbuf_free(body);

    soup_message_body_complete(message->response_body);
}

TEST_F(EWK2UnitTestBase, ewk_view_navigation)
{
    OwnPtr<EWK2UnitTestServer> httpServer = adoptPtr(new EWK2UnitTestServer);
    httpServer->run(serverCallbackNavigation);

    // Visit Page1
    ASSERT_TRUE(loadUrlSync(httpServer->getURLForPath("/Page1").data()));
    ASSERT_STREQ("Page1", ewk_view_title_get(webView()));
    ASSERT_FALSE(ewk_view_back_possible(webView()));
    ASSERT_FALSE(ewk_view_forward_possible(webView()));

    // Visit Page2
    ASSERT_TRUE(loadUrlSync(httpServer->getURLForPath("/Page2").data()));
    ASSERT_STREQ("Page2", ewk_view_title_get(webView()));
    ASSERT_TRUE(ewk_view_back_possible(webView()));
    ASSERT_FALSE(ewk_view_forward_possible(webView()));

    // Go back to Page1
    ewk_view_back(webView());
    ASSERT_TRUE(waitUntilTitleChangedTo("Page1"));
    ASSERT_STREQ("Page1", ewk_view_title_get(webView()));
    ASSERT_FALSE(ewk_view_back_possible(webView()));
    ASSERT_TRUE(ewk_view_forward_possible(webView()));

    // Go forward to Page2
    ewk_view_forward(webView());
    ASSERT_TRUE(waitUntilTitleChangedTo("Page2"));
    ASSERT_STREQ("Page2", ewk_view_title_get(webView()));
    ASSERT_TRUE(ewk_view_back_possible(webView()));
    ASSERT_FALSE(ewk_view_forward_possible(webView()));
}

TEST_F(EWK2UnitTestBase, ewk_view_setting_encoding_custom)
{
    ASSERT_FALSE(ewk_view_setting_encoding_custom_get(webView()));
    ASSERT_TRUE(ewk_view_setting_encoding_custom_set(webView(), "UTF-8"));
    ASSERT_STREQ("UTF-8", ewk_view_setting_encoding_custom_get(webView()));
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
    ASSERT_EQ(3, eina_list_count(fieldNames));
    void* data;
    EINA_LIST_FREE(fieldNames, data)
        eina_stringshare_del(static_cast<char*>(data));

    const char* value1 = ewk_form_submission_request_field_value_get(request, "text1");
    ASSERT_STREQ("value1", value1);
    eina_stringshare_del(value1);
    const char* value2 = ewk_form_submission_request_field_value_get(request, "text2");
    ASSERT_STREQ("value2", value2);
    eina_stringshare_del(value2);
    const char* password = ewk_form_submission_request_field_value_get(request, "password");
    ASSERT_STREQ("secret", password);
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

static inline void checkBasicPopupMenuItem(Ewk_Popup_Menu_Item* item, const char* title, bool enabled)
{
    EXPECT_EQ(EWK_POPUP_MENU_ITEM, ewk_popup_menu_item_type_get(item));
    EXPECT_STREQ(title, ewk_popup_menu_item_text_get(item));
    EXPECT_EQ(enabled, ewk_popup_menu_item_enabled_get(item));
}

static Eina_Bool selectItemAfterDelayed(void* data)
{
    EXPECT_TRUE(ewk_view_popup_menu_select(static_cast<Evas_Object*>(data), 0));
    return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool showPopupMenu(Ewk_View_Smart_Data* smartData, Eina_Rectangle, Ewk_Text_Direction, double, Eina_List* list, int selectedIndex)
{
    EXPECT_EQ(2, selectedIndex);

    Ewk_Popup_Menu_Item* item = static_cast<Ewk_Popup_Menu_Item*>(eina_list_nth(list, 0));
    checkBasicPopupMenuItem(item, "first", true);
    EXPECT_EQ(EWK_TEXT_DIRECTION_LEFT_TO_RIGHT, ewk_popup_menu_item_text_direction_get(item));
    EXPECT_STREQ("", ewk_popup_menu_item_tooltip_get(item));
    EXPECT_STREQ("", ewk_popup_menu_item_accessibility_text_get(item));
    EXPECT_FALSE(ewk_popup_menu_item_is_label_get(item));
    EXPECT_FALSE(ewk_popup_menu_item_selected_get(item));

    item = static_cast<Ewk_Popup_Menu_Item*>(eina_list_nth(list, 1));
    checkBasicPopupMenuItem(item, "second", false);
    EXPECT_FALSE(ewk_popup_menu_item_enabled_get(item));

    item = static_cast<Ewk_Popup_Menu_Item*>(eina_list_nth(list, 2));
    checkBasicPopupMenuItem(item, "third", true);
    EXPECT_EQ(EWK_TEXT_DIRECTION_RIGHT_TO_LEFT, ewk_popup_menu_item_text_direction_get(item));
    EXPECT_STREQ("tooltip", ewk_popup_menu_item_tooltip_get(item));
    EXPECT_STREQ("aria", ewk_popup_menu_item_accessibility_text_get(item));
    EXPECT_TRUE(ewk_popup_menu_item_selected_get(item));

    item = static_cast<Ewk_Popup_Menu_Item*>(eina_list_nth(list, 3));
    checkBasicPopupMenuItem(item, "label", false);
    EXPECT_TRUE(ewk_popup_menu_item_is_label_get(item));

    item = static_cast<Ewk_Popup_Menu_Item*>(eina_list_nth(list, 4));
    checkBasicPopupMenuItem(item, "    forth", true);

    item = static_cast<Ewk_Popup_Menu_Item*>(eina_list_nth(list, 5));
    EXPECT_EQ(EWK_POPUP_MENU_UNKNOWN, ewk_popup_menu_item_type_get(item));
    EXPECT_STREQ(0, ewk_popup_menu_item_text_get(item));

    ecore_timer_add(0, selectItemAfterDelayed, smartData->self);
    return true;
}

TEST_F(EWK2UnitTestBase, ewk_view_popup_menu_select)
{
    const char* selectHTML =
        "<!doctype html><body><select onchange=\"document.title=this.value;\">"
        "<option>first</option><option disabled>second</option><option selected dir=\"rtl\" title=\"tooltip\" aria-label=\"aria\">third</option>"
        "<optgroup label=\"label\"><option>forth</option></optgroup>"
        "</select></body>";

    ewkViewClass()->popup_menu_show = showPopupMenu;

    ewk_view_html_string_load(webView(), selectHTML, "file:///", 0);
    ASSERT_TRUE(waitUntilLoadFinished());
    mouseClick(30, 20);
    ASSERT_TRUE(waitUntilTitleChangedTo("first"));

    EXPECT_TRUE(ewk_view_popup_menu_close(webView()));
    EXPECT_FALSE(ewk_view_popup_menu_select(webView(), 0));
}

TEST_F(EWK2UnitTestBase, ewk_view_settings_get)
{
    Ewk_Settings* settings = ewk_view_settings_get(webView());
    ASSERT_TRUE(settings);
    ASSERT_EQ(settings, ewk_view_settings_get(webView()));
}

TEST_F(EWK2UnitTestBase, ewk_view_theme_set)
{
    const char* buttonHTML = "<html><body><input type='button' id='btn'>"
        "<script>document.title=document.getElementById('btn').clientWidth;</script>"
        "</body></html>";

    ewk_view_html_string_load(webView(), buttonHTML, "file:///", 0);
    EXPECT_TRUE(waitUntilTitleChangedTo("30")); // button of default theme has 30px as padding (15 to -16)

    ewk_view_theme_set(webView(), environment->pathForResource("it_does_not_exist.edj").data());
    ewk_view_html_string_load(webView(), buttonHTML, "file:///", 0);
    EXPECT_TRUE(waitUntilTitleChangedTo("30")); // the result should be same as default theme

    ewk_view_theme_set(webView(), environment->pathForResource("empty_theme.edj").data());
    ewk_view_html_string_load(webView(), buttonHTML, "file:///", 0);
    EXPECT_TRUE(waitUntilTitleChangedTo("30")); // the result should be same as default theme

    ewk_view_theme_set(webView(), environment->pathForResource("big_button_theme.edj").data());
    ewk_view_html_string_load(webView(), buttonHTML, "file:///", 0);
    EXPECT_TRUE(waitUntilTitleChangedTo("299")); // button of big button theme has 299px as padding (150 to -150)
}

TEST_F(EWK2UnitTestBase, ewk_view_mouse_events_enabled)
{
    ASSERT_TRUE(ewk_view_mouse_events_enabled_set(webView(), EINA_TRUE));
    ASSERT_TRUE(ewk_view_mouse_events_enabled_get(webView()));

    ASSERT_TRUE(ewk_view_mouse_events_enabled_set(webView(), 2));
    ASSERT_TRUE(ewk_view_mouse_events_enabled_get(webView()));

    ASSERT_TRUE(ewk_view_mouse_events_enabled_set(webView(), EINA_FALSE));
    ASSERT_FALSE(ewk_view_mouse_events_enabled_get(webView()));
}

static Eina_Bool fullScreenCallback(Ewk_View_Smart_Data* smartData)
{
    fullScreenCallbackCalled = true;
    return false;
}

TEST_F(EWK2UnitTestBase, ewk_view_full_screen_enter)
{
    const char fullscreenHTML[] =
        "<!doctype html><head><script>function makeFullScreen(){"
        "var div = document.getElementById(\"fullscreen\");"
        "div.webkitRequestFullScreen();"
        "document.title = \"fullscreen entered\";"
        "}</script></head>"
        "<body><div id=\"fullscreen\" style=\"width:100px; height:100px\" onclick=\"makeFullScreen()\"></div></body>";

    ewkViewClass()->fullscreen_enter = fullScreenCallback;

    ewk_view_html_string_load(webView(), fullscreenHTML, "file:///", 0);
    ASSERT_TRUE(waitUntilLoadFinished());
    mouseClick(50, 50);
    ASSERT_TRUE(waitUntilTitleChangedTo("fullscreen entered"));
    ASSERT_TRUE(fullScreenCallbackCalled);
}

TEST_F(EWK2UnitTestBase, ewk_view_full_screen_exit)
{
    const char fullscreenHTML[] =
        "<!doctype html><head><script>function makeFullScreenAndExit(){"
        "var div = document.getElementById(\"fullscreen\");"
        "div.webkitRequestFullScreen();"
        "document.webkitCancelFullScreen();"
        "document.title = \"fullscreen exited\";"
        "}</script></head>"
        "<body><div id=\"fullscreen\" style=\"width:100px; height:100px\" onclick=\"makeFullScreenAndExit()\"></div></body>";

    ewkViewClass()->fullscreen_exit = fullScreenCallback;

    ewk_view_html_string_load(webView(), fullscreenHTML, "file:///", 0);
    ASSERT_TRUE(waitUntilLoadFinished());
    mouseClick(50, 50);
    ASSERT_TRUE(waitUntilTitleChangedTo("fullscreen exited"));
    ASSERT_TRUE(fullScreenCallbackCalled);
}

TEST_F(EWK2UnitTestBase, ewk_view_same_page_navigation)
{
    // Tests that same page navigation updates the page URL.
    String testUrl = environment->urlForResource("same_page_navigation.html").data();
    ASSERT_TRUE(loadUrlSync(testUrl.utf8().data()));
    ASSERT_STREQ(testUrl.utf8().data(), ewk_view_url_get(webView()));
    mouseClick(50, 50);
    testUrl = testUrl + '#';
    ASSERT_TRUE(waitUntilURLChangedTo(testUrl.utf8().data()));
}

TEST_F(EWK2UnitTestBase, ewk_view_title_changed)
{
    const char* titleChangedHTML =
        "<!doctype html><head><title>Title before changed</title></head>"
        "<body onload=\"document.title='Title after changed';\"></body>";
    ewk_view_html_string_load(webView(), titleChangedHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo("Title after changed"));
    EXPECT_STREQ("Title after changed", ewk_view_title_get(webView()));

    titleChangedHTML =
        "<!doctype html><head><title>Title before changed</title></head>"
        "<body onload=\"document.title='';\"></body>";
    ewk_view_html_string_load(webView(), titleChangedHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo(""));
    EXPECT_STREQ("", ewk_view_title_get(webView()));

    titleChangedHTML =
        "<!doctype html><head><title>Title before changed</title></head>"
        "<body onload=\"document.title=null;\"></body>";
    ewk_view_html_string_load(webView(), titleChangedHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo(""));
    EXPECT_STREQ("", ewk_view_title_get(webView()));
}

static struct {
    const char* expectedMessage;
    bool called;
} alertCallbackData;

static struct {
    const char* expectedMessage;
    bool result;
    bool called;
} confirmCallbackData;

static struct {
    const char* expectedMessage;
    const char* expectedDefaultValue;
    const char* result;
    bool called;
} promptCallbackData;

static void checkAlert(Ewk_View_Smart_Data*, const char* message)
{
    alertCallbackData.called = true;
    EXPECT_STREQ(message, alertCallbackData.expectedMessage);
}

TEST_F(EWK2UnitTestBase, ewk_view_run_javascript_alert)
{
    ewkViewClass()->run_javascript_alert = checkAlert;

    const char* alertHTML = "<!doctype html><body onload=\"alert('Alert message');\"></body>";
    alertCallbackData.expectedMessage = "Alert message";
    alertCallbackData.called = false;
    ewk_view_html_string_load(webView(), alertHTML, 0, 0);
    EXPECT_TRUE(waitUntilLoadFinished());
    EXPECT_TRUE(alertCallbackData.called);

    alertHTML = "<!doctype html><body onload=\"alert('');\"></body>";
    alertCallbackData.expectedMessage = "";
    alertCallbackData.called = false;
    ewk_view_html_string_load(webView(), alertHTML, 0, 0);
    EXPECT_TRUE(waitUntilLoadFinished());
    EXPECT_TRUE(alertCallbackData.called);

    alertHTML = "<!doctype html><body onload=\"alert(null);\"></body>";
    alertCallbackData.expectedMessage = "null";
    alertCallbackData.called = false;
    ewk_view_html_string_load(webView(), alertHTML, 0, 0);
    EXPECT_TRUE(waitUntilLoadFinished());
    EXPECT_TRUE(alertCallbackData.called);

    alertHTML = "<!doctype html><body onload=\"alert();\"></body>";
    alertCallbackData.expectedMessage = "undefined";
    alertCallbackData.called = false;
    ewk_view_html_string_load(webView(), alertHTML, 0, 0);
    EXPECT_TRUE(waitUntilLoadFinished());
    EXPECT_TRUE(alertCallbackData.called);

    ewkViewClass()->run_javascript_alert = 0;

    alertCallbackData.called = false;
    ewk_view_html_string_load(webView(), alertHTML, 0, 0);
    EXPECT_TRUE(waitUntilLoadFinished());
    EXPECT_FALSE(alertCallbackData.called);
}

static Eina_Bool checkConfirm(Ewk_View_Smart_Data*, const char* message)
{
    confirmCallbackData.called = true;
    EXPECT_STREQ(message, confirmCallbackData.expectedMessage);
    return confirmCallbackData.result;
}

TEST_F(EWK2UnitTestBase, ewk_view_run_javascript_confirm)
{
    ewkViewClass()->run_javascript_confirm = checkConfirm;

    const char* confirmHTML = "<!doctype html><body onload=\"document.title = confirm('Confirm message');\"></body>";
    confirmCallbackData.expectedMessage = "Confirm message";
    confirmCallbackData.result = true;
    confirmCallbackData.called = false;
    ewk_view_html_string_load(webView(), confirmHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo("true"));
    EXPECT_STREQ("true", ewk_view_title_get(webView()));
    EXPECT_TRUE(confirmCallbackData.called);

    confirmCallbackData.expectedMessage = "Confirm message";
    confirmCallbackData.result = false;
    confirmCallbackData.called = false;
    ewk_view_html_string_load(webView(), confirmHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo("false"));
    EXPECT_STREQ("false", ewk_view_title_get(webView()));
    EXPECT_TRUE(confirmCallbackData.called);

    confirmHTML = "<!doctype html><body onload=\"document.title = confirm('');\"></body>";
    confirmCallbackData.expectedMessage = "";
    confirmCallbackData.result = true;
    confirmCallbackData.called = false;
    ewk_view_html_string_load(webView(), confirmHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo("true"));
    EXPECT_STREQ("true", ewk_view_title_get(webView()));
    EXPECT_TRUE(confirmCallbackData.called);

    confirmHTML = "<!doctype html><body onload=\"document.title = confirm(null);\"></body>";
    confirmCallbackData.expectedMessage = "null";
    confirmCallbackData.result = true;
    confirmCallbackData.called = false;
    ewk_view_html_string_load(webView(), confirmHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo("true"));
    EXPECT_STREQ("true", ewk_view_title_get(webView()));
    EXPECT_TRUE(confirmCallbackData.called);

    confirmHTML = "<!doctype html><body onload=\"document.title = confirm();\"></body>";
    confirmCallbackData.expectedMessage = "undefined";
    confirmCallbackData.result = true;
    confirmCallbackData.called = false;
    ewk_view_html_string_load(webView(), confirmHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo("true"));
    EXPECT_STREQ("true", ewk_view_title_get(webView()));
    EXPECT_TRUE(confirmCallbackData.called);

    ewkViewClass()->run_javascript_confirm = 0;

    confirmCallbackData.called = false;
    ewk_view_html_string_load(webView(), confirmHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo("false"));
    EXPECT_STREQ("false", ewk_view_title_get(webView()));
    EXPECT_FALSE(confirmCallbackData.called);
}

static const char* checkPrompt(Ewk_View_Smart_Data*, const char* message, const char* defaultValue)
{
    promptCallbackData.called = true;
    EXPECT_STREQ(message, promptCallbackData.expectedMessage);
    EXPECT_STREQ(defaultValue, promptCallbackData.expectedDefaultValue);

    if (!promptCallbackData.result)
        return 0;

    return eina_stringshare_add(promptCallbackData.result);
}

TEST_F(EWK2UnitTestBase, ewk_view_run_javascript_prompt)
{
    static const char promptMessage[] = "Prompt message";
    static const char promptResult[] = "Prompt result";

    ewkViewClass()->run_javascript_prompt = checkPrompt;

    const char* promptHTML = "<!doctype html><body onload=\"document.title = prompt('Prompt message', 'Prompt default value');\"></body>";
    promptCallbackData.expectedMessage = promptMessage;
    promptCallbackData.expectedDefaultValue = "Prompt default value";
    promptCallbackData.result = promptResult;
    promptCallbackData.called = false;
    ewk_view_html_string_load(webView(), promptHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo(promptResult));
    EXPECT_STREQ(promptResult, ewk_view_title_get(webView()));
    EXPECT_TRUE(promptCallbackData.called);

    promptHTML = "<!doctype html><body onload=\"document.title = prompt('Prompt message', '');\"></body>";
    promptCallbackData.expectedMessage = promptMessage;
    promptCallbackData.expectedDefaultValue = "";
    promptCallbackData.result = promptResult;
    promptCallbackData.called = false;
    ewk_view_html_string_load(webView(), promptHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo(promptResult));
    EXPECT_STREQ(promptResult, ewk_view_title_get(webView()));
    EXPECT_TRUE(promptCallbackData.called);

    promptHTML = "<!doctype html><body onload=\"document.title = prompt('Prompt message');\"></body>";
    promptCallbackData.expectedMessage = promptMessage;
    promptCallbackData.expectedDefaultValue = "";
    promptCallbackData.result = promptResult;
    promptCallbackData.called = false;
    ewk_view_html_string_load(webView(), promptHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo(promptResult));
    EXPECT_STREQ(promptResult, ewk_view_title_get(webView()));
    EXPECT_TRUE(promptCallbackData.called);

    promptHTML = "<!doctype html><body onload=\"document.title = prompt('');\"></body>";
    promptCallbackData.expectedMessage = "";
    promptCallbackData.expectedDefaultValue = "";
    promptCallbackData.result = promptResult;
    promptCallbackData.called = false;
    ewk_view_html_string_load(webView(), promptHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo(promptResult));
    EXPECT_STREQ(promptResult, ewk_view_title_get(webView()));
    EXPECT_TRUE(promptCallbackData.called);

    promptHTML = "<!doctype html><body onload=\"document.title = prompt();\"></body>";
    promptCallbackData.expectedMessage = "undefined";
    promptCallbackData.expectedDefaultValue = "";
    promptCallbackData.result = promptResult;
    promptCallbackData.called = false;
    ewk_view_html_string_load(webView(), promptHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo(promptResult));
    EXPECT_STREQ(promptResult, ewk_view_title_get(webView()));
    EXPECT_TRUE(promptCallbackData.called);

    promptHTML = "<html><head><title>Default title</title></head>"
                 "<body onload=\"var promptResult = prompt('Prompt message');"
                 "if (promptResult == null) document.title='null';"
                 "else document.title = promptResult;\"></body></html>";
    promptCallbackData.expectedMessage = promptMessage;
    promptCallbackData.expectedDefaultValue = "";
    promptCallbackData.result = "";
    promptCallbackData.called = false;
    ewk_view_html_string_load(webView(), promptHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo(""));
    EXPECT_STREQ("", ewk_view_title_get(webView()));
    EXPECT_TRUE(promptCallbackData.called);

    promptCallbackData.expectedMessage = promptMessage;
    promptCallbackData.expectedDefaultValue = "";
    promptCallbackData.result = 0;
    promptCallbackData.called = false;
    ewk_view_html_string_load(webView(), promptHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo("null"));
    EXPECT_STREQ("null", ewk_view_title_get(webView()));
    EXPECT_TRUE(promptCallbackData.called);

    ewkViewClass()->run_javascript_prompt = 0;

    promptCallbackData.called = false;
    ewk_view_html_string_load(webView(), promptHTML, 0, 0);
    EXPECT_TRUE(waitUntilTitleChangedTo("null"));
    EXPECT_STREQ("null", ewk_view_title_get(webView()));
    EXPECT_FALSE(promptCallbackData.called);
}

#if ENABLE(INPUT_TYPE_COLOR)
static const int initialRed = 0x12;
static const int initialGreen = 0x34;
static const int initialBlue = 0x56;
static const int initialAlpha = 0xff;
static const int changedRed = 0x98;
static const int changedGreen = 0x76;
static const int changedBlue = 0x54;
static const int changedAlpha = 0xff;

static bool isColorPickerShown = false;

static void onColorPickerDone(void* userData, Evas_Object*, void*)
{
    bool* handled = static_cast<bool*>(userData);

    *handled = true;
}

static unsigned char setColorPickerColor(void* data)
{
    Ewk_View_Smart_Data* smartData = static_cast<Ewk_View_Smart_Data*>(data);

    // 4. Change color to changed color.
    EXPECT_TRUE(ewk_view_color_picker_color_set(smartData->self, changedRed, changedGreen, changedBlue, changedAlpha));

    evas_object_smart_callback_call(smartData->self, "input,type,color,request", 0);

    return 0;
}

static Eina_Bool showColorPicker(Ewk_View_Smart_Data* smartData, int r, int g, int b, int a)
{
    static bool isFirstRun = true;

    isColorPickerShown = true;

    if (isFirstRun) {
        // 2. Check initial value from html file.
        EXPECT_EQ(initialRed, r);
        EXPECT_EQ(initialGreen, g);
        EXPECT_EQ(initialBlue, b);
        EXPECT_EQ(initialAlpha, a);

        isFirstRun = false;
    } else {
        // 7. Input values should be same as changed color.
        EXPECT_EQ(changedRed, r);
        EXPECT_EQ(changedGreen, g);
        EXPECT_EQ(changedBlue, b);
        EXPECT_EQ(changedAlpha, a);

        evas_object_smart_callback_call(smartData->self, "input,type,color,request", 0);
        return true;
    }

    // 3. Return after making a color picker.
    ecore_timer_add(0.0, setColorPickerColor, smartData);
    return true;
}

static Eina_Bool hideColorPicker(Ewk_View_Smart_Data*)
{
    // 5. Test color picker is shown.
    EXPECT_TRUE(isColorPickerShown);
    isColorPickerShown = false;
}

static Eina_Bool hideColorPickerByRemovingElement(Ewk_View_Smart_Data* smartData)
{
    // 9. input_picker_color_dismiss() is called if the element is removed.
    EXPECT_TRUE(isColorPickerShown);
    isColorPickerShown = false;
    evas_object_smart_callback_call(smartData->self, "input,type,color,request", 0);
}

TEST_F(EWK2UnitTestBase, ewk_view_color_picker_color_set)
{
    Ewk_View_Smart_Class* api = ewkViewClass();
    api->input_picker_color_request = showColorPicker;
    api->input_picker_color_dismiss = hideColorPicker;

    const char colorPickerHTML[] =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<script>function removeInputElement(){"
        "var parentElement = document.getElementById('parent');"
        "var inputElement = document.getElementById('color');"
        "parentElement.removeChild(inputElement);"
        "}</script>"
        "</head>"
        "<body>"
        "<div id='parent'>"
        "<input type='color' value='#123456' id='color'>"
        "<button onclick='removeInputElement();'>Remove Element</button>"
        "</div>"
        "</body>"
        "</html>";

    ewk_view_html_string_load(webView(), colorPickerHTML, 0, 0);
    waitUntilLoadFinished();

    // 1. Click input element to show color picker.
    mouseClick(30, 20);

    bool handled = false;
    evas_object_smart_callback_add(webView(), "input,type,color,request", onColorPickerDone, &handled);
    while (!handled)
        ecore_main_loop_iterate();

    // 6. Click input element to show color picker again.
    mouseClick(30, 20);

    handled = false;
    while (!handled)
        ecore_main_loop_iterate();

    // 8. Click button to remove input element durlng color picker is shown.
    api->input_picker_color_dismiss = hideColorPickerByRemovingElement;
    mouseClick(80, 20);

    handled = false;
    while (!handled)
        ecore_main_loop_iterate();
    evas_object_smart_callback_del(webView(), "input,type,color,request", onColorPickerDone);
}
#endif // ENABLE(INPUT_TYPE_COLOR)

TEST_F(EWK2UnitTestBase, ewk_view_context_get)
{
    Ewk_Context* context = ewk_view_context_get(webView());
    ASSERT_TRUE(context);
    ASSERT_EQ(context, ewk_view_context_get(webView()));
}

TEST_F(EWK2UnitTestBase, ewk_view_feed_touch_event)
{
    Eina_List* points = 0;
    Ewk_Touch_Point point1 = { 0, 0, 0, EVAS_TOUCH_POINT_DOWN };
    Ewk_Touch_Point point2 = { 1, 0, 0, EVAS_TOUCH_POINT_DOWN };
    points = eina_list_append(points, &point1);
    points = eina_list_append(points, &point2);
    ASSERT_TRUE(ewk_view_feed_touch_event(webView(), EWK_TOUCH_START, points, evas_key_modifier_get(evas_object_evas_get(webView()))));

    point1.state = EVAS_TOUCH_POINT_STILL;
    point2.x = 100;
    point2.y = 100;
    point2.state = EVAS_TOUCH_POINT_MOVE;
    ASSERT_TRUE(ewk_view_feed_touch_event(webView(), EWK_TOUCH_MOVE, points, evas_key_modifier_get(evas_object_evas_get(webView()))));

    point2.state = EVAS_TOUCH_POINT_UP;
    ASSERT_TRUE(ewk_view_feed_touch_event(webView(), EWK_TOUCH_END, points, evas_key_modifier_get(evas_object_evas_get(webView()))));
    points = eina_list_remove(points, &point2);

    point1.state = EVAS_TOUCH_POINT_CANCEL;
    ASSERT_TRUE(ewk_view_feed_touch_event(webView(), EWK_TOUCH_CANCEL, points, evas_key_modifier_get(evas_object_evas_get(webView()))));
    points = eina_list_remove(points, &point1);

    eina_list_free(points);
}

static void onTextFound(void* userData, Evas_Object*, void* eventInfo)
{
    int* result = static_cast<int*>(userData);
    unsigned* matchCount = static_cast<unsigned*>(eventInfo);

    *result = *matchCount;
}

TEST_F(EWK2UnitTestBase, ewk_view_text_find)
{
    const char textFindHTML[] =
        "<!DOCTYPE html>"
        "<body>"
        "apple apple apple banana banana coconut"
        "</body>";
    ewk_view_html_string_load(webView(), textFindHTML, 0, 0);
    waitUntilLoadFinished();

    int matchCount = -1;
    evas_object_smart_callback_add(webView(), "text,found", onTextFound, &matchCount);

    ewk_view_text_find(webView(), "apple", EWK_FIND_OPTIONS_SHOW_OVERLAY, 100);
    while (matchCount < 0)
        ecore_main_loop_iterate();
    EXPECT_EQ(3, matchCount);

    matchCount = -1;
    ewk_view_text_find(webView(), "mango", EWK_FIND_OPTIONS_SHOW_OVERLAY, 100);
    while (matchCount < 0)
        ecore_main_loop_iterate();
    EXPECT_EQ(0, matchCount);

    evas_object_smart_callback_del(webView(), "text,found", onTextFound);
}

TEST_F(EWK2UnitTestBase, ewk_view_text_matches_count)
{
    const char textFindHTML[] =
        "<!DOCTYPE html>"
        "<body>"
        "apple Apple apple apple banana bananaApple banana coconut"
        "</body>";
    ewk_view_html_string_load(webView(), textFindHTML, 0, 0);
    waitUntilLoadFinished();

    int matchCount = -1;
    evas_object_smart_callback_add(webView(), "text,found", onTextFound, &matchCount);

    ewk_view_text_matches_count(webView(), "apple", EWK_FIND_OPTIONS_NONE, 100);
    while (matchCount < 0)
        ecore_main_loop_iterate();
    EXPECT_EQ(3, matchCount);

    matchCount = -1;
    ewk_view_text_matches_count(webView(), "apple", EWK_FIND_OPTIONS_CASE_INSENSITIVE, 100);
    while (matchCount < 0)
        ecore_main_loop_iterate();
    EXPECT_EQ(5, matchCount);

    matchCount = -1;
    ewk_view_text_matches_count(webView(), "Apple", EWK_FIND_OPTIONS_AT_WORD_STARTS, 100);
    while (matchCount < 0)
        ecore_main_loop_iterate();
    EXPECT_EQ(1, matchCount);

    matchCount = -1;
    ewk_view_text_matches_count(webView(), "Apple", EWK_FIND_OPTIONS_TREAT_MEDIAL_CAPITAL_AS_WORD_START, 100);
    while (matchCount < 0)
        ecore_main_loop_iterate();
    EXPECT_EQ(2, matchCount);

    matchCount = -1;
    ewk_view_text_matches_count(webView(), "mango", EWK_FIND_OPTIONS_NONE, 100);
    while (matchCount < 0)
        ecore_main_loop_iterate();
    EXPECT_EQ(0, matchCount);

    // If we have more matches than allowed, -1 is returned as a matched count.
    matchCount = -2;
    ewk_view_text_matches_count(webView(), "apple", EWK_FIND_OPTIONS_NONE, 2);
    while (matchCount < -1)
        ecore_main_loop_iterate();
    EXPECT_EQ(-1, matchCount);

    evas_object_smart_callback_del(webView(), "text,found", onTextFound);
}

TEST_F(EWK2UnitTestBase, ewk_view_touch_events_enabled)
{
    ASSERT_FALSE(ewk_view_touch_events_enabled_get(webView()));

    ASSERT_TRUE(ewk_view_touch_events_enabled_set(webView(), true));
    ASSERT_TRUE(ewk_view_touch_events_enabled_get(webView()));

    ASSERT_TRUE(ewk_view_touch_events_enabled_set(webView(), 2));
    ASSERT_TRUE(ewk_view_touch_events_enabled_get(webView()));

    const char* touchHTML =
        "<!doctype html><body><div style=\"width:100px; height:100px;\" "
        "ontouchstart=\"document.title='touchstarted' + event.touches.length;\" "
        "ontouchmove=\"document.title='touchmoved' + event.touches.length;\" "
        "ontouchend=\"document.title='touchended' + event.touches.length;\">"
        "</div></body>";

    ewk_view_html_string_load(webView(), touchHTML, "file:///", 0);
    ASSERT_TRUE(waitUntilLoadFinished());

    mouseDown(10, 10);
    ASSERT_TRUE(waitUntilTitleChangedTo("touchstarted1"));

    multiDown(1, 30, 30);
    ASSERT_TRUE(waitUntilTitleChangedTo("touchstarted2"));

    multiMove(1, 40, 40);
    ASSERT_TRUE(waitUntilTitleChangedTo("touchmoved2"));

    multiUp(1, 40, 40);
    ASSERT_TRUE(waitUntilTitleChangedTo("touchended1"));

    mouseMove(20, 20);
    ASSERT_TRUE(waitUntilTitleChangedTo("touchmoved1"));

    mouseUp(20, 20);
    ASSERT_TRUE(waitUntilTitleChangedTo("touchended0"));

    ASSERT_TRUE(ewk_view_touch_events_enabled_set(webView(), false));
    ASSERT_FALSE(ewk_view_touch_events_enabled_get(webView()));
}

Eina_Bool windowMoveResizeTimedOut(void* data)
{
    *static_cast<bool*>(data) = true;
}

TEST_F(EWK2UnitTestBase, window_move_resize)
{
    int x, y, width, height;
    Ecore_Evas* ee = ecore_evas_ecore_evas_get(evas_object_evas_get(webView()));
    ecore_evas_geometry_get(ee, 0, 0, &width, &height);

    EXPECT_EQ(800, width);
    EXPECT_EQ(600, height);

    ewk_view_url_set(webView(), environment->urlForResource("window_move_resize.html").data());
    ASSERT_TRUE(waitUntilTitleChangedTo("Moved and resized"));

    // Check that the window has been moved and resized.
    ecore_evas_request_geometry_get(ee, &x, &y, &width, &height);
    EXPECT_EQ(150, x);
    EXPECT_EQ(200, y);
    EXPECT_EQ(200, width);
    EXPECT_EQ(100, height);
}

TEST_F(EWK2UnitTestBase, ewk_view_inspector)
{
#if ENABLE(INSPECTOR)
    ASSERT_TRUE(ewk_view_inspector_show(webView()));
    ASSERT_TRUE(ewk_view_inspector_close(webView()));
#else
    ASSERT_FALSE(ewk_view_inspector_show(webView()));
    ASSERT_FALSE(ewk_view_inspector_close(webView()));
#endif
}
