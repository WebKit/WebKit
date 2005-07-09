/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
				  2004 Rob Buis <buis@kde.org>
				  
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

#include <stdarg.h>

#include <kdebug.h>
#include <kurl.h>

#include <qbuffer.h>
#include <qdir.h>

#include <libxml/SAX.h>
#include <libxml/xmlmemory.h>
#include <libxml/parserInternals.h>

#include "DOMError.h"
#include "kdom/impl/DOMErrorImpl.h"
#include "kdom/impl/DOMLocatorImpl.h"
#include "kdom/impl/DOMConfigurationImpl.h"
#include "DOMConfiguration.h"
#include "kdom/impl/NodeImpl.h"
#include "kdom/impl/DocumentImpl.h"
#include "Document.h"
#include "KDOMDocumentBuilder.h"
#include "LibXMLParser.moc"
#include "Namespace.h"
#include "kdom.h"

using namespace KDOM;

// Some macros to make the life with libxml a bit easier
#define GET_PARSER					xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(closure); \
									LibXMLParser *parser = static_cast<LibXMLParser *>(ctxt->_private);
#define GET_BUILDER					GET_PARSER DocumentBuilder *docBuilder = parser->documentBuilder();
#define CONV_STRING_LEN(Name, Len)	QString::fromUtf8(reinterpret_cast<const char *>(Name), Len)
#define CONV_STRING(Name)			CONV_STRING_LEN(Name, (Name) ? xmlStrlen(Name) : 0)

xmlEntityPtr sax_get_entity(void *closure, const xmlChar *name)
{
	kdDebug(26001) << "LibXMLParser::sax_get_entity " << CONV_STRING(name) << endl;

	xmlEntityPtr ent = xmlGetPredefinedEntity(name);
	if(ent)
		return ent;

	GET_PARSER

	// Set the parser into a special mode, as sax_characters()
	// will be called twice if the entityMode is unknown here...
	if(parser->domConfig()->getParameter(FEATURE_ENTITIES) == false)
		ctxt->replaceEntities = true;
	else
	{
		bool inAttr = ctxt->instate == XML_PARSER_ATTRIBUTE_VALUE;
		ent = xmlGetDocEntity(ctxt->myDoc, name);
		if(ent)
			ctxt->replaceEntities = inAttr || (ent->etype != XML_INTERNAL_GENERAL_ENTITY);
	}

	if(ctxt->instate == XML_PARSER_CONTENT && ent && ent->etype == XML_INTERNAL_GENERAL_ENTITY)
	{
		GET_BUILDER
		docBuilder->entityReferenceStart(CONV_STRING(name));
		parser->setEntityRef(CONV_STRING(ctxt->name));
	}

	return 0;
}

void sax_notation_decl(void *closure, const xmlChar *name, const xmlChar *publicId, const xmlChar *systemId)
{
	kdDebug(26001) << "LibXMLParser::sax_notation_decl " << CONV_STRING(name) << endl;

	GET_BUILDER
	docBuilder->notationDecl(CONV_STRING(name), CONV_STRING(publicId), CONV_STRING(systemId));
}

void sax_unparsed_entity(void *closure, const xmlChar *name, const xmlChar *publicId, const xmlChar *systemId, const xmlChar *notationName)
{
	kdDebug(26001) << "LibXMLParser::sax_unparsed_entity " << CONV_STRING(name) << endl;

	GET_BUILDER
	docBuilder->unparsedEntityDecl(CONV_STRING(name), CONV_STRING(publicId), CONV_STRING(systemId), CONV_STRING(notationName));
}

void sax_start_doc(void *closure)
{
	kdDebug(26001) << "LibXMLParser::sax_start_doc" << endl;

	GET_PARSER

	if(parser->entityMode() != KDOM_ENTITY_NEW_TREE)
	{
		parser->documentBuilder()->startDocument(parser->url());

		// xml:standalone, xml:encoding and xml:version support
		DocumentImpl *doc = static_cast<DocumentImpl *>(parser->document().handle());
		doc->setStandalone(ctxt->standalone == 1);
		doc->setXmlEncoding(CONV_STRING(ctxt->encoding));
		if(ctxt->encoding)
			doc->setInputEncoding(CONV_STRING(ctxt->encoding));
		else
			doc->setInputEncoding(QString("UTF-8"));
		doc->setXmlVersion(CONV_STRING(ctxt->version));

		xmlSAX2StartDocument(closure);
	}
}

