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

#ifndef qscriptvalue_h
#define qscriptvalue_h

#include <QtCore/qlist.h>
#include <QtCore/qshareddata.h>

class QScriptEngine;
class QScriptValuePrivate;

class QScriptValue;
typedef QList<QScriptValue> QScriptValueList;

typedef double qsreal;

class QScriptValue {
public:    
    enum SpecialValue {
        NullValue,
        UndefinedValue
    };

    QScriptValue();
    QScriptValue(bool value);
    QScriptValue(int value);
    QScriptValue(uint value);
    QScriptValue(qsreal value);
    QScriptValue(const QString& value);
    QScriptValue(const char* value);
    QScriptValue(SpecialValue value);
    QScriptValue(const QScriptValue& other);

    QScriptValue(QScriptEngine* engine, bool value);
    QScriptValue(QScriptEngine* engine, int value);
    QScriptValue(QScriptEngine* engine, uint value);
    QScriptValue(QScriptEngine* engine, qsreal value);
    QScriptValue(QScriptEngine* engine, const QString& value);
    QScriptValue(QScriptEngine* engine, const char* value);
    QScriptValue(QScriptEngine* engine, SpecialValue value);

    ~QScriptValue();

    QScriptValue& operator=(const QScriptValue& other);
    bool equals(const QScriptValue& other) const;
    bool strictlyEquals(const QScriptValue& other) const;

    QScriptEngine* engine() const;

    bool isValid() const;
    bool isBool() const;
    bool isBoolean() const;
    bool isNumber() const;
    bool isFunction() const;
    bool isNull() const;
    bool isString() const;
    bool isUndefined() const;
    bool isObject() const;
    bool isError() const;

    QString toString() const;
    qsreal toNumber() const;
    bool toBool() const;
    bool toBoolean() const;
    qsreal toInteger() const;
    qint32 toInt32() const;
    quint32 toUInt32() const;
    quint16 toUInt16() const;

    QScriptValue call(const QScriptValue& thisObject = QScriptValue(),
                      const QScriptValueList& args = QScriptValueList());

private:
    QScriptValue(void*);
    QScriptValue(QScriptValuePrivate*);

    QExplicitlySharedDataPointer<QScriptValuePrivate> d_ptr;

    friend class QScriptValuePrivate;
};

#endif // qscriptvalue_h
