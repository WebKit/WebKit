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

#ifndef KDOM_kxpointer_H
#define KDOM_kxpointer_H

namespace KDOM
{

/**
 * @brief An implementation of XPointer.
 *
 * \section conformance Conformance
 *
 * The following is supported:
 *
 * <ul>
 * 	<li>The XPointer Framework</li>
 * 	<li>Shorthand pointers</li>
 * 	<li>The xmlns() scheme</li>
 * 	<li>The element() scheme</li>
 * </ul>
 *
 * The following is not supported:
 * <ul>
 * 	<li>The xpointer() scheme</li>
 * 	<li>Any 3rd party schemes.</li>
 * </ul>
 *
 * \section usage Usage
 *
 * Using XPointers is done via the DOM interface Document, inheriting from XPointerEvaluator(), which
 * contains the entry points for XPointer use.
 *
 * @see XPointerEvaluator
 *
 * \section reading Related Reading:
 *
 * <ul>
 * <li>An EBNF grammar of the xpointer() scheme:
 * http://xpointerlib.mozdev.org/xpointerGrammar.html</li>
 *
 * <li>Mozilla's XPointer Lib:
 * http://xpointerlib.mozdev.org/</li>
 *
 * <li>An IDL For Mozilla's XPointer interface:
 * http://www.mozdev.org/source/browse/xpointerlib/src/idl/nsIXPointerService.idl?rev=1.6&content-type=text/x-cvsweb-markup</li>
 *
 * <li>Listings of some 3rdparty schemes: http://simonstl.com/ietf/.</li>
 * </ul>
 *
 *
 * @author Frans Englich <frans.englich@telia.com>
 */
namespace XPointer
{

	/**
	 * An integer indicating the type of error generated.
	 */
	enum ExceptionCode
	{
		/**
		 * An XPointer was syntactically invalid.
		 */
		INVALID_EXPRESSION_ERR = 1,

		/**
		 * Thrown by the XPointerResultImpl's functions when requested
		 * result type and contained result doesn't matc.
		 */
		TYPE_ERR = 3,

		/**
		 * The circumflex(^) were used for invalid escaping. This is a subset of
		 * syntax error.
		 */
		INVALID_ENCODING = 4
	};
};
};

#endif

// vim:ts=4:noet
