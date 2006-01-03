/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2004 Apple Computer, Inc.
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

#include "config.h"
#include "xmlhttprequest.h"

#include "kjs_window.h"
#include "kjs_events.h"

#include "dom/dom_exception.h"
#include "dom/dom_string.h"
#include "misc/loader.h"
#include "html/html_documentimpl.h"
#include "xml/dom2_eventsimpl.h"
#include "xml/EventNames.h"

#include "khtml_part.h"
#include "khtmlview.h"

#include <kdebug.h>
#include <kio/job.h>
#include <qobject.h>
#include <qregexp.h>

#include "KWQLoader.h"

#include "xmlhttprequest.lut.h"

using namespace DOM;
using namespace DOM::EventNames;

using khtml::Decoder;

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
KJS_DEFINE_PROTOTYPE(XMLHttpRequestProto)
KJS_IMPLEMENT_PROTOFUNC(XMLHttpRequestProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("XMLHttpRequest",XMLHttpRequestProto,XMLHttpRequestProtoFunc)

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

XMLHttpRequestConstructorImp::XMLHttpRequestConstructorImp(ExecState *exec, DOM::DocumentImpl *d)
    : doc(d)
{
    putDirect(prototypePropertyName, XMLHttpRequestProto::self(exec), DontEnum|DontDelete|ReadOnly);
}

XMLHttpRequestConstructorImp::~XMLHttpRequestConstructorImp()
{
}

bool XMLHttpRequestConstructorImp::implementsConstruct() const
{
  return true;
}

JSObject *XMLHttpRequestConstructorImp::construct(ExecState *exec, const List &)
{
  return new XMLHttpRequest(exec, doc.get());
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

bool XMLHttpRequest::getOwnPropertySlot(ExecState *exec, const Identifier& propertyName, PropertySlot& slot)
{
  return getStaticValueSlot<XMLHttpRequest, DOMObject>(exec, &XMLHttpRequestTable, this, propertyName, slot);
}

JSValue *XMLHttpRequest::getValueProperty(ExecState *exec, int token) const
{
  switch (token) {
  case ReadyState:
    return jsNumber(state);
  case ResponseText:
    return jsStringOrNull(DOM::DOMString(response));
  case ResponseXML:
    if (state != Completed) {
      return jsUndefined();
    }

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

    if (!typeIsXML) {
      return jsUndefined();
    }

    return getDOMNode(exec, responseXML.get());
  case Status:
    return getStatus();
  case StatusText:
    return getStatusText();
  case Onreadystatechange:
   if (onReadyStateChangeListener && onReadyStateChangeListener->listenerObj()) {
     return onReadyStateChangeListener->listenerObj();
   } else {
     return jsNull();
   }
  case Onload:
   if (onLoadListener && onLoadListener->listenerObj()) {
     return onLoadListener->listenerObj();
   } else {
     return jsNull();
   }
  default:
    kdWarning() << "XMLHttpRequest::getValueProperty unhandled token " << token << endl;
    return NULL;
  }
}

void XMLHttpRequest::put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr)
{
  lookupPut<XMLHttpRequest,DOMObject>(exec, propertyName, value, attr, &XMLHttpRequestTable, this );
}

void XMLHttpRequest::putValueProperty(ExecState *exec, int token, JSValue *value, int /*attr*/)
{
  switch(token) {
  case Onreadystatechange:
    onReadyStateChangeListener = Window::retrieveActive(exec)->getJSUnprotectedEventListener(value, true);
    break;
  case Onload:
    onLoadListener = Window::retrieveActive(exec)->getJSUnprotectedEventListener(value, true);
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
    createdDocument(false),
    aborted(false)
{
  setPrototype(XMLHttpRequestProto::self(exec));
}

XMLHttpRequest::~XMLHttpRequest()
{
    delete qObject;
}

void XMLHttpRequest::changeState(XMLHttpRequestState newState)
{
  if (state != newState) {
    state = newState;
    
    if (doc && doc->part() && onReadyStateChangeListener) {
      int ignoreException;
      RefPtr<EventImpl> ev = doc->createEvent("HTMLEvents", ignoreException);
      ev->initEvent(readystatechangeEvent, true, true);
      onReadyStateChangeListener->handleEventImpl(ev.get(), true);
    }
    
    if (doc && doc->part() && state == Completed && onLoadListener) {
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

void XMLHttpRequest::open(const QString& _method, const KURL& _url, bool _async)
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

  
  method = _method;
  url = _url;
  async = _async;

  changeState(Loading);
}

void XMLHttpRequest::send(const DOMString& _body)
{
  if (!doc)
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
    QByteArray data;
    KURL finalURL;
    QString headers;

    { // scope
        // avoid deadlock in case the loader wants to use JS on a background thread
        JSLock::DropAllLocks dropLocks;

        data = KWQServeSynchronousRequest(khtml::Cache::loader(), doc->docLoader(), job, finalURL, headers);
    }

    job = 0;
    processSyncLoadResults(data, finalURL, headers);
    
    return;
  }

  {
    JSLock lock;
    gcProtect(this);
  }
  
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

  if (hadJob) {
    JSLock lock;
    gcUnprotect(this);
  }
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

QString XMLHttpRequest::getRequestHeader(const QString& name) const
{
  return getSpecificHeader(requestHeaders, name);
}

JSValue *XMLHttpRequest::getAllResponseHeaders() const
{
  if (responseHeaders.isEmpty()) {
    return jsUndefined();
  }

  int endOfLine = responseHeaders.find("\n");

  if (endOfLine == -1) {
    return jsUndefined();
  }

  return jsString(responseHeaders.mid(endOfLine + 1) + "\n");
}

QString XMLHttpRequest::getResponseHeader(const QString& name) const
{
  return getSpecificHeader(responseHeaders, name);
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
        mimeType = getMIMEType(getResponseHeader("Content-Type"));
    if (mimeType.isEmpty())
      mimeType = "text/xml";
    
    return DOMImplementationImpl::isXMLMIMEType(mimeType);
}

JSValue *XMLHttpRequest::getStatus() const
{
  if (responseHeaders.isEmpty()) {
    return jsUndefined();
  }
  
  int endOfLine = responseHeaders.find("\n");
  QString firstLine = endOfLine == -1 ? responseHeaders : responseHeaders.left(endOfLine);
  int codeStart = firstLine.find(" ");
  int codeEnd = firstLine.find(" ", codeStart + 1);
  
  if (codeStart == -1 || codeEnd == -1) {
    return jsUndefined();
  }
  
  QString number = firstLine.mid(codeStart + 1, codeEnd - (codeStart + 1));
  
  bool ok = false;
  int code = number.toInt(&ok);
  if (!ok) {
    return jsUndefined();
  }

  return jsNumber(code);
}

JSValue *XMLHttpRequest::getStatusText() const
{
  if (responseHeaders.isEmpty()) {
    return jsUndefined();
  }
  
  int endOfLine = responseHeaders.find("\n");
  QString firstLine = endOfLine == -1 ? responseHeaders : responseHeaders.left(endOfLine);
  int codeStart = firstLine.find(" ");
  int codeEnd = firstLine.find(" ", codeStart + 1);

  if (codeStart == -1 || codeEnd == -1) {
    return jsUndefined();
  }
  
  QString statusText = firstLine.mid(codeEnd + 1, endOfLine - (codeEnd + 1)).stripWhiteSpace();
  
  return jsString(statusText);
}

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

void XMLHttpRequest::slotFinished(KIO::Job *)
{
  if (responseHeaders.isEmpty() && job)
    responseHeaders = job->queryMetaData("HTTP-Headers");

  if (state < Loaded)
    changeState(Loaded);

  if (decoder)
    response += decoder->flush();

  removeFromRequestsByDocument();
  job = 0;

  changeState(Completed);
  decoder = 0;

  JSLock lock;
  gcUnprotect(this);
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
      encoding = getCharset(getResponseHeader("Content-Type"));
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

JSValue *XMLHttpRequestProtoFunc::callAsFunction(ExecState *exec, JSObject *thisObj, const List &args)
{
  if (!thisObj->inherits(&XMLHttpRequest::info))
    return throwError(exec, TypeError);

  XMLHttpRequest *request = static_cast<XMLHttpRequest *>(thisObj);

  switch (id) {
  case XMLHttpRequest::Abort: {
    request->abort();
    return jsUndefined();
  }
  case XMLHttpRequest::GetAllResponseHeaders: {
    if (args.size() != 0) {
      return jsUndefined();
    }

    return request->getAllResponseHeaders();
  }
  case XMLHttpRequest::GetResponseHeader: {
    if (args.size() != 1) {
      return jsUndefined();
    }
    
    QString header = request->getResponseHeader(args[0]->toString(exec).qstring());

    if (header.isNull())
      return jsUndefined();

    return jsString(header);
  }
  case XMLHttpRequest::Open:
    {
      if (args.size() < 2 || args.size() > 5) {
	return jsUndefined();
      }
    
      QString method = args[0]->toString(exec).qstring();
      KURL url = KURL(Window::retrieveActive(exec)->part()->xmlDocImpl()->completeURL(args[1]->toString(exec).qstring()));

      bool async = true;
      if (args.size() >= 3) {
	async = args[2]->toBoolean(exec);
      }
    
      if (args.size() >= 4) {
	url.setUser(args[3]->toString(exec).qstring());
      }
      
      if (args.size() >= 5) {
	url.setPass(args[4]->toString(exec).qstring());
      }

      request->open(method, url, async);

      return jsUndefined();
    }
  case XMLHttpRequest::Send:
    {
      if (args.size() > 1) {
	return jsUndefined();
      }

      if (request->state != Loading) {
	return jsUndefined();
      }

      DOMString body;

      if (args.size() >= 1) {
	if (args[0]->toObject(exec)->inherits(&DOMDocument::info)) {
	  DocumentImpl *doc = static_cast<DocumentImpl *>(static_cast<DOMDocument *>(args[0]->toObject(exec))->impl());
          body = doc->toString().qstring();
	} else {
	  // converting certain values (like null) to object can set an exception
	  exec->clearException();
	  body = args[0]->toString(exec).domString();
	}
      }

      request->send(body);

      return jsUndefined();
    }
  case XMLHttpRequest::SetRequestHeader: {
    if (args.size() != 2) {
      return jsUndefined();
    }
    
    request->setRequestHeader(args[0]->toString(exec).qstring(), args[1]->toString(exec).qstring());
    
    return jsUndefined();
  }
  case XMLHttpRequest::OverrideMIMEType: {
    if (args.size() != 1) {
      return jsUndefined();
    }
    request->MIMETypeOverride = args[0]->toString(exec).qstring();
    return jsUndefined();
  }
  }

  return jsUndefined();
}

} // end namespace
