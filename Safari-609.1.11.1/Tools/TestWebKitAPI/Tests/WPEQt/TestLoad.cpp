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

#include <QtCore/QTemporaryFile>
#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qtemporarydir.h>

class TestLoad : public WPEQtTest {
Q_OBJECT
private:
    void main() override;
};

void TestLoad::main()
{
    QString cacheLocation = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    static const char* testHTML = "<html><head><title>FooBar</title></head><body><p>This is a test</p></body></html>";

    QTemporaryFile file(cacheLocation + QStringLiteral("/XXXXXXfile.html"));
    QVERIFY2(file.open(), qPrintable(QStringLiteral("Cannot create temporary file:") + file.errorString()));

    file.write(testHTML);
    const QString fileName(file.fileName());
    file.close();

    const QUrl url(QUrl::fromLocalFile(fileName));
    m_view->setUrl(url);

    waitForLoadSucceeded(m_view);
    QTRY_COMPARE(m_view->loadProgress(), 100);
    QTRY_VERIFY(!m_view->isLoading());
    QCOMPARE(m_view->title(), QStringLiteral("FooBar"));
    QVERIFY(!m_view->canGoBack());
    QVERIFY(!m_view->canGoForward());
    QCOMPARE(m_view->url(), url);
}

QTEST_APPLESS_MAIN(TestLoad)
#include "TestLoad.moc"
