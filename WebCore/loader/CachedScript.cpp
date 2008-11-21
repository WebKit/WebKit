/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#include "config.h"
#include "CachedScript.h"

#include "CachedResourceClient.h"
#include "CachedResourceClientWalker.h"
#include <wtf/Vector.h>

namespace WebCore {

CachedScript::CachedScript(const String& url, const String& charset)
    : CachedResource(url, Script)
    , m_encoding(charset)
    , m_decodedDataDeletionTimer(this, &CachedScript::decodedDataDeletionTimerFired)
{
    // It's javascript we want.
    // But some websites think their scripts are <some wrong mimetype here>
    // and refuse to serve them if we only accept application/x-javascript.
    setAccept("*/*");
    if (!m_encoding.isValid())
        m_encoding = Latin1Encoding();
}

CachedScript::~CachedScript()
{
}

void CachedScript::addClient(CachedResourceClient* c)
{
    CachedResource::addClient(c);
    if (!m_loading)
        c->notifyFinished(this);
}

void CachedScript::allClientsRemoved()
{
    destroyDecodedData();
    m_decodedDataDeletionTimer.stop();
}

void CachedScript::setEncoding(const String& chs)
{
    TextEncoding encoding(chs);
    if (encoding.isValid())
        m_encoding = encoding;
}

String CachedScript::encoding() const
{
    return m_encoding.name();
}

const String& CachedScript::script()
{
    if (!m_script && m_data) {
        m_script = m_encoding.decode(m_data->data(), encodedSize());
        setDecodedSize(m_script.length() * sizeof(UChar));
    }

    m_decodedDataDeletionTimer.startOneShot(0);
    return m_script;
}

void CachedScript::data(PassRefPtr<SharedBuffer> data, bool allDataReceived)
{
    if (!allDataReceived)
        return;

    m_data = data;
    setEncodedSize(m_data.get() ? m_data->size() : 0);
    m_loading = false;
    checkNotify();
}

void CachedScript::checkNotify()
{
    if (m_loading)
        return;

    CachedResourceClientWalker w(m_clients);
    while (CachedResourceClient* c = w.next())
        c->notifyFinished(this);
}

void CachedScript::error()
{
    m_loading = false;
    m_errorOccurred = true;
    checkNotify();
}

void CachedScript::destroyDecodedData()
{
    m_script = String();
    setDecodedSize(0);
}

void CachedScript::decodedDataDeletionTimerFired(Timer<CachedScript>*)
{
    destroyDecodedData();
}

} // namespace WebCore
