/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#include <kurl.h>
#include <kwqdebug.h>

#ifndef USING_BORROWED_KURL

#import <Foundation/NSURLPathUtilities.h>

class KURL::KWQKURLPrivate
{
public:
    KWQKURLPrivate(const QString &url);
    KWQKURLPrivate(const KWQKURLPrivate &other);
    ~KWQKURLPrivate();

    void init(const QString &);
    void makeRef();
    void decompose();
    void compose();
    
    CFURLRef urlRef;
    QString sURL;
    QString sProtocol;
    QString sHost;
    unsigned short int iPort;
    QString sPass;
    QString sUser;
    QString sRef;
    QString sQuery;
    QString sPath;
    QString escapedPath;
    bool addedSlash;

    int refCount;
};


KURL::KWQKURLPrivate::KWQKURLPrivate(const QString &url) :
    urlRef(nil),
    iPort(0),
    addedSlash(false),
    refCount(0)
{
    init(url);
}

KURL::KWQKURLPrivate::KWQKURLPrivate(const KWQKURLPrivate &other) :
    urlRef(other.urlRef != NULL ? CFRetain(other.urlRef) : NULL),
    sURL(other.sURL),
    sProtocol(other.sProtocol),
    sHost(other.sHost),
    iPort(other.iPort),   
    sPass(other.sPass),   
    sUser(other.sUser),   
    sRef(other.sRef),  
    sQuery(other.sQuery),   
    sPath(other.sPath),   
    escapedPath(other.escapedPath),   
    addedSlash(other.addedSlash)
{
}

KURL::KWQKURLPrivate::~KWQKURLPrivate()
{
    if (urlRef != NULL) {
        CFRelease(urlRef);
    }
}

void KURL::KWQKURLPrivate::init(const QString &s)
{
    // Save original string
    sURL = s;

    decompose();
}

void KURL::KWQKURLPrivate::makeRef()
{
    // Not a path and no scheme, so bail out, because CFURL considers such
    // things valid URLs for some reason.
    if (!sURL.contains(':') && (sURL.length() == 0 || sURL[0] != '/') || sURL == "file:" || sURL == "file://") {
        urlRef = NULL;
        return;
    }

    // Create CFURLRef object
    if (sURL.length() > 0 && sURL[0] == '/') {
        sURL = (QString("file://")) + sURL;
    } else if (sURL.startsWith("file:/") && !sURL.startsWith("file://")) {
        sURL = (QString("file:///") + sURL.mid(6));
    } 

    QString sURLMaybeAddSlash;
    int colonPos = sURL.find(':');
    int slashSlashPos = sURL.find("//", colonPos);
    if (slashSlashPos != -1 && sURL.find('/', slashSlashPos + 2) == -1) {
        sURLMaybeAddSlash = sURL + "/";
	addedSlash = true;
    } else {
        sURLMaybeAddSlash = sURL;
	addedSlash = false;
    }

    urlRef = CFURLCreateWithString(NULL, sURLMaybeAddSlash.getCFMutableString(), NULL);
}

static inline QString CFStringToQString(CFStringRef cfs)
{
    QString qs;

    if (cfs != NULL) {
        qs = QString::fromCFString(cfs);
	CFRelease(cfs);
    } else {
        qs = QString();
    }

    return qs;
}

static inline QString escapeQString(QString str)
{
    return CFStringToQString(CFURLCreateStringByAddingPercentEscapes(NULL, str.getCFMutableString(), NULL, NULL, kCFStringEncodingUTF8));
}

static bool pathEndsWithSlash(QString sURL)
{
    int endOfPath = sURL.findRev('?', sURL.findRev('#'));
    if (endOfPath == -1) {
        endOfPath = sURL.length();
    }
    if (sURL[endOfPath-1] == '/') {
        return true;
    } else {
        return false;
    }
}


CFStringRef KWQCFURLCopyEscapedPath(CFURLRef anURL)
{
    NSRange path;
    CFStringRef urlString = CFURLGetString(anURL);

    _NSParseStringToGenericURLComponents((NSString *)urlString, NULL, NULL, NULL, NULL, NULL, &path, NULL, NULL, NULL);

    return CFStringCreateWithSubstring(NULL, urlString, CFRangeMake(path.location, path.length));
}

