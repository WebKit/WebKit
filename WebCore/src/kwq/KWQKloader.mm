/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)

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

#undef CACHE_DEBUG
//#define CACHE_DEBUG
#include <assert.h>

#include "loader.h"
#ifdef APPLE_CHANGES
#include <kwqdebug.h>
#import <WebFoundation/WebFoundation.h>
#include <WCLoadProgress.h>
#include <external.h>
#endif /* APPLE_CHANGES */

// up to which size is a picture for sure cacheable
#define MAXCACHEABLE 40*1024
// default cache size
#define DEFCACHESIZE 512*1024

#include <qasyncio.h>
#include <qasyncimageio.h>
#include <qpainter.h>
#include <qbitmap.h>
#include <qmovie.h>

#include <kio/job.h>
#include <kio/jobclasses.h>
#include <kglobal.h>
#include <kimageio.h>
#include <kcharsets.h>
#include <kiconloader.h>
#include <scheduler.h>
#include <kdebug.h>
#include "khtml_factory.h"
#include "khtml_part.h"

#include "css/css_stylesheetimpl.h"
#include "xml/dom_docimpl.h"

using namespace khtml;
using namespace DOM;

#ifdef APPLE_CHANGES
static bool cacheDisabled;
#endif

#ifdef APPLE_CHANGES
WCIFLoadProgressMakeFunc WCIFLoadProgressMake;

void WCSetIFLoadProgressMakeFunc(WCIFLoadProgressMakeFunc func)
{
    WCIFLoadProgressMake = func;
}
#endif /* APPLE_CHANGES */

void CachedObject::finish()
{
    if( m_size > MAXCACHEABLE )
    {
        m_status = Uncacheable;
        //Cache::flush(true); // Force flush.
    }
    else
        m_status = Cached;
    KURL url(m_url.string());
    if (m_expireDate && url.protocol().startsWith("http"))
    {
        KIO::http_update_cache(url, false, m_expireDate);
#ifdef CACHE_DEBUG
        kdDebug(6060) << " Setting expire date for image "<<m_url.string()<<" to " << m_expireDate << endl;
#endif
    }
#ifdef CACHE_DEBUG
    else kdDebug(6060) << " No expire date for image "<<m_url.string()<<endl;
#endif
}

void CachedObject::setExpireDate(int _expireDate)
{
    // assert(_expireDate);
    if ( _expireDate != m_expireDate && (m_status == Uncacheable || m_status == Cached))
    {
        finish();
    }
    m_expireDate = _expireDate;
}

void CachedObject::setRequest(Request *_request)
{
    if ( _request && !m_request )
        m_status = Pending;
    m_request = _request;
    if (canDelete() && m_free)
        delete this;
}

// -------------------------------------------------------------------------------------------

CachedCSSStyleSheet::CachedCSSStyleSheet(DocLoader* dl, const DOMString &url, bool reload, int _expireDate, const QString& charset)
#if APPLE_CHANGES
    : CachedObject(dl, url, CSSStyleSheet, reload, _expireDate)
#else
    : CachedObject(url, CSSStyleSheet, reload, _expireDate)
#endif
{
    // It's css we want.
    setAccept( QString::fromLatin1("text/css") );
    // load the file
    Cache::loader()->load(dl, this, false);
    m_loading = true;
    bool b;
    if(!charset.isEmpty())
	m_codec = KGlobal::charsets()->codecForName(charset, b);
    else
        m_codec = QTextCodec::codecForMib(4); // latin-1
}

CachedCSSStyleSheet::~CachedCSSStyleSheet()
{
}

void CachedCSSStyleSheet::ref(CachedObjectClient *c)
{
    // make sure we don't get it twice...
    m_clients.remove(c);
    m_clients.append(c);

    if(!m_loading) c->setStyleSheet( m_url, m_sheet );
}

void CachedCSSStyleSheet::deref(CachedObjectClient *c)
{
    m_clients.remove(c);
    if ( canDelete() && m_free )
      delete this;
}

void CachedCSSStyleSheet::data( QBuffer &buffer, bool eof )
{
    if(!eof) return;
    buffer.close();
    m_size = buffer.buffer().size();
    QString data = m_codec->toUnicode( buffer.buffer().data(), m_size );
    m_sheet = DOMString(data);
    m_loading = false;

    checkNotify();
}

void CachedCSSStyleSheet::checkNotify()
{
    if(m_loading) return;

#ifdef CACHE_DEBUG
    kdDebug( 6060 ) << "CachedCSSStyleSheet:: finishedLoading " << m_url.string() << endl;
#endif

    CachedObjectClient *c;
    for ( c = m_clients.first(); c != 0; c = m_clients.next() )
        c->setStyleSheet( m_url, m_sheet );
}


void CachedCSSStyleSheet::error( int /*err*/, const char */*text*/ )
{
    m_loading = false;
    checkNotify();
}

// -------------------------------------------------------------------------------------------

CachedScript::CachedScript(DocLoader* dl, const DOMString &url, bool reload, int _expireDate, const QString& charset)
#if APPLE_CHANGES
    : CachedObject(dl, url, Script, reload, _expireDate)
#else
    : CachedObject(url, Script, reload, _expireDate)
#endif
{
    // It's javascript we want.
    // But some websites think their scripts are <some wrong mimetype here>
    // and refuse to serve them if we only accept application/x-javascript.
    setAccept( QString::fromLatin1("*/*") );
    // load the file
    Cache::loader()->load(dl, this, false);
    m_loading = true;
    bool b;
    if(!charset.isEmpty())
        m_codec = KGlobal::charsets()->codecForName(charset, b);
    else
	m_codec = QTextCodec::codecForMib(4); // latin-1
}

CachedScript::~CachedScript()
{
}

void CachedScript::ref(CachedObjectClient *c)
{
    // make sure we don't get it twice...
    m_clients.remove(c);
    m_clients.append(c);

    if(!m_loading) c->notifyFinished(this);
}

void CachedScript::deref(CachedObjectClient *c)
{
    m_clients.remove(c);
    if ( canDelete() && m_free )
      delete this;
}

void CachedScript::data( QBuffer &buffer, bool eof )
{
    if(!eof) return;
    buffer.close();
    m_size = buffer.buffer().size();
    QString data = m_codec->toUnicode( buffer.buffer().data(), m_size );
    m_script = DOMString(data);
    m_loading = false;
    checkNotify();
}

void CachedScript::checkNotify()
{
    if(m_loading) return;

    CachedObjectClient *c;
    for ( c = m_clients.first(); c != 0; c = m_clients.next() )
        c->notifyFinished(this);
}


