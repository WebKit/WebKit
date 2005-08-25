/*
 * XPathResultImpl.h - Copyright 2005 Frerich Raabe <raabe@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef XPATHRESULTIMPL_H
#define XPATHRESULTIMPL_H

#include <kdom/Shared.h>

#include "impl/expression.h"

namespace KDOM
{

class DOMStringImpl;
class NodeImpl;

namespace XPath
{

class XPathResultImpl : public Shared
{
	public:
		XPathResultImpl();
		XPathResultImpl( const Value &value );

		void convertTo( unsigned short type );

		unsigned short resultType() const;

		double numberValue() const;
		DOMStringImpl *stringValue() const;
		bool booleanValue() const;
		NodeImpl *singleNodeValue() const;

		bool invalidIteratorState() const;
		unsigned long snapshotLength() const;
		NodeImpl *iterateNext();
		NodeImpl *snapshotItem( unsigned long index );

	private:
		Value m_value;
		DomNodeList::Iterator m_nodeIterator;
		unsigned short m_resultType;
};

}

}

#endif // XPATHRESULTIMPL_H

