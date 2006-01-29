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

void CachedImage::ref(CachedObjectClient *c)
{
    CachedObject::ref(c);

    // for mouseovers, dynamic changes
    if (!decodedRect().isNull())
        c->imageChanged(this, decodedRect());

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

const Image &CachedImage::image() const
{
    if (m_errorOccurred)
        return *Cache::brokenImage;

    if (m_image)
        return *m_image;

    return *Cache::nullImage;
}

IntSize CachedImage::imageSize() const
{
    return (m_image ? m_image->size() : IntSize());
}

IntRect CachedImage::decodedRect() const
{
    return (m_image ? m_image->rect() : IntRect());
}

void CachedImage::notifyObservers(const IntRect& r)
{
    CachedObjectClientWalker w(m_clients);
    while (CachedObjectClient *c = w.next())
        c->imageChanged(this, r);
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

void CachedImage::data(QBuffer& _buffer, bool eof)
{
    bool canDraw = false;
    
    m_dataSize = _buffer.size();

    // If we're at eof and don't have a image yet, the data
    // must have arrived in one chunk.  This avoids the attempt
    // to perform incremental decoding.
    if (eof && !m_image) {
        m_image = new Image(_buffer.buffer(), KWQResponseMIMEType(m_response));
        canDraw = true;
    } else {
        // Always attempt to load the image incrementally.
        if (!m_image)
            m_image = new Image(KWQResponseMIMEType(m_response));
        canDraw = m_image->decode(_buffer.buffer(), eof);
    }
    
    // If we have a decoder, we'll be notified when decoding has completed.
    if (canDraw || eof) {
        if (m_image->isNull()) {
            m_errorOccurred = true;
            notifyObservers(image().rect());
            Cache::remove(this);
        }
        else
            notifyObservers(m_image->rect());

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
    notifyObservers(IntRect(0, 0, 16, 16));
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

}
