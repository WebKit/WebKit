/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

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
#ifndef DNSPREFETCHHELPER_H
#define DNSPREFETCHHELPER_H

#include <QObject>
#include <QCache>
#include <QHostInfo>
#include <QSet>
#include <QString>
#include <QTime>
#include "qwebsettings.h"

namespace WebCore {

    class DnsPrefetchHelper : public QObject {
        Q_OBJECT
    public:
        DnsPrefetchHelper() : QObject(), currentLookups(0) {};

    public slots:
        void lookup(QString hostname)
        {
            if (hostname.isEmpty())
                return; // this actually happens
            if (currentLookups >= 10)
                return; // do not launch more than 10 lookups at the same time

            QTime* entryTime = lookupCache.object(hostname);
            if (entryTime && entryTime->elapsed() > 300*1000) {
                // delete knowledge about lookup if it is already 300 seconds old
                lookupCache.remove(hostname);
            } else if (!entryTime) {
                // not in cache yet, can look it up
                QTime *tmpTime = new QTime();
                *tmpTime = QTime::currentTime();
                lookupCache.insert(hostname, tmpTime);
                currentLookups++;
                QHostInfo::lookupHost(hostname, this, SLOT(lookedUp(QHostInfo)));
            }
        }

        void lookedUp(const QHostInfo&)
        {
            // we do not cache the result, we throw it away.
            // we currently rely on the OS to cache the results. If it does not do that
            // then at least the ISP nameserver did it.
            currentLookups--;
        }

    protected:
        QCache<QString, QTime> lookupCache; // 100 entries
        int currentLookups;
    };


}

#endif // DNSPREFETCHHELPER_H
