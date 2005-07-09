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

#include <qmap.h>

#include "DOMString.h"
#include "Node.h"
#include <kdom/Shared.h>

#include "NBCImpl.h"

#include "PointerPartImpl.h"
#include "XPointerResultImpl.h"

using namespace KDOM;
using namespace KDOM::XPointer;

PointerPartImpl::PointerPartImpl(const DOMString &name, const DOMString &schemeData, NBCImpl *nbc)
: Shared(true), m_data(schemeData), m_name(name), m_nbc(nbc)
{
	kdDebug(26550) << "Created PointerPart: name=\"" << name << "\" schemeData=\"" << schemeData << "\"." << endl;

	if(m_nbc)
		m_nbc->ref();
}

PointerPartImpl::~PointerPartImpl()
{
	if(m_nbc)
		m_nbc->deref();
}

XPointerResultImpl *PointerPartImpl::evaluate(NodeImpl *) const
{
	/* Unknown schemes are plain PointerPartImpl instances. This ensures
	 * they trigger an evaluation of the next scheme. */
	return new XPointerResultImpl(XPointerResult::NO_MATCH);
}

NBCImpl *PointerPartImpl::nbc() const
{
	return m_nbc;
}

DOMString PointerPartImpl::name() const
{
	return m_name;
}

DOMString PointerPartImpl::data() const
{
	return m_data;
}

// vim:ts=4:noet
