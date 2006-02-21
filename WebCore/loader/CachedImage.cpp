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
#include "CachedImage.h"

#include "Cache.h"
#include "CachedObjectClient.h"
#include "CachedObjectClientWalker.h"
#include "DocLoader.h"
#include "KWQLoader.h"
#include "Image.h"

namespace WebCore {

CachedImage::CachedImage(DocLoader* dl, const DOMString &url, KIO::CacheControl _cachePolicy, time_t _expireDate)
    : CachedObject(url, ImageResource, _cachePolicy, _expireDate)
    , m_dataSize(0)
{
    m_image = 0;
    m_errorOccurred = false;
    m_status = Unknown;
    m_loading = true;
    m_showAnimations = dl->showAnimations();
}

CachedImage::~CachedImage()
{
    clear();
}

void CachedImage::ref(CachedObjectClient* c)
{
    CachedObject::ref(c);

    if (!imageRect().isEmpty())
        c->imageChanged(this);

    if (!m_loading)
        c->notifyFinished(this);
}

void CachedImage::deref(CachedObjectClient *c)
{
    Cache::flush();
    CachedObject::deref(c);
    if (canDelete() && m_free)
        delete this;
}

Image* CachedImage::image() const
{
    if (m_errorOccurred)
        return Cache::brokenImage;

    if (m_image)
        return m_image;

    return Cache::nullImage;
}

IntSize CachedImage::imageSize() const
{
    return (m_image ? m_image->size() : IntSize());
}

IntRect CachedImage::imageRect() const
{
    return (m_image ? m_image->rect() : IntRect());
}

void CachedImage::notifyObservers()
{
    CachedObjectClientWalker w(m_clients);
    while (CachedObjectClient *c = w.next())
        c->imageChanged(this);
}

void CachedImage::setShowAnimations( KHTMLSettings::KAnimationAdvice showAnimations )
{
    m_showAnimations = showAnimations;
}

void CachedImage::clear()
{
    delete m_image;
    m_image = 0;
    setSize(0);
}

void CachedImage::data(ByteArray& data, bool eof)
{
    bool sizeAvailable = false;
    
    m_dataSize = data.size();

    // Create the image if it doesn't yet exist.
    if (!m_image)
        m_image = new Image(this, KWQResponseMIMEType(m_response) == "application/pdf");
        
    // Give all the data so far to the image object for processing.
    // It will not do anything now, but will delay decoding until queried for info (like size or specific image frames).
    sizeAvailable = m_image->setData(data, eof);

    // Go ahead and tell our observers to try to draw if we have either
    // received all the data or the size is known.  Each chunk from the
    // network causes observers to repaint, which will force that chunk
    // to decode.
    if (sizeAvailable || eof) {
        if (m_image->isNull()) {
            m_errorOccurred = true;
            notifyObservers();
            Cache::remove(this);
        }
        else
            notifyObservers();

        IntSize s = imageSize();
        setSize(s.width() * s.height() * 2); // This is really just a rough estimate of the decoded size.
    }
    if (eof) {
        m_loading = false;
        checkNotify();
    }
}

void CachedImage::error( int /*err*/, const char */*text*/ )
{
    clear();
    m_errorOccurred = true;
    notifyObservers();
    m_loading = false;
    checkNotify();
}

void CachedImage::checkNotify()
{
    if (m_loading)
        return;

    CachedObjectClientWalker w(m_clients);
    while (CachedObjectClient *c = w.next())
        c->notifyFinished(this);
}

bool CachedImage::shouldStopAnimation(const Image* image)
{
    if (image != m_image)
        return false;
    
    CachedObjectClientWalker w(m_clients);
    while (CachedObjectClient *c = w.next())
        if (c->willRenderImage(this))
            return false;
    
    return true;
}

void CachedImage::animationAdvanced(const Image* image)
{
    if (image == m_image)
        notifyObservers();
}

}
