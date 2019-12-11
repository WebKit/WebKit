/*
 * Copyright (C) 2018, 2019 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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

#include "config.h"

#include "WPEQtTest.h"

class TestLoadHtml : public WPEQtTest {
Q_OBJECT
private:
    void main() override;
};

void TestLoadHtml::main()
{
    m_view->loadHtml(QString("<html><head><title>WebViewTitle</title></head><body />"));
    waitForLoadSucceeded(m_view);
    QTRY_COMPARE(m_view->loadProgress(), 100);
    QTRY_VERIFY(!m_view->isLoading());
    QCOMPARE(m_view->title(), QStringLiteral("WebViewTitle"));
}

QTEST_APPLESS_MAIN(TestLoadHtml)
#include "TestLoadHtml.moc"
