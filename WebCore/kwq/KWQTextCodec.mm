/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQTextCodec.h"

#import "KWQCharsets.h"

static CFMutableDictionaryRef encodingToCodec = NULL;

static QTextCodec *codecForCFStringEncoding(CFStringEncoding encoding)
{
    const void *value;
    QTextCodec *codec;

    if (encodingToCodec == NULL) {
        encodingToCodec = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    }
    
    if (CFDictionaryGetValueIfPresent(encodingToCodec, (void *)encoding, &value)) {
        return (QTextCodec *)value;
    } else {
        codec = new QTextCodec(encoding);
	CFDictionarySetValue(encodingToCodec, (void *)encoding, codec);
	return codec;
    }
}

QTextCodec *QTextCodec::codecForMib(int mib)
{
    CFStringEncoding encoding;

    encoding = KWQCFStringEncodingFromMIB(mib);

    if (encoding == kCFStringEncodingInvalidId) {
        return NULL;
    } else {
        return codecForCFStringEncoding(encoding);
    }
}

QTextCodec *QTextCodec::codecForName(const char *name)
{
    CFStringRef cfname;
    CFStringEncoding encoding;
    
    cfname = CFStringCreateWithCString(NULL, name, kCFStringEncodingASCII);

    encoding = KWQCFStringEncodingFromIANACharsetName(cfname);
    CFRelease(cfname);

    if (encoding == kCFStringEncodingInvalidId) {
        return NULL;
    } else {
        return codecForCFStringEncoding(encoding);
    }
}

QTextCodec *QTextCodec::codecForLocale()
{
    return codecForCFStringEncoding(CFStringGetSystemEncoding());
}

const char *QTextCodec::name() const
{
    return [(NSString *)KWQCFStringEncodingToIANACharsetName(encoding) cString];
}

int QTextCodec::mibEnum() const
{
    return KWQCFStringEncodingToMIB(encoding);
}

QTextDecoder *QTextCodec::makeDecoder() const
{
    return new QTextDecoder(this);
}

QCString QTextCodec::fromUnicode(const QString &qcs) const
{
    CFStringRef cfs = qcs.getCFString();
    CFRange range = CFRangeMake(0, CFStringGetLength(cfs));
    CFIndex bufferLength;
    CFStringGetBytes(cfs, range, encoding, '?', false, NULL, 0x7FFFFFFF, &bufferLength);
    QCString result(bufferLength + 1);
    CFStringGetBytes(cfs, range, encoding, '?', false, (UInt8 *)result.data(), bufferLength, &bufferLength);
    result[bufferLength] = 0;
    return result;
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
