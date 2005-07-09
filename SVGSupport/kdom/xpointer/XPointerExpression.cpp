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

#include <qvaluelist.h>

#include "DOMString.h"
#include "Document.h"
#include "NodeImpl.h"
#include <kdom/Shared.h>

#include "XPointerExpression.h"
#include "XPointerResult.h"

#include "XPointerExpressionImpl.h"
#include "XPointerResultImpl.h"

using namespace KDOM;
using namespace KDOM::XPointer;

XPointerExpression XPointerExpression::null;

XPointerExpression::XPointerExpression() : d(0)
{
}

XPointerExpression::XPointerExpression(const XPointerExpression &other): d(0)
{
	(*this) = other;
}

XPointerExpression::XPointerExpression(XPointerExpressionImpl *i): d(i)
{
	if(d)
		d->ref();
}

KDOM_IMPL_DTOR_ASSIGN_OP(XPointerExpression)

DOMString XPointerExpression::string() const
{
	if(!d)
		return DOMString();

	return d->string();
}

XPointerResult XPointerExpression::evaluate() const
{
	if(!d)
		return XPointerResult::null;
	
	return XPointerResult(d->evaluate());
}

// vim:ts=4:noet
