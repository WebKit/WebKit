/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 1998 Lars Knoll <knoll@kde.org>
    Copyright (C) 2001-2003 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2003 Apple Computer, Inc

    This file is part of the KDE project

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
*/

#include <assert.h>

#include <kdebug.h>
#include <kglobal.h>

#include <qimage.h>
#include <qbitmap.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qasyncimageio.h>

#include "IconData.h"
#include "KDOMCache.h"
#include "KDOMLoader.h"
#include "KDOMCacheHelper.h"
#include "KDOMCachedImage.moc"
#include "KDOMCachedObjectClient.h"

using namespace KDOM;

// All supported mimetypes.
static QString buildAcceptHeader()
{
	return QString::fromLatin1("image/png, image/jpeg, video/x-mng, image/jp2, image/gif;q=0.5,*/*;q=0.1");
}

CachedImage::CachedImage(DocumentLoader *docLoader, const DOMString &url, KIO::CacheControl cachePolicy, const char *) : QObject(), CachedObject(url, Image, cachePolicy, 0)
{
	static const QString &acceptHeader = KGlobal::staticQString(buildAcceptHeader());
	setAccept(acceptHeader);

	m_bg = 0;
	m_movie = 0;
	m_pixmap = 0;
	m_pixPart = 0;

	m_bgColor = qRgba(0, 0, 0, 0xFF);
	
	m_imgSource = 0;
	m_formatType = 0;

	m_width = 0;
	m_height = 0;

	m_status = Unknown;
	m_monochrome = false;
	m_typeChecked = false;
	m_isFullyTransparent = false;

	m_showAnimations = docLoader->showAnimations();

/* TODO: Add KHTML Settings class
	if(KHTMLFactory::defaultHTMLSettings()->isAdFiltered(url.string()))
*/
	if(false)
	{
		m_wasBlocked = true;
		if(!Cache::blockedPixmap)
		{
			Cache::blockedPixmap = new QPixmap();
			Cache::blockedPixmap->loadFromData(blocked_icon_data, blocked_icon_len);
		}
		
		CachedObject::finish();
	}
}

CachedImage::~CachedImage()
{
	clear();
}

const QPixmap &CachedImage::pixmap() const
{
	if(m_hadError)
		return *Cache::brokenPixmap;

	if(m_wasBlocked)
		return *Cache::blockedPixmap;

	if(m_movie)
	{
		if(m_movie->framePixmap().size() != m_movie->getValidRect().size())
		{
			// pixmap is not yet completely loaded, so we
			// return a clipped version. asserting here
			// that the valid rect is always from 0/0 to fullwidth/ someheight
			if(!m_pixPart)
				m_pixPart = new QPixmap();

			(*m_pixPart) = m_movie->framePixmap();

			if(m_movie->getValidRect().size().isValid())
				m_pixPart->resize(m_movie->getValidRect().size());
			else
				m_pixPart->resize(0, 0);
			
			return *m_pixPart;
		}
		else
			return m_movie->framePixmap();
	}
	else if(m_pixmap)
		return *m_pixmap;

	return *Cache::nullPixmap;
}

#define BGMINWIDTH 32
#define BGMINHEIGHT 32

