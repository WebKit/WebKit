/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
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
 * $Id$
 */

#ifndef _XML_Tokenizer_h_
#define _XML_Tokenizer_h_

#include "html/htmltokenizer.h"

#include <qxml.h>
#include <qlist.h>
#include <qvaluelist.h>

namespace DOM {
    class DocumentImpl;
    class NodeImpl;
    class HTMLScriptElementImpl;
}



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

    bool fatalError( const QXmlParseException& exception );

    unsigned long errorLine;
    unsigned long errorCol;

private:
    QString errorProt;
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
    QValueList<DOMString> sheetElemId;
};


class XMLTokenizer : public Tokenizer, public khtml::CachedObjectClient
{
    Q_OBJECT
public:
    XMLTokenizer(DOM::DocumentPtr *, KHTMLView * = 0);
    virtual ~XMLTokenizer();
    virtual void begin();
    virtual void write( const QString &str, bool );
    virtual void end();
    virtual void finish();

    // from CachedObjectClient
    void notifyFinished(khtml::CachedObject *finishedObj);

protected:
    DocumentPtr *m_doc;
    KHTMLView *m_view;

    void executeScripts();
    void addScripts(DOM::NodeImpl *n);

    QString m_xmlCode;
    QList<HTMLScriptElementImpl> m_scripts;
    QListIterator<HTMLScriptElementImpl> *m_scriptsIt;
    khtml::CachedScript *m_cachedScript;


};

#endif