void CachedScript::error( int /*err*/, const char */*text*/ )
{
    m_loading = false;
    checkNotify();
}

// ------------------------------------------------------------------------------------------

namespace khtml
{

    class ImageSource : public QDataSource
    {
    public:
        ImageSource(QByteArray buf);

        /**
         * Overload QDataSource::readyToSend() and returns the number
         * of bytes ready to send if not eof instead of returning -1.
         */
        int readyToSend();

        /*!
          Reads and sends a block of data.
        */
        void sendTo(QDataSink*, int count);

        /**
         * Sets the EOF state.
         */
        void setEOF( bool state );

        /*!
          KHTMLImageSource's is rewindable.
        */
        bool rewindable() const;

        /*!
          Enables rewinding.  No special action is taken.
        */
        void enableRewind(bool on);

        /*
          Calls reset() on the QIODevice.
        */
        void rewind();

        /*
          Indicates that the buffered data is no longer
          needed.
        */
        void cleanBuffer();

        QByteArray buffer;
        unsigned int pos;
    private:
        bool eof     : 1;
        bool rew     : 1;
        bool rewable : 1;
    };
}


/*!
  This Class defines the DataSource for incremental loading of images.
*/
ImageSource::ImageSource(QByteArray buf)
{
  buffer = buf;
  rew = false;
  pos = 0;
  eof = false;
  rewable = true;
}

/**
 * Overload QDataSource::readyToSend() and returns the number
 * of bytes ready to send if not eof instead of returning -1.
*/
int ImageSource::readyToSend()
{
    if(eof && pos == buffer.size())
        return -1;

    return  buffer.size() - pos;
}

/*!
  Reads and sends a block of data.
*/
void ImageSource::sendTo(QDataSink* sink, int n)
{
    sink->receive((const uchar*)&buffer.at(pos), n);

    pos += n;

    // buffer is no longer needed
    if(eof && pos == buffer.size() && !rewable)
    {
        buffer.resize(0);
        pos = 0;
    }
}

/**
 * Sets the EOF state.
 */
void ImageSource::setEOF( bool state )
{
    eof = state;
}

// ImageSource's is rewindable.
bool ImageSource::rewindable() const
{
    return rewable;
}

// Enables rewinding.  No special action is taken.
void ImageSource::enableRewind(bool on)
{
    rew = on;
}

// Calls reset() on the QIODevice.
void ImageSource::rewind()
{
    pos = 0;
    if (!rew) {
        QDataSource::rewind();
    } else
        ready();
}


void ImageSource::cleanBuffer()
{
    // if we need to be able to rewind, buffer is needed
    if(rew)
        return;

    rewable = false;

    // buffer is no longer needed
    if(eof && pos == buffer.size())
    {
        buffer.resize(0);
        pos = 0;
    }
}

static QString buildAcceptHeader()
{
    QString result = KImageIO::mimeTypes( KImageIO::Reading ).join(", ");
    if (result.right(2) == ", ")
        result = result.left(result.length()-2);
    return result;
}

// -------------------------------------------------------------------------------------

CachedImage::CachedImage(DocLoader* dl, const DOMString &url, bool reload, int _expireDate)
#if APPLE_CHANGES
    : QObject(), CachedObject(dl, url, Image, reload, _expireDate)
#else
    : QObject(), CachedObject(url, Image, reload, _expireDate)
#endif
{
    static const QString &acceptHeader = KGlobal::staticQString( buildAcceptHeader() );

    m = 0;
    p = 0;
    pixPart = 0;
    bg = 0;
    bgColor = qRgba( 0, 0, 0, 0xFF );
    typeChecked = false;
    isFullyTransparent = false;
    errorOccured = false;
    monochrome = false;
    formatType = 0;
    m_status = Unknown;
    m_size = 0;
    imgSource = 0;
    setAccept( acceptHeader );
}

CachedImage::~CachedImage()
{
    clear();
}

void CachedImage::ref( CachedObjectClient *c )
{
#ifdef CACHE_DEBUG
    kdDebug( 6060 ) << this << " CachedImage::ref(" << c << ") " << endl;
#endif

    // make sure we don't get it twice...
    m_clients.remove(c);
    m_clients.append(c);

    if( m ) {
        m->unpause();
        if( m->finished() )
            m->restart();
    }

    // for mouseovers, dynamic changes
    if ( m_status >= Persistent && !valid_rect().isNull() )
        c->setPixmap( pixmap(), valid_rect(), this);
}

void CachedImage::deref( CachedObjectClient *c )
{
#ifdef CACHE_DEBUG
    kdDebug( 6060 ) << this << " CachedImage::deref(" << c << ") " << endl;
#endif
    m_clients.remove( c );
    if(m && m_clients.isEmpty() && m->running())
        m->pause();

    if ( canDelete() && m_free )
        delete this;
}

#define BGMINWIDTH      32
#define BGMINHEIGHT     32

const QPixmap &CachedImage::tiled_pixmap(const QColor& newc)
{
    static QRgb bgTransparant = qRgba( 0, 0, 0, 0xFF );
    if ( (bgColor != bgTransparant) && (bgColor != newc.rgb()) ) {
        delete bg; bg = 0;
    }

    if (bg)
        return *bg;

    const QPixmap &r = pixmap();

    if (r.isNull()) return r;

    // no error indication for background images
    if(errorOccured) return *Cache::nullPixmap;

    bool isvalid = newc.isValid();
    QSize s(pixmap_size());
    int w = r.width();
    int h = r.height();
    if ( w*h < 8192 )
    {
        if ( r.width() < BGMINWIDTH )
            w = ((BGMINWIDTH  / s.width())+1) * s.width();
        if ( r.height() < BGMINHEIGHT )
            h = ((BGMINHEIGHT / s.height())+1) * s.height();
    }
    if ( (w != r.width()) || (h != r.height()) )
    {
        QPixmap pix = r;
        if ( w != r.width() )
        {
            bg = new QPixmap(w, r.height());
            QPainter p(bg);
            if(isvalid) p.fillRect(0, 0, w, r.height(), newc);
            p.drawTiledPixmap(0, 0, w, r.height(), pix);
            if(!isvalid && pix.mask())
            {
                // unfortunately our anti-transparency trick doesn't work here
                // we need to create a mask.
                QBitmap newmask(w, r.height());
                QPainter pm(&newmask);
                pm.drawTiledPixmap(0, 0, w, r.height(), *pix.mask());
                bg->setMask(newmask);
                bgColor = bgTransparant;
            }
            else
                bgColor= newc.rgb();
            pix = *bg;
        }
        if ( h != r.height() )
        {
            delete bg;
            bg = new QPixmap(w, h);
            QPainter p(bg);
            if(isvalid) p.fillRect(0, 0, w, h, newc);
            p.drawTiledPixmap(0, 0, w, h, pix);
            if(!isvalid && pix.mask())
            {
                // unfortunately our anti-transparency trick doesn't work here
                // we need to create a mask.
                QBitmap newmask(w, h);
                QPainter pm(&newmask);
                pm.drawTiledPixmap(0, 0, w, h, *pix.mask());
                bg->setMask(newmask);
                bgColor = bgTransparant;
            }
            else
                bgColor= newc.rgb();
        }
        return *bg;
    }

    return r;
}