void sax_end_doc(void *closure)
{
	GET_PARSER

	if(parser->entityMode() == KDOM_ENTITY_NEW_TREE)
	{
		parser->setEntityMode(KDOM_ENTITY_UNKNOWN);
		parser->documentBuilder()->internalEntityDeclEnd();
	}
	else
		parser->endDocument();
}

void sax_start_element(void *closure, const xmlChar *name, const xmlChar **attributes)
{
	GET_BUILDER

	docBuilder->startElement(CONV_STRING(name));

	for(int i = 0; attributes && attributes[i]; i += 2)
		docBuilder->startAttribute(CONV_STRING(attributes[i]), CONV_STRING(attributes[i + 1]));

	docBuilder->startElementEnd();
}

void sax_end_element(void *closure, const xmlChar *name)
{
	GET_BUILDER
	parser->tryEndEntityRef(CONV_STRING(name));
	docBuilder->endElement(CONV_STRING(name));

	// Better safe then sorry...
	if(parser->entityMode() != KDOM_ENTITY_NEW_TREE)
		parser->setEntityMode(KDOM_ENTITY_UNKNOWN);
}

void sax_start_element_ns(void *closure, const xmlChar *localname, const xmlChar *prefix, const xmlChar *URI, int nb_namespaces, const xmlChar **namespaces, int nb_attributes, int, const xmlChar **attributes)
{
	GET_BUILDER

	QString namespaceURI = CONV_STRING(URI);
	docBuilder->startElementNS(namespaceURI, CONV_STRING(prefix), CONV_STRING(localname));

	if(parser->domConfig()->getParameter(FEATURE_NAMESPACE_DECLARATIONS))
	{
		for(int i = 0; i < nb_namespaces * 2; ++i)
		{
			DOMString qName("xmlns");
			if(namespaces[i])
				qName = qName + ":" + CONV_STRING(namespaces[i]);

			i++;
			docBuilder->startAttributeNS(NS_XMLNS, qName, CONV_STRING(namespaces[i]));
		}
	}

	for(int i = 0;i < nb_attributes * 5; i += 5)
	{
		if(attributes[i + 2])
		{
			if(!CONV_STRING(attributes[i + 1]).isEmpty())
				docBuilder->startAttributeNS(CONV_STRING(attributes[i + 2]), CONV_STRING(attributes[i + 1]) + ":" + CONV_STRING(attributes[i]), CONV_STRING_LEN(attributes[i + 3], (int)(attributes[i + 4] - attributes[i + 3])));
			else
				docBuilder->startAttributeNS(DOMString(""), CONV_STRING(attributes[i]), CONV_STRING_LEN(attributes[i + 3], (int)(attributes[i + 4] - attributes[i + 3])));
		}
		else
			docBuilder->startAttribute(CONV_STRING(attributes[i]), CONV_STRING_LEN(attributes[i + 3], (int)(attributes[i + 4] - attributes[i + 3])));
	}

	docBuilder->startElementEnd();
}

void sax_end_element_ns(void *closure, const xmlChar *localname, const xmlChar *prefix, const xmlChar *URI)
{
	GET_BUILDER
	parser->tryEndEntityRef(CONV_STRING(localname));
	docBuilder->endElementNS(CONV_STRING(URI), CONV_STRING(prefix), CONV_STRING(localname));

	// Better safe then sorry...
	if(parser->entityMode() != KDOM_ENTITY_NEW_TREE)
		parser->setEntityMode(KDOM_ENTITY_UNKNOWN);
}

void sax_ignorable_ws(void *, const xmlChar *, int)
{
}

void sax_characters(void *closure, const xmlChar *ch, int len)
{
	GET_BUILDER

	// Work around libxml2 bug (sax_characters() is called twice per entity value)
	QString compareString = CONV_STRING_LEN(ch, len);
	if(parser->domConfig() &&
	   !parser->domConfig()->getParameter(FEATURE_WHITESPACE_IN_ELEMENT_CONTENT) &&
	   compareString.stripWhiteSpace().isEmpty())
		return;

	parser->tryEndEntityRef(CONV_STRING(ctxt->name));
	docBuilder->characters(compareString);
}

void sax_pi(void *closure, const xmlChar *target, const xmlChar *data)
{
	GET_BUILDER
	parser->tryEndEntityRef(CONV_STRING(ctxt->name));
	docBuilder->startPI(CONV_STRING(target), CONV_STRING(data));
}

