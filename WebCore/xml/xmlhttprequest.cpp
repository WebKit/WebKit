/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2004 Apple Computer, Inc.
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

#include "config.h"
#include "xmlhttprequest.h"

#include "kjs/protect.h"

#include "dom/dom_exception.h"
#include "dom/dom_string.h"
#include "dom/dom2_events.h"
#include "Cache.h"
#include "html/html_documentimpl.h"
#include "misc/formdata.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/EventNames.h"
#include "DOMImplementationImpl.h"

#include <kio/job.h>
#include <qobject.h>
#include <qregexp.h>
#include <qtextcodec.h>

#include "KWQLoader.h"

using namespace WebCore::EventNames;

static inline QString getMIMEType(const QString& contentTypeString)
{
    return QStringList::split(";", contentTypeString, true)[0].stripWhiteSpace();
}

static QString getCharset(const QString& contentTypeString)
{
    int pos = 0;
    int length = (int)contentTypeString.length();
    
    while (pos < length) {
        pos = contentTypeString.find("charset", pos, false);
        if (pos <= 0)
            return QString();
        
        // is what we found a beginning of a word?
        if (contentTypeString[pos-1] > ' ' && contentTypeString[pos-1] != ';') {
            pos += 7;
            continue;
        }
        
        pos += 7;

        // skip whitespace
        while (pos != length && contentTypeString[pos] <= ' ')
            ++pos;
    
        if (contentTypeString[pos++] != '=') // this "charset" substring wasn't a parameter name, but there may be others
            continue;

        while (pos != length && (contentTypeString[pos] <= ' ' || contentTypeString[pos] == '"' || contentTypeString[pos] == '\''))
            ++pos;

        // we don't handle spaces within quoted parameter values, because charset names cannot have any
        int endpos = pos;
        while (pos != length && contentTypeString[endpos] > ' ' && contentTypeString[endpos] != '"' && contentTypeString[endpos] != '\'' && contentTypeString[endpos] != ';')
            ++endpos;
    
        return contentTypeString.mid(pos, endpos-pos);
    }
    
    return QString();
}