const QPixmap &CachedImage::pixmap( ) const
{
    if(errorOccured)
        return *Cache::brokenPixmap;

    if(m)
    {
        if(m->framePixmap().size() != m->getValidRect().size() && m->getValidRect().size().isValid())
        {
            // pixmap is not yet completely loaded, so we
            // return a clipped version. asserting here
            // that the valid rect is always from 0/0 to fullwidth/ someheight
            if(!pixPart) pixPart = new QPixmap(m->getValidRect().size());

            (*pixPart) = m->framePixmap();
            pixPart->resize(m->getValidRect().size());
            return *pixPart;
        }
        else
            return m->framePixmap();
    }
    else if(p)
        return *p;

    return *Cache::nullPixmap;
}


QSize CachedImage::pixmap_size() const
{
    return (m ? m->framePixmap().size() : ( p ? p->size() : QSize()));
}


QRect CachedImage::valid_rect() const
{
    return m ? m->getValidRect() : ( p ? p->rect() : QRect());
}


void CachedImage::do_notify(const QPixmap& p, const QRect& r)
{
    CachedObjectClient *c;

    for ( c = m_clients.first(); c != 0; c = m_clients.next() ) {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "found a client to update: " << c << endl;
#endif
        c->setPixmap( p, r, this);
    }
}


void CachedImage::movieUpdated( const QRect& r )
{
#ifdef CACHE_DEBUG
    qDebug("movie updated %d/%d/%d/%d, pixmap size %d/%d", r.x(), r.y(), r.right(), r.bottom(),
           m->framePixmap().size().width(), m->framePixmap().size().height());
#endif

    do_notify(m->framePixmap(), r);
}

void CachedImage::movieStatus(int status)
{
#ifdef CACHE_DEBUG
    qDebug("movieStatus(%d)", status);
#endif

    // ### the html image objects are supposed to send the load event after every frame (according to
    // netscape). We have a problem though where an image is present, and js code creates a new Image object,
    // which uses the same CachedImage, the one in the document is not supposed to be notified

    // just another Qt 2.2.0 bug. we cannot call
    // QMovie::frameImage if we're after QMovie::EndOfMovie
    if(status == QMovie::EndOfFrame)
    {
        const QImage& im = m->frameImage();
#ifndef APPLE_CHANGES
        monochrome = ( ( im.depth() <= 8 ) && ( im.numColors() - int( im.hasAlphaBuffer() ) <= 2 ) );
#endif
        if(im.width() < 5 && im.height() < 5 && im.hasAlphaBuffer()) // only evaluate for small images
        {
            QImage am = im.createAlphaMask();
            if(am.depth() == 1)
            {
                bool solid = false;
                for(int y = 0; y < am.height(); y++)
                    for(int x = 0; x < am.width(); x++)
                        if(am.pixelIndex(x, y)) {
                            solid = true;
                            break;
                        }
                isFullyTransparent = (!solid);
            }
        }

        // we have to delete our tiled bg variant here
        // because the frame has changed (in order to keep it in sync)
        delete bg;
        bg = 0;
    }


#ifdef APPLE_CHANGES
    if (status == QMovie::EndOfMovie)
#else
    if((status == QMovie::EndOfMovie) ||
       ((status == QMovie::EndOfLoop) && (m_showAnimations == KHTMLSettings::KAnimationLoopOnce)) ||
       ((status == QMovie::EndOfFrame) && (m_showAnimations == KHTMLSettings::KAnimationDisabled))
      )
#endif
    {
#if 0
        // the movie has ended and it doesn't loop nor is it an animation,
        // so there is no need to keep the buffer in memory
        if(imgSource && (m->frameNumber() == 1))
#else
        // WABA: Throw away the movie when it gets to the end.
        // We might want to do a pause instead in some cases if there is
        // a chance that we want to play the movie again.
        if(imgSource)
#endif
        {
#ifndef APPLE_CHANGES
            setShowAnimations( KHTMLSettings::KAnimationDisabled );

            // monochrome alphamasked images are usually about 10000 times
            // faster to draw, so this is worth the hack
            if ( p && monochrome && p->depth() > 1 )
            {
                QPixmap* pix = new QPixmap;
                pix->convertFromImage( p->convertToImage().convertDepth( 1 ), MonoOnly|AvoidDither );
                if ( p->mask() )
                    pix->setMask( *p->mask() );
                delete p;
                p = pix;
                monochrome = false;
            }
#endif
        }

	CachedObjectClient *c;
        for ( c = m_clients.first(); c != 0; c = m_clients.next() )
            c->notifyFinished(this);
    }

    if((status == QMovie::EndOfFrame) || (status == QMovie::EndOfMovie))
    {
#ifdef CACHE_DEBUG
//        QRect r(valid_rect());
//        qDebug("movie Status frame update %d/%d/%d/%d, pixmap size %d/%d", r.x(), r.y(), r.right(), r.bottom(),
//               pixmap().size().width(), pixmap().size().height());
#endif
            do_notify(pixmap(), valid_rect());
    }
}

void CachedImage::movieResize(const QSize& /*s*/)
{
//    do_notify(m->framePixmap(), QRect());
}

#ifndef APPLE_CHANGES
void CachedImage::setShowAnimations( KHTMLSettings::KAnimationAdvice showAnimations )
{
    m_showAnimations = showAnimations;
    if ( (m_showAnimations == KHTMLSettings::KAnimationDisabled) && imgSource ) {
        imgSource->cleanBuffer();
        delete p;
        p = new QPixmap(m->framePixmap());
        m->disconnectUpdate( this, SLOT( movieUpdated( const QRect &) ));
        m->disconnectStatus( this, SLOT( movieStatus( int ) ));
        m->disconnectResize( this, SLOT( movieResize( const QSize& ) ) );
        QTimer::singleShot(0, this, SLOT( deleteMovie()));
        imgSource = 0;
    }
}
#endif

