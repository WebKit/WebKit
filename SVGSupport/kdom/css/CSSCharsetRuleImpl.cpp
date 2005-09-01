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

#include "DOMStringImpl.h"
#include "CSSCharsetRuleImpl.h"

using namespace KDOM;

CSSCharsetRuleImpl::CSSCharsetRuleImpl(StyleBaseImpl *parent) : CSSRuleImpl(parent)
{
    m_type = CHARSET_RULE;
    m_encoding = 0;
}

CSSCharsetRuleImpl::~CSSCharsetRuleImpl()
{
    if(m_encoding)
        m_encoding->deref();
}

DOMStringImpl *CSSCharsetRuleImpl::encoding() const
{
    return m_encoding;
}

void CSSCharsetRuleImpl::setEncoding(DOMStringImpl *encoding)
{
    KDOM_SAFE_SET(m_encoding, encoding);
}

// vim:ts=4:noet