#ifndef APPLE_COMPILE_HACK
const QPixmap &CachedImage::tiled_pixmap(const QColor &newc)
{
	static QRgb bgTransparant = qRgba(0, 0, 0, 0xFF);
	if((m_bgColor != bgTransparant) && (m_bgColor != newc.rgb()))
	{
		delete m_bg;
		m_bg = 0;
	}

	if(m_bg)
		return *m_bg;

	const QPixmap &r = pixmap();

	if(r.isNull())
		return r;

	// no error indication for background images
	if(m_hadError || m_wasBlocked)
		return *Cache::nullPixmap;

	QSize s(pixmap_size());
	bool isvalid = newc.isValid();
	
	int w = r.width();
	int h = r.height();
	if(w * h < 8192)
	{
		if(r.width() < BGMINWIDTH)
			w = ((BGMINWIDTH  / s.width()) + 1) * s.width();
			
		if(r.height() < BGMINHEIGHT)
			h = ((BGMINHEIGHT / s.height()) + 1) * s.height();
	}

#ifdef Q_WS_X11
	if(r.hasAlphaChannel() && ((w != r.width()) || (h != r.height())))
	{
		m_bg = new QPixmap(w, h);

		// Tile horizontally on the first stripe
		for(int x = 0; x < w; x += r.width())
			copyBlt(m_bg, x, 0, &r, 0, 0, r.width(), r.height());

		// Copy first stripe down
		for(int y = r.height(); y < h; y += r.height())
			copyBlt(m_bg, 0, y, m_bg, 0, 0, w, r.height());

		return *m_bg;
	}
#endif

	if(
#ifdef Q_WS_X11
	   !r.hasAlphaChannel() &&
#endif
	   ((w != r.width()) || (h != r.height()) || (isvalid && r.mask())))
	{
		QPixmap pix = r;
		if(w != r.width() || (isvalid && pix.mask()))
		{
			m_bg = new QPixmap(w, r.height());
			
			QPainter p(m_bg);
			if(isvalid)
				p.fillRect(0, 0, w, r.height(), newc);
				
			p.drawTiledPixmap(0, 0, w, r.height(), pix);
			p.end();
			
			if(!isvalid && pix.mask())
			{
				// unfortunately our anti-transparency trick doesn't work here
				// we need to create a mask.
				QBitmap newmask(w, r.height());
				
				QPainter pm(&newmask);
				pm.drawTiledPixmap(0, 0, w, r.height(), *pix.mask());
				
				m_bg->setMask(newmask);
				m_bgColor = bgTransparant;
			}
			else
				m_bgColor= newc.rgb();
			
			pix = *m_bg;
		}

		if(h != r.height())
		{
			delete m_bg;
			m_bg = new QPixmap(w, h);
			
			QPainter p(m_bg);
			if(isvalid)
				p.fillRect(0, 0, w, h, newc);
			
			p.drawTiledPixmap(0, 0, w, h, pix);
			if(!isvalid && pix.mask())
			{
				// unfortunately our anti-transparency trick doesn't work here
				// we need to create a mask.
				QBitmap newmask(w, h);
				
				QPainter pm(&newmask);				
				pm.drawTiledPixmap(0, 0, w, h, *pix.mask());
		
				m_bg->setMask(newmask);
				m_bgColor = bgTransparant;
			}
			else
				m_bgColor = newc.rgb();

		}
		
		return *m_bg;
	}

	return r;
}
#endif

QSize CachedImage::pixmap_size() const
{
	if(m_wasBlocked)
		return Cache::blockedPixmap->size();
		
	return (m_hadError ? Cache::brokenPixmap->size() :
			(m_movie ? m_movie->framePixmap().size() :
			(m_pixmap ? m_pixmap->size() : QSize())));
}


QRect CachedImage::valid_rect() const
{
	if(m_wasBlocked)
		return Cache::blockedPixmap->rect();
		
	return (m_hadError ? Cache::brokenPixmap->rect() :
			(m_movie ? m_movie->getValidRect() :
			(m_pixmap ? m_pixmap->rect() : QRect())));
}


void CachedImage::do_notify(const QPixmap &p, const QRect &r)
{
	for(QPtrDictIterator<CachedObjectClient> it(m_clients); it.current(); )
		it()->setPixmap(p, r, this);
}

void CachedImage::movieUpdated(const QRect &r)
{
#ifdef LOADER_DEBUG
	qDebug("movie updated %d/%d/%d/%d, pixmap size %d/%d", r.x(), r.y(), r.right(), r.bottom(),
														   m_movie->framePixmap().size().width(),
														   m_movie->framePixmap().size().height());
#endif

	do_notify(m_movie->framePixmap(), r);
}

