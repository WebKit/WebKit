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

class TestLoadRequest : public WPEQtTest {
Q_OBJECT
private:
    void main() override;
};

void TestLoadRequest::main()
{
    {
        QString cacheLocation = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QTemporaryFile file(cacheLocation + QStringLiteral("/XXXXXXfile.html"));
        QVERIFY2(file.open(), qPrintable(QStringLiteral("Cannot create temporary file:") + file.errorString()));

        file.write("<html><head><title>FooBar</title></head><body />");
        const QString fileName = file.fileName();
        file.close();
        const QUrl url = QUrl::fromLocalFile(fileName);
        QSignalSpy loadChangedSignalSpy(m_view, SIGNAL(loadingChanged(WPEQtViewLoadRequest*)));
        m_view->setUrl(url);
        waitForLoadSucceeded(m_view);
        QVERIFY(!m_view->isLoading());
        QCOMPARE(m_view->loadProgress(), 100);
        QCOMPARE(m_view->title(), QStringLiteral("FooBar"));
        QCOMPARE(m_view->url(), url);
        QCOMPARE(loadChangedSignalSpy.count(), 2);
    }

    {
        QSignalSpy loadChangedSignalSpy(m_view, SIGNAL(loadingChanged(WPEQtViewLoadRequest*)));
        m_view->setUrl(QUrl("file://IDONTEXIST.html"));
        waitForLoadFailed(m_view);
        QCOMPARE(loadChangedSignalSpy.count(), 2);
    }
}

QTEST_APPLESS_MAIN(TestLoadRequest)
#include "TestLoadRequest.moc"
