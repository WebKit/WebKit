/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2003 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef _XML_Tokenizer_h_
#define _XML_Tokenizer_h_

#include <qxml.h>
#include <qptrlist.h>
#include <qobject.h>
#include "misc/loader_client.h"
#include "misc/stringit.h"

#if APPLE_CHANGES
#include "KWQSignal.h"
#endif

class KHTMLView;

namespace DOM {
    class DocumentImpl;
    class NodeImpl;
    class HTMLScriptElementImpl;
    class DocumentPtr;
    class HTMLScriptElementImpl;
};

namespace khtml {
    
class CachedObject;
class CachedScript;

class XMLHandler : public QXmlDefaultHandler
{
public:
    XMLHandler(DOM::DocumentPtr *_doc, KHTMLView *_view);
    virtual ~XMLHandler();

    // return the error protocol if parsing failed
    QString errorProtocol();

    // overloaded handler functions
    bool startDocument();
    bool startElement(const QString& namespaceURI, const QString& localName, const QString& qName, const QXmlAttributes& atts);
    bool endElement(const QString& namespaceURI, const QString& localName, const QString& qName);
    bool startCDATA();
    bool endCDATA();
    bool characters(const QString& ch);
    bool comment(const QString & ch);
    bool processingInstruction(const QString &target, const QString &data);


    // from QXmlDeclHandler
    bool attributeDecl(const QString &eName, const QString &aName, const QString &type, const QString &valueDefault, const QString &value);
    bool externalEntityDecl(const QString &name, const QString &publicId, const QString &systemId);
    bool internalEntityDecl(const QString &name, const QString &value);

    // from QXmlDTDHandler
    bool notationDecl(const QString &name, const QString &publicId, const QString &systemId);
    bool unparsedEntityDecl(const QString &name, const QString &publicId, const QString &systemId, const QString &notationName);

    bool enterText();
    void exitText();

    QString errorString();

    bool error( const QXmlParseException& exception );
    bool fatalError( const QXmlParseException& exception );
    bool warning( const QXmlParseException& exception );
    
    int errorLine;
    int errorCol;

protected:
    QString errorProt;
    int m_errorCount;
    DOM::DocumentPtr *m_doc;
    KHTMLView *m_view;
    DOM::NodeImpl *m_currentNode;
    DOM::NodeImpl *m_rootNode;

    enum State {
        StateInit,
        StateDocument,
        StateQuote,
        StateLine,
        StateHeading,
        StateP
    };
    State state;
};

class Tokenizer : public QObject
{
    Q_OBJECT
public:
    virtual void begin() = 0;
    // script output must be prepended, while new data
    // received during executing a script must be appended, hence the
    // extra bool to be able to distinguish between both cases. document.write()
    // always uses false, while khtmlpart uses true
    virtual void write(const TokenizerString &str, bool appendData) = 0;
    virtual void end() = 0;
    virtual void finish() = 0;
    virtual void setOnHold(bool /*_onHold*/) {}
    virtual bool isWaitingForScripts() = 0;

signals:
    void finishedParsing();

#if APPLE_CHANGES
public:
    Tokenizer();
private:
    KWQSignal m_finishedParsing;
#endif
};

class XMLTokenizer : public Tokenizer, public CachedObjectClient
{
public:
    XMLTokenizer(DOM::DocumentPtr *, KHTMLView * = 0);
    virtual ~XMLTokenizer();
    virtual void begin();
    virtual void write(const TokenizerString &str, bool);
    virtual void end();
    virtual void finish();

    // from CachedObjectClient
    void notifyFinished(CachedObject *finishedObj);

    virtual bool isWaitingForScripts();
protected:
    DOM::DocumentPtr *m_doc;
    KHTMLView *m_view;

    void executeScripts();
    void addScripts(DOM::NodeImpl *n);

    QString m_xmlCode;
    QPtrList<DOM::HTMLScriptElementImpl> m_scripts;
    QPtrListIterator<DOM::HTMLScriptElementImpl> *m_scriptsIt;
    CachedScript *m_cachedScript;
};

}

#endif
