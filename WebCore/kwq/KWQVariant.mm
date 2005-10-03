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
#import "KWQVariant.h"

#import "KWQString.h"

class QVariant::QVariantPrivate {
public:
    QVariantPrivate(QVariant::Type type = QVariant::Invalid);
    ~QVariantPrivate();
    
    QVariant::Type t;

    union {
        bool b;
        uint u;
        double d;
        void *p;
        int i;
    } value;

    int refCount;
};

QVariant::QVariantPrivate::QVariantPrivate(QVariant::Type type)
    : t(type), refCount(0)
{
}

QVariant::QVariantPrivate::~QVariantPrivate()
{
    if (t == QVariant::String)
        delete (QString*)value.p;
}

QVariant::QVariant() : d(new QVariantPrivate)
{
}

QVariant::QVariant(bool val, int) : d(new QVariantPrivate(Bool))
{
    d->value.b = val;
}

QVariant::QVariant(int val) : d(new QVariantPrivate(Int))
{
    d->value.i = val;
}

QVariant::QVariant(uint val) : d(new QVariantPrivate(UInt))
{
    d->value.u = val;
}

QVariant::QVariant(double val) : d(new QVariantPrivate(Double))
{
    d->value.d = val;
}

QVariant::QVariant(const QString &s) : d(new QVariantPrivate(String))
{
    d->value.p = new QString(s);
}

QVariant::~QVariant()
{
}

QVariant::QVariant(const QVariant &v)
    : d(v.d)
{
}

QVariant &QVariant::operator=(const QVariant &v)
{
    d = v.d;
    return *this;
}

QVariant::Type QVariant::type() const
{
    return d->t;
}

bool QVariant::toBool() const
{
    switch (d->t) {
    case Bool:
        return d->value.b;
    case Double:
        return d->value.d != 0.0;
    case Int:
        return d->value.i;
    case UInt:
        return d->value.u;
    case Invalid:
    case String:
        break;
    }
    return false;
}

int QVariant::toInt() const
{
    switch (d->t) {
    case Bool:
        return d->value.b;
    case Double:
        return (int)d->value.d;
    case Int:
        return d->value.i;
    case UInt:
        return d->value.u > INT_MAX ? 0 : d->value.u;
    case Invalid:
    case String:
        break;
    }
    return 0;
}

uint QVariant::toUInt() const
{
    switch (d->t) {
    case Bool:
        return d->value.b;
    case Double:
        return (uint)d->value.d;
    case Int:
        return d->value.i < 0 ? 0 : d->value.i;
    case UInt:
        return d->value.u;
    case Invalid:
    case String:
        break;
    }
    return 0;
}

QString QVariant::asString() const
{
    switch (d->t) {
    case Bool:
        return QString(d->value.b ? "true" : "false");
    case Double:
        return QString().setNum(d->value.d);
    case Int:
        return QString().setNum(d->value.i);
    case String:
        return *(QString *)d->value.p;
    case UInt:
        return QString().setNum(d->value.u);
    case Invalid:
        break;
    }
    return QString();
}
