/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import "KWQXml.h"

#import "KWQAssertions.h"

#import "KWQString.h"

QString QXmlAttributes::value(const QString &) const
{
    ERROR("not yet implemented");
    return QString();
}

int QXmlAttributes::length() const
{
    ERROR("not yet implemented");
    return 0;
}

QString QXmlAttributes::localName(int index) const
{
    ERROR("not yet implemented");
    return QString();
}

QString QXmlAttributes::value(int index) const
{
    ERROR("not yet implemented");
    return QString();
}

QString QXmlAttributes::uri(int) const
{
    ERROR("not yet implemented");
    return QString();
}

void QXmlInputSource::setData(const QString& data)
{
    ERROR("not yet implemented");
}

void QXmlSimpleReader::setContentHandler(QXmlContentHandler *handler)
{
    ERROR("not yet implemented");
}

bool QXmlSimpleReader::parse(const QXmlInputSource &input)
{
    ERROR("not yet implemented");
    return FALSE;
}

void QXmlSimpleReader::setLexicalHandler(QXmlLexicalHandler *handler)
{
    ERROR("not yet implemented");
}

void QXmlSimpleReader::setDTDHandler(QXmlDTDHandler *handler)
{
    ERROR("not yet implemented");
}

void QXmlSimpleReader::setDeclHandler(QXmlDeclHandler *handler)
{
    ERROR("not yet implemented");
}

void QXmlSimpleReader::setErrorHandler(QXmlErrorHandler *handler)
{
    ERROR("not yet implemented");
}


QString QXmlParseException::message() const
{
    ERROR("not yet implemented");
    return QString();
}

int QXmlParseException::columnNumber() const
{
    ERROR("not yet implemented");
    return 0;
}

int QXmlParseException::lineNumber() const
{
    ERROR("not yet implemented");
    return 0;
}

bool QXmlDefaultHandler::startDocument()
{
    return true;
}

bool QXmlDefaultHandler::endDocument()
{
    return true;
}

bool QXmlDefaultHandler::startPrefixMapping(const QString &prefix, const QString &URI)
{
    return true;
}

bool QXmlDefaultHandler::endPrefixMapping(const QString &prefix)
{
    return true;
}

bool QXmlDefaultHandler::startElement(const QString &namespaceURI, const QString &localName, const QString &qName, const QXmlAttributes &attributes)
{
    return true;
}

bool QXmlDefaultHandler::endElement(const QString &namespaceURI, const QString &localName, const QString &qName)
{
    return true;
}

bool QXmlDefaultHandler::characters(const QString &characters)
{
    return true;
}

bool QXmlDefaultHandler::ignorableWhitespace(const QString &characters)
{
    return true;
}

bool QXmlDefaultHandler::processingInstruction(const QString &target, const QString &data)
{
    return true;
}

bool QXmlDefaultHandler::skippedEntity(const QString &name)
{
    return true;
}

bool QXmlDefaultHandler::startDTD(const QString &name, const QString &publicId, const QString &systemId)
{
    return true;
}

bool QXmlDefaultHandler::endDTD()
{
    return true;
}

bool QXmlDefaultHandler::startEntity(const QString &name)
{
    return true;
}

bool QXmlDefaultHandler::endEntity(const QString &name)
{
    return true;
}

bool QXmlDefaultHandler::startCDATA()
{
    return true;
}

bool QXmlDefaultHandler::endCDATA()
{
    return true;
}

bool QXmlDefaultHandler::comment(const QString &characters)
{
    return true;
}

bool QXmlDefaultHandler::warning(const QXmlParseException &exception)
{
    return true;
}

bool QXmlDefaultHandler::error(const QXmlParseException &exception)
{
    return true;
}

bool QXmlDefaultHandler::fatalError(const QXmlParseException &exception)
{
    return true;
}

bool QXmlDefaultHandler::attributeDecl(const QString &entityName, const QString &attributeName, const QString &type, const QString &valueDefault, const QString &value)
{
    return true;
}

bool QXmlDefaultHandler::externalEntityDecl(const QString &name, const QString &publicId, const QString &systemId)
{
    return true;
}

bool QXmlDefaultHandler::internalEntityDecl(const QString &name, const QString &value)
{
    return true;
}

bool QXmlDefaultHandler::notationDecl(const QString& name, const QString& publicId, const QString& systemId)
{
    return true;
}

bool QXmlDefaultHandler::unparsedEntityDecl(const QString& name, const QString& publicId, const QString& systemId, const QString& notationName)
{
    return true;
}

QString QXmlDefaultHandler::errorString()
{
    return QString();
}