void CachedImage::deleteMovie()
{
    delete m; m = 0;
}

void CachedImage::clear()
{
    delete m;   m = 0;
    delete p;   p = 0;
    delete bg;  bg = 0;
    bgColor = qRgba( 0, 0, 0, 0xff );
    delete pixPart; pixPart = 0;

    formatType = 0;

    typeChecked = false;
    m_size = 0;

    // No need to delete imageSource - QMovie does it for us
    imgSource = 0;
}

void CachedImage::data ( QBuffer &_buffer, bool eof )
{
#if APPLE_CHANGES
    // FIXME!!!
    bool UseQPixmapForImageLoading = TRUE;
#endif /* APPLE_CHANGES */

#ifdef CACHE_DEBUG
    kdDebug( 6060 ) << this << "in CachedImage::data(buffersize " << _buffer.buffer().size() <<", eof=" << eof << endl;
#endif
    if ( !typeChecked )
    {
        formatType = QImageDecoder::formatName( (const uchar*)_buffer.buffer().data(), _buffer.size());
        typeChecked = true;

#if APPLE_CHANGES
        if ( !UseQPixmapForImageLoading && formatType )  // movie format exists
#else /* APPLE_CHANGES not defined */
        if ( formatType )  // movie format exists
#endif /* APPLE_CHANGES not defined */
        {
            imgSource = new ImageSource( _buffer.buffer());
            m = new QMovie( imgSource, 8192 );
            m->connectUpdate( this, SLOT( movieUpdated( const QRect &) ));
            m->connectStatus( this, SLOT( movieStatus(int)));
            m->connectResize( this, SLOT( movieResize( const QSize& ) ) );
        }
    }

    if ( imgSource )
    {
        imgSource->setEOF(eof);
        imgSource->maybeReady();
    }

    if(eof)
    {
        // QMovie currently doesn't support all kinds of image formats
        // so we need to use a QPixmap here when we finished loading the complete
        // picture and display it then all at once.
#if APPLE_CHANGES        
        // FIXME: this is a hack put in place until the QMovie stuff works right
        if(UseQPixmapForImageLoading || (typeChecked && !formatType))
#else /* APPLE_CHANGES not defined */
        if(typeChecked && !formatType)
#endif /* APPLE_CHANGES not defined */
        {
#ifdef CACHE_DEBUG
            kdDebug(6060) << "CachedImage::data(): reloading as pixmap:" << endl;
#endif
            p = new QPixmap( _buffer.buffer() );
            // set size of image.
#ifdef CACHE_DEBUG
            kdDebug(6060) << "CachedImage::data(): image is null: " << p->isNull() << endl;
#endif
                if(p->isNull())
                {
                    errorOccured = true;
                    do_notify(pixmap(), QRect(0, 0, 16, 16)); // load "broken image" icon
                }
                else
                    do_notify(*p, p->rect());
        }

        QSize s = pixmap_size();
        m_size = s.width() * s.height() * 2;
    }
}

void CachedImage::error( int /*err*/, const char */*text*/ )
{
#ifdef CACHE_DEBUG
    kdDebug(6060) << "CahcedImage::error" << endl;
#endif

    clear();
    typeChecked = true;
    errorOccured = true;
    do_notify(pixmap(), QRect(0, 0, 16, 16));
}

// ------------------------------------------------------------------------------------------

Request::Request(DocLoader* dl, CachedObject *_object, bool _incremental)
{
    object = _object;
    object->setRequest(this);
    incremental = _incremental;
    m_docLoader = dl;
}

Request::~Request()
{
    object->setRequest(0);
}

// ------------------------------------------------------------------------------------------

DocLoader::DocLoader(KHTMLPart* part, DocumentImpl* doc)
{
    m_reloading = false;
    m_expireDate = 0;
    m_bautoloadImages = true;
#ifndef APPLE_CHANGES
    m_showAnimations = KHTMLSettings::KAnimationEnabled;
#endif
    m_part = part;
    m_doc = doc;

#if APPLE_CHANGES
    Cache::init();
#endif
    Cache::docloader->append( this );
}

DocLoader::~DocLoader()
{
    Cache::docloader->remove( this );
}

void DocLoader::setExpireDate(int _expireDate)
{
    m_expireDate = _expireDate;
}

CachedImage *DocLoader::requestImage( const DOM::DOMString &url)
{
    KURL fullURL = m_doc->completeURL( url.string() );
    if ( m_part && m_part->onlyLocalReferences() && fullURL.protocol() != "file") return 0;

    if (m_reloading) {
        if (!m_reloadedURLs.contains(fullURL.url())) {
            CachedObject *existing = Cache::cache->find(fullURL.url());
            if (existing)
                Cache::removeCacheEntry(existing);
            m_reloadedURLs.append(fullURL.url());
            return Cache::requestImage(this, url, true, m_expireDate);
        }
    }

    CachedImage* ci = Cache::requestImage(this, url, false, m_expireDate);

    return ci;
}

CachedCSSStyleSheet *DocLoader::requestStyleSheet( const DOM::DOMString &url, const QString& charset)
{
//    KURL fullURL = url.string();
    KURL fullURL = m_doc->completeURL( url.string() );

    if ( m_part && m_part->onlyLocalReferences() && fullURL.protocol() != "file") return 0;

    if (m_reloading) {
        if (!m_reloadedURLs.contains(fullURL.url())) {
            CachedObject *existing = Cache::cache->find(fullURL.url());
            if (existing)
                Cache::removeCacheEntry(existing);
            m_reloadedURLs.append(fullURL.url());
            return Cache::requestStyleSheet(this, url, true,m_expireDate, charset);
        }
    }

    return Cache::requestStyleSheet(this, url, false,m_expireDate, charset);
}

CachedScript *DocLoader::requestScript( const DOM::DOMString &url, const QString& charset)
{
    KURL fullURL = m_doc->completeURL( url.string() );
//    KURL fullURL = url.string();
    if ( m_part && m_part->onlyLocalReferences() && fullURL.protocol() != "file") return 0;

    if (m_reloading) {
        if (!m_reloadedURLs.contains(fullURL.url())) {
            CachedObject *existing = Cache::cache->find(fullURL.url());
            if (existing)
                Cache::removeCacheEntry(existing);
            m_reloadedURLs.append(fullURL.url());
            return Cache::requestScript(this, url, true,m_expireDate, charset);
        }
    }

    return Cache::requestScript(this, url, false,m_expireDate, charset);
}

