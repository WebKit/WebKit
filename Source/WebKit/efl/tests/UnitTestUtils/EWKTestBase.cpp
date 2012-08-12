/*
    Copyright (C) 2012 Samsung Electronics

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
#include "EWKTestBase.h"

#include "EWKTestConfig.h"
#include "EWKTestView.h"
#include <EWebKit.h>

#include <Ecore.h>
#include <Edje.h>

int EWKUnitTests::EWKTestBase::useX11Window;

namespace EWKUnitTests {

bool EWKTestBase::init()
{
    if (!ecore_evas_init())
        return false;

    if (!edje_init()) {
        ecore_evas_shutdown();
        return false;
    }

    int ret = ewk_init();
    const char* proxyUri = getenv("http_proxy");

    if (ret && proxyUri)
        ewk_network_proxy_uri_set(proxyUri);

    return ret;
}

void EWKTestBase::shutdownAll()
{
    int count = 0;

    while ((count = ecore_evas_shutdown()) > 0) { }
    while ((count = edje_shutdown()) > 0) { }
    while ((count = ewk_shutdown()) > 0) { }
}

void EWKTestBase::startTest()
{
    ecore_main_loop_begin();
}

void EWKTestBase::endTest()
{
    ecore_main_loop_quit();
}

bool EWKTestBase::createTest(const char* url, void (*event_callback)(void*, Evas_Object*, void*), const char* event_name, void* event_data)
{
    EFL_INIT_RET();

    EWKTestEcoreEvas evas(useX11Window);
    if (!evas.evas())
        return false;
    evas.show();

    EWKTestView view(evas.evas(), url);
    if (!view.init())
        return false;

    view.bindEvents(event_callback, event_name, event_data);
    view.show();

    START_TEST();

    return true;
}

bool EWKTestBase::runTest(void (*event_callback)(void*, Evas_Object*, void*), const char* event_name, void* event_data)
{
    return createTest(Config::defaultTestPage, event_callback, event_name, event_data);
}

bool EWKTestBase::runTest(const char* url, void (*event_callback)(void*, Evas_Object*, void*), const char* event_name, void* event_data)
{
    return createTest(url, event_callback, event_name, event_data);
}

}
