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

// FIXME: obviously many functions here can be made inline

#import <qtextcodec.h>
#import <kwqdebug.h>
#import <KWQCharsets.h>

// USING_BORROWED_QSTRING ======================================================
#ifndef USING_BORROWED_QSTRING

static CFMutableDictionaryRef encodingToCodec = NULL;

static QTextCodec *codecForCFStringEncoding(CFStringEncoding encoding)
{
    const void *value;
    QTextCodec *codec;

    if (encodingToCodec == NULL) {
        encodingToCodec =  CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    }
    
    if (CFDictionaryGetValueIfPresent(encodingToCodec, (void *)encoding, &value)) {
        return (QTextCodec *)value;
    } else {
        codec = new QTextCodec(encoding);
	CFDictionarySetValue(encodingToCodec, (void *)encoding, (void *)codec);
	return codec;
    }
}





// class QTextDecoder ==========================================================

// constructors, copy constructors, and destructors ----------------------------

QTextDecoder::QTextDecoder(const QTextCodec *tc)
{
    textCodec = tc;
}

QTextDecoder::~QTextDecoder()
{
    // do nothing
}

// member functions --------------------------------------------------------

QString QTextDecoder::toUnicode(const char *chs, int len)
{
    return textCodec->toUnicode(chs, len);
}


// class QTextCodec ============================================================

// static member functions -----------------------------------------------------

QTextCodec *QTextCodec::codecForMib(int mib)
{
    CFStringEncoding encoding;

    encoding = KWQCFStringEncodingFromMIB(mib);

    // FIXME: This cast to CFStringEncoding is a workaround for Radar 2912404.
    if (encoding == (CFStringEncoding) kCFStringEncodingInvalidId) {
        return NULL;
    } else {
        return codecForCFStringEncoding(encoding);
    }
}


QTextCodec *QTextCodec::codecForName(const char *name, int accuracy)
{
    CFStringRef cfname;
    CFStringEncoding encoding;

    cfname = CFStringCreateWithCString(NULL, name, kCFStringEncodingASCII);

    encoding = KWQCFStringEncodingFromIANACharsetName(cfname);
    CFRelease(cfname);

    // FIXME: This cast to CFStringEncoding is a workaround for Radar 2912404.
    if (encoding == (CFStringEncoding) kCFStringEncodingInvalidId) {
        return NULL;
    } else {
        return codecForCFStringEncoding(encoding);
    }
}

QTextCodec *QTextCodec::codecForLocale()
{
    return codecForCFStringEncoding(CFStringGetSystemEncoding());
}

// constructors, copy constructors, and destructors ----------------------------

QTextCodec::QTextCodec(CFStringEncoding e)
{
    encoding = e;
}

QTextCodec::~QTextCodec()
{
    // do nothing
}

// member functions --------------------------------------------------------

const char* QTextCodec::name() const
{
    return QString::fromCFString(KWQCFStringEncodingToIANACharsetName(encoding)).latin1();
}

int QTextCodec::mibEnum() const
{
  return KWQCFStringEncodingToMIB(encoding);
}

QTextDecoder *QTextCodec::makeDecoder() const
{
    // FIXME: will this leak or do clients dispose of the object?
    return new QTextDecoder(this);
}

QCString QTextCodec::fromUnicode(const QString &qcs) const
{
    // FIXME: is there a more efficient way to do this?
    return QCString(qcs.latin1());
}

QString QTextCodec::toUnicode(const char *chs, int len) const
{
    return QString::fromStringWithEncoding(chs, len, encoding);
}

QString QTextCodec::toUnicode(const QByteArray &qba, int len) const
{
    return QString::fromStringWithEncoding(qba, len, encoding);
}

QString QTextCodec::toUnicode(const char *chs) const
{
    return QString::fromStringWithEncoding(chs, -1, encoding);
}

#else // USING_BORROWED_QSTRING

QTextCodec *QTextCodec::codecForMib(int)
{
    // FIXME: danger Will Robinson!!!
    _logNotYetImplemented();
    return NULL;
}

QTextCodec *QTextCodec::codecForName(const char *, int)
{
    // FIXME: danger Will Robinson!!!
    _logNotYetImplemented();
    return NULL;
}

QTextCodec *QTextCodec::codecForLocale()
{
    // FIXME: danger Will Robinson!!!
    _logNotYetImplemented();
    return NULL;
}

QCString QTextCodec::fromUnicode(const QString &qcs) const
{
    // FIXME: is there a more efficient way to do this?
    return QCString(qcs.latin1());
}

#endif // USING_BORROWED_QSTRING
