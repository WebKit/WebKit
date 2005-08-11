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

#include "StyleSheetImpl.h"
#include "CSSStyleSheetImpl.h"
#include "StyleSheetListImpl.h"

using namespace KDOM;

StyleSheetListImpl::StyleSheetListImpl() : Shared(false)
{
}

StyleSheetListImpl::~StyleSheetListImpl()
{
	QPtrListIterator<StyleSheetImpl> it(styleSheets);
	for(; it.current(); ++it)
		it.current()->deref();
}

unsigned long StyleSheetListImpl::length() const
{
	// hack so implicit BODY stylesheets don't get counted here
	unsigned long l = 0;
	QPtrListIterator<StyleSheetImpl> it(styleSheets);
	for(; it.current(); ++it)
	{
		if(!it.current()->isCSSStyleSheet() || !static_cast<CSSStyleSheetImpl*>(it.current())->implicit())
			++l;
	}

	return l;
}

StyleSheetImpl *StyleSheetListImpl::item(unsigned long index) const
{
	unsigned long l = 0;
	QPtrListIterator<StyleSheetImpl> it(styleSheets);
	for(; it.current(); ++it)
	{
		if(!it.current()->isCSSStyleSheet() || !static_cast<CSSStyleSheetImpl *>(it.current())->implicit())
		{
			if(l == index)
				return it.current();

			++l;
		}
	}

	return 0;
}

void StyleSheetListImpl::add(StyleSheetImpl *s)
{
	if(!styleSheets.containsRef(s))
	{
		s->ref();
		styleSheets.append(s);
	}
}

void StyleSheetListImpl::remove(StyleSheetImpl *s)
{
	if(styleSheets.removeRef(s))
		s->deref();
}

// vim:ts=4:noet
