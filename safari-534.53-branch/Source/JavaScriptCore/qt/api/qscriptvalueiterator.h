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

#ifndef qscriptvalueiterator_h
#define qscriptvalueiterator_h

#include "qtscriptglobal.h"
#include "qscriptstring.h"
#include <QtCore/qshareddata.h>
#include "qscriptvalue.h"

class QScriptValue;
class QScriptValueIteratorPrivate;


class Q_JAVASCRIPT_EXPORT QScriptValueIterator {
public:
    QScriptValueIterator(const QScriptValue& value);
    ~QScriptValueIterator();

    bool hasNext() const;
    void next();

    bool hasPrevious() const;
    void previous();

    QString name() const;
    QScriptString scriptName() const;

    QScriptValue value() const;
    void setValue(const QScriptValue& value);

    void remove();
    QScriptValue::PropertyFlags flags() const;

    void toFront();
    void toBack();

    QScriptValueIterator& operator=(QScriptValue& value);
private:
    QExplicitlySharedDataPointer<QScriptValueIteratorPrivate> d_ptr;

    Q_DECLARE_PRIVATE(QScriptValueIterator)
    Q_DISABLE_COPY(QScriptValueIterator)
};

#endif // qscriptvalueiterator_h
