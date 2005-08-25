/*
 * This file is part of the KDE libraries
 *
 * Copyright (C) 2005 Frans Englich 	<frans.englich@telia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <kdebug.h>

#include <kdom/kdom.h>
#include <kdom/Helper.h>

#include "ElementImpl.h"
#include "DocumentImpl.h"

#include "kdomxpointer.h"

#include "ShortHandImpl.h"
#include "PointerPartImpl.h"
#include "XPointerResultImpl.h"
#include "XPointerExceptionImpl.h"

using namespace KDOM;
using namespace KDOM::XPointer;

ShortHandImpl::ShortHandImpl(DOMStringImpl *str)
: PointerPartImpl(str, str, 0) // shortHand pointers have no data nor NBC, set it to 0.
{
	if(!Helper::IsValidNCName(str))
	{
		kdWarning() << "\"" << str << "\" is not a valid short hand pointer. TODO DOMError." << endl;
		throw new XPointerExceptionImpl(INVALID_EXPRESSION_ERR);
	}
}

ShortHandImpl::~ShortHandImpl()
{
}

XPointerResultImpl *ShortHandImpl::evaluate(NodeImpl *context) const
{
	if(!context)
		return 0;

	NodeImpl *node = static_cast<DocumentImpl*>(context)->getElementById(data());

	if(!node)
		return new XPointerResultImpl(NO_MATCH);
		
	XPointerResultImpl *result = new XPointerResultImpl(SINGLE_NODE);
	result->setSingleNodeValue(node);
	return result;
}

// vim:ts=4:noet
