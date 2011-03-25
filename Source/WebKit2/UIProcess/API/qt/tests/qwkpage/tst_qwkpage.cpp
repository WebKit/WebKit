/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <QScopedPointer>
#include <QtTest/QtTest>
#include <qwkcontext.h>
#include <qwkpage.h>

class tst_QWKPage : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void loadEmptyUrl();

private:
    QScopedPointer<QWKContext> m_context;
    QScopedPointer<QWKPage> m_page;
};

void tst_QWKPage::init()
{
    m_context.reset(new QWKContext);
    m_page.reset(new QWKPage(m_context.data()));
}

void tst_QWKPage::cleanup()
{
    m_page.reset();
    m_context.reset();
}

void tst_QWKPage::loadEmptyUrl()
{
    m_page->load(QUrl());
    m_page->load(QUrl(QLatin1String("")));
}

QTEST_MAIN(tst_QWKPage)

#include "tst_qwkpage.moc"
