/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#include <kdom/cache/KDOMLoader.h>

#include <kdom/css/cssvalues.h>
#include "DocumentImpl.h"
#include "CSSImageValueImpl.h"
#include "CSSStyleSheetImpl.h"

using namespace KDOM;

CSSImageValueImpl::CSSImageValueImpl(CDFInterface *interface)
: CSSPrimitiveValueImpl(interface, CSS_VAL_NONE), CachedObjectClient()
{
    m_image = 0;
}

CSSImageValueImpl::CSSImageValueImpl(CDFInterface *interface, const KURL &url, const StyleBaseImpl *style)
: CSSPrimitiveValueImpl(interface, DOMString(url.url()).handle(), CSS_URI), CachedObjectClient()
{
    DocumentLoader *docLoader = 0;
    const StyleBaseImpl *root = style;
    while(root->parent())
        root = root->parent();

    if(root->isCSSStyleSheet())
        docLoader = static_cast<const CSSStyleSheetImpl *>(root)->doc()->docLoader();

    m_image = docLoader->requestImage(url);
    if(m_image)
        m_image->ref(this);
}

CSSImageValueImpl::~CSSImageValueImpl()
{
    if(m_image)
        m_image->deref(this);
}

// vim:ts=4:noet
