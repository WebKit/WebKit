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

#ifndef KDOM_XPointerResult_H
#define KDOM_XPointerResult_H

namespace KDOM
{
	class Node;

namespace XPointer
{
	class XPointerResultImpl;

	/**
	 * The XPointerResult interface represents the result of the evaluation of an XPointer expression 
	 * within the context of a particular node. Since evaluation of an XPointer expression can result 
	 * in various result types, this object makes it possible to discover and the type 
	 * and value of the result.
	 *
	 * @author Frans Englich <frans.englich@telia.com>
	 */
	class XPointerResult
	{
	public:

		/**
		 * An integer indicating what type of result this is.
		 */
		enum ResultType
		{

			/**
			 * The pointer matched no nodes.
			 */
			NO_MATCH = 1,

			/**
			 * The pointer identifed a single node.
			 */
			SINGLE_NODE = 2
		};

		XPointerResult();
		explicit XPointerResult(XPointerResultImpl *i);
		XPointerResult(const XPointerResult &other);
		virtual ~XPointerResult();

		XPointerResult &operator=(const XPointerResult &other);
		bool operator==(const XPointerResult &other) const;
		bool operator!=(const XPointerResult &other) const;

		/**
		 * The value of this single node result.
		 * 
		 * @throws TYPE_ERR when the result is not of type SINGLE_NODE
		 */
		Node singleNodeValue() const;
		
		/**
		 * A code representing the type of this result, 
		 * as defined by the type constants.
		 */
		ResultType resultType() const;

		KDOM_INTERNAL_BASE(XPointerResult)

	protected:
		XPointerResultImpl *d;
	};
};

};

#endif

// vim:ts=4:noet
