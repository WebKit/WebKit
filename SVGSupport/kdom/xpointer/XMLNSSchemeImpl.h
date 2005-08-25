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

#ifndef KDOM_XPointer_XMLNSSchemeImpl_H
#define KDOM_XPointer_XMLNSSchemeImpl_H

#include <kdom/xpointer/impl/NBCImpl.h>

namespace KDOM
{

namespace XPointer
{
	class NBC;

	/**
	 * An implementation of the xmlns() scheme: http://www.w3.org/TR/xptr-xmlns/
	 *
	 * @author Frans Englich <frans.englich@telia.com>
	 */
	class XMLNSSchemeImpl : public NBCImpl
	{
	public:
		XMLNSSchemeImpl(DOMStringImpl *schemeData, NBCImpl *nbc);
		virtual ~XMLNSSchemeImpl();
	};
};

};

#endif

// vim:ts=4:noet
