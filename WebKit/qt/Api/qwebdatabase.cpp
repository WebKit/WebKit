/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

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
#include "qwebdatabase.h"
#include "qwebdatabase_p.h"
#include "qwebsecurityorigin.h"
#include "qwebsecurityorigin_p.h"
#include "DatabaseDetails.h"
#include "DatabaseTracker.h"

using namespace WebCore;

QWebDatabase::QWebDatabase(const QWebDatabase& other) : d(other.d)
{
}

QWebDatabase& QWebDatabase::operator=(const QWebDatabase& other)
{
    d = other.d;
    return *this;
}

QString QWebDatabase::name() const
{
    return d->name;
}

QString QWebDatabase::displayName() const
{
    DatabaseDetails details = DatabaseTracker::tracker().detailsForNameAndOrigin(d->name, d->origin.get());
    return details.displayName();
}

qint64 QWebDatabase::expectedSize() const
{
    DatabaseDetails details = DatabaseTracker::tracker().detailsForNameAndOrigin(d->name, d->origin.get());
    return details.expectedUsage();
}

qint64 QWebDatabase::size() const
{
    DatabaseDetails details = DatabaseTracker::tracker().detailsForNameAndOrigin(d->name, d->origin.get());
    return details.currentUsage();
}

QWebDatabase::QWebDatabase(QWebDatabasePrivate* priv)
{
    d = priv;
}

QString QWebDatabase::absoluteFilePath() const
{
    return DatabaseTracker::tracker().fullPathForDatabase(d->origin.get(), d->name, false);
}

QWebSecurityOrigin QWebDatabase::origin() const
{
    QWebSecurityOriginPrivate* priv = new QWebSecurityOriginPrivate(d->origin.get());
    QWebSecurityOrigin origin(priv);
    return origin;
}

void QWebDatabase::remove()
{
    DatabaseTracker::tracker().deleteDatabase(d->origin.get(), d->name);
}


QWebDatabase::~QWebDatabase()
{
}
