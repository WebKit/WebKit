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
#include "CachedImageCallback.h"
#include "CachedObjectClient.h"
#include "CachedObjectClientWalker.h"
#include "DocLoader.h"
#include "KWQLoader.h"
#include <qpixmap.h>

namespace WebCore {

CachedImage::CachedImage(DocLoader* dl, const DOMString &url, KIO::CacheControl _cachePolicy, time_t _expireDate)
    : QObject(), CachedObject(url, Image, _cachePolicy, _expireDate)
    , m_dataSize(0)
{
    p = 0;
    pixPart = 0;
    bg = 0;
    isFullyTransparent = false;
    errorOccured = false;
    monochrome = false;
    m_status = Unknown;
    m_loading = true;
    m_showAnimations = dl->showAnimations();
    if (QPixmap::shouldUseThreadedDecoding())
        m_decoderCallback = new CachedImageCallback(this);
    else
        m_decoderCallback = 0;
}

CachedImage::~CachedImage()
{
    clear();
}

void CachedImage::ref( CachedObjectClient *c )
{
    CachedObject::ref(c);

    // for mouseovers, dynamic changes
    if (!valid_rect().isNull())
        c->setPixmap( pixmap(), valid_rect(), this);

    if(!m_loading)
        c->notifyFinished(this);
}

void CachedImage::deref( CachedObjectClient *c )
{
    Cache::flush();
    CachedObject::deref(c);
    if (canDelete() && m_free)
        delete this;
}

const QPixmap &CachedImage::tiled_pixmap(const QColor& newc)
{
    return pixmap();
}

const QPixmap &CachedImage::pixmap( ) const
{
    if(errorOccured)
        return *Cache::brokenPixmap;

    if (p)
        return *p;

    return *Cache::nullPixmap;
}

IntSize CachedImage::pixmap_size() const
{
    return (p ? p->size() : IntSize());
}

IntRect CachedImage::valid_rect() const
{
    return (p ? p->rect() : IntRect());
}

void CachedImage::do_notify(const QPixmap& p, const IntRect& r)
{
    CachedObjectClientWalker w(m_clients);
    while (CachedObjectClient *c = w.next())
        c->setPixmap(p, r, this);
}

void CachedImage::setShowAnimations( KHTMLSettings::KAnimationAdvice showAnimations )
{
    m_showAnimations = showAnimations;
}

void CachedImage::clear()
{
    delete p;   p = 0;
    delete bg;  bg = 0;
    delete pixPart; pixPart = 0;

    setSize(0);

    if (m_decoderCallback) {
        m_decoderCallback->clear();
        m_decoderCallback->deref();
        m_decoderCallback = 0;
    }
}

void CachedImage::data(QBuffer& _buffer, bool eof)
{
#ifdef CACHE_DEBUG
    kdDebug( 6060 ) << this << "in CachedImage::data(buffersize " << _buffer.buffer().size() <<", eof=" << eof << endl;
#endif

    bool canDraw = false;
    
    m_dataSize = _buffer.size();
        
    // If we're at eof and don't have a pixmap yet, the data
    // must have arrived in one chunk.  This avoids the attempt
    // to perform incremental decoding.
    if (eof && !p) {
#if __APPLE__
        p = new QPixmap(_buffer.buffer(), KWQResponseMIMEType(m_response));
#endif
        if (m_decoderCallback)
            m_decoderCallback->notifyFinished();
        canDraw = true;
    } else {
        // Always attempt to load the image incrementally.
#if __APPLE__
        if (!p)
            p = new QPixmap(KWQResponseMIMEType(m_response));
#endif
        canDraw = p->receivedData(_buffer.buffer(), eof, m_decoderCallback);
    }
    
    // If we have a decoder, we'll be notified when decoding has completed.
    if (!m_decoderCallback) {
        if (canDraw || eof) {
            if (p->isNull()) {
                errorOccured = true;
                QPixmap ep = pixmap();
                do_notify(ep, ep.rect());
                Cache::remove(this);
            }
            else
                do_notify(*p, p->rect());

            IntSize s = pixmap_size();
            setSize(s.width() * s.height() * 2);
        }
        if (eof) {
            m_loading = false;
            checkNotify();
        }
    }
}

void CachedImage::error( int /*err*/, const char */*text*/ )
{
    clear();
    errorOccured = true;
    do_notify(pixmap(), IntRect(0, 0, 16, 16));
    m_loading = false;
    checkNotify();
}

void CachedImage::checkNotify()
{
    if(m_loading) return;

    CachedObjectClientWalker w(m_clients);
    while (CachedObjectClient *c = w.next()) {
        c->notifyFinished(this);
    }
}

}
