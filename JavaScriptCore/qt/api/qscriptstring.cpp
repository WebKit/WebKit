/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)

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

#include "config.h"

#include "qscriptstring.h"

#include "qscriptstring_p.h"
#include <QtCore/qhash.h>

/*!
  Constructs an invalid QScriptString.
*/
QScriptString::QScriptString()
    : d_ptr(new QScriptStringPrivate())
{
}
/*!
  Constructs an QScriptString from internal representation
  \internal
*/
QScriptString::QScriptString(QScriptStringPrivate* d)
    : d_ptr(d)
{
}

/*!
  Constructs a new QScriptString that is a copy of \a other.
*/
QScriptString::QScriptString(const QScriptString& other)
{
    d_ptr = other.d_ptr;
}

/*!
  Destroys this QScriptString.
*/
QScriptString::~QScriptString()
{
}

/*!
  Assigns the \a other value to this QScriptString.
*/
QScriptString& QScriptString::operator=(const QScriptString& other)
{
    d_ptr = other.d_ptr;
    return *this;
}

/*!
  Returns true if this QScriptString is valid; otherwise
  returns false.
*/
bool QScriptString::isValid() const
{
    return d_ptr->isValid();
}

/*!
  Returns true if this QScriptString is equal to \a other;
  otherwise returns false.
*/
bool QScriptString::operator==(const QScriptString& other) const
{
    return d_ptr == other.d_ptr || *d_ptr == *(other.d_ptr);
}

/*!
  Returns true if this QScriptString is not equal to \a other;
  otherwise returns false.
*/
bool QScriptString::operator!=(const QScriptString& other) const
{
    return d_ptr != other.d_ptr || *d_ptr != *(other.d_ptr);
}

/*!
  Attempts to convert this QScriptString to a QtScript array index,
  and returns the result.

  If a conversion error occurs, *\a{ok} is set to false; otherwise
  *\a{ok} is set to true.
*/
quint32 QScriptString::toArrayIndex(bool* ok) const
{
    return d_ptr->toArrayIndex(ok);
}

/*!
  Returns the string that this QScriptString represents, or a
  null string if this QScriptString is not valid.

  \sa isValid()
*/
QString QScriptString::toString() const
{
    return d_ptr->toString();
}

/*!
  Returns the string that this QScriptString represents, or a
  null string if this QScriptString is not valid.

  \sa toString()
*/
QScriptString::operator QString() const
{
    return d_ptr->toString();
}

uint qHash(const QScriptString& key)
{
    return qHash(QScriptStringPrivate::get(key)->id());
}
