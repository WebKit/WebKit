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

#include <qmap.h>
#include <qpair.h>
#include <qstring.h>
#include <qvaluelist.h>

#include <kdebug.h>

#include <kdom/Helper.h>

#include "DocumentImpl.h"
#include "DOMStringImpl.h"

#include "XPointerHelper.h"
#include "kdomxpointer.h"

#include "ElementSchemeImpl.h"
#include "NBCImpl.h"
#include "ShortHandImpl.h"
#include "XMLNSSchemeImpl.h"
#include "XPointerEvaluatorImpl.h"
#include "XPointerExceptionImpl.h"
#include "XPointerExpressionImpl.h"
#include "XPointerResultImpl.h"
#include "XPointerSchemeImpl.h"

/**
 * Cast away constness and make it a DocumentImpl pointer.
 */
#define THIS (static_cast<DocumentImpl *>(const_cast<XPointerEvaluatorImpl *>(this)))

using namespace KDOM;
using namespace KDOM::XPointer;

XPointerEvaluatorImpl::XPointerEvaluatorImpl()
{
}

XPointerEvaluatorImpl::~XPointerEvaluatorImpl()
{
}

XPointerExpressionImpl *XPointerEvaluatorImpl::createXPointer(DOMStringImpl *stringImpl,
															  NodeImpl *relatedNode) const
{
	DOMString string(stringImpl);

	/* Possible optimization:
	 * All compiled XPointerExpressions could be pooled in a dictionary. Hence, compilations
	 * are saved at the cost of an overhead of a dictionary lookup at compile time. */

	const unsigned int length = string.length();
	const int firstParan = string.find('(');
	
	const QString qString = string.string();
	const QString stripped = qString.stripWhiteSpace();

	if(firstParan == -1) /* We have an invalid SchemeName or a ShortHand pointer. Assume the latter. */
	{
		XPointerExpressionImpl *xpr = new XPointerExpressionImpl(stringImpl, relatedNode, THIS);
		xpr->ref();

		ShortHandImpl *sh = new ShortHandImpl(DOMString(stripped).handle());
		sh->ref();

		xpr->appendPart(sh);
		return xpr;
	}

	/* Tokenize the scheme string. See in particular the escape semantics:
	 * http://www.w3.org/TR/xptr-framework/xptr-framework.html#NT-EscapedData */

	int paranDepth = 0; /* This may be negative if the paranteses are balanced improperly. */

	DOMString schemeName;

	DOMString schemeData;
	unsigned int endPrevScheme = 0; /* The index of where the previous scheme ended. That's after it's
								       closing parantese. */
	unsigned int startData = 0; /* The index of where the scheme data starts. That's right after the parantese
								   after the scheme name. */

	typedef QPair<DOMString, DOMString> StrPair;
	
	/* A list of pair of SchemeName and SchemeData, all valid. */
	typedef QValueList<StrPair> StrPairList;
	StrPairList pointerParts;

	for(unsigned int i = 0; i < length; i++)
	{
		QChar in = string[i];

		if(in == '(')
		{
			paranDepth++;

			if(paranDepth == 1) /* We're not inside scheme data. */
			{
				startData = i + 1;				
				schemeName = qString.mid(endPrevScheme, i - endPrevScheme).stripWhiteSpace();
	
				if(!Helper::IsValidQName(schemeName.handle()))
					throw new XPointerExceptionImpl(INVALID_EXPRESSION_ERR);
			}

			continue;
		}
		else if(in == ')')
		{
			paranDepth--;

			if(paranDepth == 0) /* ...since scheme data can contain un-escaped paranteses
								   as long as they're balanced. */
			{
				schemeData = qString.mid(startData, i - startData);
				endPrevScheme = i + 1; /* Skip the parantese. */
				pointerParts.append(qMakePair(schemeName, XPointerHelper::DecodeSchemeData(schemeData)));
			}

			continue;
		}
		else if(in == '^')
		{
			QChar of = string[i + 1]; /* Offset string */

			if(of != '(' && of != ')' && of != '^') /* A circumflex can only be used for escaping. */
				throw new XPointerExceptionImpl(INVALID_EXPRESSION_ERR);

			i++; /* Skip the escaped character. */
			continue;
		}
	}

	if(paranDepth != 0) /* Un-balanced paranteses. */
		throw new XPointerExceptionImpl(INVALID_EXPRESSION_ERR);

	if(!stripped.endsWith(QString::fromLatin1(")"))) /* The scheme didn't end with (). */
		throw new XPointerExceptionImpl(INVALID_EXPRESSION_ERR);

	StrPairList::const_iterator it;
	StrPairList::const_iterator end = pointerParts.constEnd();

	XPointerExpressionImpl *xpr = new XPointerExpressionImpl(stringImpl, relatedNode, THIS);
	
	NBCImpl *currentNBC = new NBCImpl(0 /* This is the parent of the initial NBC, which is null. */);
	currentNBC->ref();

	for(it = pointerParts.constBegin(); it != end; ++it)
	{
		schemeName = (*it).first;
		schemeData = (*it).second;

		if(schemeName == "element")
		{
			ElementSchemeImpl *p = new ElementSchemeImpl(schemeData.handle());
			p->ref();
			xpr->appendPart(p);
		}
		else if(schemeName == "xmlns")
		{
			/* nbc becomes the parentNBC for p */
			XMLNSSchemeImpl *p = new XMLNSSchemeImpl(schemeData.handle(), currentNBC);

			/* Make p the new 'current' nbc. */
			currentNBC->deref();
			currentNBC = new NBCImpl(p);
			currentNBC->ref();
		}
		else if(schemeName == "xpointer")
		{
			/* Pass current nbc to the XPointerScheme -- it needs it. */
			XPointerSchemeImpl *p = new XPointerSchemeImpl(schemeData.handle(), currentNBC);
			p->ref();
			xpr->appendPart(p);
		}
		else
		{
			DOMStringImpl *prefix = 0, *dummy = 0;
			Helper::SplitPrefixLocalName(schemeName.handle(), prefix, dummy);

			if(prefix)
				prefix->ref();
			if(dummy)
				dummy->ref();

			/* 3.3 "If the namespace binding context contains no corresponding prefix,
			 * or if the (namespace name, LocalPart) pair does not correspond to a 
			 * scheme name supported by the XPointer processor, the pointer part is skipped." */
			if(prefix && !prefix->isEmpty() && DOMString(currentNBC->lookupNamespaceURI(prefix)).isEmpty())
				continue; 

			kdDebug() << "Encountered unknown scheme: \"" << DOMString(schemeName).string() << "\"." << endl;
			PointerPartImpl *p = new PointerPartImpl(schemeName.handle(), schemeData.handle(), currentNBC);
			p->ref();
			xpr->appendPart(p);

			if(prefix)
				prefix->deref();
			if(dummy)
				dummy->deref();
		}

		/* Do nothing for now, in other words.  */
	}

	return xpr;
}

XPointerResultImpl *XPointerEvaluatorImpl::evaluateXPointer(DOMStringImpl *string, NodeImpl *relatedNode) const
{
	XPointerExpressionImpl *xptr = createXPointer(string, relatedNode);
	Q_ASSERT(xptr);
	
	return xptr->evaluate();
}

// vim:ts=4:noet
