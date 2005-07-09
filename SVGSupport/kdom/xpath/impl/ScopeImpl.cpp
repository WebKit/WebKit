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

#include <qmap.h>
#include <qstring.h>

#include "DocumentImpl.h"
#include "ScopeImpl.h"
#include "XPathEvaluatorImpl.h"
#include "XPathNSResolverImpl.h"

using namespace KDOM;
using namespace KDOM::XPath;

ScopeImpl::ScopeImpl(XPathEvaluatorImpl *evaluator, XPathNSResolverImpl *ns, DocumentImpl *d)
					 : Shared(true), m_evaluator(evaluator), m_resolver(ns), m_doc(d)
{
	Q_ASSERT(m_resolver);
	Q_ASSERT(m_doc);
	m_resolver->ref();
	m_doc->ref();

	static_cast<DocumentImpl *>(m_evaluator)->ref();
}

ScopeImpl::~ScopeImpl()
{
	m_resolver->deref();
	m_doc->deref();
	static_cast<DocumentImpl *>(m_evaluator)->deref();
}

XPathEvaluatorImpl *ScopeImpl::evaluator() const
{
	return m_evaluator;
}

bool ScopeImpl::hasVariable(const QString &name) const
{
	return m_vars.contains(name);
}

void ScopeImpl::setVariable(const QString &name, ValueImpl *val)
{
	m_vars[name] = val;
}

ValueImpl *ScopeImpl::variable(const QString &name) const
{
	return m_vars[name];
}

XPathNSResolverImpl *ScopeImpl::nsResolver() const
{
	return m_resolver;
}

DocumentImpl *ScopeImpl::ownerDocument() const
{
	return m_doc;
}

// vim:ts=4:noet
