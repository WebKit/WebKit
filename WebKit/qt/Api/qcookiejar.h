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

#ifndef QCOOKIEJAR_H
#define QCOOKIEJAR_H

#include <QtCore/qobject.h>
#include <QtCore/qurl.h>
#include "qwebkitglobal.h"

class QCookieJarPrivate;

class QWEBKIT_EXPORT QCookieJar : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled)
public:
    QCookieJar();
    ~QCookieJar();

    virtual void setCookies(const QUrl& url, const QUrl& policyUrl, const QString& value);
    virtual QString cookies(const QUrl& url);

    bool isEnabled() const;

    static void setCookieJar(QCookieJar *jar);
    static QCookieJar *cookieJar();

public slots:
    virtual void setEnabled(bool enabled);

private:
    friend class QCookieJarPrivate;
    QCookieJarPrivate *d;
};


#endif
