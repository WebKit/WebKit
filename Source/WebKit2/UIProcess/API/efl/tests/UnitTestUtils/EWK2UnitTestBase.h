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

#ifndef EWK2UnitTestBase_h
#define EWK2UnitTestBase_h

#include <Ecore_Evas.h>
#include <Evas.h>
#include <gtest/gtest.h>

namespace EWK2UnitTest {

class EWK2UnitTestBase : public ::testing::Test {
public:
    Evas_Object* webView() { return m_webView; }

protected:
    EWK2UnitTestBase();

    virtual void SetUp();
    virtual void TearDown();

    void loadUrlSync(const char* url);
    void waitUntilTitleChangedTo(const char* expectedTitle);
    void mouseClick(int x, int y);

private:
    Evas_Object* m_webView;
    Ecore_Evas* m_ecoreEvas;
};

} // namespace EWK2UnitTest

#endif // EWK2UnitTestBase_h