void DocLoader::setAutoloadImages( bool enable )
{
    if ( enable == m_bautoloadImages )
        return;

    m_bautoloadImages = enable;

    if ( !m_bautoloadImages ) return;

    for ( const CachedObject* co=m_docObjects.first(); co; co=m_docObjects.next() )
        if ( co->type() == CachedObject::Image )
        {
            CachedImage *img = const_cast<CachedImage*>( static_cast<const CachedImage *>( co ) );

            CachedObject::Status status = img->status();
            if ( status != CachedObject::Unknown )
                continue;

            Cache::loader()->load(this, img, true);
        }
}

void DocLoader::setReloading( bool enable )
{
    m_reloading = enable;
}

#ifndef APPLE_CHANGES
void DocLoader::setShowAnimations( KHTMLSettings::KAnimationAdvice showAnimations )
{
    if ( showAnimations == m_showAnimations ) return;
    m_showAnimations = showAnimations;

    const CachedObject* co;
    for ( co=m_docObjects.first(); co; co=m_docObjects.next() )
        if ( co->type() == CachedObject::Image )
        {
            CachedImage *img = const_cast<CachedImage*>( static_cast<const CachedImage *>( co ) );

            img->setShowAnimations( showAnimations );
        }
}
#endif

void DocLoader::removeCachedObject( CachedObject* o ) const
{
    m_docObjects.removeRef( o );
}

// ------------------------------------------------------------------------------------------

#if APPLE_CHANGES
// Class URLLoadClient ======================================================================

@implementation URLLoadClient

-(id)initWithLoader:(Loader *)loader dataSource: dataSource
{
    if ((self = [super init])) {
        m_loader = loader;
        m_dataSource = [dataSource retain];
        return self;
    }
    
    return nil;
}

- (void)dealloc
{
    [m_dataSource autorelease];
    [super dealloc];
}

- (void)IFURLHandleResourceDidBeginLoading:(IFURLHandle *)sender
{
    KWQDEBUGLEVEL(KWQ_LOG_LOADING, "dataSource = %p for URL %s\n", m_dataSource,
                  static_cast<KIO::TransferJob *>(
                  [[sender attributeForKey:IFURLHandleUserData] pointerValue])->url().url().latin1());
}

- (void)IFURLHandleResourceDidCancelLoading:(IFURLHandle *)sender
{
    id controller;
    void *userData;
    
    userData = [[sender attributeForKey:IFURLHandleUserData] pointerValue];
    
    KIO::TransferJob *job = static_cast<KIO::TransferJob *>(userData);
    QString urlString = job->url().url();

    [m_dataSource _removeURLHandle: job->handle()];
    
    KWQDEBUGLEVEL (KWQ_LOG_LOADING, "dataSource = %p for URL %s\n", m_dataSource, urlString.latin1());

    m_loader->slotFinished(job);
    
    IFLoadProgress *loadProgress = WCIFLoadProgressMake();
    loadProgress->totalToLoad = -1;
    loadProgress->bytesSoFar = -1;

    controller = [m_dataSource controller];
    [controller _receivedProgress: (IFLoadProgress *)loadProgress forResource: QSTRING_TO_NSSTRING(urlString) fromDataSource: m_dataSource];

    delete job;
}

- (void)IFURLHandleResourceDidFinishLoading:(IFURLHandle *)sender data: (NSData *)data
{
    id controller;
    void *userData;
    
    userData = [[sender attributeForKey:IFURLHandleUserData] pointerValue];
    
    KIO::TransferJob *job = static_cast<KIO::TransferJob *>(userData);
    QString urlString = job->url().url();

    [m_dataSource _removeURLHandle: job->handle()];
    
    KWQDEBUGLEVEL (KWQ_LOG_LOADING, "dataSource = %p for URL %s data at %p, length %d\n", m_dataSource, urlString.latin1(), data, [data length]);

    m_loader->slotFinished(job);
    
    IFLoadProgress *loadProgress = WCIFLoadProgressMake();
    loadProgress->totalToLoad = [data length];
    loadProgress->bytesSoFar = [data length];

    controller = [m_dataSource controller];
    [controller _receivedProgress: (IFLoadProgress *)loadProgress forResource: QSTRING_TO_NSSTRING(urlString) fromDataSource: m_dataSource];

    delete job;
}

- (void)IFURLHandle:(IFURLHandle *)sender resourceDataDidBecomeAvailable:(NSData *)data
{
    void *userData;
    
    userData = [[sender attributeForKey:IFURLHandleUserData] pointerValue];
    
    KIO::TransferJob *job = static_cast<KIO::TransferJob *>(userData);
    QString urlString = job->url().url();
    
    KWQDEBUGLEVEL (KWQ_LOG_LOADING, "dataSource = %p for URL %s data at %p, length %d\n", m_dataSource, urlString.latin1(), data, [data length]);

    m_loader->slotData(job, (const char *)[data bytes], [data length]);    

    id controller;

    IFLoadProgress *loadProgress = WCIFLoadProgressMake();
    loadProgress->totalToLoad = [sender contentLength];
    loadProgress->bytesSoFar = [sender contentLengthReceived];
    
    controller = [m_dataSource controller];
    [controller _receivedProgress: (IFLoadProgress *)loadProgress forResource: QSTRING_TO_NSSTRING(urlString) fromDataSource: m_dataSource];
}

- (void)IFURLHandle:(IFURLHandle *)sender resourceDidFailLoadingWithResult:(IFError *)result
{
    void *userData;
    
    userData = [[sender attributeForKey:IFURLHandleUserData] pointerValue];
    
    KIO::TransferJob *job = static_cast<KIO::TransferJob *>(userData);
    KWQDEBUGLEVEL (KWQ_LOG_LOADING, "dataSource = %p, result = %s, URL = %s\n", m_dataSource, [[result errorDescription] lossyCString], job->url().url().latin1());

    [m_dataSource _removeURLHandle: job->handle()];

    id <IFLoadHandler> controller = [m_dataSource controller];
    
    IFLoadProgress *loadProgress = WCIFLoadProgressMake();
    loadProgress->totalToLoad = [sender contentLength];
    loadProgress->bytesSoFar = [sender contentLengthReceived];

    m_loader->slotFinished(job);

    [controller _receivedError: result forResource: QSTRING_TO_NSSTRING(job->url().url()) partialProgress: loadProgress fromDataSource: m_dataSource];

    delete job;
}