void sax_comment(void *closure, const xmlChar *ch)
{
	GET_BUILDER
	parser->tryEndEntityRef(CONV_STRING(ctxt->name));
	docBuilder->comment(CONV_STRING(ch));
}

void sax_start_cdata(void *closure, const xmlChar *ch, int len)
{
	GET_BUILDER

	docBuilder->startCDATA();
	docBuilder->characters(CONV_STRING_LEN(ch, len));
	docBuilder->endCDATA();
}

void sax_internal_subset(void *closure, const xmlChar *name, const xmlChar *publicId, const xmlChar *systemId)
{
	GET_BUILDER
	docBuilder->startDTD(CONV_STRING(name), CONV_STRING(publicId), CONV_STRING(systemId));
	xmlSAX2InternalSubset(closure, name, publicId, systemId);
}

void sax_warning(void *closure, const char *msg, ...) __attribute__ ((format (printf, 2, 3)));
void sax_warning(void *closure, const char *msg, ...)
{
	fprintf(stderr, "sax_warning\n");
	GET_BUILDER

	va_list args;
	char buf[255];

	va_start(args, msg);
	vsprintf(buf, msg, args);
	va_end(args);

	DOMErrorImpl *err = new DOMErrorImpl();
	err->setMessage(QString(buf));
	err->setSeverity(SEVERITY_WARNING);
	err->location()->setLineNumber(ctxt->input->line);
	err->location()->setColumnNumber(ctxt->input->col);
	err->location()->setByteOffset(ctxt->input->consumed);
	err->location()->setRelatedNode(static_cast<NodeImpl *>(docBuilder->currentNode().handle()));
	if(ctxt->input->filename)
		err->location()->setUri(CONV_STRING(reinterpret_cast<const xmlChar *>(ctxt->input->filename)));

	parser->handleError(err);
}

void sax_error(void *closure, const char *msg, ...) __attribute__ ((format (printf, 2, 3)));
void sax_error(void *closure, const char *msg, ...)
{
	fprintf(stderr, "sax_error\n");
	GET_BUILDER

	va_list args;
	char buf[255];

	va_start(args, msg);
	vsprintf(buf, msg, args);
	va_end(args);

	QString qBuf = QString(buf);

	DOMErrorImpl *err = new DOMErrorImpl();
	err->setMessage(qBuf);
	
	// Well, I consider waiting forever a fatal error..
	if(qBuf == QString::fromLatin1("internal error"))
		err->setSeverity(SEVERITY_FATAL_ERROR);
	else
		err->setSeverity(SEVERITY_ERROR);

	err->location()->setLineNumber(ctxt->input->line);
	err->location()->setColumnNumber(ctxt->input->col);
	err->location()->setByteOffset(ctxt->input->consumed);
	err->location()->setRelatedNode(static_cast<NodeImpl *>(docBuilder->currentNode().handle()));
	if(ctxt->input->filename)
		err->location()->setUri(CONV_STRING(reinterpret_cast<const xmlChar *>(ctxt->input->filename)));

	parser->handleError(err);
	err->deref();
}

void sax_fatal_error(void *closure, const char *msg, ...) __attribute__ ((format (printf, 2, 3)));
void sax_fatal_error(void *closure, const char *msg, ...)
{
	fprintf(stderr, "sax_fatal_error\n");
	GET_BUILDER

	va_list args;
	char buf[255];

	va_start(args, msg);
	vsprintf(buf, msg, args);
	va_end(args);

	DOMErrorImpl *err = new DOMErrorImpl();
	err->setMessage(QString(buf));
	err->setSeverity(SEVERITY_FATAL_ERROR);
	err->location()->setLineNumber(ctxt->input->line);
	err->location()->setColumnNumber(ctxt->input->col);
	err->location()->setByteOffset(ctxt->input->consumed);
	err->location()->setRelatedNode(static_cast<NodeImpl *>(docBuilder->currentNode().handle()));
	if(ctxt->input->filename)
		err->location()->setUri(CONV_STRING(reinterpret_cast<const xmlChar *>(ctxt->input->filename)));

	parser->handleError(err);
	err->deref();
}

void sax_entity_decl(void *closure, const xmlChar *name, int type,  const xmlChar *publicId, const xmlChar *systemId, xmlChar *content);

