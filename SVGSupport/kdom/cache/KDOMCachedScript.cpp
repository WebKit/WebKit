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

#include <qtextcodec.h>

#include "KDOMCache.h"
#include "KDOMLoader.h"
#include "KDOMCachedScript.h"
#include "KDOMCachedObjectClient.h"

using namespace KDOM;

CachedScript::CachedScript(DocumentLoader *docLoader, const DOMString &url, KIO::CacheControl cachePolicy, const char *) : CachedObject(url, Script, cachePolicy, 0)
{
    // It's javascript we want.
    // But some websites think their scripts are <some wrong mimetype here>
    // and refuse to serve them if we only accept application/x-javascript.
    setAccept(QString::fromLatin1("*/*"));

    // load the file
    Cache::loader()->load(docLoader, this, false);
    m_loading = true;
}

void CachedScript::ref(CachedObjectClient *consumer)
{
    CachedObject::ref(consumer);
    if(!m_loading)
        consumer->notifyFinished(this);
}

void CachedScript::data(QBuffer &buffer, bool eof)
{
    if(!eof)
        return;
    
    buffer.close();
    setSize(buffer.buffer().size());

    QTextCodec *c = codecForBuffer(m_charset, buffer.buffer());
    QString data = c->toUnicode( buffer.buffer().data(), m_size );
    m_script = (data[0].unicode() == QChar::byteOrderMark) ? DOMString(data.mid(1)) : DOMString(data);
    m_loading = false;
    checkNotify();
}

void CachedScript::checkNotify()
{
    if(m_loading)
        return;

    for(QPtrDictIterator<CachedObjectClient> it(m_clients); it.current();)
        it()->notifyFinished(this);
}

void CachedScript::error(int, const char *)
{
    m_loading = false;
    checkNotify();
}

// vim:ts=4:noet
