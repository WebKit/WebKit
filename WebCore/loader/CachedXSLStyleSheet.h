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

#ifndef KHTML_CachedXSLStyleSheet_h
#define KHTML_CachedXSLStyleSheet_h

#include "CachedObject.h"

#include <kio/global.h>
#include "String.h"

class QString;
class QBuffer;

namespace khtml
{
    class DocLoader;
    class Decoder;
    class CachedObjectClient;

#ifdef KHTML_XSLT
    class CachedXSLStyleSheet : public CachedObject
    {
public:
        CachedXSLStyleSheet(DocLoader* dl, const DOM::DOMString &url, KIO::CacheControl cachePolicy, time_t _expireDate);

        const DOM::DOMString& sheet() const { return m_sheet; }
        
        virtual void ref(CachedObjectClient *consumer);
        virtual void deref(CachedObjectClient *consumer);
        
        virtual void setCharset(const QString &chs);
        virtual void data(QBuffer &buffer, bool eof);
        virtual void error(int err, const char *text);
        
        virtual bool schedule() const { return true; }
        
        void checkNotify();
        
protected:
        DOM::DOMString m_sheet;
        RefPtr<Decoder> m_decoder;
    };

#endif

};

#endif
