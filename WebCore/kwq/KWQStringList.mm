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

#include "config.h"
#import "KWQStringList.h"

#import <CoreFoundation/CoreFoundation.h>

// No need to CFRelease return value
static CFStringRef GetCFString(const QString &s)
{
    CFStringRef cfs = s.getCFString();
    if (cfs == NULL) {
        cfs = CFSTR("");
    }
    return cfs;
}

QStringList QStringList::split(const QString &separator, const QString &s, bool allowEmptyEntries)
{
    CFArrayRef cfresult;
    QStringList result;

    cfresult = CFStringCreateArrayBySeparatingStrings(NULL, GetCFString(s), GetCFString(separator));
    
    CFIndex cfResultSize = CFArrayGetCount(cfresult);

    for (CFIndex i = 0; i < cfResultSize; i++) {
        QString entry = QString::fromCFString((CFStringRef)CFArrayGetValueAtIndex(cfresult, i));
	if (!entry.isEmpty() || allowEmptyEntries) {
	    result.append(entry);
	}
    }

    CFRelease(cfresult);

    return result;
}
 
QStringList QStringList::split(const QChar &separator, const QString &s, bool allowEmptyEntries)
{
    return QStringList::split(QString(separator), s, allowEmptyEntries);
}

QString QStringList::join(const QString &separator) const
{
    QString result;
    
    for (ConstIterator i = begin(), j = ++begin(); i != end(); ++i, ++j) {
        result += *i;
	if (j != end()) {
	    result += separator;
	}
    }

    return result;
}

QString QStringList::pop_front()
{
    QString front = first();
    remove(begin());
    return front;
}

NSArray *QStringList::getNSArray() const
{
    NSMutableArray *array = [NSMutableArray array];
    for (ConstIterator it = begin(); it != end(); ++it) {
        [array addObject:(*it).getNSString()];
    }
    return array;
}
