/* This file is part of the KDE project
 *
 * Copyright (C) 2000 Wynn Wilkes <wynnw@caldera.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef KJAVADOWNLOADER_H
#define KJAVADOWNLOADER_H

#include <qobject.h>
#include <kio/jobclasses.h>

/**
 * @short A class for handling downloads from KIO
 *
 * This class handles a KIO::get job and passes the data
 * back to the AppletServer.
 *
 * @author Wynn Wilkes, wynnw@calderasystems.com
 */


class KJavaDownloaderPrivate;
class KJavaDownloader : public QObject
{
Q_OBJECT

public:
	KJavaDownloader( QString& ID, QString& url );
	~KJavaDownloader();

protected slots:
    void slotData( KIO::Job*, const QByteArray& );
    void slotResult( KIO::Job* );

private:
    KJavaDownloaderPrivate* d;

};

#endif
