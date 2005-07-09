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

#include "Node.h"
#include "kdomxpointer.h"

#include "XPointerExceptionImpl.h"
#include "XPointerResultImpl.h"

using namespace KDOM;
using namespace KDOM::XPointer;
	
XPointerResultImpl::XPointerResultImpl(const XPointerResult::ResultType code)
: Shared(true), m_single(0), m_resultType(code)
{
}

XPointerResultImpl::~XPointerResultImpl()
{
}

XPointerResult::ResultType XPointerResultImpl::resultType() const
{
	return m_resultType;
}

void XPointerResultImpl::setResultType(const XPointerResult::ResultType code)
{
	m_resultType = code;
}

NodeImpl *XPointerResultImpl::singleNodeValue() const
{
	if(resultType() != XPointerResult::SINGLE_NODE)
		throw new XPointerExceptionImpl(TYPE_ERR);

	Q_ASSERT(m_single);
	return m_single;
}

void XPointerResultImpl::setSingleNodeValue(NodeImpl *node)
{
	Q_ASSERT(node);
	m_single = node;
}

// vim:ts=4:noet
