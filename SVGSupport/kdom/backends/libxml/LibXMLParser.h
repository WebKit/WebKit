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

#ifndef KDOM_LibXmlParser_H
#define KDOM_LibXmlParser_H

#include <kdom/parser/KDOMParser.h>

namespace KDOM
{
	typedef enum
	{
		KDOM_ENTITY_UNKNOWN = 0,
		KDOM_INTERNAL_ENTITY = 1,
		KDOM_EXTERNAL_ENTITY = 2,
		KDOM_ENTITY_NEW_TREE = 4,
		KDOM_ENTITY_LIBXML2_BUG = 8
	} LibXmlEntityMode;
	
	// A libxml2 based KDOM parser
	class LibXMLParser : public Parser
	{
	Q_OBJECT
	public:
		LibXMLParser(const KURL &url);
		virtual ~LibXMLParser();

		// Starts parsing the document
		virtual void startParsing(bool incremental);

		virtual Document parse(QBuffer *buffer = 0);
		void doOneShotParse(const char *rawData, unsigned int rawLength);

		// User-defined break of parsing on specific errors
		virtual void stopParsing(const DOMString &errorDescription = DOMString());

		// Helper for sax_end_doc
		void endDocument();

		LibXmlEntityMode entityMode() { return m_entityMode; }
		void setEntityMode(LibXmlEntityMode mode) { m_entityMode = mode; } 

		QString entityRef() const { return m_entityRef; }
		void setEntityRef(const QString &entityRef) { m_entityRef = entityRef; }

		void tryEndEntityRef(const QString &name);

	public slots:
		void slotLoadingFinished(QBuffer *buffer);
		void slotFeedData(const QByteArray &data, bool eof);

	private:
		// Real type: xmlParserCtxtPtr
		void *m_incrementalParserContext;

		QString m_entityRef;
		LibXmlEntityMode m_entityMode;
	};
};

#endif

// vim:ts=4:noet
