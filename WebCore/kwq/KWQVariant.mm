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
#include <kwqdebug.h>

#include <qvariant.h>
#include <qstring.h>
#include <_qshared.h>

class QVariantPrivate : public QShared {
friend class QVariant;
public:
    QVariantPrivate() {
        t = QVariant::Invalid;
    }
    
    QVariantPrivate(QVariantPrivate *other) {
        switch (other->t) {
            case QVariant::Invalid:
                break;
            case QVariant::String:
                value.p = new QString(*((QString*)other->value.p));
                break;
            case QVariant::UInt:
                value.u = other->value.u;
                break;
            case QVariant::Double:
                value.d = other->value.d;
                break;
            case QVariant::Bool:
                value.b = other->value.b;
                break;
        }
    }
    
    ~QVariantPrivate() {
         clear();
    }
    
    void clear()
    {
        switch (t) {
            case QVariant::Invalid:
            case QVariant::UInt:
            case QVariant::Double:
            case QVariant::Bool:
                break;
            case QVariant::String:
                delete (QString*)value.p;
                break;
        }
        t = QVariant::Invalid;
    }
    
    QVariant::Type t;

    union {
        bool b;
        uint u;
        double d;
        void *p;
    } value;
};

QVariant::QVariant()
{
    d = new QVariantPrivate();
}


QVariant::QVariant(bool val, int i)
{
    d = new QVariantPrivate();
    d->t = Bool;
    d->value.d = val;
}


QVariant::QVariant(double val)
{
    d = new QVariantPrivate();
    d->t = Double;
    d->value.d = val;
}


QVariant::QVariant(const QString &s)
{
    d = new QVariantPrivate();
    d->t = String;
    d->value.p = new QString(s);
}


QVariant::QVariant(const QVariant &other)
{
    d = new QVariantPrivate();
    *this = other;
}


QVariant::~QVariant()
{
    if (d->deref()) {
        delete d;
    }
}


QVariant::Type QVariant::type() const
{
    return d->t;
}


bool QVariant::toBool() const
{
    if (d->t == Bool) {
        return d->value.b;
    }
    if (d->t == Double) {
        return d->value.d != 0.0;
    }
    if (d->t == UInt) {
        return d->value.u != 0;
    }

    return FALSE;
}


uint QVariant::toUInt() const
{
    if (d->t == UInt) {
        return (int)d->value.u;
    }
    if (d->t == Double) {
        return (int)d->value.d;
    }
    if (d->t == Bool) {
        return (int)d->value.b;
    }
    
    return 0;
}


QVariant &QVariant::operator=(const QVariant &other)
{
    QVariant &variant = (QVariant &)other;
    
    variant.d->ref();
    if (d->deref()) {
        delete d;
    }
    d = variant.d;
    
    return *this;
}

