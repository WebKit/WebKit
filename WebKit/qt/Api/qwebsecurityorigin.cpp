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
#include "qwebsecurityorigin.h"
#include "qwebsecurityorigin_p.h"
#include "qwebdatabase.h"
#include "qwebdatabase_p.h"

#include "DatabaseTracker.h"
#include "KURL.h"
#include "SecurityOrigin.h"
#include <QStringList>

using namespace WebCore;

QWebSecurityOrigin::QWebSecurityOrigin(const QWebSecurityOrigin& other) : d(other.d)
{
}

QWebSecurityOrigin& QWebSecurityOrigin::operator=(const QWebSecurityOrigin& other)
{
    d = other.d;
    return *this;
}

QString QWebSecurityOrigin::scheme() const
{
    return d->origin->protocol();
}

QString QWebSecurityOrigin::host() const
{
    return d->origin->host();
}


int QWebSecurityOrigin::port() const
{
    return d->origin->port();
}

qint64 QWebSecurityOrigin::databaseUsage() const
{
    return DatabaseTracker::tracker().usageForOrigin(d->origin.get());
}

qint64 QWebSecurityOrigin::databaseQuota() const
{
    return DatabaseTracker::tracker().quotaForOrigin(d->origin.get());
}

// Sets the storage quota (in bytes)
// If the quota is set to a value lower than the current usage, that quota will "stick" but no data will be purged to meet the new quota.
// This will simply prevent new data from being added to databases in that origin
void QWebSecurityOrigin::setDatabaseQuota(qint64 quota)
{
    DatabaseTracker::tracker().setQuota(d->origin.get(), quota);
}

QWebSecurityOrigin::~QWebSecurityOrigin()
{
}

QWebSecurityOrigin::QWebSecurityOrigin(QWebSecurityOriginPrivate* priv)
{
    d = priv;
}

QList<QWebSecurityOrigin> QWebSecurityOrigin::allOrigins()
{
    Vector<RefPtr<SecurityOrigin> > coreOrigins;
    DatabaseTracker::tracker().origins(coreOrigins);
    QList<QWebSecurityOrigin> webOrigins;

    for (unsigned i = 0; i < coreOrigins.size(); ++i) {
        QWebSecurityOriginPrivate* priv = new QWebSecurityOriginPrivate(coreOrigins[i].get());
        webOrigins.append(priv);
    }
    return webOrigins;
}

QList<QWebDatabase> QWebSecurityOrigin::databases() const
{
    Vector<String> nameVector;
    QList<QWebDatabase> databases;
    if (!DatabaseTracker::tracker().databaseNamesForOrigin(d->origin.get(), nameVector))
        return databases;
    for (unsigned i = 0; i < nameVector.size(); ++i) {
        QWebDatabasePrivate* priv = new QWebDatabasePrivate();
        priv->name = nameVector[i];
        priv->origin = this->d->origin;
        QWebDatabase webDatabase(priv);
        databases.append(webDatabase);
    }
    return databases;
}

