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

#include "config.h"
#include <kdom/core/domattrs.h>
#include <kdom/core/AttrImpl.h>
#include <kdom/core/DOMStringImpl.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGElementImpl.h"
#include "SVGLangSpaceImpl.h"

using namespace KSVG;

SVGLangSpaceImpl::SVGLangSpaceImpl()
{
}

SVGLangSpaceImpl::~SVGLangSpaceImpl()
{
}

const KDOM::AtomicString& SVGLangSpaceImpl::xmllang() const
{
    return m_lang;
}

void SVGLangSpaceImpl::setXmllang(const KDOM::AtomicString& xmlLang)
{
    m_lang = xmlLang;
}

const KDOM::AtomicString& SVGLangSpaceImpl::xmlspace() const
{
    return m_space;
}

void SVGLangSpaceImpl::setXmlspace(const KDOM::AtomicString& xmlSpace)
{
    m_space = xmlSpace;
}

bool SVGLangSpaceImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    if (attr->name() == SVGNames::langAttr)
    {
        setXmllang(attr->value());
        return true;
    }
    else if (attr->name() == SVGNames::spaceAttr)
    {
        setXmlspace(attr->value());
        return true;
    }

    return false;
}

// vim:ts=4:noet
