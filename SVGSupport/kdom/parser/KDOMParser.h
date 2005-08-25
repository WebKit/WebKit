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

#ifndef KDOM_Parser_H
#define KDOM_Parser_H

#include <qobject.h>

#include <kdom/DOMString.h>
#include <kdom/impl/DOMErrorHandlerImpl.h>
#include <kdom/cache/KDOMCachedObjectClient.h>

class KURL;
class QBuffer;

namespace KDOM
{
	class DocumentImpl;
	class DocumentBuilder;
	class DOMConfigurationImpl;

	/**
	 * @short KDOM Parser.
	 *
	 * @description This class is a base class for custom parsers.
	 *
	 * KDOM provides 'qxml' & 'libxml' parsing backends.
	 *
	 * @author Nikolas Zimmermann <wildfox@kde.org>
	 * @author Rob Buis <buis@kde.org>
  	 */
	class Parser : public QObject,
				   public DOMErrorHandlerImpl,
				   public CachedObjectClient
	{
	friend class ParserFactory;

	Q_OBJECT

	public:
		// This class can not be constructed directly.
		virtual ~Parser();

		/**
		 * @returns the url passed in the constructor
		 */
		KURL url() const;

		/**
		 * @returns the parsed Document
		 */
		DocumentImpl *document() const;

		/**
		 * @returns the DOMConfigurationImpl object which
		 * holds certain parameters which influence parsing.
		 * (ie. white-space behaviour/entity substitution)
		 */
		DOMConfigurationImpl *domConfig() const;

		/**
		 * @returns the DocumentBuilder in use
		 */
		DocumentBuilder *documentBuilder() const;

		/**
		 * Sets the document builder to @p builder.
		 *
		 * @param builder the DocumentBuilder to use
		 */
		void setDocumentBuilder(DocumentBuilder *builder);

		/**
		 * This parses the document; the byte stream that the Document was built is
		 * NOT being cached. You'll directly get a 'KDOM::Document' object as result.
		 *
		 * @param buffer if set, it can be used to parse an in-memory buffer.
		 *               (in this case, no network accesses will be performed!)
		 */
		virtual DocumentImpl *syncParse(QBuffer *buffer = 0);
		
		/**
		 * This parses the document; the byte stream that the Document was built
		 * from is being cached internally. Non-incremental is not-working atm. FIX IT!
		 *
		 * When downloading and parsing is done, the parsingFinished() signalled is 
		 * emitted and the Document can be retrieved via document().
		 *
		 * @param incremental (incremental parsing mode)
		 * @param accept (http header accept string)
		 */
		virtual void asyncParse(bool incremental = false, const char *accept = 0);

		/**
		 * This forces any loading/parsing to immediately stop.
		 */
		virtual void abortWork();
	
	signals:
		/**
		 * Tells the user whether async parsing was successful or not.
		 *
		 * @param error if not 0, an error occoured.
		 */
		void parsingFinished(KDOM::DOMErrorImpl *error);

	protected:
		/**
		 * Default constructor to be used from custom parsers.
		 *
		 * @param url The absolute URL to load.
		 */
		Parser(const KURL &url);

		/**
		 * This is the callback for retrieving cached data.
		 */
		virtual void notifyFinished(CachedObject *object);

	public:	// Should really only be used by custom parsers...
		/**
		 * Error reporting functionality. (used by custom parsers.)
		 */
		bool handleError(DOMErrorImpl *err);

		/**
		 * Tells the 'custom parser' on top of us, about new data
		 * in incremental parsing mode. (Internal usage only!)
		 */
		virtual void handleIncomingData(QBuffer *buffer, bool eof)
		{ Q_UNUSED(buffer); Q_UNUSED(eof); }

		/**
		 * Just downloads certain data in a sync way. May be used
		 * for instance for external entity loading by a custom parser.
		 */
		QBuffer *bufferForUrl(const KURL &url) const;
	
	private:
		class Private;
		Private *d;
	};
};

#endif

// vim:ts=4:noet
