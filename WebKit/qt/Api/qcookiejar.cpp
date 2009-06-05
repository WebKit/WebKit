/*
    Copyright (C) 2007 Staikos Computing Services Inc.

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

#include "config.h"
#include "qcookiejar.h"
#include <QCoreApplication>
uint qHash(const QUrl&);
#include <QHash>
#include <QPointer>

class QCookieJarPrivate {
    public:
        QCookieJarPrivate() {
            enabled = true;
        }
        bool enabled;
        QHash<QUrl, QString> jar;
};


uint qHash(const QUrl& url) {
    return qHash(url.toString());
}


QCookieJar::QCookieJar()
: QObject(), d(new QCookieJarPrivate)
{
}


QCookieJar::~QCookieJar()
{
    delete d;
}


void QCookieJar::setCookies(const QUrl& url, const QUrl& policyUrl, const QString& value)
{
    Q_UNUSED(policyUrl)
    d->jar.insert(url, value);
}


QString QCookieJar::cookies(const QUrl& url)
{
    return d->jar.value(url);
}


bool QCookieJar::isEnabled() const
{
    return d->enabled;
}


void QCookieJar::setEnabled(bool enabled)
{
    d->enabled = enabled;
}


static QPointer<QCookieJar> gJar;
static bool gRoutineAdded = false;

static void gCleanupJar()
{
    delete gJar;
}


void QCookieJar::setCookieJar(QCookieJar *jar)
{
    if (!gRoutineAdded) {
        qAddPostRoutine(gCleanupJar);
        gRoutineAdded = true;
    }
    delete gJar;
    gJar = jar;
}


QCookieJar *QCookieJar::cookieJar()
{
    if (!gJar) {
        setCookieJar(new QCookieJar);
    }
    return gJar;
}
