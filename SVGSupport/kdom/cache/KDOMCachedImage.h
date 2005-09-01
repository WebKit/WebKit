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

#ifndef KDOM_CachedImage_H
#define KDOM_CachedImage_H

#include <qmovie.h>
#include <qpixmap.h>

#include <kdom/cache/KDOMLoader.h>
#include <kdom/cache/ImageSource.h>
#include <kdom/cache/KDOMCachedObject.h>

namespace KDOM
{
    class CachedImage : public QObject,
                        public CachedObject
    {
    Q_OBJECT
    public:
        CachedImage(DocumentLoader *docLoader, const DOMString &url, KIO::CacheControl cachePolicy, const char *accept);
        virtual ~CachedImage();

        const QPixmap &pixmap() const;
        const QPixmap &tiled_pixmap(const QColor &bg);

        QSize pixmap_size() const; // returns the size of the complete (i.e. when finished) loading
        QRect valid_rect() const; // returns the rectangle of pixmap that has been loaded already

        virtual void ref(CachedObjectClient *consumer);
        virtual void deref(CachedObjectClient *consumer);

        virtual void data(QBuffer &buffer, bool eof);
        virtual void error(int err, const char *text);

        // Helpers
        bool isTransparent() const { return m_isFullyTransparent; }
        bool isErrorImage() const { return m_hadError; }
        bool isBlockedImage() const { return m_wasBlocked; }
        
        const QString &suggestedFilename() const { return m_suggestedFilename; }
        void setSuggestedFilename( const QString& s ) { m_suggestedFilename = s;  }
        
        const QString &suggestedTitle() const { return m_suggestedFilename; }

        virtual bool schedule() const { return false; }
        virtual void finish();

        bool isLoaded() const { return !m_loading; }

        void setShowAnimations(KDOMSettings::KAnimationAdvice showAnimations);

    protected:
        void clear();
    
    private slots:
        /**
          * gets called, whenever a QMovie changes frame
          */
        void movieUpdated(const QRect &rect);
        void movieStatus(int status);
        void movieResize(const QSize &size);
        void deleteMovie();

    private:
        friend class Cache; // for statistics()
        
        void do_notify(const QPixmap &p, const QRect &r);

        QString m_suggestedFilename;

        QMovie *m_movie;
        QPixmap *m_pixmap;
        QPixmap *m_bg;
        QRgb m_bgColor;
        
        mutable QPixmap *m_pixPart;

        ImageSource *m_imgSource;
        const char *m_formatType; // Is the name of the movie format type

        int m_width;
        int m_height;
    
        // Is set if movie format type (incremental/animation) was checked
        bool m_monochrome : 1;
        bool m_typeChecked : 1;
        bool m_isFullyTransparent : 1;
        KDOMSettings::KAnimationAdvice m_showAnimations : 2;
    };
};

#endif

// vim:ts=4:noet
