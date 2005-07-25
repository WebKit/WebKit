// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2004 Apple Computer, Inc.
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

#include "xmlhttprequest.h"

#include "kjs_window.h"
#include "kjs_events.h"

#include "dom/dom_exception.h"
#include "dom/dom_string.h"
#include "misc/loader.h"
#include "html/html_documentimpl.h"
#include "xml/dom2_eventsimpl.h"

#include "khtml_part.h"
#include "khtmlview.h"

#include <kdebug.h>
#include <kio/job.h>
#include <qobject.h>
#include <qregexp.h>

#ifdef APPLE_CHANGES
#include "KWQLoader.h"
#endif

#include "xmlhttprequest.lut.h"

using DOM::DocumentImpl;
using DOM::EventImpl;

using khtml::Decoder;

namespace KJS {

////////////////////// XMLHttpRequest Object ////////////////////////

/* Source for XMLHttpRequestProtoTable.
@begin XMLHttpRequestProtoTable 7
  abort			XMLHttpRequest::Abort			DontDelete|Function 0
  getAllResponseHeaders	XMLHttpRequest::GetAllResponseHeaders	DontDelete|Function 0
  getResponseHeader	XMLHttpRequest::GetResponseHeader	DontDelete|Function 1
  open			XMLHttpRequest::Open			DontDelete|Function 5
  overrideMimeType      XMLHttpRequest::OverrideMIMEType        DontDelete|Function 1
  send			XMLHttpRequest::Send			DontDelete|Function 1
  setRequestHeader	XMLHttpRequest::SetRequestHeader	DontDelete|Function 2
@end
*/
DEFINE_PROTOTYPE("XMLHttpRequest",XMLHttpRequestProto)
IMPLEMENT_PROTOFUNC(XMLHttpRequestProtoFunc)
IMPLEMENT_PROTOTYPE(XMLHttpRequestProto,XMLHttpRequestProtoFunc)

XMLHttpRequestQObject::XMLHttpRequestQObject(XMLHttpRequest *_jsObject) 
{
  jsObject = _jsObject; 
}

#if APPLE_CHANGES
void XMLHttpRequestQObject::slotData( KIO::Job* job, const char *data, int size )
{
  jsObject->slotData(job, data, size);
}
#else
void XMLHttpRequestQObject::slotData( KIO::Job* job, const QByteArray &data )
{
  jsObject->slotData(job, data);
}
#endif

void XMLHttpRequestQObject::slotFinished( KIO::Job* job )
{
  jsObject->slotFinished(job); 
}

void XMLHttpRequestQObject::slotRedirection( KIO::Job* job, const KURL& url)
{ 
  jsObject->slotRedirection( job, url ); 
}

XMLHttpRequestConstructorImp::XMLHttpRequestConstructorImp(ExecState *, DOM::DocumentImpl *d)
    : doc(d)
{
}

XMLHttpRequestConstructorImp::~XMLHttpRequestConstructorImp()
{
}

bool XMLHttpRequestConstructorImp::implementsConstruct() const
{
  return true;
}

Object XMLHttpRequestConstructorImp::construct(ExecState *exec, const List &)
{
  return Object(new XMLHttpRequest(exec, doc.get()));
}

const ClassInfo XMLHttpRequest::info = { "XMLHttpRequest", 0, &XMLHttpRequestTable, 0 };

/* Source for XMLHttpRequestTable.
@begin XMLHttpRequestTable 7
  readyState		XMLHttpRequest::ReadyState		DontDelete|ReadOnly
  responseText		XMLHttpRequest::ResponseText		DontDelete|ReadOnly
  responseXML		XMLHttpRequest::ResponseXML		DontDelete|ReadOnly
  status		XMLHttpRequest::Status			DontDelete|ReadOnly
  statusText		XMLHttpRequest::StatusText		DontDelete|ReadOnly
  onreadystatechange	XMLHttpRequest::Onreadystatechange	DontDelete
  onload		XMLHttpRequest::Onload			DontDelete
@end
*/

bool XMLHttpRequest::getOwnProperty(ExecState *exec, const Identifier &propertyName, Value& result) const
{
  return lookupGetOwnValue<XMLHttpRequest,DOMObject>(exec, propertyName, &XMLHttpRequestTable, this, result);
}

Value XMLHttpRequest::getValueProperty(ExecState *exec, int token) const
{
  switch (token) {
  case ReadyState:
    return Number(state);
  case ResponseText:
    return getStringOrNull(DOM::DOMString(response));
  case ResponseXML:
    if (state != Completed) {
      return Undefined();
    }
    if (!createdDocument) {
      QString mimeType;
      
      if (MIMETypeOverride.isEmpty()) {
        Value header = getResponseHeader("Content-Type");
        if (header.type() == UndefinedType) {
          mimeType = "text/xml";
        } else {
	  mimeType = QStringList::split(";", header.toString(exec).qstring())[0].stripWhiteSpace();
        }
      } else {
        mimeType = MIMETypeOverride;
      }
      
      if (mimeType == "text/xml" || mimeType == "application/xml" || mimeType == "application/xhtml+xml" ||
          mimeType == "text/xsl" || mimeType == "application/rss+xml" || mimeType == "application/atom+xml") {
	responseXML.reset(doc->implementation()->createDocument());

	DocumentImpl *docImpl = responseXML.get();
	
	docImpl->open();
	docImpl->write(response);
	docImpl->finishParsing();
	docImpl->close();
	typeIsXML = true;
      } else {
	typeIsXML = false;
      }
      createdDocument = true;
    }

    if (!typeIsXML) {
      return Undefined();
    }

    return getDOMNode(exec, responseXML.get());
  case Status:
    return getStatus();
  case StatusText:
    return getStatusText();
  case Onreadystatechange:
   if (onReadyStateChangeListener && onReadyStateChangeListener->listenerObjImp()) {
     return onReadyStateChangeListener->listenerObj();
   } else {
     return Null();
   }
  case Onload:
   if (onLoadListener && onLoadListener->listenerObjImp()) {
     return onLoadListener->listenerObj();
   } else {
     return Null();
   }
  default:
    kdWarning() << "XMLHttpRequest::getValueProperty unhandled token " << token << endl;
    return Value();
  }
}

void XMLHttpRequest::put(ExecState *exec, const Identifier &propertyName, const Value& value, int attr)
{
  lookupPut<XMLHttpRequest,DOMObject>(exec, propertyName, value, attr, &XMLHttpRequestTable, this );
}

void XMLHttpRequest::putValueProperty(ExecState *exec, int token, const Value& value, int /*attr*/)
{
  switch(token) {
  case Onreadystatechange:
    onReadyStateChangeListener = Window::retrieveActive(exec)->getJSUnprotectedEventListener(value, true);
    if (onReadyStateChangeListener) onReadyStateChangeListener->ref();
    break;
  case Onload:
    onLoadListener = Window::retrieveActive(exec)->getJSUnprotectedEventListener(value, true);
    if (onLoadListener) onLoadListener->ref();
    break;
  default:
    kdWarning() << "HTMLDocument::putValueProperty unhandled token " << token << endl;
  }
}

void XMLHttpRequest::mark()
{
  DOMObject::mark();

  if (onReadyStateChangeListener)
    onReadyStateChangeListener->mark();

  if (onLoadListener)
    onLoadListener->mark();
}


XMLHttpRequest::XMLHttpRequest(ExecState *exec, DOM::DocumentImpl *d)
  : qObject(new XMLHttpRequestQObject(this)),
    doc(d),
    async(true),
    job(0),
    state(Uninitialized),
    onReadyStateChangeListener(0),
    onLoadListener(0),
    decoder(0),
    createdDocument(false),
    aborted(false)
{
  setPrototype(XMLHttpRequestProto::self(exec));
}

XMLHttpRequest::~XMLHttpRequest()
{
  delete qObject;
  if (decoder) {
    decoder->deref();
  }
}

void XMLHttpRequest::changeState(XMLHttpRequestState newState)
{
  if (state != newState) {
    state = newState;
    
    if (onReadyStateChangeListener != 0 && doc->part()) {
      int ignoreException;
      EventImpl *ev = doc->createEvent("HTMLEvents", ignoreException);
      ev->ref();
      ev->initEvent("readystatechange", true, true);
      onReadyStateChangeListener->handleEventImpl(ev, true);
      ev->deref();
    }
    
    if (state == Completed && onLoadListener != 0 && doc->part()) {
      int ignoreException;
      EventImpl *ev = doc->createEvent("HTMLEvents", ignoreException);
      ev->ref();
      ev->initEvent("load", true, true);
      onLoadListener->handleEventImpl(ev, true);
      ev->deref();
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

void XMLHttpRequest::open(const QString& _method, const KURL& _url, bool _async)
{
  abort();
  aborted = false;

  // clear stuff from possible previous load
  requestHeaders = QString();
  responseHeaders = QString();
  response = QString();
  createdDocument = false;
  responseXML.reset();

  changeState(Uninitialized);

  if (aborted) {
    return;
  }

  if (!urlMatchesDocumentDomain(_url)) {
    return;
  }

  
  method = _method;
  url = _url;
  async = _async;

  changeState(Loading);
}

void XMLHttpRequest::send(const QString& _body)
{
  if (!doc)
    return;

  // FIXME: Should this abort instead if we already have a job going?
  if (job)
    return;

  aborted = false;

#if !APPLE_CHANGES
  if (!async) {
    return;
  }
#endif

  if (method.lower() == "post" && (url.protocol().lower() == "http" || url.protocol().lower() == "https") ) {
      // FIXME: determine post encoding correctly by looking in headers for charset
      job = KIO::http_post( url, _body.utf8(), false );
  }
  else
  {
     job = KIO::get( url, false, false );
  }
  if (requestHeaders.length() > 0) {
    job->addMetaData("customHTTPHeader", requestHeaders);
  }

#if APPLE_CHANGES
  if (!async) {
    QByteArray data;
    KURL finalURL;
    QString headers;

    data = KWQServeSynchronousRequest(khtml::Cache::loader(), doc->docLoader(), job, finalURL, headers);
    job = 0;
    processSyncLoadResults(data, finalURL, headers);
    
    return;
  }
#endif

  gcProtect (this);
  
  qObject->connect( job, SIGNAL( result( KIO::Job* ) ),
		    SLOT( slotFinished( KIO::Job* ) ) );
#if APPLE_CHANGES
  qObject->connect( job, SIGNAL( data( KIO::Job*, const char*, int ) ),
		    SLOT( slotData( KIO::Job*, const char*, int ) ) );
#else
  qObject->connect( job, SIGNAL( data( KIO::Job*, const QByteArray& ) ),
		    SLOT( slotData( KIO::Job*, const QByteArray& ) ) );
#endif
  qObject->connect( job, SIGNAL(redirection(KIO::Job*, const KURL& ) ),
		    SLOT( slotRedirection(KIO::Job*, const KURL&) ) );

  addToRequestsByDocument();

#ifdef APPLE_CHANGES
  KWQServeRequest(khtml::Cache::loader(), doc->docLoader(), job);
#else 
  KIO::Scheduler::scheduleJob( job );
#endif
}

void XMLHttpRequest::abort()
{
  bool hadJob = job;

  if (hadJob) {
    removeFromRequestsByDocument();
    job->kill();
    job = 0;
  }
  if (decoder) {
    decoder->deref();
    decoder = 0;
  }
  aborted = true;

  if (hadJob)
    gcUnprotect(this);
}

void XMLHttpRequest::setRequestHeader(const QString& name, const QString &value)
{
  if (requestHeaders.length() > 0) {
    requestHeaders += "\r\n";
  }
  requestHeaders += name;
  requestHeaders += ": ";
  requestHeaders += value;
}

Value XMLHttpRequest::getAllResponseHeaders() const
{
  if (responseHeaders.isEmpty()) {
    return Undefined();
  }

  int endOfLine = responseHeaders.find("\n");

  if (endOfLine == -1) {
    return Undefined();
  }

  return String(responseHeaders.mid(endOfLine + 1) + "\n");
}

Value XMLHttpRequest::getResponseHeader(const QString& name) const
{
  if (responseHeaders.isEmpty()) {
    return Undefined();
  }

  QRegExp headerLinePattern(name + ":", false);

  int matchLength;
  int headerLinePos = headerLinePattern.match(responseHeaders, 0, &matchLength);
  while (headerLinePos != -1) {
    if (headerLinePos == 0 || responseHeaders[headerLinePos-1] == '\n') {
      break;
    }
    
    headerLinePos = headerLinePattern.match(responseHeaders, headerLinePos + 1, &matchLength);
  }


  if (headerLinePos == -1) {
    return Undefined();
  }
    
  int endOfLine = responseHeaders.find("\n", headerLinePos + matchLength);

  return String(responseHeaders.mid(headerLinePos + matchLength, endOfLine - (headerLinePos + matchLength)).stripWhiteSpace());
}

Value XMLHttpRequest::getStatus() const
{
  if (responseHeaders.isEmpty()) {
    return Undefined();
  }
  
  int endOfLine = responseHeaders.find("\n");
  QString firstLine = endOfLine == -1 ? responseHeaders : responseHeaders.left(endOfLine);
  int codeStart = firstLine.find(" ");
  int codeEnd = firstLine.find(" ", codeStart + 1);
  
  if (codeStart == -1 || codeEnd == -1) {
    return Undefined();
  }
  
  QString number = firstLine.mid(codeStart + 1, codeEnd - (codeStart + 1));
  
  bool ok = false;
  int code = number.toInt(&ok);
  if (!ok) {
    return Undefined();
  }

  return Number(code);
}

Value XMLHttpRequest::getStatusText() const
{
  if (responseHeaders.isEmpty()) {
    return Undefined();
  }
  
  int endOfLine = responseHeaders.find("\n");
  QString firstLine = endOfLine == -1 ? responseHeaders : responseHeaders.left(endOfLine);
  int codeStart = firstLine.find(" ");
  int codeEnd = firstLine.find(" ", codeStart + 1);

  if (codeStart == -1 || codeEnd == -1) {
    return Undefined();
  }
  
  QString statusText = firstLine.mid(codeEnd + 1, endOfLine - (codeEnd + 1)).stripWhiteSpace();
  
  return String(statusText);
}

#if APPLE_CHANGES   
void XMLHttpRequest::processSyncLoadResults(const QByteArray &data, const KURL &finalURL, const QString &headers)
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
#endif

void XMLHttpRequest::slotFinished(KIO::Job *)
{
  if (decoder) {
    response += decoder->flush();
  }

  removeFromRequestsByDocument();
  job = 0;

  changeState(Completed);
  
  if (decoder) {
    decoder->deref();
    decoder = 0;
  }

  gcUnprotect (this);
}

void XMLHttpRequest::slotRedirection(KIO::Job*, const KURL& url)
{
  if (!urlMatchesDocumentDomain(url)) {
    abort();
  }
}

#if APPLE_CHANGES
void XMLHttpRequest::slotData( KIO::Job*, const char *data, int len )
#else
void XMLHttpRequest::slotData(KIO::Job*, const QByteArray &_data)
#endif
{
  if (state < Loaded) {
    responseHeaders = job->queryMetaData("HTTP-Headers");
    encoding = job->queryMetaData("charset");
    changeState(Loaded);
  }
  
#if !APPLE_CHANGES
  const char *data = (const char *)_data.data();
  int len = (int)_data.size();
#endif

  if ( decoder == NULL ) {
    decoder = new Decoder;
    if (!encoding.isNull())
      decoder->setEncoding(encoding.latin1(), Decoder::EncodingFromHTTPHeader);
    else {
      // FIXME: Inherit the default encoding from the parent document?
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

QPtrDict< QPtrDict<XMLHttpRequest> > &XMLHttpRequest::requestsByDocument()
{
    static QPtrDict< QPtrDict<XMLHttpRequest> > dictionary;
    return dictionary;
}

void XMLHttpRequest::addToRequestsByDocument()
{
  assert(doc);

  QPtrDict<XMLHttpRequest> *requests = requestsByDocument().find(doc);
  if (!requests) {
    requests = new QPtrDict<XMLHttpRequest>;
    requestsByDocument().insert(doc, requests);
  }

  assert(requests->find(this) == 0);
  requests->insert(this, this);
}

void XMLHttpRequest::removeFromRequestsByDocument()
{
  assert(doc);

  QPtrDict<XMLHttpRequest> *requests = requestsByDocument().find(doc);

  // Since synchronous loads are not added to requestsByDocument(), we need to make sure we found the request.
  if (!requests || !requests->find(this))
    return;

  requests->remove(this);

  if (requests->isEmpty()) {
    requestsByDocument().remove(doc);
    delete requests;
  }
}

void XMLHttpRequest::cancelRequests(DOM::DocumentImpl *d)
{
  while (QPtrDict<XMLHttpRequest> *requests = requestsByDocument().find(d))
    QPtrDictIterator<XMLHttpRequest>(*requests).current()->abort();
}

Value XMLHttpRequestProtoFunc::call(ExecState *exec, Object &thisObj, const List &args)
{
  if (!thisObj.inherits(&XMLHttpRequest::info)) {
    Object err = Error::create(exec,TypeError);
    exec->setException(err);
    return err;
  }

  XMLHttpRequest *request = static_cast<XMLHttpRequest *>(thisObj.imp());

  switch (id) {
  case XMLHttpRequest::Abort: {
    request->abort();
    return Undefined();
  }
  case XMLHttpRequest::GetAllResponseHeaders: {
    if (args.size() != 0) {
      return Undefined();
    }

    return request->getAllResponseHeaders();
  }
  case XMLHttpRequest::GetResponseHeader: {
    if (args.size() != 1) {
      return Undefined();
    }

    return request->getResponseHeader(args[0].toString(exec).qstring());
  }
  case XMLHttpRequest::Open:
    {
      if (args.size() < 2 || args.size() > 5) {
	return Undefined();
      }
    
      QString method = args[0].toString(exec).qstring();
      KURL url = KURL(Window::retrieveActive(exec)->part()->xmlDocImpl()->completeURL(args[1].toString(exec).qstring()));

      bool async = true;
      if (args.size() >= 3) {
	async = args[2].toBoolean(exec);
      }
    
      if (args.size() >= 4) {
	url.setUser(args[3].toString(exec).qstring());
      }
      
      if (args.size() >= 5) {
	url.setPass(args[4].toString(exec).qstring());
      }

      request->open(method, url, async);

      return Undefined();
    }
  case XMLHttpRequest::Send:
    {
      if (args.size() > 1) {
	return Undefined();
      }

      if (request->state != Loading) {
	return Undefined();
      }

      QString body;

      if (args.size() >= 1) {
	if (args[0].toObject(exec).inherits(&DOMDocument::info)) {
	  DocumentImpl *doc = static_cast<DocumentImpl *>(static_cast<DOMDocument *>(args[0].toObject(exec).imp())->impl());
          body = doc->toString().string();
          // FIXME: also need to set content type, including encoding!
	} else {
	  // converting certain values (like null) to object can set an exception
	  exec->clearException();
	  body = args[0].toString(exec).qstring();
	}
      }

      request->send(body);

      return Undefined();
    }
  case XMLHttpRequest::SetRequestHeader: {
    if (args.size() != 2) {
      return Undefined();
    }
    
    request->setRequestHeader(args[0].toString(exec).qstring(), args[1].toString(exec).qstring());
    
    return Undefined();
  }
  case XMLHttpRequest::OverrideMIMEType: {
    if (args.size() != 1) {
      return Undefined();
    }
    request->MIMETypeOverride = args[0].toString(exec).qstring();
    return Undefined();
  }
  }

  return Undefined();
}

} // end namespace
