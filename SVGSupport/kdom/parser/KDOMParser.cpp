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
#include <qapplication.h>

#include <kurl.h>
#include <kdebug.h>
#include <kio/job.h>

#include "kdom.h"
#include "Document.h"
#include "DOMError.h"
#include "DOMString.h"
#include "KDOMLoader.h"
#include "KDOMParser.moc"
#include "DOMErrorImpl.h"
#include "DOMLocatorImpl.h"
#include "KDOMCachedDocument.h"
#include "KDOMDocumentBuilder.h"
#include "DOMConfigurationImpl.h"

using namespace KDOM;

class Parser::Private
{
public:
	Private(const KURL &u): url(u), jobTicket(0), docLoader(0), cachedDoc(0), docBuilder(0), config(0)
	{
	}

	~Private()
	{
		if(config)
			config->deref();

		delete docBuilder;
		delete docLoader;
	}

	KURL url;

	// DataSlave interface
	unsigned long jobTicket;

	DocumentLoader *docLoader;
	CachedDocument *cachedDoc;
	DocumentBuilder *docBuilder;
	DOMConfigurationImpl *config;
};

Parser::Parser(const KURL &url) : QObject(0, "KDOM Parser"), d(new Private(url))
{
#ifndef APPLE_COMPILE_HACK
	// Make @p url absolute from current working directory if it's local(and relative).
	if(KURL::isRelativeURL(url.url()))
		d->url = KURL(KURL(QDir::currentDirPath() + '/'), url.url());
#endif
}

Parser::~Parser()
{
	delete d;
}

KURL Parser::url() const
{
	return d->url;
}

Document Parser::document() const
{
	if(!d->docBuilder)
		return Document::null;

	return Document(d->docBuilder->document());
}

void Parser::setDocumentBuilder(DocumentBuilder *builder)
{
	delete d->docBuilder;
	d->docBuilder = builder;
}

DocumentBuilder *Parser::documentBuilder() const
{
	return d->docBuilder;
}

void Parser::slotNotify(unsigned long jobTicket, QBuffer *buffer, bool /*hasError*/)
{

	if(d->jobTicket == jobTicket)
	{
		emit loadingFinished(buffer);	
		d->jobTicket = 0;
	}
}

void Parser::slotNotifyIncremental(unsigned long jobTicket, const QByteArray &data, bool eof, bool hasError)
{
	if(d->jobTicket == jobTicket)
	{
		if(hasError)
		{
			emit parsingFinished(true, DOMString());
			return;
		}

		if(eof)
		{
			emit loadingFinished(0);
			d->jobTicket = 0;
			return;
		}

		emit feedData(data, eof);
	}
}

void Parser::startParsing(bool incremental)
{
#ifndef APPLE_COMPILE_HACK
	// Pass the document url as baseURL to our data slave...
	DataSlave::self()->setBaseURL(url());

	d->jobTicket = DataSlave::self()->newAsyncJob(d->url, this, incremental);
#endif
}

Document Parser::parse(QBuffer *buffer)
{
#ifndef APPLE_COMPILE_HACK
	// Pass the document url as baseURL to our data slave...
	DataSlave::self()->setBaseURL(url());

	if(!buffer)
	{
		buffer = DataSlave::self()->newSyncJob(d->url);
		if(!buffer)
			return Document::null;
	}

	emit loadingFinished(buffer);

	// Deregister our baseURL, when we're done with the current document!
	KDOM::DataSlave::self()->setBaseURL(KURL());
#endif
	return document();
}

void Parser::cachedParse(bool /* incremental */, const char *accept)
{
	if(!d->docLoader)
		d->docLoader = new DocumentLoader(0, 0); // TODO: Fix usage of DocumentLoader!

	d->cachedDoc = d->docLoader->requestDocument(url(), QString::fromLatin1(accept));
	d->cachedDoc->ref(this);
}

void Parser::notifyFinished(CachedObject *object)
{
	if(object == d->cachedDoc)
	{
		d->cachedDoc->deref(this);
		
		emit loadingFinished(d->cachedDoc->documentBuffer());

		d->cachedDoc = 0;
	}
}

bool Parser::handleError(DOMErrorImpl *err)
{
	if(err->severity() == SEVERITY_WARNING)
	{
		QString str;
		str.sprintf("[%ld:%ld]: WARNING: %s\n", err->location()->lineNumber(),
					err->location()->columnNumber(), DOMString(err->message()).string().latin1());

		kdDebug(26001) << str << endl;
	}
	else if(err->severity() == SEVERITY_ERROR)
	{
		QString str;
		str.sprintf("[%ld:%ld]: ERROR: %s\n", err->location()->lineNumber(),
					err->location()->columnNumber(), DOMString(err->message()).string().latin1());
		kdDebug(26001) << str << endl;
	}
	else
	{
		QString str;
		str.sprintf("[%ld:%ld]: FATAL ERROR: %s\n", err->location()->lineNumber(),
					err->location()->columnNumber(), DOMString(err->message()).string().latin1());
					
		kdDebug(26001) << str << endl;
		
		emit parsingFinished(true, DOMString(err->message()));
                assert(0);
		return false;
	}

	return true;
}

DOMConfigurationImpl *Parser::domConfig() const
{
	if(!d->config)
	{
		d->config = new DOMConfigurationImpl();
		d->config->ref();
	}

	return d->config;
}

#ifdef APPLE_CHANGES
void Parser::loadingFinished(QBuffer *buffer)
{

}

void Parser::feedData(const QByteArray &data, bool eof)
{

}

void Parser::parsingFinished(bool error, const KDOM::DOMString &errorDescription)
{

}

#endif

// vim:ts=4:noet
