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
#include "KWQPtrStack.h"

struct KWQXmlNamespace {
    QString m_prefix;
    QString m_uri;
    KWQXmlNamespace* m_parent;
    
    int m_ref;
    
    KWQXmlNamespace() :m_parent(0), m_ref(0) {}
    
    KWQXmlNamespace(const QString& p, const QString& u, KWQXmlNamespace* parent) 
        :m_prefix(p),
         m_uri(u),
         m_parent(parent), 
         m_ref(0) 
    { 
        if (m_parent) m_parent->ref();
    }
    
    QString uriForPrefix(const QString& prefix) {
        if (prefix == m_prefix)
            return m_uri;
        if (m_parent)
            return m_parent->uriForPrefix(prefix);
        return "";
    }
    
    void ref() { m_ref++; }
    void deref() { if (--m_ref == 0) { if (m_parent) m_parent->deref(); delete this; } }
};

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
    QXmlParseException(const QString &m, int c, int l) : m_message(m), m_column(c), m_line(l) { }
    QString message() const { return m_message; }
    int columnNumber() const { return m_column; }
    int lineNumber() const { return m_line; }
private:
    QString m_message;
    int m_column;
    int m_line;
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
    QXmlErrorHandler *errorHandler() const { return _errorHandler; }
    QXmlLexicalHandler *lexicalHandler() const { return _lexicalHandler; }

    bool parse(const QXmlInputSource &input);

    KWQXmlNamespace* pushNamespaces(QXmlAttributes& attributes);
    KWQXmlNamespace* popNamespaces();
    KWQXmlNamespace* xmlNamespace() { return m_namespaceStack.current(); }

    bool parserStopped() const { return m_parserStopped; }
    void stopParsing();

    bool sawError() const { return m_sawError; }
    void recordError() { m_sawError = true; }
    
    int lineNumber() const;
    int columnNumber() const;
    
private:
    QXmlContentHandler *_contentHandler;
    QXmlDeclHandler *_declarationHandler;
    QXmlDTDHandler *_DTDHandler;
    QXmlErrorHandler *_errorHandler;
    QXmlLexicalHandler *_lexicalHandler;
    QPtrStack<KWQXmlNamespace> m_namespaceStack;
    struct _xmlParserCtxt *m_context;
    bool m_parserStopped : 1;
    bool m_sawError : 1;
};

#endif
