// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003 Apple Computer, Inc.
 *  Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _XMLHTTPREQUEST_H_
#define _XMLHTTPREQUEST_H_

#include <qguardedptr.h>
#include <qobject.h>
#include <kurl.h>
#include <qptrdict.h>

#include "kjs_dom.h"

namespace khtml {
    class Decoder;
}

namespace KIO {
    class Job;
    class TransferJob;
}

namespace KJS {

  class JSUnprotectedEventListener;
  class XMLHttpRequestQObject;

  // these exact numeric values are important because JS expects them
  enum XMLHttpRequestState {
    Uninitialized = 0,  // open() has not been called yet
    Loading = 1,        // send() has not been called yet
    Loaded = 2,         // send() has been called, headers and status are available
    Interactive = 3,    // Downloading, responseText holds the partial data
    Completed = 4       // Finished with all operations
  };

  class XMLHttpRequestConstructorImp : public JSObject {
  public:
    XMLHttpRequestConstructorImp(ExecState *exec, DOM::DocumentImpl *d);
    ~XMLHttpRequestConstructorImp();
    virtual bool implementsConstruct() const;
    virtual JSObject *construct(ExecState *exec, const List &args);
  private:
    RefPtr<DOM::DocumentImpl> doc;
  };

  class XMLHttpRequest : public DOMObject {
  public:
    XMLHttpRequest(ExecState *, DOM::DocumentImpl *d);
    ~XMLHttpRequest();
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual void put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr = None);
    void putValueProperty(ExecState *exec, int token, JSValue *value, int /*attr*/);
    virtual bool toBoolean(ExecState *) const { return true; }
    virtual void mark();

    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { Onload, Onreadystatechange, ReadyState, ResponseText, ResponseXML, Status,
        StatusText, Abort, GetAllResponseHeaders, GetResponseHeader, Open, Send, SetRequestHeader,
        OverrideMIMEType };

    static void cancelRequests(DOM::DocumentImpl *d);

  private:
    friend class XMLHttpRequestProtoFunc;
    friend class XMLHttpRequestQObject;

    JSValue *getStatusText() const;
    JSValue *getStatus() const;
    bool urlMatchesDocumentDomain(const KURL&) const;

    XMLHttpRequestQObject *qObject;

    void slotData( KIO::Job* job, const char *data, int size );
    void slotFinished( KIO::Job* );
    void slotRedirection( KIO::Job*, const KURL& );

    void processSyncLoadResults(const QByteArray &data, const KURL &finalURL, const QString &headers);

    void open(const QString& _method, const KURL& _url, bool _async);
    void send(const QString& _body);
    void abort();
    void setRequestHeader(const QString& name, const QString &value);
    QString getRequestHeader(const QString& name) const;
    JSValue *getAllResponseHeaders() const;
    QString getResponseHeader(const QString& name) const;
    bool responseIsXML() const;
    
    static QString getSpecificHeader(const QString& headers, const QString& name);

    void changeState(XMLHttpRequestState newState);

    static QPtrDict< QPtrDict<XMLHttpRequest> > &requestsByDocument();
    void addToRequestsByDocument();
    void removeFromRequestsByDocument();

    QGuardedPtr<DOM::DocumentImpl> doc;

    KURL url;
    QString method;
    bool async;
    QString requestHeaders;

    KIO::TransferJob * job;

    XMLHttpRequestState state;
    RefPtr<JSUnprotectedEventListener> onReadyStateChangeListener;
    RefPtr<JSUnprotectedEventListener> onLoadListener;

    RefPtr<khtml::Decoder> decoder;
    QString encoding;
    QString responseHeaders;
    QString MIMETypeOverride;

    QString response;
    mutable bool createdDocument;
    mutable bool typeIsXML;
    mutable RefPtr<DOM::DocumentImpl> responseXML;

    bool aborted;
  };


  class XMLHttpRequestQObject : public QObject {
    Q_OBJECT
	
  public:
    XMLHttpRequestQObject(XMLHttpRequest *_jsObject);

  public slots:
    void slotData( KIO::Job* job, const char *data, int size );
    void slotFinished( KIO::Job* job );
    void slotRedirection( KIO::Job* job, const KURL& url);

  private:
    XMLHttpRequest *jsObject;
  };

} // namespace

#endif