static xmlSAXHandler KDOM_PARSER_SAX_HANDLER =
{
	sax_internal_subset,	/* internalSubset */
	0,						/* isStandalone */
	0,						/* hasInternalSubset */
	0,						/* hasExternalSubset */
	0,						/* resolveEntity */
	sax_get_entity,			/* getEntity */
	sax_entity_decl,		/* entityDecl */
	sax_notation_decl,		/* notationDecl */
	0,						/* attributeDecl */
	0,						/* elementDecl */
	sax_unparsed_entity,	/* unparsedEntityDecl */
	0,						/* setDocumentLocator */
	sax_start_doc,			/* startDocument */
	sax_end_doc,			/* endDocument */
	sax_start_element,		/* startElement */
	sax_end_element,		/* endElement */
	0,						/* reference */
	sax_characters,			/* characters */
	sax_ignorable_ws,		/* ignorableWhitespace */
	sax_pi,					/* processingInstruction */
	sax_comment,			/* comment */
	sax_warning,			/* xmlParserWarning */
	sax_error,				/* xmlParserError */
	sax_fatal_error,		/* xmlParserFatalError */
	0,						/* getParameterEntity */
	sax_start_cdata,		/* cdataBlock */
	0,
	XML_SAX2_MAGIC,
	0,
	sax_start_element_ns,
	sax_end_element_ns,
	0
};

void sax_entity_decl(void *closure, const xmlChar *name, int type,  const xmlChar *publicId, const xmlChar *systemId, xmlChar *content)
{
	kdDebug(26001) << "LibXMLParser::sax_entity_decl " << CONV_STRING(name) << endl;

	GET_BUILDER

	if(type == XML_INTERNAL_GENERAL_ENTITY)
	{
		parser->setEntityMode(KDOM_INTERNAL_ENTITY);

		QString cont = CONV_STRING(content);
		bool deep = cont.contains('<');

		docBuilder->internalEntityDecl(CONV_STRING(name), cont, deep);
		if(deep)
		{
			parser->setEntityMode(KDOM_ENTITY_NEW_TREE);
			xmlSAXParseMemoryWithData(&KDOM_PARSER_SAX_HANDLER, cont.utf8(), cont.length(), 0, ctxt->_private);
		}
	}
	else
	{
		parser->setEntityMode(KDOM_EXTERNAL_ENTITY);
		docBuilder->externalEntityDecl(CONV_STRING(name), CONV_STRING(publicId), CONV_STRING(systemId));
	}

	xmlSAX2EntityDecl(closure, name, type, publicId, systemId, content);
}

#ifndef APPLE_COMPILE_HACK
// My external entity loader
xmlParserInputPtr xmlMyExternalEntityLoader(const char *URL, const char *, xmlParserCtxtPtr ctxt)
{
	QString qUrl = CONV_STRING(reinterpret_cast<const xmlChar *>(URL));
	if(qUrl.isEmpty())
		return 0;

	LibXMLParser *parser = static_cast<LibXMLParser *>(ctxt->_private);
	if(!parser)
	{
		xmlChar *xmlData = static_cast<xmlChar *>(xmlMalloc(2));

		xmlData[0] = ' ';
		xmlData[1] = '\0';
		
		return xmlNewStringInputStream(ctxt, xmlData);
	}
	
	QBuffer *buffer = KDOM::DataSlave::self()->newSyncJob(KURL(qUrl));
	if(!buffer)
		return 0;
	
	const char *data = buffer->buffer().data();
	unsigned int length = buffer->buffer().size();

	xmlChar *xmlData = static_cast<xmlChar *>(xmlMalloc(length + 1));
	memcpy(xmlData, data, length); 
	xmlData[length] = '\0'; // Important null-termination!

	delete buffer;
	return xmlNewStringInputStream(ctxt, xmlData);
}
#endif

LibXMLParser::LibXMLParser(const KURL &url) : Parser(url)
{
	m_incrementalParserContext = 0;

#ifndef APPLE_COMPILE_HACK
	xmlSetExternalEntityLoader(xmlMyExternalEntityLoader);
#endif
	xmlSubstituteEntitiesDefault(1);

	m_entityMode = KDOM_ENTITY_UNKNOWN;
}

LibXMLParser::~LibXMLParser()
{
}

