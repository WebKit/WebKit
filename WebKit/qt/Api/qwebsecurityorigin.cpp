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

/*!
    \class QWebSecurityOrigin
    \since 4.5
    \brief The QWebSecurityOrigin class defines a security boundary for web sites.

    QWebSecurityOrigin provides access to the security domains defined by web sites.
    An origin consists of a host name, a scheme, and a port number. Web sites with the same
    security origin can access each other's resources for client-side scripting or databases.

    ### diagram

    For example the site \c{http://www.example.com/my/page.html} is allowed to share the same
    database as \c{http://www.example.com/my/overview.html}, or access each other's
    documents when used in HTML frame sets and JavaScript. At the same time it prevents
    \c{http://www.malicious.com/evil.html} from accessing \c{http://www.example.com/}'s resources,
    because they are of a different security origin.

    QWebSecurity also provides access to all databases defined within a security origin.

    For more information refer to the
    \l{http://en.wikipedia.org/wiki/Same_origin_policy}{"Same origin policy" Wikipedia Article}.

    \sa QWebFrame::securityOrigin()
*/

/*!
    Constructs a security origin from \a other.
*/
QWebSecurityOrigin::QWebSecurityOrigin(const QWebSecurityOrigin& other) : d(other.d)
{
}

/*!
    Assigns the \a other security origin to this.
*/
QWebSecurityOrigin& QWebSecurityOrigin::operator=(const QWebSecurityOrigin& other)
{
    d = other.d;
    return *this;
}

/*!
    Returns the scheme defining the security origin.
*/
QString QWebSecurityOrigin::scheme() const
{
    return d->origin->protocol();
}

/*!
    Returns the host name defining the security origin.
*/
QString QWebSecurityOrigin::host() const
{
    return d->origin->host();
}

/*!
    Returns the port number defining the security origin.
*/
int QWebSecurityOrigin::port() const
{
    return d->origin->port();
}

/*!
    Returns the number of bytes all databases in the security origin
    use on the disk.
*/
qint64 QWebSecurityOrigin::databaseUsage() const
{
#if ENABLE(DATABASE)
    return DatabaseTracker::tracker().usageForOrigin(d->origin.get());
#else
    return 0;
#endif
}

/*!
    Returns the quota for the databases in the security origin.
*/
qint64 QWebSecurityOrigin::databaseQuota() const
{
#if ENABLE(DATABASE)
    return DatabaseTracker::tracker().quotaForOrigin(d->origin.get());
#else
    return 0;
#endif
}

/*!
    Sets the quota for the databases in the security origin to \a quota bytes.

    If the quota is set to a value less than the current usage, the quota will remain
    and no data will be purged to meet the new quota. However, no new data can be added
    to databases in this origin.
*/
void QWebSecurityOrigin::setDatabaseQuota(qint64 quota)
{
#if ENABLE(DATABASE)
    DatabaseTracker::tracker().setQuota(d->origin.get(), quota);
#endif
}

/*!
    Destroys the security origin.
*/
QWebSecurityOrigin::~QWebSecurityOrigin()
{
}

/*!
    \internal
*/
QWebSecurityOrigin::QWebSecurityOrigin(QWebSecurityOriginPrivate* priv)
{
    d = priv;
}

/*!
    Returns a list of all security origins with a database quota defined.
*/
QList<QWebSecurityOrigin> QWebSecurityOrigin::allOrigins()
{
    QList<QWebSecurityOrigin> webOrigins;

#if ENABLE(DATABASE)
    Vector<RefPtr<SecurityOrigin> > coreOrigins;
    DatabaseTracker::tracker().origins(coreOrigins);

    for (unsigned i = 0; i < coreOrigins.size(); ++i) {
        QWebSecurityOriginPrivate* priv = new QWebSecurityOriginPrivate(coreOrigins[i].get());
        webOrigins.append(priv);
    }
#endif

    return webOrigins;
}

/*!
    Returns a list of all databases defined in the security origin.
*/
QList<QWebDatabase> QWebSecurityOrigin::databases() const
{
    QList<QWebDatabase> databases;

#if ENABLE(DATABASE)
    Vector<String> nameVector;

    if (!DatabaseTracker::tracker().databaseNamesForOrigin(d->origin.get(), nameVector))
        return databases;
    for (unsigned i = 0; i < nameVector.size(); ++i) {
        QWebDatabasePrivate* priv = new QWebDatabasePrivate();
        priv->name = nameVector[i];
        priv->origin = this->d->origin;
        QWebDatabase webDatabase(priv);
        databases.append(webDatabase);
    }
#endif

    return databases;
}