- (void)IFURLHandle:(IFURLHandle *)sender didRedirectToURL:(NSURL *)url
{
    KWQDEBUGLEVEL (KWQ_LOG_LOADING, "url = %s\n", [[url absoluteString] cString]);
    [m_dataSource _part]->setBaseURL([[url absoluteString] cString]);
    
    [m_dataSource _setFinalURL: url];
    
    [[m_dataSource controller] serverRedirectTo: url forDataSource: m_dataSource];
}

@end
#endif

#if APPLE_CHANGES
namespace khtml {

class LoaderPrivate
{
friend class Loader;
public:
    LoaderPrivate(Loader *parent)
    {
        _parent = parent;
        //m_recv = [[URLLoadClient alloc] initWithLoader:parent dataSource: nil];
    } 
    
    ~LoaderPrivate()
    {
        //[m_recv autorelease];
    }       

private:
    Loader *_parent;
    //URLLoadClient *m_recv;
};

} // namespace khtml
#endif

Loader::Loader() : QObject()
{
    m_requestsPending.setAutoDelete( true );
    m_requestsLoading.setAutoDelete( true );
#if APPLE_CHANGES
    d = new LoaderPrivate(this);
#endif
}

Loader::~Loader()
{
#if APPLE_CHANGES
    delete d;
#endif
}


void Loader::load(DocLoader* dl, CachedObject *object, bool incremental = true)
{
    Request *req = new Request(dl, object, incremental);

#if APPLE_CHANGES
    id client;
    
    id dataSource = ((KHTMLPart *)(((DocLoader *)(object->loader()))->part()))->getDataSource();
    client = [[[URLLoadClient alloc] initWithLoader:this dataSource: dataSource] autorelease];
    req->client = client;
#endif

    m_requestsPending.append(req);
    
#ifndef APPLE_CHANGES
    emit requestStarted( req->m_docLoader, req->object );
#endif

    servePendingRequests();
}

void Loader::servePendingRequests()
{
  if ( m_requestsPending.count() == 0 )
      return;

  // get the first pending request
  Request *req = m_requestsPending.take(0);

#ifdef CACHE_DEBUG
  kdDebug( 6060 ) << "starting Loader url=" << req->object->url().string() << endl;
#endif

  KIO::TransferJob* job = KIO::get( req->object->url().string(), req->object->reload(), false /*no GUI*/);

#ifdef APPLE_CHANGES
  KWQDEBUGLEVEL (KWQ_LOG_LOADING, "Serving request for base %s, url %s\n", 
		  req->m_docLoader->part()->baseURL().url().latin1(), req->object->url().string().latin1());
  //job->begin(d->m_recv, job);
  job->begin((URLLoadClient *)req->client, job);
  [((URLLoadClient *)req->client)->m_dataSource _addURLHandle: job->handle()];
#else
  if (!req->object->accept().isEmpty())
      job->addMetaData("accept", req->object->accept());
  if ( req->m_docLoader )  {
      KURL r = req->m_docLoader->doc()->URL();
      if ( r.protocol().startsWith( "http" ) && r.path().isEmpty() )
          r.setPath( "/" );
 
      job->addMetaData("referrer", r.url());

      if (req->m_docLoader->part()->restored())
      {
//          kdDebug() << "USING NON-VALIDATING CACHE!"<< endl;
          job->addMetaData("cache","Cache");
      }

  }

  connect( job, SIGNAL( result( KIO::Job * ) ), this, SLOT( slotFinished( KIO::Job * ) ) );
  connect( job, SIGNAL( data( KIO::Job*, const QByteArray &)),
           SLOT( slotData( KIO::Job*, const QByteArray &)));

  if ( req->object->schedule() )
      KIO::Scheduler::scheduleJob( job );
#endif

  m_requestsLoading.insert(job, req);
}

void Loader::slotFinished( KIO::Job* job )
{
  Request *r = m_requestsLoading.take( job );
  KIO::TransferJob* j = static_cast<KIO::TransferJob*>(job);

  if ( !r )
    return;

  if (j->error() || j->isErrorPage())
  {
      kdDebug(6060) << "Loader::slotFinished, with error. job->error()= " << j->error() << " job->isErrorPage()=" << j->isErrorPage() << endl;
      r->object->error( job->error(), job->errorText().ascii() );
      emit requestFailed( r->m_docLoader, r->object );
  }
  else
  {
      r->object->data(r->m_buffer, true);
      emit requestDone( r->m_docLoader, r->object );
  }
    
#if APPLE_CHANGES
    NSString *urlString;
    urlString = [NSString stringWithCString:r->object->url().string().latin1()];
    if ([urlString hasSuffix:@"/"]) {
        urlString = [urlString substringToIndex:([urlString length] - 1)];
    }
    [[NSNotificationCenter defaultCenter] postNotificationName:urlString object:nil];
#endif

  r->object->finish();

#ifdef CACHE_DEBUG
  kdDebug( 6060 ) << "Loader:: JOB FINISHED " << r->object << ": " << r->object->url().string() << endl;
#endif

  delete r;
  servePendingRequests();
}

#ifdef APPLE_CHANGES
void Loader::slotData( KIO::Job*job, const char *data, int size )
#else /* APPLE_CHANGES not defined */
void Loader::slotData( KIO::Job*job, const QByteArray &data )
#endif /* APPLE_CHANGES not defined */
{
    Request *r = m_requestsLoading[job];
    if(!r) {
        kdDebug( 6060 ) << "got data for unknown request!" << endl;
        return;
    }

    if ( !r->m_buffer.isOpen() )
        r->m_buffer.open( IO_WriteOnly );

#ifdef APPLE_CHANGES
    r->m_buffer.writeBlock( data, size );
#else /* APPLE_CHANGES not defined */
    r->m_buffer.writeBlock( data.data(), data.size() );
#endif /* APPLE_CHANGES not defined */

    if(r->incremental)
        r->object->data( r->m_buffer, false );
}

int Loader::numRequests( DocLoader* dl ) const
{
    int res = 0;

    QPtrListIterator<Request> pIt( m_requestsPending );
    for (; pIt.current(); ++pIt )
        if ( pIt.current()->m_docLoader == dl )
            res++;

    QPtrDictIterator<Request> lIt( m_requestsLoading );
    for (; lIt.current(); ++lIt )
        if ( lIt.current()->m_docLoader == dl )
            res++;

    return res;
}

