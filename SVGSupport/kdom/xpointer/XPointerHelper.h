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

#ifndef KDOM_XPointerHelper_H
#define KDOM_XPointerHelper_H

namespace KDOM
{
	class DOMString;
	class Element;
	class Node;

namespace XPointer
{

	/**
	 * A utility class for XPointer related functions.
	 *
	 * @author Frans Englich <frans.englich@telia.com>
	 */
	class XPointerHelper
	{
	public:

		/**
		 * Performs XPointer escaping on @p string.
		 *
		 * An example:
		 * \code
		 * DOMString str("str'ing(to be^ escaped%))");
		 *
		 * DOMString result = XPointerHelper::EncodeSchemeData(str);
		 *
		 * // result == "str'ing^(to be^^ escaped%^)^)"
		 * \endcode
		 *
		 * All paranteses are escaped, even if they are balanced.
		 */
		static DOMString EncodeSchemeData(const DOMString &string);

		/**
		 * Removes any XPointer escaping of @pstring and returns a plain string.
		 *
		 * An example:
		 * \code
		 * DOMString str("str'ing^(to be^^ un-escaped%^)^)");
		 *
		 * DOMString result = XPointerHelper::DecodeSchemeData(str);
		 *
		 * // result == "str'ing(to be^ un-escaped%))"
		 * \endcode
		 *
		 * @throws INVALID_ENCODING when the circumflex(^) is used to
		 * escape anything but paranteses or itself.
		 */
		static DOMString DecodeSchemeData(const DOMString &string);

	};
};

};

#endif

// vim:ts=4:noet
