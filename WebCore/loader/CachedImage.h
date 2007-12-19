/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

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

#ifndef CachedImage_h
#define CachedImage_h

#include "CachedResource.h"
#include "ImageObserver.h"
#include "IntRect.h"
#include <wtf/Vector.h>

namespace WebCore {

class DocLoader;
class Cache;
class Image;

class CachedImage : public CachedResource, public ImageObserver {
    friend class Cache;

public:
    CachedImage(DocLoader*, const String& url, bool forCache);
    CachedImage(Image*);
    virtual ~CachedImage();

    Image* image() const;

    bool canRender() const { return !errorOccurred() && imageSize().width() > 0 && imageSize().height() > 0; }

    // These are only used for SVGImage right now
    void setImageContainerSize(const IntSize&);
    bool usesImageContainerSize() const;
    bool imageHasRelativeWidth() const;
    bool imageHasRelativeHeight() const;
    
    IntSize imageSize() const;  // returns the size of the complete image
    IntRect imageRect() const;  // The size of the image.

    virtual void ref(CachedResourceClient*);
    
    virtual void allReferencesRemoved();
    virtual void destroyDecodedData();

    virtual void data(PassRefPtr<SharedBuffer> data, bool allDataReceived);
    virtual void error();

    virtual bool schedule() const { return true; }

    void checkNotify();
    
    virtual bool isImage() const { return true; }

    void clear();
    
    bool stillNeedsLoad() const { return !m_errorOccurred && m_status == Unknown && m_loading == false; }
    void load();

    // ImageObserver
    virtual void decodedSizeChanged(const Image* image, int delta);
    virtual void didDraw(const Image*);

    virtual bool shouldPauseAnimation(const Image*);
    virtual void animationAdvanced(const Image*);

private:
    void createImage();
    void notifyObservers();

    Image* m_image;
};

}

#endif
