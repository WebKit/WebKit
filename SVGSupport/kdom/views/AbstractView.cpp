/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>

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
#include "AbstractView.h"
#include "DocumentView.h"
#include "AbstractViewImpl.h"

#include "ViewConstants.h"
#include "AbstractView.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin AbstractView::s_hashTable 2
 document	AbstractViewConstants::Document	DontDelete|ReadOnly
@end
*/

ValueImp *AbstractView::getValueProperty(ExecState *exec, int token) const
{
	// No exceptions thrown here, no need for KDOM_ENTER/LEAVE_SAFE

	switch(token)
	{
		case AbstractViewConstants::Document:
			return document().cache(exec);
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

AbstractView AbstractView::null;

AbstractView::AbstractView() : d(0)
{
}

AbstractView::AbstractView(AbstractViewImpl *i) : d(i)
{
	if(d)
		d->ref();
}

AbstractView::AbstractView(const AbstractView &other) : d(0)
{
	(*this) = other;
}

KDOM_IMPL_DTOR_ASSIGN_OP(AbstractView)

DocumentView AbstractView::document() const
{
	if(!d)
		return DocumentView::null;

	return DocumentView(d->document());
}

// vim:ts=4:noet
