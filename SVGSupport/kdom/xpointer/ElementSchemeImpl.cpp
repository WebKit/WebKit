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

#include <qstring.h>
#include <qstringlist.h>

#include <kdebug.h>

#include "DOMString.h"
#include "kdom.h"
#include "NodeImpl.h"
#include "DocumentImpl.h"
#include "NodeListImpl.h"

#include "XPointerResult.h"
#include "kdomxpointer.h"

#include "ElementSchemeImpl.h"
#include "ShortHandImpl.h"
#include "XPointerExceptionImpl.h"
#include "XPointerResultImpl.h"

using namespace KDOM;
using namespace KDOM::XPointer;

ElementSchemeImpl::ElementSchemeImpl(const DOMString &schemeData)
: PointerPartImpl("element", schemeData, 0), m_shorthand(0)
{
	const QChar *unicode = schemeData.unicode();
	const QChar ch = unicode[0];

	QString qSchemeData(unicode, schemeData.length());

	/* In either case is this wrong. */
	if(qSchemeData.endsWith(QString::fromLatin1("/")))
		throw new XPointerExceptionImpl(INVALID_EXPRESSION_ERR);

	if(ch.isLetter() || ch == '_') /* We have an NCName; a ShortHand pointer */
	{
		const int startSeq = schemeData.find('/', 0);
		
		if(startSeq == -1)
			m_shorthand = new ShortHandImpl(schemeData);
		else
			m_shorthand = new ShortHandImpl(schemeData.string().left(startSeq));

		m_shorthand->ref();
		if(startSeq != -1)
		{ /* We have a child sequence too */
			parseChildSequence(QStringList::split('/', schemeData.string().mid(startSeq)));
			return;
		}
	}
	else if(ch == '/') /* We have only a child sequence. */
	{
		parseChildSequence(QStringList::split('/', schemeData.string()));

		if(m_childSequence.first() != 1)
		{
			kdWarning() << "An element scheme with only a child sequence can only have '1' as the "
					"first node step, identifying the document element. TODO DOMError" << endl;
			throw new XPointerExceptionImpl(INVALID_EXPRESSION_ERR);
		}

		if(m_childSequence.isEmpty())
		{
			kdWarning() << "\"" << schemeData << "\" cannot match anything." << endl;
			throw new XPointerExceptionImpl(INVALID_EXPRESSION_ERR);
		}
	}
	else /* element()'s scheme data may only start with child sequence or a (valid) NCName. */
	{
		kdWarning() << "Scheme data for element() may only start with a child sequence(\"/\") "
				"or an XML name, not \"" << ch << "\". TODO Submit DOMError" << endl;
		throw new XPointerExceptionImpl(INVALID_EXPRESSION_ERR);
	}
}

ElementSchemeImpl::~ElementSchemeImpl()
{
	if(m_shorthand)
		m_shorthand->deref();
}

void ElementSchemeImpl::parseChildSequence(const QStringList &steps)
{
	QStringList::const_iterator it;
	QStringList::const_iterator end(steps.constEnd());

	for(it = steps.constBegin(); it != end; ++it )
	{
		bool success = false;
		unsigned int result = (*it).toUInt(&success);

		if(success && result >= 1)
			m_childSequence.append(result);
		else
		{
			kdWarning() << "\"" << (*it) << "\" is not a valid step in a child sequence, it must be a non-zero "
					"and non-negative number. TODO Submit DOMError" << endl;
			throw new XPointerExceptionImpl(INVALID_EXPRESSION_ERR);
		}
	}
}

XPointerResultImpl *ElementSchemeImpl::evaluate(NodeImpl *context) const
{
	Q_ASSERT(context);

	NodeImpl *resultNode = 0;

	/* We can't pre-compile this since the Document is live. */
	if(m_shorthand)
	{
		XPointerResultImpl *result = m_shorthand->evaluate(context);
		Q_ASSERT(result);

		if(result->resultType() == XPointerResult::SINGLE_NODE)
			resultNode = result->singleNodeValue();
		else
			return new XPointerResultImpl(XPointerResult::NO_MATCH);
	}
	else
		resultNode = context; /* Guranteed to be document node. */

	Q_ASSERT(resultNode);
	QValueList<unsigned int>::const_iterator it;
	QValueList<unsigned int>::const_iterator end(m_childSequence.constEnd());

	for(it = m_childSequence.constBegin(); it != end; ++it)
	{
		NodeListImpl *children = resultNode->childNodes();

		if(!children)
			continue;
		
		const unsigned int specifiedLevel = (*it);
		const unsigned long length = children->length();

		unsigned int level = 0;
		for(unsigned long i = 0; i < length; i++)
		{
			NodeImpl *child = children->item(i);
			if(!child)
				continue;

			if(child->nodeType() == ELEMENT_NODE)
			{
				level++;

				if(level == specifiedLevel)
				{
					resultNode = child;
					break;
				}
			}
		}
		if(specifiedLevel > level)
			return new XPointerResultImpl(XPointerResult::NO_MATCH);
	}

	if(!resultNode)
		return new XPointerResultImpl(XPointerResult::NO_MATCH);
	
	XPointerResultImpl *result = new XPointerResultImpl(XPointerResult::SINGLE_NODE);
	result->setSingleNodeValue(resultNode);
	return result;
}

// vim:ts=4:noet
