/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "config.h"
#include "CSSImageValue.h"

#include "Cache.h"
#include "CachedImage.h"
#include "CSSValueKeywords.h"
#include "DocLoader.h"

namespace WebCore {

CSSImageValue::CSSImageValue(const String& url, StyleBase* style)
    : CSSPrimitiveValue(url, CSS_URI)
    , m_image(0)
    , m_accessedImage(false)
{
}

CSSImageValue::CSSImageValue()
    : CSSPrimitiveValue(CSS_VAL_NONE)
    , m_image(0)
    , m_accessedImage(true)
{
}

CSSImageValue::~CSSImageValue()
{
    if (m_image)
        m_image->deref(this);
}

CachedImage* CSSImageValue::image(DocLoader* loader)
{
    if (!m_accessedImage) {
        m_accessedImage = true;

        if (loader)
            m_image = loader->requestImage(getStringValue());
        else
            m_image = Cache::requestImage(0, getStringValue());
        
        if (m_image)
            m_image->ref(this);
    }
    
    return m_image;
}

}
