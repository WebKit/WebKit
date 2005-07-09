/*
    Copyright(C) 2004 Richard Moore <rich@kde.org>
    Copyright(C) 2005 Frans Englich <frans.englich@telia.com>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or(at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "DOMString.h"

#include "ContextImpl.h"
#include "StringImpl.h"
#include "ValueImpl.h"

#include "concat_fnxp1.h"

using namespace KDOM;
using namespace KDOM::XPath;

concatXP1FunctionCallImpl::concatXP1FunctionCallImpl() : FunctionCallImpl()
{
}

concatXP1FunctionCallImpl::~concatXP1FunctionCallImpl()
{
}

ValueImpl *concatXP1FunctionCallImpl::evaluate(ContextImpl *context) const
{
	DOMString res;

	for(int i = 0; i < argCount(); i++)
	{
		StringImpl *s = argString(context, i);
		res += s->value();
	}

	return new StringImpl(res);
}

QString concatXP1FunctionCallImpl::dump() const
{
	return QString::fromLatin1("concatXP1FunctionCallImpl");
}

// vim:ts=4:noet
