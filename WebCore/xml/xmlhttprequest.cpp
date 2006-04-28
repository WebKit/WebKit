/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2004, 2006 Apple Computer, Inc.
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

#include "Cache.h"
#include "DOMImplementation.h"
#include "EventListener.h"
#include "EventNames.h"
#include "KWQLoader.h"
#include "dom2_eventsimpl.h"
#include "PlatformString.h"
#include "FormData.h"
#include "HTMLDocument.h"
#include "kjs_binding.h"
#include "TransferJob.h"
#include <kjs/protect.h>
#include "RegularExpression.h"
#include "TextEncoding.h"

using namespace KIO;

namespace WebCore {

using namespace EventNames;

typedef HashSet<XMLHttpRequest*> RequestsSet;

static HashMap<Document*, RequestsSet*>& requestsByDocument()
{
    static HashMap<Document*, RequestsSet*> map;
    return map;
}

static void addToRequestsByDocument(Document* doc, XMLHttpRequest* req)
{
    ASSERT(doc);
    ASSERT(req);

    RequestsSet* requests = requestsByDocument().get(doc);
    if (!requests) {
        requests = new RequestsSet;
        requestsByDocument().set(doc, requests);
    }

    ASSERT(!requests->contains(req));
    requests->add(req);
}

static void removeFromRequestsByDocument(Document* doc, XMLHttpRequest* req)
{
    ASSERT(doc);
    ASSERT(req);

    RequestsSet* requests = requestsByDocument().get(doc);
    ASSERT(requests);
    ASSERT(requests->contains(req));
    requests->remove(req);
    if (requests->isEmpty()) {
        requestsByDocument().remove(doc);
        delete requests;
    }
}

static inline String getMIMEType(const String& contentTypeString)
{
    String mimeType;
    unsigned length = contentTypeString.length();
    for (unsigned offset = 0; offset < length; offset++) {
        QChar c = contentTypeString[offset];
        if (c == ';')
            break;
        else if (c.isSpace()) // FIXME: This seems wrong, " " is an invalid MIME type character according to RFC 2045.  bug 8644
            continue;
        mimeType += String(c);
    }
    return mimeType;
}

static String getCharset(const String& contentTypeString)
{
    int pos = 0;
    int length = (int)contentTypeString.length();
    
    while (pos < length) {
        pos = contentTypeString.find("charset", pos, false);
        if (pos <= 0)
            return String();
        
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
    
        return contentTypeString.substring(pos, endpos-pos);
    }
    
    return String();
}

XMLHttpRequestState XMLHttpRequest::getReadyState() const
{
    return m_state;
}

String XMLHttpRequest::getResponseText() const
{
    return m_response;
}

Document* XMLHttpRequest::getResponseXML() const
{
    if (m_state != Completed)
        return 0;

    if (!m_createdDocument) {
        if (responseIsXML()) {
            m_responseXML = m_doc->implementation()->createDocument();
            m_responseXML->open();
            m_responseXML->write(m_response);
            m_responseXML->finishParsing();
            m_responseXML->close();
        }
        m_createdDocument = true;
    }

    return m_responseXML.get();
}

EventListener* XMLHttpRequest::onReadyStateChangeListener() const
{
    return m_onReadyStateChangeListener.get();
}

void XMLHttpRequest::setOnReadyStateChangeListener(EventListener* eventListener)
{
    m_onReadyStateChangeListener = eventListener;
}

EventListener* XMLHttpRequest::onLoadListener() const
{
    return m_onLoadListener.get();
}

void XMLHttpRequest::setOnLoadListener(EventListener* eventListener)
{
    m_onLoadListener = eventListener;
}

XMLHttpRequest::XMLHttpRequest(Document *d)
    : m_doc(d)
    , m_async(true)
    , m_job(0)
    , m_state(Uninitialized)
    , m_createdDocument(false)
    , m_aborted(false)
{
    ASSERT(m_doc);
    addToRequestsByDocument(m_doc, this);
}

XMLHttpRequest::~XMLHttpRequest()
{
    if (m_doc)
        removeFromRequestsByDocument(m_doc, this);
}

void XMLHttpRequest::changeState(XMLHttpRequestState newState)
{
    if (m_state != newState) {
        m_state = newState;
        callReadyStateChangeListener();
    }
}

void XMLHttpRequest::callReadyStateChangeListener()
{
    if (m_doc && m_doc->frame() && m_onReadyStateChangeListener) {
        ExceptionCode ec;
        RefPtr<Event> ev = m_doc->createEvent("HTMLEvents", ec);
        ev->initEvent(readystatechangeEvent, true, true);
        m_onReadyStateChangeListener->handleEvent(ev.get(), true);
    }
    
    if (m_doc && m_doc->frame() && m_state == Completed && m_onLoadListener) {
        ExceptionCode ec;
        RefPtr<Event> ev = m_doc->createEvent("HTMLEvents", ec);
        ev->initEvent(loadEvent, true, true);
        m_onLoadListener->handleEvent(ev.get(), true);
    }
}

bool XMLHttpRequest::urlMatchesDocumentDomain(const KURL& url) const
{
  KURL documentURL(m_doc->URL());

    // a local file can load anything
    if (documentURL.protocol().lower() == "file")
        return true;

    // but a remote document can only load from the same port on the server
    if (documentURL.protocol().lower() == url.protocol().lower()
            && documentURL.host().lower() == url.host().lower()
            && documentURL.port() == url.port())
        return true;

    return false;
}

void XMLHttpRequest::open(const String& method, const KURL& url, bool async, const String& user, const String& password)
{
    abort();
    m_aborted = false;

    // clear stuff from possible previous load
    m_requestHeaders = DeprecatedString();
    m_responseHeaders = String();
    m_response = DeprecatedString();
    m_createdDocument = false;
    m_responseXML = 0;

    changeState(Uninitialized);

    if (m_aborted)
        return;

    if (!urlMatchesDocumentDomain(url))
        return;

    m_url = url;

    if (!user.isNull())
        m_url.setUser(user.deprecatedString());

    if (!password.isNull())
        m_url.setPass(password.deprecatedString());

    // Method names are case sensitive. But since Firefox uppercases method names it knows, we'll do the same.
    String methodUpper(method.upper());
    if (methodUpper == "CONNECT" || methodUpper == "COPY" || methodUpper == "DELETE" || methodUpper == "GET" || methodUpper == "HEAD"
        || methodUpper == "INDEX" || methodUpper == "LOCK" || methodUpper == "M-POST" || methodUpper == "MKCOL" || methodUpper == "MOVE" 
        || methodUpper == "OPTIONS" || methodUpper == "POST" || methodUpper == "PROPFIND" || methodUpper == "PROPPATCH" || methodUpper == "PUT" 
        || methodUpper == "TRACE" || methodUpper == "UNLOCK")
        m_method = methodUpper.deprecatedString();
    else
        m_method = method.deprecatedString();

    m_async = async;

    changeState(Loading);
}

void XMLHttpRequest::send(const String& body)
{
    if (!m_doc)
        return;

    if (m_state != Loading)
        return;
  
    // FIXME: Should this abort instead if we already have a m_job going?
    if (m_job)
        return;

    m_aborted = false;

    if (!body.isNull() && m_method != "GET" && m_method != "HEAD" && (m_url.protocol().lower() == "http" || m_url.protocol().lower() == "https")) {
        String contentType = getRequestHeader("Content-Type");
        String charset;
        if (contentType.isEmpty())
            setRequestHeader("Content-Type", "application/xml");
        else
            charset = getCharset(contentType);
      
        if (charset.isEmpty())
            charset = "UTF-8";
      
        TextEncoding m_encoding = TextEncoding(charset.deprecatedString().latin1());
        if (!m_encoding.isValid())   // FIXME: report an error?
            m_encoding = TextEncoding(UTF8Encoding);

        m_job = new TransferJob(m_async ? this : 0, m_method, m_url, m_encoding.fromUnicode(body.deprecatedString()));
    } else {
        // FIXME: HEAD requests just crash; see <rdar://4460899> and the commented out tests in http/tests/xmlhttprequest/methods.html.
        if (m_method == "HEAD")
            m_method = "GET";
        m_job = new TransferJob(m_async ? this : 0, m_method, m_url);
    }

    if (m_requestHeaders.length())
        m_job->addMetaData("customHTTPHeader", m_requestHeaders);

    if (!m_async) {
        DeprecatedByteArray data;
        KURL finalURL;
        DeprecatedString headers;

        {
            // avoid deadlock in case the loader wants to use JS on a background thread
            KJS::JSLock::DropAllLocks dropLocks;
            data = KWQServeSynchronousRequest(Cache::loader(), m_doc->docLoader(), m_job, finalURL, headers);
        }

        m_job = 0;
        processSyncLoadResults(data, finalURL, headers);
    
        return;
    }

    // Neither this object nor the JavaScript wrapper should be deleted while
    // a request is in progress because we need to keep the listeners alive,
    // and they are referenced by the JavaScript wrapper.
    ref();
    {
        KJS::JSLock lock;
        gcProtectNullTolerant(KJS::ScriptInterpreter::getDOMObject(this));
    }
  
    m_job->start(m_doc->docLoader());
}

void XMLHttpRequest::abort()
{
    bool hadJob = m_job;

    if (hadJob) {
        m_job->kill();
        m_job = 0;
    }
    m_decoder = 0;
    m_aborted = true;

    if (hadJob) {
        {
            KJS::JSLock lock;
            gcUnprotectNullTolerant(KJS::ScriptInterpreter::getDOMObject(this));
        }
        deref();
    }
}

void XMLHttpRequest::overrideMIMEType(const String& override)
{
    m_mimeTypeOverride = override;
}

void XMLHttpRequest::setRequestHeader(const String& name, const String& value)
{
    if (m_requestHeaders.length() > 0)
        m_requestHeaders += "\r\n";
    m_requestHeaders += name.deprecatedString();
    m_requestHeaders += ": ";
    m_requestHeaders += value.deprecatedString();
}

DeprecatedString XMLHttpRequest::getRequestHeader(const DeprecatedString& name) const
{
    return getSpecificHeader(m_requestHeaders, name);
}

String XMLHttpRequest::getAllResponseHeaders() const
{
    if (m_responseHeaders.isEmpty())
        return String();

    int endOfLine = m_responseHeaders.find("\n");
    if (endOfLine == -1)
        return String();

    return m_responseHeaders.substring(endOfLine + 1) + "\n";
}

String XMLHttpRequest::getResponseHeader(const String& name) const
{
    return getSpecificHeader(m_responseHeaders.deprecatedString(), name.deprecatedString());
}

DeprecatedString XMLHttpRequest::getSpecificHeader(const DeprecatedString& headers, const DeprecatedString& name)
{
    if (headers.isEmpty())
        return DeprecatedString();

    RegularExpression headerLinePattern(name + ":", false);

    int matchLength;
    int headerLinePos = headerLinePattern.match(headers, 0, &matchLength);
    while (headerLinePos != -1) {
        if (headerLinePos == 0 || headers[headerLinePos-1] == '\n')
            break;
        headerLinePos = headerLinePattern.match(headers, headerLinePos + 1, &matchLength);
    }
    if (headerLinePos == -1)
        return DeprecatedString();
    
    int endOfLine = headers.find("\n", headerLinePos + matchLength);
    return headers.mid(headerLinePos + matchLength, endOfLine - (headerLinePos + matchLength)).stripWhiteSpace();
}

bool XMLHttpRequest::responseIsXML() const
{
    String mimeType = getMIMEType(m_mimeTypeOverride);
    if (mimeType.isEmpty())
        mimeType = getMIMEType(getResponseHeader("Content-Type"));
    if (mimeType.isEmpty())
        mimeType = "text/xml";
    return DOMImplementation::isXMLMIMEType(mimeType);
}

int XMLHttpRequest::getStatus() const
{
    if (m_responseHeaders.isEmpty())
        return -1;
  
    int endOfLine = m_responseHeaders.find("\n");
    String firstLine = endOfLine == -1 ? m_responseHeaders : m_responseHeaders.substring(0, endOfLine);
    int codeStart = firstLine.find(" ");
    int codeEnd = firstLine.find(" ", codeStart + 1);
    if (codeStart == -1 || codeEnd == -1)
        return -1;
  
    String number = firstLine.substring(codeStart + 1, codeEnd - (codeStart + 1));
    bool ok = false;
    int code = number.toInt(&ok);
    if (!ok)
        return -1;
    return code;
}

String XMLHttpRequest::getStatusText() const
{
    if (m_responseHeaders.isEmpty())
        return String();
  
    int endOfLine = m_responseHeaders.find("\n");
    String firstLine = endOfLine == -1 ? m_responseHeaders : m_responseHeaders.substring(0, endOfLine);
    int codeStart = firstLine.find(" ");
    int codeEnd = firstLine.find(" ", codeStart + 1);
    if (codeStart == -1 || codeEnd == -1)
        return String();
  
    return firstLine.substring(codeEnd + 1, endOfLine - (codeEnd + 1)).deprecatedString().stripWhiteSpace();
}

void XMLHttpRequest::processSyncLoadResults(const DeprecatedByteArray &data, const KURL &finalURL, const DeprecatedString &headers)
{
  if (!urlMatchesDocumentDomain(finalURL)) {
    abort();
    return;
  }
  
  m_responseHeaders = headers;
  changeState(Loaded);
  if (m_aborted)
    return;
  
  const char *bytes = (const char *)data.data();
  int len = (int)data.size();

  receivedData(0, bytes, len);
  if (m_aborted)
    return;

  receivedAllData(0);
}

void XMLHttpRequest::receivedAllData(TransferJob*)
{
    if (m_responseHeaders.isEmpty() && m_job)
        m_responseHeaders = m_job->queryMetaData("HTTP-Headers");

    if (m_state < Loaded)
        changeState(Loaded);

    if (m_decoder)
        m_response += m_decoder->flush();

    bool hadJob = m_job;
    m_job = 0;

    changeState(Completed);
    m_decoder = 0;

    if (hadJob) {
        {
            KJS::JSLock lock;
            gcUnprotectNullTolerant(KJS::ScriptInterpreter::getDOMObject(this));
        }
        deref();
    }
}

void XMLHttpRequest::receivedRedirect(TransferJob*, const KURL& m_url)
{
    if (!urlMatchesDocumentDomain(m_url))
        abort();
}

void XMLHttpRequest::receivedData(TransferJob*, const char *data, int len)
{
    if (m_responseHeaders.isEmpty() && m_job)
        m_responseHeaders = m_job->queryMetaData("HTTP-Headers");

    if (m_state < Loaded)
        changeState(Loaded);
  
    if (!m_decoder) {
        m_encoding = getCharset(m_mimeTypeOverride);
        if (m_encoding.isEmpty())
            m_encoding = getCharset(getResponseHeader("Content-Type"));
        if (m_encoding.isEmpty() && m_job)
            m_encoding = m_job->queryMetaData("charset");
    
        m_decoder = new Decoder;
        if (!m_encoding.isEmpty())
            m_decoder->setEncodingName(m_encoding.deprecatedString().latin1(), Decoder::EncodingFromHTTPHeader);
        else
            // only allow Decoder to look inside the m_response if it's XML
            m_decoder->setEncodingName("UTF-8", responseIsXML() ? Decoder::DefaultEncoding : Decoder::EncodingFromHTTPHeader);
    }
    if (len == 0)
        return;

    if (len == -1)
        len = strlen(data);

    DeprecatedString decoded = m_decoder->decode(data, len);

    m_response += decoded;

    if (!m_aborted) {
        if (m_state != Interactive)
            changeState(Interactive);
        else
            // Firefox calls readyStateChanged every time it receives data, 4449442
            callReadyStateChangeListener();
    }
}

void XMLHttpRequest::cancelRequests(Document* m_doc)
{
    RequestsSet* requests = requestsByDocument().get(m_doc);
    if (!requests)
        return;
    RequestsSet copy = *requests;
    RequestsSet::const_iterator end = copy.end();
    for (RequestsSet::const_iterator it = copy.begin(); it != end; ++it)
        (*it)->abort();
}

void XMLHttpRequest::detachRequests(Document* m_doc)
{
    RequestsSet* requests = requestsByDocument().get(m_doc);
    if (!requests)
        return;
    requestsByDocument().remove(m_doc);
    RequestsSet::const_iterator end = requests->end();
    for (RequestsSet::const_iterator it = requests->begin(); it != end; ++it) {
        (*it)->m_doc = 0;
        (*it)->abort();
    }
    delete requests;
}

} // end namespace
