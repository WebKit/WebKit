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

#ifndef KWQXMLSIMPLEREADER_H
#define KWQXMLSIMPLEREADER_H

#include "KWQString.h"

class QXmlAttributes;

class QXmlInputSource {
public:
    void setData(const QString &data) { _data = data; }
    QString data() const { return _data; }
private:
    QString _data;
};

class QXmlParseException {
public:
    QString message() const;
    int columnNumber() const;
    int lineNumber() const;
};

class QXmlContentHandler {
public:
    virtual bool startDocument() = 0;
    virtual bool endDocument() = 0;
    virtual bool startPrefixMapping(const QString &prefix, const QString &URI) = 0;
    virtual bool endPrefixMapping(const QString &prefix) = 0;
    virtual bool startElement(const QString &namespaceURI, const QString &localName, const QString &qName, const QXmlAttributes &attributes) = 0;
    virtual bool endElement(const QString &namespaceURI, const QString &localName, const QString &qName) = 0;
    virtual bool characters(const QString &characters) = 0;
    virtual bool ignorableWhitespace(const QString &characters) = 0;
    virtual bool processingInstruction(const QString &target, const QString &data) = 0;
    virtual bool skippedEntity(const QString &name) = 0;
    virtual QString errorString() = 0;
};

class QXmlLexicalHandler {
public:
    virtual bool startDTD(const QString &name, const QString &publicId, const QString &systemId) = 0;
    virtual bool endDTD() = 0;
    virtual bool startEntity(const QString &name) = 0;
    virtual bool endEntity(const QString &name) = 0;
    virtual bool startCDATA() = 0;
    virtual bool endCDATA() = 0;
    virtual bool comment(const QString &characters) = 0;
    virtual QString errorString() = 0;
};

class QXmlErrorHandler {
public:
    virtual bool warning(const QXmlParseException &exception) = 0;
    virtual bool error(const QXmlParseException &exception) = 0;
    virtual bool fatalError(const QXmlParseException &exception) = 0;
    virtual QString errorString() = 0;
};

class QXmlDeclHandler {
public:
    virtual bool attributeDecl(const QString &entityName, const QString &attributeName, const QString &type, const QString &valueDefault, const QString &value) = 0;
    virtual bool externalEntityDecl(const QString &name, const QString &publicId, const QString &systemId) = 0;
    virtual bool internalEntityDecl(const QString &name, const QString &value) = 0;
};

class QXmlDTDHandler {
public:
    virtual bool notationDecl(const QString& name, const QString& publicId, const QString& systemId) = 0;
    virtual bool unparsedEntityDecl(const QString& name, const QString& publicId, const QString& systemId, const QString& notationName) = 0;
    virtual QString errorString() = 0;
};

class QXmlSimpleReader {
public:
    QXmlSimpleReader();
    
    void setContentHandler(QXmlContentHandler *handler) { _contentHandler = handler; }
    void setDeclHandler(QXmlDeclHandler *handler) { _declarationHandler= handler; }
    void setDTDHandler(QXmlDTDHandler *handler) { _DTDHandler = handler; }
    void setErrorHandler(QXmlErrorHandler *handler) { _errorHandler = handler; }
    void setLexicalHandler(QXmlLexicalHandler *handler) { _lexicalHandler = handler; }
    
    QXmlContentHandler *contentHandler() const { return _contentHandler; }
    QXmlLexicalHandler *lexicalHandler() const { return _lexicalHandler; }

    bool parse(const QXmlInputSource &input);

private:
    QXmlContentHandler *_contentHandler;
    QXmlDeclHandler *_declarationHandler;
    QXmlDTDHandler *_DTDHandler;
    QXmlErrorHandler *_errorHandler;
    QXmlLexicalHandler *_lexicalHandler;
};

#endif
