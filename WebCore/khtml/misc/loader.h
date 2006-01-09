/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004 Apple Computer, Inc.

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#ifndef KHTML_Loader_h
#define KHTML_Loader_h

#include <qptrlist.h>
#include <qobject.h>
#include <qptrdict.h>

#include <kurl.h>
#include <kio/global.h>

#include <dom/dom_string.h>
#include "Request.h"

namespace KIO {
    class Job;
}

class KWQLoader;

#if __OBJC__
@class NSData;
@class NSURLResponse;
#else
class NSData;
class NSURLResponse;
#endif


namespace khtml
{
    class CachedObject;
    class DocLoader;
    class Decoder;

    class Loader : public QObject
    {
    public:	
	Loader();
	~Loader();

	void load(DocLoader* dl, CachedObject *object, bool incremental = true);

        int numRequests( DocLoader* dl ) const;
        void cancelRequests( DocLoader* dl );

	void removeBackgroundDecodingRequest (Request *r);
	
        // may return 0L
        KIO::Job *jobForRequest( const DOM::DOMString &url ) const;

        KWQLoader *kwq;

    signals:
	friend class CachedImageCallback;

        void requestStarted( khtml::DocLoader* dl, khtml::CachedObject* obj );
	void requestDone( khtml::DocLoader* dl, khtml::CachedObject *obj );
	void requestFailed( khtml::DocLoader* dl, khtml::CachedObject *obj );

    protected slots:
        void slotFinished( KIO::Job * , NSData *allData);
	void slotData( KIO::Job *, const char *data, int size );
        void slotReceivedResponse ( KIO::Job *, NSURLResponse *response );

    private:
	void servePendingRequests();

        virtual bool isKHTMLLoader() const;

	QPtrList<Request> m_requestsPending;
	QPtrDict<Request> m_requestsLoading;

	QPtrList<Request> m_requestsBackgroundDecoding;
    };

};

#endif
