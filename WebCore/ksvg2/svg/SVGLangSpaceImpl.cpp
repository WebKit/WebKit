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

#include <kdom/impl/domattrs.h>
#include <kdom/impl/AttrImpl.h>
#include <kdom/impl/DOMStringImpl.h>

#include "ksvg.h"
#include "svgattrs.h"
#include "SVGElementImpl.h"
#include "SVGLangSpaceImpl.h"

using namespace KSVG;

SVGLangSpaceImpl::SVGLangSpaceImpl()
{
	m_lang = 0;
	m_space = 0;
}

SVGLangSpaceImpl::~SVGLangSpaceImpl()
{
	if(m_lang)
		m_lang->deref();
	if(m_space)
		m_space->deref();
}

KDOM::DOMStringImpl *SVGLangSpaceImpl::xmlLang() const
{
	return m_lang;
}

void SVGLangSpaceImpl::setXmlLang(KDOM::DOMStringImpl *xmlLang)
{
	KDOM_SAFE_SET(m_lang, xmlLang);
}

KDOM::DOMStringImpl *SVGLangSpaceImpl::xmlSpace() const
{
	return m_space;
}

void SVGLangSpaceImpl::setXmlSpace(KDOM::DOMStringImpl *xmlSpace)
{
	KDOM_SAFE_SET(m_space, xmlSpace);
}

bool SVGLangSpaceImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
	int id = (attr->id() & NodeImpl_IdLocalMask);
	switch(id)
	{
		case ATTR_LANG:
		{
			if(attr->val())
				setXmlLang(attr->val()->copy());
			else
				setXmlLang(0);
			
			return true;
		}
		case ATTR_SPACE:
		{
			if(attr->val())
				setXmlSpace(attr->val()->copy());
			else
				setXmlSpace(0);
			
			return true;
		}
	};

	return false;
}

// vim:ts=4:noet