void KURL::KWQKURLPrivate::decompose()
{
    makeRef();

    if (urlRef == NULL) {
        // failed to parse
        return;
    }

    // decompose into parts

    sProtocol = CFStringToQString(CFURLCopyScheme(urlRef));

    QString hostName = CFStringToQString(CFURLCopyHostName(urlRef));
    if (sProtocol != "file") {
        sHost =  hostName;
    }

    SInt32 portNumber = CFURLGetPortNumber(urlRef);
    if (portNumber < 0) {
        iPort = 0;
    } else {
        iPort = portNumber;
    }

    sPass = CFStringToQString(CFURLCopyPassword(urlRef));
    sUser = CFStringToQString(CFURLCopyUserName(urlRef));
    sRef = CFStringToQString(CFURLCopyFragment(urlRef, NULL));

    sQuery = CFStringToQString(CFURLCopyQueryString(urlRef, NULL));
    if (!sQuery.isEmpty()) {
        sQuery = QString("?") + sQuery;
    }

    if (CFURLCanBeDecomposed(urlRef)) {
	if (addedSlash) {
	    sPath = "";
	    escapedPath = "";
	} else {
	    sPath = CFStringToQString(CFURLCopyFileSystemPath(urlRef, kCFURLPOSIXPathStyle));
	    escapedPath = CFStringToQString(KWQCFURLCopyEscapedPath(urlRef));
	}
	QString param = CFStringToQString(CFURLCopyParameterString(urlRef, CFSTR("")));
	if (!param.isEmpty()) {
	    sPath += ";" + param;
	    QString escapedParam = CFStringToQString(CFURLCopyParameterString(urlRef, NULL));

	    escapedPath += ";" + escapedParam;
	}

	if (pathEndsWithSlash(sURL) && sPath.right(1) != "/") {
	    sPath += "/";
	}
    } else {
        sPath = CFStringToQString(CFURLCopyResourceSpecifier(urlRef));
	escapedPath = sPath;
    }

    if (sProtocol == "file" && !hostName.isEmpty() && hostName != "localhost") {
        sPath = QString("//") + hostName + sPath;
	escapedPath = QString("//") + hostName + escapedPath;
    }

    // could lead to poor performance - perhaps compose manually in ::url?
    if (sURL != "file://localhost") {
	compose();
    }
}

void KURL::KWQKURLPrivate::compose()
{
    if (!sProtocol.isEmpty()) {
        QString result = escapeQString(sProtocol) + ":";

	if (!sHost.isEmpty()) {
	    result += "//";
	    if (!sUser.isEmpty()) {
	        result += escapeQString(sUser);
		if (!sPass.isEmpty()) {
		    result += ":" + escapeQString(sPass);
		}
		result += "@";
	    }
	    result += escapeQString(sHost);
	    if (iPort != 0) {
	        result += ":" + QString::number(iPort);
	    }
	}

	result += escapedPath + sQuery;
	
	if (!sRef.isEmpty()) {
	    result += "#" + sRef;
	}

	if (urlRef != NULL) {
	    CFRelease(urlRef);
	}

	sURL = result;

	makeRef();
    }
}

// KURL

KURL::KURL() : 
    d(new KURL::KWQKURLPrivate(QString()))
{
}

KURL::KURL(const char *url, int encoding_hint=0) :
    d(new KURL::KWQKURLPrivate(url))
{
}

KURL::KURL(const QString &url, int encoding_hint=0) :
    d(new KURL::KWQKURLPrivate(url))
{
}

KURL::KURL(const KURL &base, const QString &relative)
{
    if (relative.isEmpty()) {
	d = base.d;
    } else {
	CFURLRef relativeURL = CFURLCreateWithString(NULL, relative.getCFMutableString(), base.d->urlRef);
	if (relativeURL == NULL) {
	    d = base.d;
	    copyOnWrite();
	    setPath(d->sPath + relative);
	} else {
	    CFURLRef absoluteURL = CFURLCopyAbsoluteURL(relativeURL);
	
	    d = KWQRefPtr<KURL::KWQKURLPrivate>(new KURL::KWQKURLPrivate(QString::fromCFString(CFURLGetString(absoluteURL))));
	    
	    CFRelease (relativeURL);
	    CFRelease (absoluteURL);
	}
    }
}

