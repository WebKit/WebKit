/*
    Copyright (C) 2005 Frans Englich <frans.englich@telia.com>

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

#include "DOMString.h"

#include "XPathResultImpl.h"
#include "NodeImpl.h"

using namespace KDOM;

XPathResultImpl::XPathResultImpl() : Shared(true)
{
}

XPathResultImpl::~XPathResultImpl()
{
}

unsigned short XPathResultImpl::resultType() const
{
	return 0;
}

double XPathResultImpl::numberValue() const
{
	return 0;
}

DOMString XPathResultImpl::stringValue() const
{
	return DOMString();
}

bool XPathResultImpl::booleanValue() const
{
	return false;
}

NodeImpl *XPathResultImpl::singleNodeValue() const
{
	return 0;
}

bool XPathResultImpl::invalidIteratorState() const
{
	return false;
}

unsigned long XPathResultImpl::snapshotLength() const
{
	return 0;
}

NodeImpl *XPathResultImpl::iterateNext() const
{
	return 0;
}

NodeImpl *XPathResultImpl::snapshotItem(const unsigned long /*index*/) const
{
	return 0;
}
// vim:ts=4:noet
