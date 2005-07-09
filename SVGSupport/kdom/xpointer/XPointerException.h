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

#ifndef KDOM_XPointerException_H
#define KDOM_XPointerException_H

#include <kdom/Shared.h>

namespace KDOM
{

namespace XPointer
{
	class XPointerExceptionImpl;

	/**
	 * Exception for errors occuring in XPointer code.
	 *
	 * @author Frans Englich <frans.englich@telia.com>
	 * @see ExceptionCode
	 */
	class XPointerException
	{
	public:
		XPointerException();
		XPointerException(XPointerExceptionImpl *i);
		XPointerException(const XPointerException &other);
		virtual ~XPointerException();

		XPointerException &operator=(const XPointerException &other);
		bool operator==(const XPointerException &other) const;
		bool operator!=(const XPointerException &other) const;

		/**
		 * Contains a code specifying more specifically what was wrong.
		 *
		 * @see ExceptionCode
		 */
		unsigned short code() const;

		KDOM_INTERNAL_BASE(XPointerException)

	protected:
		XPointerExceptionImpl *d;
	};
};

};

#endif

// vim:ts=4:noet
