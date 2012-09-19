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
#include "EWK2UnitTestBase.h"

#include "EWK2UnitTestEnvironment.h"
#include <Ecore.h>
#include <glib-object.h>
#include <wtf/UnusedParam.h>

extern EWK2UnitTest::EWK2UnitTestEnvironment* environment;

namespace EWK2UnitTest {

static Ewk_View_Smart_Class ewk2UnitTestBrowserViewSmartClass()
{
    static Ewk_View_Smart_Class ewkViewClass = EWK_VIEW_SMART_CLASS_INIT_NAME_VERSION("Browser_View");
    return ewkViewClass;
}

EWK2UnitTestBase::EWK2UnitTestBase()
    : m_ecoreEvas(0)
    , m_webView(0)
    , m_ewkViewClass(ewk2UnitTestBrowserViewSmartClass())
{
    ewk_view_smart_class_set(&m_ewkViewClass);
}

void EWK2UnitTestBase::SetUp()
{
    ewk_init();

    unsigned int width = environment->defaultWidth();
    unsigned int height = environment->defaultHeight();

    if (environment->useX11Window())
        m_ecoreEvas = ecore_evas_new(0, 0, 0, width, height, 0);
    else
        m_ecoreEvas = ecore_evas_buffer_new(width, height);

    ecore_evas_show(m_ecoreEvas);
    Evas* evas = ecore_evas_get(m_ecoreEvas);

    Evas_Smart* smart = evas_smart_class_new(&m_ewkViewClass.sc);
    m_webView = ewk_view_smart_add(evas, smart, ewk_context_default_get());
    ewk_view_theme_set(m_webView, environment->defaultTheme());

    evas_object_resize(m_webView, width, height);
    evas_object_show(m_webView);
    evas_object_focus_set(m_webView, true);
}

void EWK2UnitTestBase::TearDown()
{
    evas_object_del(m_webView);
    ecore_evas_free(m_ecoreEvas);
    ewk_shutdown();
}

void EWK2UnitTestBase::loadUrlSync(const char* url)
{
    ewk_view_uri_set(m_webView, url);
    waitUntilLoadFinished();
}

struct LoadFinishedData {
    LoadFinishedData(double timeoutSeconds, Ecore_Task_Cb callback)
        : loadFinished(false)
        , timer(0)
        , didTimeOut(false)
    {
        if (timeoutSeconds >= 0)
            timer = ecore_timer_add(timeoutSeconds, callback, this);
    }

    ~LoadFinishedData()
    {
        if (timer)
            ecore_timer_del(timer);
    }

    bool loadFinished;
    Ecore_Timer* timer;
    bool didTimeOut;
};

static void onLoadFinished(void* userData, Evas_Object* webView, void* eventInfo)
{
    UNUSED_PARAM(webView);
    UNUSED_PARAM(eventInfo);

    LoadFinishedData* data = static_cast<LoadFinishedData*>(userData);
    data->loadFinished = true;

    if (data->timer) {
        ecore_timer_del(data->timer);
        data->timer = 0;
    }
}

static bool timeOutWhileWaitingUntilLoadFinished(void* userData)
{
    LoadFinishedData* data = static_cast<LoadFinishedData*>(userData);

    data->timer = 0;

    if (data->loadFinished)
        return ECORE_CALLBACK_CANCEL;

    data->loadFinished = true;
    data->didTimeOut = true;

    return ECORE_CALLBACK_CANCEL;
}

bool EWK2UnitTestBase::waitUntilLoadFinished(double timeoutSeconds)
{
    LoadFinishedData data(timeoutSeconds, reinterpret_cast<Ecore_Task_Cb>(timeOutWhileWaitingUntilLoadFinished));

    evas_object_smart_callback_add(m_webView, "load,finished", onLoadFinished, &data);

    while (!data.loadFinished)
        ecore_main_loop_iterate();

    evas_object_smart_callback_del(m_webView, "load,finished", onLoadFinished);

    return !data.didTimeOut;
}

struct TitleChangedData {
    TitleChangedData(const char* title, double timeoutSeconds, Ecore_Task_Cb callback)
        : expectedTitle(title)
        , done(false)
        , timer(0)
        , didTimeOut(false)
    {
        if (timeoutSeconds >= 0)
            timer = ecore_timer_add(timeoutSeconds, callback, this);
    }

    ~TitleChangedData()
    {
        if (timer)
            ecore_timer_del(timer);
    }

    CString expectedTitle;
    bool done;
    Ecore_Timer* timer;
    bool didTimeOut;
};

static void onTitleChanged(void* userData, Evas_Object* webView, void* eventInfo)
{
    TitleChangedData* data = static_cast<TitleChangedData*>(userData);

    if (strcmp(ewk_view_title_get(webView), data->expectedTitle.data()))
        return;

    if (data->timer) {
        ecore_timer_del(data->timer);
        data->timer = 0;
    }

    data->done = true;
}

static bool timeOutWhileWaitingUntilTitleChangedTo(void* userData)
{
    TitleChangedData* data = static_cast<TitleChangedData*>(userData);

    data->timer = 0;

    if (data->done)
        return ECORE_CALLBACK_CANCEL;

    data->done = true;
    data->didTimeOut = true;

    return ECORE_CALLBACK_CANCEL;
}

bool EWK2UnitTestBase::waitUntilTitleChangedTo(const char* expectedTitle, double timeoutSeconds)
{
    TitleChangedData data(expectedTitle, timeoutSeconds, reinterpret_cast<Ecore_Task_Cb>(timeOutWhileWaitingUntilTitleChangedTo));

    evas_object_smart_callback_add(m_webView, "title,changed", onTitleChanged, &data);

    while (!data.done)
        ecore_main_loop_iterate();

    evas_object_smart_callback_del(m_webView, "title,changed", onTitleChanged);

    return !data.didTimeOut;
}

void EWK2UnitTestBase::mouseClick(int x, int y)
{
    Evas* evas = evas_object_evas_get(m_webView);
    evas_event_feed_mouse_move(evas, x, y, 0, 0);
    evas_event_feed_mouse_down(evas, /* Left */ 1, EVAS_BUTTON_NONE, 0, 0);
    evas_event_feed_mouse_up(evas, /* Left */ 1, EVAS_BUTTON_NONE, 0, 0);
}

} // namespace EWK2UnitTest
