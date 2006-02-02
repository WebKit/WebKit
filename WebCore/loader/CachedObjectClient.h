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

        // Called whenever a frame of an image changes, either because we got more data from the network or
        // because we are animating.
        virtual void imageChanged(CachedImage*) { };
        
        // Called to find out if this client wants to actually display the image.  Used to tell when we
        // can halt animation.  Content nodes that hold image refs for example would not render the image,
        // but RenderImages would (assuming they have visibility: visible and their render tree isn't hidden
        // e.g., in the b/f cache or in a background tab).
        virtual bool willRenderImage(CachedImage*) { return false; }

        virtual void setStyleSheet(const DOMString& /*URL*/, const DOMString& /*sheet*/) { }

#ifndef KHTML_NO_XBL
        virtual void setXBLDocument(const DOMString& /*URL*/, XBL::XBLDocumentImpl*) { }
#endif

        virtual void notifyFinished(CachedObject*) { }
    };

}

#endif
