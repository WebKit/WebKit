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

#include "Ecma.h"
#include "NodeImpl.h"
#include "DocumentStyle.h"
#include "StyleSheetList.h"
#include "DocumentStyleImpl.h"

#include "kdom/data/CSSConstants.h"
#include "DocumentStyle.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin DocumentStyle::s_hashTable 2
 styleSheets	DocumentStyleConstants::StyleSheets	DontDelete|ReadOnly
@end
*/

ValueImp *DocumentStyle::getValueProperty(ExecState *exec, int token) const
{
	switch(token)
	{
		case DocumentStyleConstants::StyleSheets:
			return safe_cache<StyleSheetList>(exec, styleSheets());
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
};

DocumentStyle DocumentStyle::null;

DocumentStyle::DocumentStyle() : d(0)
{
}

DocumentStyle::DocumentStyle(DocumentStyleImpl *i) : d(i)
{
}

DocumentStyle::DocumentStyle(const DocumentStyle &other) : d(0)
{
	(*this) = other;
}

DocumentStyle::~DocumentStyle()
{
}

DocumentStyle &DocumentStyle::operator=(const DocumentStyle &other)
{
	if(d != other.d)
		d = other.d;

	return *this;
}

DocumentStyle &DocumentStyle::operator=(DocumentStyleImpl *other)
{
	if(d != other)
		d = other;

	return *this;
}

StyleSheetList DocumentStyle::styleSheets() const
{
	if(!d)
		return StyleSheetList::null;

	return StyleSheetList(d->styleSheets());
}

// vim:ts=4:noet
