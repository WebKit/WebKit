/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "KWQXmlSimpleReader.h"

#include <libxml/parser.h>
#include <libxml/parserInternals.h>

#include "KWQAssertions.h"
#include "KWQXmlAttributes.h"

static void startElementHandler(void *userData, const xmlChar *name, const xmlChar **libxmlAttributes)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    
    if (reader->parserStopped()) {
        return;
    }

    QXmlAttributes attributes(reinterpret_cast<const char **>(libxmlAttributes));
    KWQXmlNamespace* ns = reader->pushNamespaces(attributes);
    attributes.split(ns);
    
    QString qName = QString::fromUtf8(reinterpret_cast<const char *>(name));
    QString localName;
    QString uri;
    QString prefix;
    int colonPos = qName.find(':');
    if (colonPos != -1) {
        localName = qName.right(qName.length() - colonPos - 1);
        prefix = qName.left(colonPos);
    }
    else
        localName = qName;
    uri = reader->xmlNamespace()->uriForPrefix(prefix);
    
    // We pass in the namespace of the element, and then the name both with and without
    // the namespace prefix.
    reader->contentHandler()->startElement(uri, localName, qName, attributes);
}

static void endElementHandler(void *userData, const xmlChar *name)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    if (reader->parserStopped()) {
        return;
    }
    QString qName = QString::fromUtf8(reinterpret_cast<const char *>(name));
    QString localName;
    QString uri;
    QString prefix;
    int colonPos = qName.find(':');
    if (colonPos != -1) {
        localName = qName.right(qName.length() - colonPos - 1);
        prefix = qName.left(colonPos);
    }
    else
        localName = qName;
    uri = reader->xmlNamespace()->uriForPrefix(prefix);
    
    KWQXmlNamespace* ns = reader->popNamespaces();
    if (ns)
        ns->deref();
    
    reader->contentHandler()->endElement(uri, localName, qName);
}

static void charactersHandler(void *userData, const xmlChar *s, int len)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    if (reader->parserStopped()) {
        return;
    }
    reader->contentHandler()->characters(QString::fromUtf8(reinterpret_cast<const char *>(s), len));
}

static void processingInstructionHandler(void *userData, const xmlChar *target, const xmlChar *data)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    if (reader->parserStopped()) {
        return;
    }
    reader->contentHandler()->processingInstruction(
        QString::fromUtf8(reinterpret_cast<const char *>(target)),
        QString::fromUtf8(reinterpret_cast<const char *>(data)));
}

static void cdataBlockHandler(void *userData, const xmlChar *s, int len)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    if (reader->parserStopped()) {
        return;
    }
    reader->lexicalHandler()->startCDATA();
    reader->contentHandler()->characters(QString::fromUtf8(reinterpret_cast<const char *>(s), len));
    reader->lexicalHandler()->endCDATA();
}

static void commentHandler(void *userData, const xmlChar *comment)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    reader->lexicalHandler()->comment(QString::fromUtf8(reinterpret_cast<const char *>(comment)));
}

static void warningHandler(void *userData, const char *message, ...)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    if (reader->parserStopped()) {
        return;
    }
    if (reader->errorHandler()) {
        char *m;
        va_list args;
        va_start(args, message);
        vasprintf(&m, message, args);
        va_end(args);
        if (!reader->errorHandler()->warning(QXmlParseException(m, reader->columnNumber(), reader->lineNumber()))) {
            reader->stopParsing();
        }
        free(m);
    }
}

static void fatalErrorHandler(void *userData, const char *message, ...)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    if (reader->parserStopped()) {
        return;
    }
    if (!reader->errorHandler()) {
        reader->stopParsing();
    } else {
        char *m;
        va_list args;
        va_start(args, message);
        vasprintf(&m, message, args);
        va_end(args);
        if (!reader->errorHandler()->fatalError(QXmlParseException(m, reader->columnNumber(), reader->lineNumber()))) {
            reader->stopParsing();
        }
        reader->recordError();
        free(m);
    }
}

static void normalErrorHandler(void *userData, const char *message, ...)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    if (reader->parserStopped()) {
        return;
    }
    if (!reader->errorHandler()) {
        reader->stopParsing();
    } else {
        char *m;
        va_list args;
        va_start(args, message);
        vasprintf(&m, message, args);
        va_end(args);
        if (!reader->errorHandler()->error(QXmlParseException(m, reader->columnNumber(), reader->lineNumber()))) {
            reader->stopParsing();
        }
        reader->recordError();
        free(m);
    }
}

QXmlSimpleReader::QXmlSimpleReader()
    : _contentHandler(0)
    , _declarationHandler(0)
    , _DTDHandler(0)
    , _errorHandler(0)
    , _lexicalHandler(0)
{
}

KWQXmlNamespace* QXmlSimpleReader::pushNamespaces(QXmlAttributes& attrs)
{
    KWQXmlNamespace* ns = m_namespaceStack.current();
    if (!ns)
        ns = new KWQXmlNamespace();
    
    // Search for any xmlns attributes.
    for (int i = 0; i < attrs.length(); i++) {
        QString qName = attrs.qName(i);
        if (qName == "xmlns")
            ns = new KWQXmlNamespace(QString::null, attrs.value(i), ns);
        else if (qName.startsWith("xmlns:"))
            ns = new KWQXmlNamespace(qName.right(qName.length()-6), attrs.value(i), ns);
    }
    
    m_namespaceStack.push(ns);
    ns->ref();
    return ns;
}

KWQXmlNamespace* QXmlSimpleReader::popNamespaces()
{
    return m_namespaceStack.pop();
}

bool QXmlSimpleReader::parse(const QXmlInputSource &input)
{
    if (_contentHandler && !_contentHandler->startDocument()) {
        return false;
    }

    static bool didInit = false;
    if (!didInit) {
        xmlInitParser();
        didInit = true;
    }
    xmlSAXHandler handler;
    memset(&handler, 0, sizeof(handler));
    handler.error = normalErrorHandler;
    handler.fatalError = fatalErrorHandler;
    if (_contentHandler) {
        handler.characters = charactersHandler;
        handler.endElement = endElementHandler;
        handler.processingInstruction = processingInstructionHandler;
        handler.startElement = startElementHandler;
    }
    if (_lexicalHandler) {
        handler.cdataBlock = cdataBlockHandler;
        handler.comment = commentHandler;
    }
    if (_errorHandler) {
        handler.warning = warningHandler;
    }
    m_parserStopped = false;
    m_sawError = false;
    m_context = xmlCreatePushParserCtxt(&handler, this, NULL, 0, NULL);
    const QChar BOM(0xFEFF);
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char *>(&BOM);
    xmlSwitchEncoding(m_context, BOMHighByte == 0xFF ? XML_CHAR_ENCODING_UTF16LE : XML_CHAR_ENCODING_UTF16BE);
    xmlParseChunk(m_context,
        reinterpret_cast<const char *>(input.data().unicode()),
        input.data().length() * sizeof(QChar), 1);
    xmlFreeParserCtxt(m_context);
    m_context = NULL;
    return !m_sawError;
}

void QXmlSimpleReader::stopParsing()
{
    xmlStopParser(m_context);
    m_parserStopped = true;
}

int QXmlSimpleReader::lineNumber() const
{
    return m_context->input->line;
}

int QXmlSimpleReader::columnNumber() const
{
    return m_context->input->col;
}
