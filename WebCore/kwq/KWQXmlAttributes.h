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

#ifndef KWQXMLATTRIBUTES_H
#define KWQXMLATTRIBUTES_H

#include "KWQString.h"

struct KWQXmlNamespace;

class QXmlAttributes {
public:
    QXmlAttributes() : _ref(0), _length(0), _names(0), _values(0), _uris(0) { }
    QXmlAttributes(const char **expatStyleAttributes);
    ~QXmlAttributes();
    
    QXmlAttributes(const QXmlAttributes &);
    QXmlAttributes &operator=(const QXmlAttributes &);
    
    int length() const { return _length; }
    QString qName(int index) const { return _names[index]; }
    QString localName(int index) const;
    QString uri(int index) const { if (!_uris) return QString::null; return _uris[index]; }
    QString value(int index) const { return _values[index]; }

    QString value(const QString &) const;

    void split(KWQXmlNamespace* ns);
    
private:
    mutable int *_ref;
    int _length;
    QString *_names;
    QString *_values;
    QString *_uris;
};

#endif
