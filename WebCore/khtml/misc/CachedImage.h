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

#ifndef KHTML_CachedImage_h
#define KHTML_CachedImage_h

#include "CachedObject.h"

#include <qobject.h>
#include <qpixmap.h>
#include <qbuffer.h>

#include <kio/global.h>

namespace khtml
{
    class DocLoader;
    class CachedImageCallback;
    class Cache;
    class CachedObjectClient;

    class CachedImage : public QObject, public CachedObject
    {
    public:
	CachedImage(DocLoader* dl, const DOM::DOMString &url, KIO::CacheControl cachePolicy, time_t _expireDate);
	virtual ~CachedImage();

	const QPixmap &pixmap() const;
	const QPixmap &tiled_pixmap(const QColor& bg);

        QSize pixmap_size() const;    // returns the size of the complete (i.e. when finished) loading
        QRect valid_rect() const;     // returns the rectangle of pixmap that has been loaded already

        void ref(CachedObjectClient *consumer);
	virtual void deref(CachedObjectClient *consumer);

	virtual void data( QBuffer &buffer, bool eof );
	virtual void error( int err, const char *text );

        bool isTransparent() const { return isFullyTransparent; }
        bool isErrorImage() const { return errorOccured; }

        void setShowAnimations( KHTMLSettings::KAnimationAdvice );

        virtual bool schedule() const { return true; }

	void checkNotify();
        
        virtual bool isImage() const { return true; }

	void clear();
        
    private:
        void do_notify(const QPixmap& p, const QRect& r);

        QPixmap* p;
	QPixmap* bg;
        QRgb bgColor;
        mutable QPixmap* pixPart;

	int width;
	int height;

	// Is set if movie format type ( incremental/animation) was checked
	bool typeChecked : 1;
        bool isFullyTransparent : 1;
        bool errorOccured : 1;
        bool monochrome : 1;
        KHTMLSettings::KAnimationAdvice m_showAnimations : 2;

        friend class Cache;

    public:
        int dataSize() const { return m_dataSize; }
	CachedImageCallback *decoderCallback() const { return m_decoderCallback; }
    private:
        friend class CachedImageCallback;
        
        int m_dataSize;
        CachedImageCallback *m_decoderCallback;
    };
};

#endif
