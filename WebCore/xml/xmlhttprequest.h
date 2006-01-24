// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003, 2006 Apple Computer, Inc.
 *  Copyright (C) 2005, 2006 Alexey Proskuryakov <ap@nypop.com>
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

#ifndef XMLHTTPREQUEST_H_
#define XMLHTTPREQUEST_H_

#include <kurl.h>
#include <kxmlcore/HashMap.h>
#include <kxmlcore/HashSet.h>
#include <qguardedptr.h>
#include <qobject.h>

namespace KIO {
    class Job;
    class TransferJob;
}

namespace WebCore {

  class Decoder;
  class DocumentImpl;
  class EventListener;
  class XMLHttpRequestQObject;

  // these exact numeric values are important because JS expects them
  enum XMLHttpRequestState {
    Uninitialized = 0,  // open() has not been called yet
    Loading = 1,        // send() has not been called yet
    Loaded = 2,         // send() has been called, headers and status are available
    Interactive = 3,    // Downloading, responseText holds the partial data
    Completed = 4       // Finished with all operations
  };

  class XMLHttpRequest : public Shared<XMLHttpRequest> {
  public:
    XMLHttpRequest(DocumentImpl* d);
    ~XMLHttpRequest();

    static void cancelRequests(DocumentImpl *d);

    DOMString getStatusText() const;
    int getStatus() const;
    XMLHttpRequestState getReadyState() const;
    void open(const DOMString& method, const KURL& url, bool async, const DOMString& user, const DOMString& password);
    void send(const DOMString& _body);
    void abort();
    void setRequestHeader(const DOMString& name, const DOMString &value);
    void overrideMIMEType(const DOMString& override);
    DOMString getAllResponseHeaders() const;
    DOMString getResponseHeader(const DOMString& name) const;
    DOMString getResponseText() const;
    DocumentImpl* getResponseXML() const;

    void setOnReadyStateChangeListener(EventListener*);
    EventListener* onReadyStateChangeListener() const;
    void setOnLoadListener(EventListener*);
    EventListener* onLoadListener() const;

  private:
    friend class XMLHttpRequestQObject;

    bool urlMatchesDocumentDomain(const KURL&) const;

    XMLHttpRequestQObject* qObject;

    void slotData(KIO::Job*, const char *data, int size);
    void slotFinished(KIO::Job*);
    void slotRedirection(KIO::Job*, const KURL&);

    void processSyncLoadResults(const ByteArray& data, const KURL& finalURL, const QString& headers);

    bool responseIsXML() const;
    
    QString getRequestHeader(const QString& name) const;
    static QString getSpecificHeader(const QString& headers, const QString& name);

    void changeState(XMLHttpRequestState newState);

    typedef HashSet<XMLHttpRequest*> RequestsSet;
    typedef HashMap<DocumentImpl*, RequestsSet*> RequestsMap;
    static RequestsMap &requestsByDocument();
    void addToRequestsByDocument();
    void removeFromRequestsByDocument();

    QGuardedPtr<DocumentImpl> doc;
    RefPtr<EventListener> m_onReadyStateChangeListener;
    RefPtr<EventListener> m_onLoadListener;

    KURL url;
    QString method;
    bool async;
    QString requestHeaders;

    KIO::TransferJob* job;

    XMLHttpRequestState state;

    RefPtr<Decoder> decoder;
    QString encoding;
    QString responseHeaders;
    QString MIMETypeOverride;

    QString response;
    mutable bool createdDocument;
    mutable RefPtr<DocumentImpl> responseXML;

    bool aborted;
  };

  class XMLHttpRequestQObject : public QObject {
  public:
    XMLHttpRequestQObject(XMLHttpRequest* r) { m_request = r; }

  public slots:
    void slotData(KIO::Job* job, const char* data, int size) { m_request->slotData(job, data, size); }
    void slotFinished(KIO::Job* job) { m_request->slotFinished(job); }
    void slotRedirection(KIO::Job* job, const KURL& url) { m_request->slotRedirection(job, url); }

  private:
    XMLHttpRequest* m_request;
  };

} // namespace

#endif