KURL::KURL(const KURL &other) : 
    d(other.d)
{
}

KURL::~KURL()
{
}

bool KURL::isEmpty() const
{
    return d->sURL.isEmpty();
}

bool KURL::isMalformed() const
{
    return (d->urlRef == NULL);
}

bool KURL::hasPath() const
{
    return !d->sPath.isEmpty();
}

QString KURL::url() const
{
    if (d->urlRef == NULL) {
        return d->sURL;
    } 

    QString qurl = QString::fromCFString(CFURLGetString(d->urlRef));

    // Special handling for file: URLs
    if (qurl.startsWith("file:///")) {
        qurl = QString("file:/") + qurl.mid(8);
    } else if (d->sURL == "file://localhost") {
        qurl = QString("file:/");
    } else if (qurl.startsWith("file://localhost/")) {
        qurl = QString("file:/") + qurl.mid(17);
    }

    if (d->addedSlash) {
	qurl = qurl.left(qurl.length()-1);
    }

    return qurl;
}

QString KURL::protocol() const
{
    return d->sProtocol;
}

QString KURL::host() const
{
    return d->sHost;
}

unsigned short int KURL::port() const
{
    return d->iPort;
}

QString KURL::pass() const
{
    return d->sPass;
}

QString KURL::user() const
{
    return d->sUser;
}

QString KURL::ref() const
{
    return d->sRef;
}

QString KURL::query() const
{
    return d->sQuery;
}

QString KURL::path() const
{
    return d->sPath;
}

void KURL::setProtocol(const QString &s)
{
    copyOnWrite();
    d->sProtocol = s;
    d->compose();
}

void KURL::setHost(const QString &s)
{
    copyOnWrite();
    d->sHost = s;
    d->compose();
}

void KURL::setPort(unsigned short i)
{
    copyOnWrite();
    d->iPort = i;
    d->compose();
}

void KURL::setRef(const QString &s)
{
    copyOnWrite();
    d->sRef = s;
    d->compose();
}

void KURL::setQuery(const QString &_txt, int encoding_hint=0)
{
    copyOnWrite();
   if (_txt.length() && (_txt[0] !='?'))
      d->sQuery = "?" + _txt;
   else
      d->sQuery = _txt;
    d->compose();
}

void KURL::setPath(const QString &s)
{
    copyOnWrite();
    d->sPath = s;
    d->escapedPath = escapeQString(s);
    d->compose();
}

QString KURL::prettyURL(int trailing=0) const
{
    if (d->urlRef == NULL) {
        return d->sURL;
    }

    QString result =  d->sProtocol + ":";

    if (!d->sHost.isEmpty()) {
        result += "//";
	if (!d->sUser.isEmpty()) {
	    result += d->sUser;
	    result += "@";
	}
	result += d->sHost;
	if (d->iPort != 0) {
	    result += ":";
	    result += QString::number(d->iPort);
	}
    }

    QString path = d->sPath;

    if (trailing == 1) {
        if (path.right(1) != "/" && !path.isEmpty()) {
	    path += "/";
	}
    } else if (trailing == -1) {
        if (path.right(1) == "/" && path.length() > 1) {
	    path = path.left(path.length()-1);
	}
    }

    result += path;
    result += d->sQuery;

    if (!d->sRef.isEmpty()) {
        result += "#" + d->sRef;
    }

    return result;
}


KURL &KURL::operator=(const KURL &other)
{
    KURL tmp(other);
    KWQRefPtr<KURL::KWQKURLPrivate> tmpD = tmp.d;
    
    tmp.d = d;
    d = tmpD;
    
    return *this;
}



QString KURL::decode_string(const QString &urlString)
{
    CFStringRef unescaped = CFURLCreateStringByReplacingPercentEscapes(NULL, urlString.getCFMutableString(), CFSTR(""));
    QString qUnescaped = QString::fromCFString(unescaped);
    CFRelease(unescaped);

    return qUnescaped;
}


void KURL::copyOnWrite()
{
    if (d->refCount > 1) {
	d = KWQRefPtr<KURL::KWQKURLPrivate>(new KURL::KWQKURLPrivate(*d));
    }
}


#endif

