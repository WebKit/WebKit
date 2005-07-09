/*
    Copyright(C) 2004 Richard Moore <rich@kde.org>
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

#ifndef KDOM_XPath_ScopeImpl_H
#define KDOM_XPath_ScopeImpl_H

#include <qstring.h>
template<typename A, typename B> class QMap;

#include <kdom/Shared.h>

namespace KDOM
{
	class DocumentImpl;
	class XPathEvaluatorImpl;
	class XPathNSResolverImpl;

namespace XPath
{
	class ValueImpl;

	class ScopeImpl : public Shared
	{
	public:
		ScopeImpl(XPathEvaluatorImpl *evaluator, XPathNSResolverImpl *ns,
				  DocumentImpl *doc);
		virtual ~ScopeImpl();

		/**
		 * Returns true iff this scope has a variable 
		 * with the specified name.
		 */
		bool hasVariable(const QString &name) const;
		
		/**
		 * Returns the value of the variable in this 
		 * scope with the specified name.
		 */
		ValueImpl *variable(const QString &name) const;
		
		/**
		 * Sets the value of the named variable in 
		 * this scope, creating it if necessary.
		 */
		void setVariable(const QString &name, ValueImpl *val);
		
		/**
		 * Returns the evaluator this scope is working with.
		 */
		XPathEvaluatorImpl *evaluator() const;
		// TODO We don't need this. Right?

		XPathNSResolverImpl *nsResolver() const;

		DocumentImpl *ownerDocument() const;

	private:

		XPathEvaluatorImpl *m_evaluator;
		XPathNSResolverImpl *m_resolver;
		DocumentImpl *m_doc;
		QMap<QString, ValueImpl *> m_vars;
	};
};
};

#endif
// vim:ts=4:noet