#ifndef APPLE_COMPILE_HACK
void CachedImage::movieStatus(int status)
{
#ifdef LOADER_DEBUG
	qDebug("movieStatus(%d)", status);
#endif

	// ### the html image objects are supposed to send the load event after every frame (according to
	// netscape). We have a problem though where an image is present, and js code creates a new Image object,
	// which uses the same CachedImage, the one in the document is not supposed to be notified

	// just another Qt 2.2.0 bug. we cannot call
	// QMovie::frameImage if we're after QMovie::EndOfMovie
	if(status == QMovie::EndOfFrame)
	{
		const QImage &im = m_movie->frameImage();
		m_monochrome = ((im.depth() <= 8 ) && (im.numColors() - int(im.hasAlphaBuffer()) <= 2));

		for(int i = 0; m_monochrome && i < im.numColors(); ++i)
		{
			if(im.colorTable()[i] != qRgb(0xff, 0xff, 0xff) &&
			   im.colorTable()[i] != qRgb(0x00, 0x00, 0x00))
				m_monochrome = false;
		}
		
		if((im.width() < 5 || im.height() < 5) && im.hasAlphaBuffer()) // only evaluate for small images
		{
			QImage am = im.createAlphaMask();
			if(am.depth() == 1)
			{
				bool solid = false;
				for(int y = 0; y < am.height(); y++)
				{
					for(int x = 0; x < am.width(); x++)
					{
						if(am.pixelIndex(x, y))
						{
							solid = true;
							break;
						}
					}
				}
					
				m_isFullyTransparent = (!solid);
			}
		}

		// we have to delete our tiled bg variant here
		// because the frame has changed (in order to keep it in sync)
		delete m_bg;
		m_bg = 0;
	}
	
	if((status == QMovie::EndOfMovie && (!m_movie || m_movie->frameNumber() <= 1)) ||
	   ((status == QMovie::EndOfLoop) && (m_showAnimations == KDOMSettings::KAnimationLoopOnce)) ||
	   ((status == QMovie::EndOfFrame) && (m_showAnimations == KDOMSettings::KAnimationDisabled)))
	{
		if(m_imgSource)
		{
			setShowAnimations(KDOMSettings::KAnimationDisabled);

			// monochrome alphamasked images are usually about 10000 times
			// faster to draw, so this is worth the hack
			if(m_pixmap && m_monochrome && m_pixmap->depth() > 1)
			{
				QPixmap *pix = new QPixmap();
				pix->convertFromImage(m_pixmap->convertToImage().convertDepth(1), MonoOnly | AvoidDither);
				
				if(m_pixmap->mask())
					pix->setMask(*m_pixmap->mask());

				delete m_pixmap;
				m_pixmap= pix;

				m_monochrome = false;
			}
		}

		for(QPtrDictIterator<CachedObjectClient> it(m_clients); it.current();)
			it()->notifyFinished(this);
	}
}
#endif

void CachedImage::movieResize(const QSize & /* s */)
{
	do_notify(m_movie->framePixmap(), QRect());
}

void CachedImage::deleteMovie()
{
	delete m_movie;
	m_movie= 0;
}

#ifndef APPLE_COMPILE_HACK
void CachedImage::setShowAnimations(KDOMSettings::KAnimationAdvice showAnimations)
{
	m_showAnimations = showAnimations;
	
	if((m_showAnimations == KDOMSettings::KAnimationDisabled) && m_imgSource)
	{
		m_imgSource->cleanBuffer();
		
		delete m_pixmap;
		m_pixmap = new QPixmap(m_movie->framePixmap());

		m_movie->disconnectUpdate(this, SLOT(movieUpdated(const QRect &)));
		m_movie->disconnectStatus(this, SLOT(movieStatus(int)));
		m_movie->disconnectResize(this, SLOT(movieResize(const QSize &)));
		
		QTimer::singleShot(0, this, SLOT(deleteMovie()));

		m_imgSource = 0;
	}
#endif
}

void CachedImage::clear()
{
	delete m_bg; m_bg = 0;
	delete m_movie; m_movie = 0;
	delete m_pixmap; m_pixmap = 0;
	delete m_pixPart; m_pixPart = 0;
	
	m_bgColor = qRgba(0, 0, 0, 0xFF);
	
	m_formatType = 0;
	m_typeChecked = false;
	setSize(0);

	// No need to delete imageSource - QMovie does it for us
	m_imgSource = 0;
}

void CachedImage::ref(CachedObjectClient *consumer)
{
	CachedObject::ref(consumer);

	if(m_movie)
	{
		m_movie->unpause();
		if(m_movie->finished() || m_clients.count() == 1 )
			m_movie->restart();
	}

	// for mouseovers, dynamic changes
	if(m_status >= Persistent && !valid_rect().isNull())
	{
		consumer->setPixmap(pixmap(), valid_rect(), this);
		consumer->notifyFinished(this);
	}
}

