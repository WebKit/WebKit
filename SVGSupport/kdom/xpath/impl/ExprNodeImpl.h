/*
   	Copyright (C) 2004 Zack Rusin <zack@kde.org>
   	Copyright (C) 2004 Richard Moore <rich@kde.org>
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

#ifndef KDOM_XPath_ExprNodeImpl_H
#define KDOM_XPath_ExprNodeImpl_H

#include <qstring.h>
#include <qvaluelist.h>

#include <kdom/Shared.h>

namespace KDOM
{

namespace XPath
{
	class BooleanImpl;
	class ContextImpl;
	class NumberImpl;
	class StringImpl;
	class ValueImpl;

	/**
	 * The internal implementation of an AST expression node.
	 *
	 * @author Zack Rusin zack@kde.org
	 * @author Richard Moore rich@kde.org
	 * @author Frans Englich frans.englich@telia.com>
	 */
	class ExprNodeImpl : public Shared
	{
	public:

		/**
		 * For convenience, can 'QValueList<ExprNodeImpl *>::const_iterator, be
		 * written as 'ExprNodeImpl::const_iterator'.
		 */
		typedef QValueList<ExprNodeImpl *>::const_iterator const_iterator;

		ExprNodeImpl();
		virtual ~ExprNodeImpl();

		virtual ValueImpl *evaluate(ContextImpl *) const;
		virtual StringImpl *argString(ContextImpl *context, unsigned int idx) const;
		virtual NumberImpl *argNumber(ContextImpl *context, unsigned int idx) const;
		virtual BooleanImpl *argBoolean(ContextImpl *context, unsigned int idx) const;

		virtual void addArg(ExprNodeImpl *);
		int argCount() const;
		ExprNodeImpl *arg(unsigned int idx) const;

		virtual void refArgs();
		virtual void derefArgs();

		virtual QString dump() const;

	private:

	    QValueList<ExprNodeImpl *> m_args;

	};
};
};

#endif
// vim:ts=4:noet
