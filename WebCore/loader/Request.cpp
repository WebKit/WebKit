/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

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
*/

#include "config.h"
#include "Request.h"

#include "CachedResource.h"

namespace WebCore {

Request::Request(DocLoader* docLoader, CachedResource* object, bool incremental, SecurityCheckPolicy shouldDoSecurityCheck, bool sendResourceLoadCallbacks)
    : m_object(object)
    , m_docLoader(docLoader)
    , m_incremental(incremental)
    , m_multipart(false)
    , m_shouldDoSecurityCheck(shouldDoSecurityCheck)
    , m_sendResourceLoadCallbacks(sendResourceLoadCallbacks)
{
    m_object->setRequest(this);
}

Request::~Request()
{
    m_object->setRequest(0);
}

} //namespace WebCore