void CachedImage::deref(CachedObjectClient *consumer)
{
	CachedObject::deref(consumer);
	if(m_movie && m_clients.isEmpty() && m_movie->running())
		m_movie->pause();
}

void CachedImage::data(QBuffer &buffer, bool eof)
#ifndef APPLE_COMPILE_HACK
{
#ifdef LOADER_DEBUG
	kdDebug( 6060 ) << this << "in CachedImage::data(buffersize " <<buffer.buffer().size() <<", eof=" << eof << endl;
#endif

	if(!m_typeChecked)
	{
		// don't attempt incremental loading if we have all the data already
		assert(!eof);

		m_formatType = QImageDecoder::formatName((const unsigned char *) buffer.buffer().data(), buffer.size());
		if(m_formatType && strcmp(m_formatType, "PNG") == 0)
			m_formatType = 0; // Some png files contain multiple images, we want to show only the first one

		m_typeChecked = true;

		if(m_formatType) // movie format exists
		{
			m_imgSource = new ImageSource(buffer.buffer());
			
			m_movie = new QMovie(m_imgSource, 8192);
			m_movie->connectUpdate(this, SLOT(movieUpdated(const QRect &)));
			m_movie->connectStatus(this, SLOT(movieStatus(int)));
			m_movie->connectResize(this, SLOT(movieResize(const QSize &)));
		}
	}

	if(m_imgSource)
	{
		m_imgSource->setEOF(eof);
		m_imgSource->maybeReady();
	}
	
	if(eof)
	{
		// QMovie currently doesn't support all kinds of image formats
		// so we need to use a QPixmap here when we finished loading the complete
		// picture and display it then all at once.
		if(m_typeChecked && !m_formatType)
		{
#ifdef CACHE_DEBUG
			kdDebug(6060) << "CachedImage::data(): reloading as pixmap:" << endl;
#endif

			m_pixmap = new QPixmap(buffer.buffer());

#ifdef CACHE_DEBUG
			kdDebug(6060) << "CachedImage::data(): image is null: " << m_pixmap->isNull() << endl;
#endif
			
			if(m_pixmap->isNull())
			{
				m_hadError = true;
				do_notify(pixmap(), QRect(0, 0, 16, 16)); // load "broken image" icon
			}
			else
				do_notify(*m_pixmap, m_pixmap->rect());

			for(QPtrDictIterator<CachedObjectClient> it(m_clients); it.current();)
				it()->notifyFinished(this);
		}
#else // APPLE_COMPILE_HACK
    bool canDraw = false;
    

	// Always attempt to load the image incrementally.
	if (!m_pixmap)
		m_pixmap = new QPixmap(KWQResponseMIMEType(m_response));
	canDraw = m_pixmap->receivedData(buffer.buffer(), eof, m_decoderCallback);
    
    // If we have a decoder, we'll be notified when decoding has completed.
    if (!m_decoderCallback) {
        if (canDraw || eof) {
            if (m_pixmap->isNull()) {
                errorOccured = true;
                QPixmap ep = pixmap();
                do_notify (ep, ep.rect());
                Cache::removeCacheEntry (this);
            }
            else
                do_notify(*m_pixmap, m_pixmap->rect());

            QSize s = pixmap_size();
            setSize(s.width() * s.height() * 2);
        }
        if (eof) {
            m_loading = false;
            checkNotify();
        }
    }
#endif // APPLE_COMPILE_HACK
	}
}

void CachedImage::finish()
{
	Status oldStatus = m_status;
	CachedObject::finish();
	
	if(oldStatus != m_status)
	{
		const QPixmap &pm = pixmap();
		do_notify(pm, pm.rect());
	}

	QSize s = pixmap_size();
	setSize(s.width() * s.height() * 2);
}

void CachedImage::error(int /* err */, const char * /* text */)
{
	clear();
	
	m_typeChecked = true;
	m_hadError = true;
	m_loading = false;
	
	do_notify(pixmap(), QRect(0, 0, 16, 16));
	
	for(QPtrDictIterator<CachedObjectClient> it( m_clients ); it.current();)
		it()->notifyFinished(this);
}

// vim:ts=4:noet