void Loader::cancelRequests( DocLoader* dl )
{
    //kdDebug( 6060 ) << "void Loader::cancelRequests()" << endl;
    //kdDebug( 6060 ) << "got " << m_requestsPending.count() << " pending requests" << endl;
    QPtrListIterator<Request> pIt( m_requestsPending );
    while ( pIt.current() )
    {
        if ( pIt.current()->m_docLoader == dl )
        {
            kdDebug( 6060 ) << "cancelling pending request for " << pIt.current()->object->url().string() << endl;
            //emit requestFailed( dl, pIt.current()->object );
            Cache::removeCacheEntry( pIt.current()->object );
            m_requestsPending.remove( pIt );
        }
        else
            ++pIt;
    }

    //kdDebug( 6060 ) << "got " << m_requestsLoading.count() << "loading requests" << endl;

    QPtrDictIterator<Request> lIt( m_requestsLoading );
    while ( lIt.current() )
    {
        if ( lIt.current()->m_docLoader == dl )
        {
            //kdDebug( 6060 ) << "cancelling loading request for " << lIt.current()->object->url().string() << endl;
            KIO::Job *job = static_cast<KIO::Job *>( lIt.currentKey() );
            Cache::removeCacheEntry( lIt.current()->object );
            m_requestsLoading.remove( lIt.currentKey() );
            job->kill();
            //emit requestFailed( dl, pIt.current()->object );
        }
        else
            ++lIt;
    }
}

KIO::Job *Loader::jobForRequest( const DOM::DOMString &url ) const
{
    QPtrDictIterator<Request> it( m_requestsLoading );

    for (; it.current(); ++it )
    {
        CachedObject *obj = it.current()->object;

        if ( obj && obj->url() == url )
            return static_cast<KIO::Job *>( it.currentKey() );
    }

    return 0;
}

// ----------------------------------------------------------------------------


QDict<CachedObject> *Cache::cache = 0;
QPtrList<DocLoader>* Cache::docloader = 0;
Cache::LRUList *Cache::lru = 0;
Loader *Cache::m_loader = 0;

int Cache::maxSize = DEFCACHESIZE;
int Cache::flushCount = 0;
int Cache::cacheSize = 0;

QPixmap *Cache::nullPixmap = 0;
QPixmap *Cache::brokenPixmap = 0;

void Cache::init()
{
    if ( !cache )
        cache = new QDict<CachedObject>(401, true);

    if ( !lru )
        lru = new LRUList;

    if ( !docloader )
        docloader = new QPtrList<DocLoader>;

    if ( !nullPixmap )
        nullPixmap = new QPixmap;

    if ( !brokenPixmap )
//        brokenPixmap = new QPixmap(KHTMLFactory::instance()->iconLoader()->loadIcon("file_broken", KIcon::FileSystem, 16, KIcon::DisabledState));
        brokenPixmap = new QPixmap(KHTMLFactory::instance()->iconLoader()->loadIcon("file_broken", KIcon::Desktop, 16, KIcon::DisabledState));

    if ( !m_loader )
        m_loader = new Loader();
}

void Cache::clear()
{
    if ( !cache ) return;
#ifdef CACHE_DEBUG
    kdDebug( 6060 ) << "Cache: CLEAR!" << endl;
    statistics();
#endif
    cache->setAutoDelete( true );
    delete cache; cache = 0;
    delete lru;   lru = 0;
    delete nullPixmap; nullPixmap = 0;
    delete brokenPixmap; brokenPixmap = 0;
    delete m_loader;   m_loader = 0;
    delete docloader; docloader = 0;
}

CachedImage *Cache::requestImage( DocLoader* dl, const DOMString & url, bool reload, int _expireDate )
{
    // this brings the _url to a standard form...
    KURL kurl;
    if ( dl )
        kurl = dl->m_doc->completeURL( url.string() );
    else
        kurl = url.string();

    CachedObject *o = 0;
    if (!reload)
        o = cache->find(kurl.url());
    if(!o)
    {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache: new: " << kurl.url() << endl;
#endif
        CachedImage *im = new CachedImage(dl, kurl.url(), reload, _expireDate);
        if ( dl && dl->autoloadImages() ) Cache::loader()->load(dl, im, true);
#ifdef APPLE_CHANGES
        if (cacheDisabled)
            im->setFree(true);
        else {
#endif
        cache->insert( kurl.url(), im );
        lru->prepend( kurl.url() );
        flush();
#ifdef APPLE_CHANGES
        }
#endif
        o = im;
    }

    o->setExpireDate(_expireDate);

    if(!o->type() == CachedObject::Image)
    {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache::Internal Error in requestImage url=" << kurl.url() << "!" << endl;
#endif
        return 0;
    }

#ifdef CACHE_DEBUG
    if( o->status() == CachedObject::Pending )
        kdDebug( 6060 ) << "Cache: loading in progress: " << kurl.url() << endl;
    else
        kdDebug( 6060 ) << "Cache: using cached: " << kurl.url() << ", status " << o->status() << endl;
#endif

    lru->touch( kurl.url() );
    if ( dl ) {
        dl->m_docObjects.remove( o );
#ifdef APPLE_CHANGES
        if (!cacheDisabled)
#endif
        dl->m_docObjects.append( o );
    }
    return static_cast<CachedImage *>(o);
}

CachedCSSStyleSheet *Cache::requestStyleSheet( DocLoader* dl, const DOMString & url, bool reload, int _expireDate, const QString& charset)
{
    // this brings the _url to a standard form...
    KURL kurl;
    if ( dl )
        kurl = dl->m_doc->completeURL( url.string() );
    else
        kurl = url.string();

    CachedObject *o = cache->find(kurl.url());
    if(!o)
    {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache: new: " << kurl.url() << endl;
#endif
        CachedCSSStyleSheet *sheet = new CachedCSSStyleSheet(dl, kurl.url(), reload, _expireDate, charset);
#ifdef APPLE_CHANGES
        if (cacheDisabled)
            sheet->setFree(true);
        else {
#endif
        cache->insert( kurl.url(), sheet );
        lru->prepend( kurl.url() );
        flush();
#ifdef APPLE_CHANGES
        }
#endif
        o = sheet;
    }

    o->setExpireDate(_expireDate);

    if(!o->type() == CachedObject::CSSStyleSheet)
    {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache::Internal Error in requestStyleSheet url=" << kurl.url() << "!" << endl;
#endif
        return 0;
    }

#ifdef CACHE_DEBUG
    if( o->status() == CachedObject::Pending )
        kdDebug( 6060 ) << "Cache: loading in progress: " << kurl.url() << endl;
    else
        kdDebug( 6060 ) << "Cache: using cached: " << kurl.url() << endl;
#endif

    lru->touch( kurl.url() );
    if ( dl ) {
        dl->m_docObjects.remove( o );
#ifdef APPLE_CHANGES
        if (!cacheDisabled)
#endif
        dl->m_docObjects.append( o );
    }
    return static_cast<CachedCSSStyleSheet *>(o);
}

