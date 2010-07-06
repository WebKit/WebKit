/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef qscriptstring_p_h
#define qscriptstring_p_h

#include "qscriptconverter_p.h"
#include "qscriptstring.h"
#include <JavaScriptCore/JavaScript.h>
#include <QtCore/qnumeric.h>
#include <QtCore/qshareddata.h>

class QScriptStringPrivate : public QSharedData {
public:
    inline QScriptStringPrivate();
    inline QScriptStringPrivate(const QString& qtstring);
    inline ~QScriptStringPrivate();

    static inline QScriptString get(QScriptStringPrivate* d);
    static inline QScriptStringPtr get(const QScriptString& p);

    inline bool isValid() const;

    inline bool operator==(const QScriptStringPrivate& other) const;
    inline bool operator!=(const QScriptStringPrivate& other) const;

    inline quint32 toArrayIndex(bool* ok = 0) const;

    inline QString toString() const;

    inline quint64 id() const;

    inline operator JSStringRef() const;

private:
    JSStringRef m_string;
};


QScriptStringPrivate::QScriptStringPrivate()
    : m_string(0)
{}

QScriptStringPrivate::QScriptStringPrivate(const QString& qtstring)
    : m_string(QScriptConverter::toString(qtstring))
{}

QScriptStringPrivate::~QScriptStringPrivate()
{
    if (isValid())
        JSStringRelease(m_string);
}

QScriptString QScriptStringPrivate::get(QScriptStringPrivate* d)
{
    Q_ASSERT(d);
    return QScriptString(d);
}

QScriptStringPtr QScriptStringPrivate::get(const QScriptString& p)
{
    return p.d_ptr;
}

bool QScriptStringPrivate::isValid() const
{
    return m_string;
}

bool QScriptStringPrivate::operator==(const QScriptStringPrivate& other) const
{
    return isValid() && other.isValid() && JSStringIsEqual(m_string, other.m_string);
}

bool QScriptStringPrivate::operator!=(const QScriptStringPrivate& other) const
{
    return isValid() && other.isValid() && !JSStringIsEqual(m_string, other.m_string);
}

quint32 QScriptStringPrivate::toArrayIndex(bool* ok) const
{
    quint32 idx = QScriptConverter::toArrayIndex(m_string);
    if (ok)
        *ok = (idx != 0xffffffff);
    return idx;
}

QString QScriptStringPrivate::toString() const
{
    return QScriptConverter::toString(m_string);
}

quint64 QScriptStringPrivate::id() const
{
    return reinterpret_cast<quint32>(m_string);
}

/*!
    \internal
    This method should be used for invoking JSC functions.
    \note This method keeps ownership of an internal JSStringRef.
*/
QScriptStringPrivate::operator JSStringRef() const
{
    return m_string;
}

#endif // qscriptstring_p_h
