/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004, 2006 Apple Computer, Inc.

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

#include "TransferJobClient.h"
#include <wtf/HashMap.h>
#include "DeprecatedPtrList.h"

#if __OBJC__
@class NSData;
@class NSURLResponse;
#else
class NSData;
class NSURLResponse;
#endif

namespace WebCore {

    class CachedObject;
    class DocLoader;
    class Request;
    class String;

    class Loader : TransferJobClient
    {
    public:
        Loader();
        ~Loader();

        void load(DocLoader*, CachedObject*, bool incremental = true);

        int numRequests(DocLoader*) const;
        void cancelRequests(DocLoader*);

        void removeBackgroundDecodingRequest(Request*);
        
        // may return 0
        TransferJob* jobForRequest(const String& URL) const;

    private:
        virtual void receivedResponse(TransferJob*, PlatformResponse);
        virtual void receivedData(TransferJob*, const char*, int);
        virtual void receivedAllData(TransferJob*, PlatformData);

        void servePendingRequests();

        DeprecatedPtrList<Request> m_requestsPending;
        typedef HashMap<TransferJob*, Request*> RequestMap;
        RequestMap m_requestsLoading;

        DeprecatedPtrList<Request> m_requestsBackgroundDecoding;
    };

}

#endif
