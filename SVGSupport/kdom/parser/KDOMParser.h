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
#include <kdom/impl/DOMErrorHandlerImpl.h>
#include <kdom/DOMString.h>
#include <kdom/cache/KDOMCachedObjectClient.h>

class KURL;
class QBuffer;

namespace KDOM
{
	class Document;
	class DocumentBuilder;
	class DOMConfigurationImpl;

	/**
	 * @short KDOM Parser.
	 *
	 * @description TODO-docs a description of what this class does.
	 *
	 * This class needs to be inherited by a custom parser, for example 
	 * QXml or libxml2.
	 *
	 * The document builder is deleted by the Parser.
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
		/**
		 * @returns the url passed in the constructor
		 */
		KURL url() const;

		/** @return the parsed Document
		 */
		Document document() const;

		/**
		 * Sets the document builder to @p builder.
		 *
		 * @param builder the DocumentBuilder to use
		 */
		void setDocumentBuilder(DocumentBuilder *builder);

		/**
		 * @returns the DocumentBuilder in use
		 */
		DocumentBuilder *documentBuilder() const;

		/**
		 * Starts parsing the document.
		 *
		 * The implementation loads the document via asyncronous KIO
		 * and emits the loadingFinished signal when done.
		 *
		 * @param incremental TODO-docs
		 */
		virtual void startParsing(bool incremental);

		/**
		 * Performs synchronous parsing. The URI of this Parser, set
		 * via the request() call in @ref ParserFactory, is downloaded
		 * and parsed into a Document which is directly returned.
		 */
		virtual Document parse(QBuffer *buffer = 0);

		/**
		 * This is similar to calling startParsing(false), with the difference
		 * that the byte stream that the Document node was built from was cached.
		 *
		 * When downloading and parsing is done, the parsingFinished() signalled is 
		 * emitted and the Document can be retrieved via document().
		 *
		 * Currently, only non-incremental is supported.
		 * 
		 * @param incremental
		 * @param accept
		 */
		virtual void cachedParse(bool incremental = false, const char* accept = 0);

		/**
		 * User-defined break of parsing on specific errors.
		 *
		 * @param errorDescription TODO-docs
		 */
		virtual void stopParsing(const DOMString &errorDescription = DOMString()) = 0;

		/**
		 * TODO-docs
		 *
		 * @param err TODO-docs
		 */
		virtual bool handleError(DOMErrorImpl *err);

		DOMConfigurationImpl *domConfig() const;

		virtual ~Parser();

	protected:
		/**
		 * This is the callback for retrieving cached data. It is only used when 
		 * doing cached parsing.
		 */
		void notifyFinished(CachedObject *object);

		/**
		 * Default constructor. TODO-docs
		 *
		 * @param url TODO-docs
		 */
		Parser(const KURL &url);

	signals:
		/**
		 * In non-incremental mode, it tells the user whether the file is
		 * ready to be parsed, otherwise it just informs that the feed is finished.
		 *
		 * @param buffer TODO-docs
		 */
		void loadingFinished(QBuffer *buffer);

		/**
		 * Tells the user about new data in incremental parsing mode.
		 *
		 * @param eof TODO-docs
		 * @param data TODO-docs
		 */
		void feedData(const QByteArray &data, bool eof);

		/**
		 * Tells the user whether parsing was successful or not.
		 *
		 * @param bool if true, an error did occur during parsing
		 * @param errorDescription TODO-docs
		 */
		void parsingFinished(bool error, const KDOM::DOMString &errorDescription);
		// FIXME, englich: Why is not a DOMError passed as errorDescription?

	private slots:

		/** 
		 * Used by the @ref DataSlave to inform kio users about file download progress.
		 */
		void slotNotify(unsigned long jobTicket, QBuffer *buffer, bool hasError);

		void slotNotifyIncremental(unsigned long jobTicket, const QByteArray &data,
								   bool eof, bool hasError);

	private:
		class Private;
		Private *d;
	};
};

#endif

// vim:ts=4:noet
