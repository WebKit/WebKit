/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
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

#ifndef LOADER_CACHED_OBJECT_CLIENT_H
#define LOADER_CACHED_OBJECT_CLIENT_H

#ifndef KHTML_NO_XBL
namespace XBL {
    class XBLDocumentImpl;
}
#endif

namespace WebCore {

    class CachedObject;
    class CachedImage;
    class DOMString;
    class Image;
    class IntRect;

    /**
     * @internal
     *
     * a client who wants to load stylesheets, images or scripts from the web has to
     * inherit from this class and overload one of the 3 functions
     *
     */
    class CachedObjectClient
    {
    public:
        virtual ~CachedObjectClient() { }

        // Called whenever a frame of an image changes (FIXME: not yet called for animating frames but will be
        // soon).  The rect represents the portion of the image that changed.  Clients that transform
        // the image should similarly transform the rect to determine the correct invalidation to perform.
        virtual void imageChanged(CachedImage*, const IntRect&) { };

        virtual void setStyleSheet(const DOMString& /*URL*/, const DOMString& /*sheet*/) { }

#ifndef KHTML_NO_XBL
        virtual void setXBLDocument(const DOMString& /*URL*/, XBL::XBLDocumentImpl*) { }
#endif

        virtual void notifyFinished(CachedObject*) { }
    };

}

#endif
