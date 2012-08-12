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

#ifndef EWKTestBase_h
#define EWKTestBase_h

#include <Evas.h>
#include <gtest/gtest.h>

#define RUN_TEST(args...)   \
    do {    \
        ASSERT_EQ(true, EWKTestBase::runTest(args));    \
    } while (0)

#define START_TEST()    \
    do {    \
        EWKTestBase::startTest();   \
    } while (0)

#define END_TEST()    \
    do {    \
        EWKTestBase::endTest(); \
    } while (0)

#define EFL_INIT_RET()  \
    do {    \
        if (!EWKTestBase::init())   \
            return false;    \
    } while (0)

#define EFL_INIT()  \
    do {    \
        EWKTestBase::init();    \
    } while (0)

namespace EWKUnitTests {

class EWKTestBase {
    static bool createTest(const char* url, void (*event_callback)(void*, Evas_Object*, void*), const char* event_name, void* event_data);
public:
    static bool init();
    static void shutdownAll();
    static void startTest();
    static void endTest();

    static bool runTest(const char* url, void (*event_callback)(void*, Evas_Object*, void*), const char* event_name = "load,finished", void* event_data = 0);
    static bool runTest(void (*event_callback)(void*, Evas_Object*, void*), const char* event_name = "load,finished", void* event_data = 0);

    static int useX11Window;
};

}

#endif
