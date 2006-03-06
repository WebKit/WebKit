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

#include "KURL.h"
#include "TransferJobClient.h"
#include <kxmlcore/HashMap.h>
#include <kxmlcore/HashSet.h>

namespace WebCore {

  class Decoder;
  class DocumentImpl;
  class EventListener;
  class String;

  // these exact numeric values are important because JS expects them
  enum XMLHttpRequestState {
    Uninitialized = 0,  // open() has not been called yet
    Loading = 1,        // send() has not been called yet
    Loaded = 2,         // send() has been called, headers and status are available
    Interactive = 3,    // Downloading, responseText holds the partial data
    Completed = 4       // Finished with all operations
  };

  class XMLHttpRequest : public Shared<XMLHttpRequest>, TransferJobClient {
  public:
    XMLHttpRequest(DocumentImpl*);
    ~XMLHttpRequest();

    static void detachRequests(DocumentImpl*);
    static void cancelRequests(DocumentImpl*);

    String getStatusText() const;
    int getStatus() const;
    XMLHttpRequestState getReadyState() const;
    void open(const String& method, const KURL& url, bool async, const String& user, const String& password);
    void send(const String& body);
    void abort();
    void setRequestHeader(const String& name, const String &value);
    void overrideMIMEType(const String& override);
    String getAllResponseHeaders() const;
    String getResponseHeader(const String& name) const;
    String getResponseText() const;
    DocumentImpl* getResponseXML() const;

    void setOnReadyStateChangeListener(EventListener*);
    EventListener* onReadyStateChangeListener() const;
    void setOnLoadListener(EventListener*);
    EventListener* onLoadListener() const;

  private:
    bool urlMatchesDocumentDomain(const KURL&) const;

    virtual void receivedRedirect(TransferJob*, const KURL&);
    virtual void receivedData(TransferJob*, const char *data, int size);
    virtual void receivedAllData(TransferJob*);

    void processSyncLoadResults(const ByteArray& data, const KURL& finalURL, const QString& headers);

    bool responseIsXML() const;
    
    QString getRequestHeader(const QString& name) const;
    static QString getSpecificHeader(const QString& headers, const QString& name);

    void changeState(XMLHttpRequestState newState);
    void callReadyStateChangeListener();

    DocumentImpl* doc;
    RefPtr<EventListener> m_onReadyStateChangeListener;
    RefPtr<EventListener> m_onLoadListener;

    KURL url;
    QString method;
    bool async;
    QString requestHeaders;

    TransferJob* job;

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

} // namespace

#endif
