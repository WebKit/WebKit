/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
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

#ifndef CachedCSSStyleSheet_H
#define CachedCSSStyleSheet_H

#include "CachedObject.h"
#include "TextEncoding.h"
#include <wtf/Vector.h>

namespace WebCore
{
    class DocLoader;

    class CachedCSSStyleSheet : public CachedObject
    {
    public:
        CachedCSSStyleSheet(DocLoader*, const String& URL, KIO::CacheControl, time_t expireDate, const DeprecatedString& charset);
        CachedCSSStyleSheet(const String& URL, const DeprecatedString& stylesheetData);
        virtual ~CachedCSSStyleSheet();

        const String& sheet() const { return m_sheet; }

        virtual void ref(CachedObjectClient*);
        virtual void deref(CachedObjectClient*);

        virtual void setCharset(const DeprecatedString&);
        virtual void data(Vector<char>&, bool allDataReceived);
        virtual void error();

        virtual bool schedule() const { return true; }

        void checkNotify();

    protected:
        String m_sheet;
        TextEncoding m_encoding;
    };

}

#endif
