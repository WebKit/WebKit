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

#ifndef KWQXMLDEFAULTHANDLER_H
#define KWQXMLDEFAULTHANDLER_H

#include "KWQXmlSimpleReader.h"

class QXmlDefaultHandler :
    public QXmlContentHandler, 
    public QXmlLexicalHandler, 
    public QXmlErrorHandler, 
    public QXmlDeclHandler, 
    public QXmlDTDHandler
{
    virtual bool startDocument();
    virtual bool endDocument();
    virtual bool startPrefixMapping(const QString &prefix, const QString &URI);
    virtual bool endPrefixMapping(const QString &prefix);
    virtual bool startElement(const QString &namespaceURI, const QString &localName, const QString &qName, const QXmlAttributes &attributes);
    virtual bool endElement(const QString &namespaceURI, const QString &localName, const QString &qName);
    virtual bool characters(const QString &characters);
    virtual bool ignorableWhitespace(const QString &characters);
    virtual bool processingInstruction(const QString &target, const QString &data);
    virtual bool skippedEntity(const QString &name);

    virtual bool startDTD(const QString &name, const QString &publicId, const QString &systemId);
    virtual bool endDTD();
    virtual bool startEntity(const QString &name);
    virtual bool endEntity(const QString &name);
    virtual bool startCDATA();
    virtual bool endCDATA();
    virtual bool comment(const QString &characters);

    virtual bool warning(const QXmlParseException &exception);
    virtual bool error(const QXmlParseException &exception);
    virtual bool fatalError(const QXmlParseException &exception);

    virtual bool attributeDecl(const QString &entityName, const QString &attributeName, const QString &type, const QString &valueDefault, const QString &value);
    virtual bool externalEntityDecl(const QString &name, const QString &publicId, const QString &systemId);
    virtual bool internalEntityDecl(const QString &name, const QString &value);

    virtual bool notationDecl(const QString& name, const QString& publicId, const QString& systemId);
    virtual bool unparsedEntityDecl(const QString& name, const QString& publicId, const QString& systemId, const QString& notationName);

    virtual QString errorString();
};

#endif