void LibXMLParser::startParsing(bool incremental)
{
	kdDebug(26001) << "LibXMLParser::startParsing!" << endl;
	connect(this, SIGNAL(loadingFinished(QBuffer *)), this, SLOT(slotLoadingFinished(QBuffer *)));

	if(incremental)
	{
		connect(this, SIGNAL(feedData(const QByteArray &, bool)), this, SLOT(slotFeedData(const QByteArray &, bool)));

		xmlParserCtxtPtr parserCtxt = xmlCreatePushParserCtxt(&KDOM_PARSER_SAX_HANDLER, 0, 0, 0, 0);
		parserCtxt->_private = this;
		parserCtxt->recovery = 1;
		KDOM_PARSER_SAX_HANDLER.comment = (domConfig()->getParameter(FEATURE_COMMENTS)) ? sax_comment : 0;
		KDOM_PARSER_SAX_HANDLER.cdataBlock = (domConfig()->getParameter(FEATURE_CDATA_SECTIONS)) ? sax_start_cdata : 0;

		m_incrementalParserContext = parserCtxt;
	}

	Parser::startParsing(incremental);
}

Document LibXMLParser::parse(QBuffer *buffer)
{
	kdDebug(26001) << "LibXMLParser::startParsing!" << endl;
	connect(this, SIGNAL(loadingFinished(QBuffer *)), this, SLOT(slotLoadingFinished(QBuffer *)));

	return Parser::parse(buffer);
}

void LibXMLParser::slotFeedData(const QByteArray &data, bool eof)
{
	const char *rawData = data.data();
	unsigned int rawLength = data.count();

	xmlParseChunk(static_cast<xmlParserCtxtPtr>(m_incrementalParserContext), rawData, rawLength, (eof ? 0 : 1));

	if(eof)
	{
		xmlFreeParserCtxt(static_cast<xmlParserCtxtPtr>(m_incrementalParserContext));
		m_incrementalParserContext = 0;

		emit parsingFinished(false, "");
	}
}

void LibXMLParser::slotLoadingFinished(QBuffer *buffer)
{
	kdDebug(26001) << "LibXMLParser::slotLoadingFinished(): " << buffer << endl;

	if(!buffer)
		return;

	KDOM_PARSER_SAX_HANDLER.comment = (domConfig()->getParameter(FEATURE_COMMENTS)) ? sax_comment : 0;
	KDOM_PARSER_SAX_HANDLER.cdataBlock = (domConfig()->getParameter(FEATURE_CDATA_SECTIONS)) ? sax_start_cdata : 0;
	xmlParserCtxtPtr ctxt = xmlCreatePushParserCtxt(&KDOM_PARSER_SAX_HANDLER, 0, 0, 0, 0);
	ctxt->_private = this;
	ctxt->recovery = 1;

	const char *rawData = buffer->buffer().data();
	unsigned int rawLength = buffer->buffer().count();

	xmlParseChunk(ctxt, rawData, rawLength, 1);
	xmlFreeParserCtxt(ctxt);

	delete buffer;
}

void LibXMLParser::doOneShotParse(const char *rawData, unsigned int rawLength)
{
	KDOM_PARSER_SAX_HANDLER.comment = (domConfig()->getParameter(FEATURE_COMMENTS)) ? sax_comment : 0;
	KDOM_PARSER_SAX_HANDLER.cdataBlock = (domConfig()->getParameter(FEATURE_CDATA_SECTIONS)) ? sax_start_cdata : 0;
	xmlParserCtxtPtr ctxt = xmlCreatePushParserCtxt(&KDOM_PARSER_SAX_HANDLER, 0, 0, 0, 0);
	ctxt->_private = this;
	ctxt->recovery = 1;

	xmlParseChunk(ctxt, rawData, rawLength, 1);
	xmlFreeParserCtxt(ctxt);
}

void LibXMLParser::stopParsing(const DOMString &errorDescription)
{
#ifndef APPLE_COMPILE_HACK
	// Deregister our baseURL, when we're done with the current document!
	KDOM::DataSlave::self()->setBaseURL(KURL());
#endif

	kdDebug(26001) << "LibXMLParser::stopParsing: " << errorDescription << endl;
	emit parsingFinished(true, errorDescription);
}

void LibXMLParser::endDocument()
{
	documentBuilder()->endDocument();
	emit parsingFinished(false, "" /* DOMString::null needed? */);

#ifndef APPLE_COMPILE_HACK
	// Deregister our baseURL, when we're done with the current document!
	KDOM::DataSlave::self()->setBaseURL(KURL());
#endif
}

void LibXMLParser::tryEndEntityRef(const QString &name)
{
	if(!entityRef().isEmpty() && entityRef() == name)
	{
		documentBuilder()->entityReferenceEnd(entityRef());
		setEntityRef(QString(""));
	}
}

// vim:ts=4:noet