CachedScript *Cache::requestScript( DocLoader* dl, const DOM::DOMString &url, bool reload, int _expireDate, const QString& charset)
{
    // this brings the _url to a standard form...
    KURL kurl;
    if ( dl )
        kurl = dl->m_doc->completeURL( url.string() );
    else
        kurl = url.string();

    CachedObject *o = cache->find(kurl.url());
    if(!o)
    {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache: new: " << kurl.url() << endl;
#endif
        CachedScript *script = new CachedScript(dl, kurl.url(), reload, _expireDate, charset);
#ifdef APPLE_CHANGES
        if (cacheDisabled)
            script->setFree(true);
        else {
#endif
        cache->insert( kurl.url(), script );
        lru->prepend( kurl.url() );
        flush();
#ifdef APPLE_CHANGES
        }
#endif
        o = script;
    }

    o->setExpireDate(_expireDate);

    if(!(o->type() == CachedObject::Script))
    {
#ifdef CACHE_DEBUG
        kdDebug( 6060 ) << "Cache::Internal Error in requestScript url=" << kurl.url() << "!" << endl;
#endif
        return 0;
    }

#ifdef CACHE_DEBUG
    if( o->status() == CachedObject::Pending )
        kdDebug( 6060 ) << "Cache: loading in progress: " << kurl.url() << endl;
    else
        kdDebug( 6060 ) << "Cache: using cached: " << kurl.url() << endl;
#endif

    lru->touch( kurl.url() );
    if ( dl ) {
        dl->m_docObjects.remove( o );
#ifdef APPLE_CHANGES
        if (!cacheDisabled)
#endif
        dl->m_docObjects.append( o );
    }
    return static_cast<CachedScript *>(o);
}

void Cache::flush(bool force)
{
    if (force)
       flushCount = 0;
    // Don't flush for every image.
    if (!lru || (lru->count() < (uint) flushCount))
       return;

    init();

#ifdef CACHE_DEBUG
    //statistics();
    kdDebug( 6060 ) << "Cache: flush()" << endl;
#endif

    int _cacheSize = 0;

    for ( QStringList::Iterator it = lru->fromLast(); it != lru->end(); )
    {
        QString url = *it;
        --it; // Update iterator, we might delete the current entry later on.
        CachedObject *o = cache->find( url );

#if APPLE_CHANGES
        if( !o ) {
            continue;
        }
#endif

        if( !o->canDelete() || o->status() == CachedObject::Persistent ) {
               continue; // image is still used or cached permanently
               // in this case don't count it for the size of the cache.
        }

        if( o->status() != CachedObject::Uncacheable )
        {
           _cacheSize += o->size();

           if( _cacheSize < maxSize )
               continue;
        }
        removeCacheEntry( o );
    }
    Cache::cacheSize = _cacheSize;

    flushCount = lru->count()+10; // Flush again when the cache has grown.
#ifdef CACHE_DEBUG
    //statistics();
#endif
}

void Cache::setSize( int bytes )
{
    maxSize = bytes;
    // may be we need to clear parts of the cache
    flushCount = 0;
    flush(true);
}

void Cache::statistics()
{
    CachedObject *o;
    // this function is for debugging purposes only
    init();

    int size = 0;
    int msize = 0;
    int movie = 0;
    int stylesheets = 0;
    QDictIterator<CachedObject> it(*cache);
    for(it.toFirst(); it.current(); ++it)
    {
        o = it.current();
        if(o->type() == CachedObject::Image)
        {
            CachedImage *im = static_cast<CachedImage *>(o);
            if(im->m != 0)
            {
                movie++;
                msize += im->size();
            }
        }
        else
        {
            if(o->type() == CachedObject::CSSStyleSheet)
                stylesheets++;

        }
        size += o->size();
    }
    size /= 1024;

    kdDebug( 6060 ) << "------------------------- image cache statistics -------------------" << endl;
    kdDebug( 6060 ) << "Number of items in cache: " << cache->count() << endl;
    kdDebug( 6060 ) << "Number of items in lru  : " << lru->count() << endl;
    kdDebug( 6060 ) << "Number of cached images: " << cache->count()-movie << endl;
    kdDebug( 6060 ) << "Number of cached movies: " << movie << endl;
    kdDebug( 6060 ) << "Number of cached stylesheets: " << stylesheets << endl;
    kdDebug( 6060 ) << "pixmaps:   allocated space approx. " << size << " kB" << endl;
    kdDebug( 6060 ) << "movies :   allocated space approx. " << msize/1024 << " kB" << endl;
    kdDebug( 6060 ) << "--------------------------------------------------------------------" << endl;
}

void Cache::removeCacheEntry( CachedObject *object )
{
  QString key = object->url().string();

  // this indicates the deref() method of CachedObject to delete itself when the reference counter
  // drops down to zero
  object->setFree( true );

  cache->remove( key );
  lru->remove( key );

  const DocLoader* dl;
  for ( dl=docloader->first(); dl; dl=docloader->next() )
      dl->removeCachedObject( object );

  if ( object->canDelete() )
     delete object;
}

#ifdef APPLE_CHANGES

Cache::Statistics Cache::getStatistics()
{
    Statistics stats;

    if (!cache)
        return stats;

    QDictIterator<CachedObject> i(*cache);
    for (i.toFirst(); i.current(); ++i) {
        CachedObject *o = i.current();
        switch (o->type()) {
            case CachedObject::Image:
                if (static_cast<CachedImage *>(o)->m) {
                    stats.movies.count++;
                    stats.movies.size += o->size();
                } else {
                    stats.images.count++;
                    stats.images.size += o->size();
                }
                break;

            case CachedObject::CSSStyleSheet:
                stats.styleSheets.count++;
                stats.styleSheets.size += o->size();
                break;

            case CachedObject::Script:
                stats.scripts.count++;
                stats.scripts.size += o->size();
                break;

            default:
                stats.other.count++;
                stats.other.size += o->size();
        }
    }
    
    return stats;
}

void Cache::flushAll()
{
    if (!cache)
        return;

    for (;;) {
        QDictIterator<CachedObject> i(*cache);
        CachedObject *o = i.toFirst();
        if (!o)
            break;
        removeCacheEntry(o);
    }
    cacheSize = 0;
}

void Cache::setCacheDisabled(bool disabled)
{
    cacheDisabled = disabled;
    if (disabled)
        flushAll();
}

#endif

#include "loader.moc"
