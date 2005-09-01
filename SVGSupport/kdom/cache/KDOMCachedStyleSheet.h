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

#ifndef KDOM_CachedStyleSheet_H
#define KDOM_CachedStyleSheet_H

#include <kdom/cache/KDOMCachedObject.h>

namespace KDOM
{
    class CachedStyleSheet : public CachedObject
    {
    public:
        CachedStyleSheet(DocumentLoader *docLoader, const DOMString &url, KIO::CacheControl cachePolicy, const char *accept);
        CachedStyleSheet(const DOMString &url, const QString &stylesheet_data);

        const DOMString &sheet() const { return m_sheet; }

        virtual void ref(CachedObjectClient *consumer);

        virtual void data( QBuffer &buffer, bool eof );
        virtual void error( int err, const char *text );

        virtual bool schedule() const { return true; }
        void setCharset( const QString& charset ) { m_charset = charset; }

    protected:
        void checkNotify();

        DOMString m_sheet;
        QString m_charset;
        int m_err;
        QString m_errText;
    };
};

#endif

// vim:ts=4:noet
