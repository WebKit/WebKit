/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

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

#include <qdir.h>
#include <qbuffer.h>
#include <qcstring.h>

#include <kmimetype.h>

#include "kdom.h"
#include "kdomls.h"
#include "DOMString.h"
#include "KDOMParser.h"
#include "LSInputImpl.h"
#include "LSParserImpl.h"
#include "NodeListImpl.h"
#include "DocumentImpl.h"
#include "LSExceptionImpl.h"
#ifndef APPLE_CHANGES
#include "KDOMParserFactory.h"
#endif
#include "KDOMDocumentBuilder.h"
#include "DOMConfigurationImpl.h"
#include "DocumentFragmentImpl.h"

using namespace KDOM;

LSParserImpl::LSParserImpl()
: EventTargetImpl(), m_activeParser(0), m_config(0)
{
}

LSParserImpl::~LSParserImpl()
{
	delete m_activeParser;

	if(m_config)
		m_config->deref();
}

DOMConfigurationImpl *LSParserImpl::domConfig() const
{
	if(!m_config)
	{
		m_config = new DOMConfigurationImpl();
		m_config->ref();
	}

	return m_config;
}

LSParserFilterImpl *LSParserImpl::filter() const
{
	return 0;
}

void LSParserImpl::setFilter(LSParserFilterImpl *filter)
{
	// TODO: implement me!
	Q_UNUSED(filter);
}

bool LSParserImpl::busy() const
{
	return m_activeParser != 0;
}

bool LSParserImpl::async() const
{
	return m_async;
}

void LSParserImpl::setASync(bool async)
{
	m_async = async;
}

#ifndef APPLE_COMPILE_HACK
static int hex2int(unsigned int _char)
{
	if(_char >= 'A' && _char <='F')
		return _char - 'A' + 10;
	if(_char >= 'a' && _char <='f')
		return _char - 'a' + 10;
	if(_char >= '0' && _char <='9')
		return _char - '0';

	return -1;
}
#endif

DocumentImpl *LSParserImpl::parse(KURL url, LSInputImpl *input, bool async, NodeImpl *contextArg)
{
#ifndef APPLE_COMPILE_HACK
	// INVALID_STATE_ERR: Raised if the LSParser.busy attribute is true.
	if(busy())
		throw new DOMExceptionImpl(INVALID_STATE_ERR);

	QBuffer *buffer = 0;
	if(!url.isEmpty())
	{
		kdDebug() << "TODO1: PARSE URL: " << url << endl;
	}
	else if(input->characterStream())
	{
		QString in = input->characterStream()->read();
		kdDebug() << "in2 : " << in << endl;
		buffer = new QBuffer();
		buffer->open(IO_ReadWrite);
		buffer->writeBlock(in.ascii(), in.length());
		buffer->close();
	}
	else if(input->byteStream() && !input->byteStream()->isEmpty())
	{
		QString str(input->byteStream()->string());
		QCString in(str.ascii(), str.length() + 1);

		int a, b;
		unsigned int i = 0;
		unsigned int len = in.length();
		buffer = new QBuffer();
		buffer->open(IO_ReadWrite);
		while(i < len)
		{
			a = hex2int(in[i]);
			b = hex2int(in[i + 1]);
			buffer->putch(a * 16 + b);
			i += 2;
		}

		buffer->close();
	}
	else if(input->stringData() && !input->stringData()->isEmpty())
	{
		QString in = input->stringData()->string();
		buffer = new QBuffer();
		buffer->open(IO_ReadWrite);
		buffer->writeBlock(in.ascii(), in.length());
		buffer->close();
	}
	else if(input->systemId() && !input->systemId()->isEmpty())
	{
		url = input->systemId()->string();
		if(KURL::isRelativeURL(url.url()))
			url = KURL(KURL(QDir::currentDirPath() + '/'), url.url());

		kdDebug() << "TODO2: PARSE URL: " << url << endl;

		// TODO!
		//buffer = DataSlave::self()->newSyncJob(url);
	}

	if(!url.isEmpty() && domConfig()->getParameter(FEATURE_SUPPORTED_MEDIA_TYPE_ONLY))
	{
		KMimeType::Ptr ptr = KMimeType::findByURL(url);
		if(!(ptr->is(QString::fromLatin1("text/xml")) || ptr->is(QString::fromLatin1("application/xml"))  ||
			ptr->is(QString::fromLatin1("application/xml-dtd")) || ptr->is(QString::fromLatin1("text/xsl")) ||
			ptr->is(QString::fromLatin1("text/xml-external-parsed-entity")) || 
			ptr->is(QString::fromLatin1("application/xml-external-parsed-entity"))))
		{
			// PARSE_ERR: Raised if the LSParser was unable to load the XML fragment.
			// DOM applications should attach a DOMErrorHandler using the parameter
			// "error-handler" if they wish to get details on the error.
			// TODO : error handling
			throw new LSExceptionImpl(PARSE_ERR);
		}
	}

	// PARSE_ERR: Raised if the LSParser was unable to load the XML fragment.
	// DOM applications should attach a DOMErrorHandler using the parameter
	// "error-handler" if they wish to get details on the error.
	if(!buffer && url.isEmpty())
		throw new LSExceptionImpl(PARSE_ERR);

	m_activeParser = ParserFactory::self()->request(url);
	if(!m_activeParser)
		return 0;

	m_activeParser->domConfig()->setParameter(FEATURE_COMMENTS, domConfig()->getParameter(FEATURE_COMMENTS));
	m_activeParser->domConfig()->setParameter(FEATURE_CDATA_SECTIONS, domConfig()->getParameter(FEATURE_CDATA_SECTIONS));
	m_activeParser->domConfig()->setParameter(FEATURE_NAMESPACE_DECLARATIONS, domConfig()->getParameter(FEATURE_NAMESPACE_DECLARATIONS));

	if(async)
		m_activeParser->asyncParse(false /* non-incremental */);
	else
	{
		if(contextArg)
		{
			m_activeParser->documentBuilder()->setDocument(contextArg->ownerDocument());
			m_activeParser->documentBuilder()->pushNode(contextArg);
		}

		DocumentImpl *ret = m_activeParser->syncParse(buffer);
		if(!ret)
			return 0; // TODO : return error?
		
		ret->ref();
		
		delete m_activeParser;
		m_activeParser = 0;

		return ret;
	}
#endif
	return 0;
}

