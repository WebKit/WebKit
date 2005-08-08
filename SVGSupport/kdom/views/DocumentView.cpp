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
#include "Document.h"
#include "DocumentView.h"
#include "AbstractView.h"
#include "DocumentImpl.h"
#include "DocumentViewImpl.h"

#include "ViewConstants.h"
#include "DocumentView.lut.h"
using namespace KDOM;
using namespace KJS;

/*
@begin DocumentView::s_hashTable 2
 defaultView	DocumentViewConstants::DefaultView	DontDelete|ReadOnly
@end
*/

ValueImp *DocumentView::getValueProperty(ExecState *exec, int token) const
{
	// No exceptions thrown here, no need for KDOM_ENTER/LEAVE_SAFE

	switch(token)
	{	
		case DocumentViewConstants::DefaultView:
			return defaultView().cache(exec);
		default:
			kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
	}

	return Undefined();
}

DocumentView DocumentView::null;

DocumentView::DocumentView() : d(0)
{
}

DocumentView::DocumentView(DocumentViewImpl *i) : d(i)
{
}

DocumentView::~DocumentView()
{
}

DocumentView &DocumentView::operator=(const DocumentView &other)
{
	if(d != other.d)
		d = other.d;

	return *this;
}

DocumentView &DocumentView::operator=(DocumentViewImpl *other)
{
	if(d != other)
		d = other;

	return *this;
}

AbstractView DocumentView::defaultView() const
{
	if(!d)
		return AbstractView::null;
	
	return AbstractView(d->defaultView());
}

// vim:ts=4:noet