namespace WebCore {

XMLHttpRequestQObject::XMLHttpRequestQObject(XMLHttpRequest *_jsObject) 
{
  jsObject = _jsObject; 
}

void XMLHttpRequestQObject::slotData( KIO::Job* job, const char *data, int size )
{
  jsObject->slotData(job, data, size);
}

void XMLHttpRequestQObject::slotFinished( KIO::Job* job )
{
  jsObject->slotFinished(job); 
}

void XMLHttpRequestQObject::slotRedirection( KIO::Job* job, const KURL& url)
{ 
  jsObject->slotRedirection( job, url ); 
}

XMLHttpRequestState XMLHttpRequest::getReadyState() const
{
    return state;
}

DOMString XMLHttpRequest::getResponseText() const
{
    return response;
}

PassRefPtr<DocumentImpl> XMLHttpRequest::getResponseXML() const
{
    if (state != Completed)
      return 0;

    if (!createdDocument) {
      if (typeIsXML = responseIsXML()) {
        responseXML = doc->implementation()->createDocument();

        DocumentImpl *docImpl = responseXML.get();
        
        docImpl->open();
        docImpl->write(response);
        docImpl->finishParsing();
        docImpl->close();
      }
      createdDocument = true;
    }

    if (!typeIsXML)
      return 0;

    return responseXML;
}

PassRefPtr<EventListener> XMLHttpRequest::getOnReadyStateChangeListener() const
{
    return onReadyStateChangeListener;
}

void XMLHttpRequest::setOnReadyStateChangeListener(EventListener* eventListener)
{
    onReadyStateChangeListener = eventListener;
}

PassRefPtr<EventListener> XMLHttpRequest::getOnLoadListener() const
{
    return onLoadListener;
}

void XMLHttpRequest::setOnLoadListener(EventListener* eventListener)
{
    onLoadListener = eventListener;
}

XMLHttpRequest::XMLHttpRequest(DocumentImpl *d)
  : qObject(new XMLHttpRequestQObject(this)),
    doc(d),
    async(true),
    job(0),
    state(Uninitialized),
    createdDocument(false),
    aborted(false)
{
}

XMLHttpRequest::~XMLHttpRequest()
{
    delete qObject;
}

void XMLHttpRequest::changeState(XMLHttpRequestState newState)
{
  if (state != newState) {
    state = newState;
    
    if (doc && doc->frame() && onReadyStateChangeListener) {
      int ignoreException;
      RefPtr<EventImpl> ev = doc->createEvent("HTMLEvents", ignoreException);
      ev->initEvent(readystatechangeEvent, true, true);
      onReadyStateChangeListener->handleEventImpl(ev.get(), true);
    }
    
    if (doc && doc->frame() && state == Completed && onLoadListener) {
      int ignoreException;
      RefPtr<EventImpl> ev = doc->createEvent("HTMLEvents", ignoreException);
      ev->initEvent(loadEvent, true, true);
      onLoadListener->handleEventImpl(ev.get(), true);
    }
  }
}

bool XMLHttpRequest::urlMatchesDocumentDomain(const KURL& _url) const
{
  KURL documentURL(doc->URL());

  // a local file can load anything
  if (documentURL.protocol().lower() == "file") {
    return true;
  }

  // but a remote document can only load from the same port on the server
  if (documentURL.protocol().lower() == _url.protocol().lower() &&
      documentURL.host().lower() == _url.host().lower() &&
      documentURL.port() == _url.port()) {
    return true;
  }

  return false;
}

void XMLHttpRequest::open(const DOMString& _method, const KURL& _url, bool _async, const DOMString& user, const DOMString& password)
{
  abort();
  aborted = false;

  // clear stuff from possible previous load
  requestHeaders = QString();
  responseHeaders = QString();
  response = QString();
  createdDocument = false;
  responseXML = 0;

  changeState(Uninitialized);

  if (aborted) {
    return;
  }

  if (!urlMatchesDocumentDomain(_url)) {
    return;
  }

  url = _url;

  if (!user.isNull())
    url.setUser(user.qstring());

  if (!password.isNull())
    url.setPass(password.qstring());

  method = _method.qstring();
  async = _async;

  changeState(Loading);
}

void XMLHttpRequest::send(const DOMString& _body)
{
  if (!doc)
    return;

  if (state != Loading)
    return;
  
  // FIXME: Should this abort instead if we already have a job going?
  if (job)
    return;

  aborted = false;

  if (method.lower() == "post" && (url.protocol().lower() == "http" || url.protocol().lower() == "https") ) {
      QString contentType = getRequestHeader("Content-Type");
      QString charset;
      if (contentType.isEmpty())
        setRequestHeader("Content-Type", "application/xml");
      else
        charset = getCharset(contentType);
      
      if (charset.isEmpty())
        charset = "UTF-8";
      
      QTextCodec *codec = QTextCodec::codecForName(charset.latin1());
      if (!codec)   // FIXME: report an error?
        codec = QTextCodec::codecForName("UTF-8");

      job = KIO::http_post(url, codec->fromUnicode(_body.qstring()), false);
  }
  else
     job = KIO::get( url, false, false );
  if (requestHeaders.length() > 0)
    job->addMetaData("customHTTPHeader", requestHeaders);

  if (!async) {
    ByteArray data;
    KURL finalURL;
    QString headers;

    { // scope
        // avoid deadlock in case the loader wants to use JS on a background thread
        KJS::JSLock::DropAllLocks dropLocks;

        data = KWQServeSynchronousRequest(khtml::Cache::loader(), doc->docLoader(), job, finalURL, headers);
    }

    job = 0;
    processSyncLoadResults(data, finalURL, headers);
    
    return;
  }

  ref(); // this object should not be deleted while a request is in progress
  
  qObject->connect( job, SIGNAL( result( KIO::Job* ) ),
                    SLOT( slotFinished( KIO::Job* ) ) );
  qObject->connect( job, SIGNAL( data( KIO::Job*, const char*, int ) ),
                    SLOT( slotData( KIO::Job*, const char*, int ) ) );
  qObject->connect( job, SIGNAL(redirection(KIO::Job*, const KURL& ) ),
                    SLOT( slotRedirection(KIO::Job*, const KURL&) ) );

  addToRequestsByDocument();

  KWQServeRequest(khtml::Cache::loader(), doc->docLoader(), job);
}

void XMLHttpRequest::abort()
{
  bool hadJob = job;

  if (hadJob) {
    removeFromRequestsByDocument();
    job->kill();
    job = 0;
  }
  decoder = 0;
  aborted = true;

  if (hadJob)
    deref();
}

void XMLHttpRequest::overrideMIMEType(const DOMString& override)
{
  MIMETypeOverride = override.qstring();
}

void XMLHttpRequest::setRequestHeader(const DOMString& name, const DOMString &value)
{
  if (requestHeaders.length() > 0) {
    requestHeaders += "\r\n";
  }
  requestHeaders += name.qstring();
  requestHeaders += ": ";
  requestHeaders += value.qstring();
}

QString XMLHttpRequest::getRequestHeader(const QString& name) const
{
  return getSpecificHeader(requestHeaders, name);
}

DOMString XMLHttpRequest::getAllResponseHeaders() const
{
  if (responseHeaders.isEmpty()) {
    return DOMString();
  }

  int endOfLine = responseHeaders.find("\n");

  if (endOfLine == -1) {
    return DOMString();
  }

  return responseHeaders.mid(endOfLine + 1) + "\n";
}

DOMString XMLHttpRequest::getResponseHeader(const DOMString& name) const
{
  return getSpecificHeader(responseHeaders, name.qstring());
}

QString XMLHttpRequest::getSpecificHeader(const QString& headers, const QString& name)
{
  if (headers.isEmpty())
    return QString();

  QRegExp headerLinePattern(name + ":", false);

  int matchLength;
  int headerLinePos = headerLinePattern.match(headers, 0, &matchLength);
  while (headerLinePos != -1) {
    if (headerLinePos == 0 || headers[headerLinePos-1] == '\n') {
      break;
    }
    
    headerLinePos = headerLinePattern.match(headers, headerLinePos + 1, &matchLength);
  }

  if (headerLinePos == -1)
    return QString();
    
  int endOfLine = headers.find("\n", headerLinePos + matchLength);
  
  return headers.mid(headerLinePos + matchLength, endOfLine - (headerLinePos + matchLength)).stripWhiteSpace();
}

bool XMLHttpRequest::responseIsXML() const
{
    QString mimeType = getMIMEType(MIMETypeOverride);
    if (mimeType.isEmpty())
        mimeType = getMIMEType(getResponseHeader("Content-Type").qstring());
    if (mimeType.isEmpty())
      mimeType = "text/xml";
    
    return DOMImplementationImpl::isXMLMIMEType(mimeType);
}

int XMLHttpRequest::getStatus() const
{
  if (responseHeaders.isEmpty())
    return -1;
  
  int endOfLine = responseHeaders.find("\n");
  QString firstLine = endOfLine == -1 ? responseHeaders : responseHeaders.left(endOfLine);
  int codeStart = firstLine.find(" ");
  int codeEnd = firstLine.find(" ", codeStart + 1);
  
  if (codeStart == -1 || codeEnd == -1)
    return -1;
  
  QString number = firstLine.mid(codeStart + 1, codeEnd - (codeStart + 1));
  
  bool ok = false;
  int code = number.toInt(&ok);
  if (!ok)
    return -1;

  return code;
}

DOMString XMLHttpRequest::getStatusText() const
{
  if (responseHeaders.isEmpty()) {
    return DOMString();
  }
  
  int endOfLine = responseHeaders.find("\n");
  QString firstLine = endOfLine == -1 ? responseHeaders : responseHeaders.left(endOfLine);
  int codeStart = firstLine.find(" ");
  int codeEnd = firstLine.find(" ", codeStart + 1);

  if (codeStart == -1 || codeEnd == -1) {
    return DOMString();
  }
  
  QString statusText = firstLine.mid(codeEnd + 1, endOfLine - (codeEnd + 1)).stripWhiteSpace();
  
  return DOMString(statusText);
}

void XMLHttpRequest::processSyncLoadResults(const ByteArray &data, const KURL &finalURL, const QString &headers)
{
  if (!urlMatchesDocumentDomain(finalURL)) {
    abort();
    return;
  }
  
  responseHeaders = headers;
  changeState(Loaded);
  if (aborted) {
    return;
  }
  
  const char *bytes = (const char *)data.data();
  int len = (int)data.size();

  slotData(0, bytes, len);

  if (aborted) {
    return;
  }

  slotFinished(0);
}

void XMLHttpRequest::slotFinished(KIO::Job *)
{
  if (responseHeaders.isEmpty() && job)
    responseHeaders = job->queryMetaData("HTTP-Headers");

  if (state < Loaded)
    changeState(Loaded);

  if (decoder)
    response += decoder->flush();

  bool hadJob = job;
  removeFromRequestsByDocument();
  job = 0;

  changeState(Completed);
  decoder = 0;

  if (hadJob)
    deref();
}

void XMLHttpRequest::slotRedirection(KIO::Job*, const KURL& url)
{
  if (!urlMatchesDocumentDomain(url)) {
    abort();
  }
}

void XMLHttpRequest::slotData(KIO::Job*, const char *data, int len)
{
  if (responseHeaders.isEmpty() && job)
    responseHeaders = job->queryMetaData("HTTP-Headers");

  if (state < Loaded)
    changeState(Loaded);
  
  if (!decoder) {
    encoding = getCharset(MIMETypeOverride);
    if (encoding.isEmpty())
      encoding = getCharset(getResponseHeader("Content-Type").qstring());
    if (encoding.isEmpty() && job)
      encoding = job->queryMetaData("charset");
    
    decoder = new Decoder;
    if (!encoding.isEmpty())
      decoder->setEncoding(encoding.latin1(), Decoder::EncodingFromHTTPHeader);
    else {
      // only allow Decoder to look inside the response if it's XML
      decoder->setEncoding("UTF-8", responseIsXML() ? Decoder::DefaultEncoding : Decoder::EncodingFromHTTPHeader);
    }
  }
  if (len == 0)
    return;

  if (len == -1)
    len = strlen(data);

  QString decoded = decoder->decode(data, len);

  response += decoded;

  if (!aborted) {
    changeState(Interactive);
  }
}

XMLHttpRequest::RequestsMap& XMLHttpRequest::requestsByDocument()
{
    static RequestsMap map;
    return map;
}

void XMLHttpRequest::addToRequestsByDocument()
{
  assert(doc);

  RequestsSet* requests = requestsByDocument().get(doc);
  if (!requests) {
    requests = new RequestsSet;
    requestsByDocument().set(doc, requests);
  }

  assert(!requests->contains(this));
  requests->insert(this);
}

void XMLHttpRequest::removeFromRequestsByDocument()
{
  assert(doc);

  RequestsSet* requests = requestsByDocument().get(doc);

  // Since synchronous loads are not added to requestsByDocument(), we need to make sure we found the request.
  if (!requests || !requests->contains(this))
    return;

  requests->remove(this);

  if (requests->isEmpty()) {
    requestsByDocument().remove(doc);
    delete requests;
  }
}

void XMLHttpRequest::cancelRequests(DocumentImpl* d)
{
  while (RequestsSet* requests = requestsByDocument().get(d))
    (*requests->begin())->abort();
}

} // end namespace
