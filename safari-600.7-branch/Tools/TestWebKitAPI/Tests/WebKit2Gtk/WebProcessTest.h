/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <webkit2/webkit-web-extension.h>
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

class WebProcessTest {
public:
    virtual ~WebProcessTest() { }
    virtual bool runTest(const char* testName, WebKitWebExtension*, GVariant* args) = 0;

    static void add(const String& testName, std::function<PassOwnPtr<WebProcessTest> ()>);
    static PassOwnPtr<WebProcessTest> create(const String& testName);
};

#define REGISTER_TEST(ClassName, TestName) \
    WebProcessTest::add(String::fromUTF8(TestName), ClassName::create)

