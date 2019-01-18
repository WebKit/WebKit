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

class TestRunJavaScript : public WPEQtTest {
    Q_OBJECT
private:
    void main() override;
};

void TestRunJavaScript::main()
{
    const QString title = QString(QLatin1String("WebViewTitle"));
    m_view->loadHtml(QString("<html><head><title>%1</title></head><body /></html>").arg(title));

    waitForLoadSucceeded(m_view);
    QCOMPARE(m_view->loadProgress(), 100);
    QVERIFY(!m_view->isLoading());
    QCOMPARE(m_view->title(), title);
    const QString tstProperty = QString(QLatin1String("Qt.tst_data"));
    QJSValue callback = m_engine.evaluate(QString("function(result) { %1 = result; }").arg(tstProperty));
    QVERIFY2(!callback.isError(), qPrintable(callback.toString()));
    QVERIFY(!callback.isUndefined());
    QVERIFY(callback.isCallable());
    m_view->runJavaScript(QString(QLatin1String("document.title")), callback);
    QTRY_COMPARE(m_engine.evaluate(tstProperty).toString(), title);
}

QTEST_APPLESS_MAIN(TestRunJavaScript)
#include "TestRunJavaScript.moc"
