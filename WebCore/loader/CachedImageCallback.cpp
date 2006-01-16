/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

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

#include "config.h"
#include "CachedImageCallback.h"

#include "Cache.h"
#include "CachedImage.h"
#include "Request.h"
#include "loader.h"
#include <qpixmap.h>

namespace WebCore {

void CachedImageCallback::notifyUpdate() 
{ 
    if (cachedImage) {
        cachedImage->do_notify(cachedImage->pixmap(), cachedImage->pixmap().rect()); 
        IntSize s = cachedImage->pixmap_size();
        cachedImage->setSize(s.width() * s.height() * 2);

        // After receiving the image header we are guaranteed to know
        // the image size.  Although all of the data may not have arrived or
        // been decoded we can consider the image loaded for purposed of
        // layout and dispatching the image's onload handler.  Removing the request from
        // the list of background decoding requests will ensure that Loader::numRequests() 
        // does not count this background request.  Further numRequests() can
        // be correctly used by the frame to determine if loading is sufficiently
        // complete to dispatch the page's onload handler.
        Request *r = cachedImage->m_request;
        DocLoader *dl = r->m_docLoader;

        Cache::loader()->removeBackgroundDecodingRequest(r);

        // Poke the frame to get it to do a checkCompleted().  Only do this for
        // the first update to minimize work.  Note that we are guaranteed to have
        // read the header when we received this first update, which is triggered
        // by the first kCGImageStatusIncomplete status from CG. kCGImageStatusIncomplete
        // really means that the CG decoder is waiting for more data, but has already
        // read the header.
        if (!headerReceived) {
            emit Cache::loader()->requestDone( dl, cachedImage );
            headerReceived = true;
        }
    }
}

void CachedImageCallback::notifyFinished()
{
    if (cachedImage) {
        cachedImage->do_notify(cachedImage->pixmap(), cachedImage->pixmap().rect()); 
        cachedImage->m_loading = false;
        cachedImage->checkNotify();
        IntSize s = cachedImage->pixmap_size();
        cachedImage->setSize(s.width() * s.height() * 2);

        Request *r = cachedImage->m_request;
        DocLoader *dl = r->m_docLoader;

        Cache::loader()->removeBackgroundDecodingRequest(r);

        // Poke the frame to get it to do a checkCompleted().
        emit Cache::loader()->requestDone( dl, cachedImage );
        
        delete r;
    }
}

void CachedImageCallback::notifyDecodingError()
{
    if (cachedImage) {
        handleError();
    }
}

void CachedImageCallback::handleError()
{
    if (cachedImage) {
        cachedImage->errorOccured = true;
        QPixmap ep = cachedImage->pixmap();
        cachedImage->do_notify(ep, ep.rect());
        Cache::remove(cachedImage);

        clear();
    }
}

void CachedImageCallback::clear() 
{
    if (cachedImage && cachedImage->m_request) {
        Request *r = cachedImage->m_request;
        DocLoader *dl = r->m_docLoader;

        Cache::loader()->removeBackgroundDecodingRequest(r);

        // Poke the frame to get it to do a checkCompleted().
        emit Cache::loader()->requestFailed( dl, cachedImage );

        delete r;
    }
    cachedImage = 0;
}

}