DocumentImpl *LSParserImpl::parse(LSInputImpl *input)
{
	return parse(KURL(), input, m_async);
}

DocumentImpl *LSParserImpl::parseURI(const DOMString &uri)
{
	return parse(KURL(uri.string()), 0, m_async);
}

void LSParserImpl::abort() const
{
	if(m_activeParser)
		m_activeParser->abortWork();
}

NodeImpl *LSParserImpl::parseWithContext(LSInputImpl *input, NodeImpl *contextArg,
										 unsigned short action)
{
	NodeImpl *node = contextArg;
	if(node && action == ACTION_APPEND_AS_CHILDREN || action == ACTION_REPLACE_CHILDREN)
		node = node->parentNode();

	// HIERARCHY_REQUEST_ERR: Raised if the content cannot replace, be inserted before,
	// after, or as a child of the context node (see also Node.insertBefore or
	// Node.replaceChild in [DOM Level 3 Core]).
	if(!node || !(node->nodeType() == DOCUMENT_NODE || node->nodeType() == ELEMENT_NODE ||
	   node->nodeType() == DOCUMENT_FRAGMENT_NODE))
		throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);

	DocumentFragmentImpl *frag = contextArg->ownerDocument()->createDocumentFragment();
	NodeImpl *parent = 0;
	if(action == ACTION_INSERT_BEFORE)
		parent = contextArg->parentNode();
	else if(action == ACTION_INSERT_AFTER || action == ACTION_REPLACE)
		parent = contextArg->parentNode();
	else if(action == ACTION_REPLACE_CHILDREN || action == ACTION_APPEND_AS_CHILDREN)
		parent = contextArg;

	if(action == ACTION_REPLACE)
		parent->removeChild(contextArg);
	else if(action == ACTION_REPLACE_CHILDREN)
		while(contextArg->childNodes()->length() > 0)
			contextArg->removeChild(contextArg->childNodes()->item(0));

	// parse
	parse(KURL(), input, false, frag);
	if(action == ACTION_INSERT_BEFORE)
		parent->insertBefore(frag, contextArg);
	else if(action == ACTION_INSERT_AFTER || action == ACTION_REPLACE)
		parent->insertBefore(frag, contextArg->nextSibling());
	else if(action == ACTION_REPLACE_CHILDREN || action == ACTION_APPEND_AS_CHILDREN)
		parent->appendChild(frag);

	NodeImpl *refChild = frag->firstChild();
	frag->deref();

	return refChild;
}

// vim:ts=4:noet
