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

#include <kurl.h>
#include <kdebug.h>

#include <kio/job.h>
#include <kio/netaccess.h>

#include "kdom.h"
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

DocumentImpl *Parser::document() const
{
	if(!d->docBuilder)
		return 0;

	return d->docBuilder->document();
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

DocumentBuilder *Parser::documentBuilder() const
{
	return d->docBuilder;
}

void Parser::setDocumentBuilder(DocumentBuilder *builder)
{
	delete d->docBuilder;
	d->docBuilder = builder;
}

DocumentImpl *Parser::syncParse(QBuffer *buffer)
{
	QBuffer *work = buffer;
	if(!work)
		work = bufferForUrl(d->url);

	if(!work)
		return 0;

	// Ask 'custom parser' on top of us, to handle the incoming xml stream...
	const_cast<Parser *>(this)->handleIncomingData(work, true);

	delete work; // Delete as it's not cached.

	// Return parsed document...
	return document();
}

void Parser::asyncParse(bool incremental, const char *accept)
{
	// Asynchronous parsing.
	// TODO: INCREMENTAL PARSING!
	if(!d->docLoader)
		d->docLoader = new DocumentLoader(0, 0); // TODO: Fix usage of DocumentLoader!

	d->cachedDoc = d->docLoader->requestDocument(d->url, QString::fromLatin1(accept));
	d->cachedDoc->ref(this);
}

void Parser::abortWork()
{
	// TODO: maybe fill DOMError object with information?
	DOMErrorImpl *error = new DOMErrorImpl();
	error->ref();
	error->setSeverity(SEVERITY_ERROR);

	handleError(error);
	error->deref();
}

void Parser::notifyFinished(CachedObject *object)
{
	if(object == d->cachedDoc)
	{
		d->cachedDoc->deref(this);

		handleIncomingData(d->cachedDoc->documentBuffer(), true);

		// No error.
		emit parsingFinished(0);

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
		emit parsingFinished(err);
		assert(0);
	}

	return true;
}

QBuffer *Parser::bufferForUrl(const KURL &url) const
{
#ifndef APPLE_COMPILE_HACK
	QString temporaryFileName;
	bool retVal = KIO::NetAccess::download(url, temporaryFileName, 0 /* No window for authentification etc.. */);
	if(!retVal) // No temporary file created so far -> nothing to remove
		return 0;

	QFile file(temporaryFileName);
	if(!file.open(IO_ReadOnly))
	{
		// Can a file be correctly downloaded but not opened?
		KIO::NetAccess::removeTempFile(temporaryFileName);
		return 0;
	}

	QBuffer *ret = new QBuffer(file.readAll());
	KIO::NetAccess::removeTempFile(temporaryFileName);

	return ret;
#else
	return NULL;
#endif
}

#ifdef APPLE_CHANGES
void Parser::parsingFinished(KDOM::DOMErrorImpl *)
{
    
}
#endif

// vim:ts=4:noet
