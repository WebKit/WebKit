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

#include "qscriptvalue.h"

#include "qscriptengine.h"
#include "qscriptengine_p.h"
#include "qscriptvalue_p.h"
#include <QtCore/qdebug.h>

/*!
    Constructs an invalid value.
*/
QScriptValue::QScriptValue()
    : d_ptr(new QScriptValuePrivate())
{
}

/*!
  Constructs a new QScriptValue with a boolean \a value.
*/
QScriptValue::QScriptValue(bool value)
    : d_ptr(new QScriptValuePrivate(value))
{
}

/*!
  Constructs a new QScriptValue with a number \a value.
*/
QScriptValue::QScriptValue(int value)
    : d_ptr(new QScriptValuePrivate(value))
{
}

/*!
  Constructs a new QScriptValue with a number \a value.
*/
QScriptValue::QScriptValue(uint value)
    : d_ptr(new QScriptValuePrivate(value))
{
}

/*!
  Constructs a new QScriptValue with a number \a value.
*/
QScriptValue::QScriptValue(qsreal value)
    : d_ptr(new QScriptValuePrivate(value))
{
}

/*!
  Constructs a new QScriptValue with a string \a value.
*/
QScriptValue::QScriptValue(const QString& value)
    : d_ptr(new QScriptValuePrivate(value))
{
}

/*!
  Constructs a new QScriptValue with a special \a value.
*/
QScriptValue::QScriptValue(SpecialValue value)
    : d_ptr(new QScriptValuePrivate(value))
{
}

/*!
  Constructs a new QScriptValue with a string \a value.
*/
QScriptValue::QScriptValue(const char* value)
    : d_ptr(new QScriptValuePrivate(QString::fromUtf8(value)))
{
}

/*!
    Block automatic convertion to bool
    \internal
*/
QScriptValue::QScriptValue(void* d)
{
    Q_ASSERT(false);
}

/*!
    Constructs a new QScriptValue from private
    \internal
*/
QScriptValue::QScriptValue(QScriptValuePrivate* d)
    : d_ptr(d)
{
}

/*!
  \obsolete

  Constructs a new QScriptValue with the boolean \a value and
  registers it with the script \a engine.
*/
QScriptValue::QScriptValue(QScriptEngine* engine, bool value)
{
    if (engine)
        d_ptr = new QScriptValuePrivate(QScriptEnginePrivate::get(engine), value);
    else
        d_ptr = new QScriptValuePrivate(value);
}

/*!
  \obsolete

  Constructs a new QScriptValue with the integer \a value and
  registers it with the script \a engine.
*/
QScriptValue::QScriptValue(QScriptEngine* engine, int value)
{
    if (engine)
        d_ptr = new QScriptValuePrivate(QScriptEnginePrivate::get(engine), value);
    else
        d_ptr = new QScriptValuePrivate(value);
}

/*!
  \obsolete

  Constructs a new QScriptValue with the unsigned integer \a value and
  registers it with the script \a engine.
 */
QScriptValue::QScriptValue(QScriptEngine* engine, uint value)
{
    if (engine)
        d_ptr = new QScriptValuePrivate(QScriptEnginePrivate::get(engine), value);
    else
        d_ptr = new QScriptValuePrivate(value);
}

/*!
  \obsolete

  Constructs a new QScriptValue with the qsreal \a value and
  registers it with the script \a engine.
*/
QScriptValue::QScriptValue(QScriptEngine* engine, qsreal value)
{
    if (engine)
        d_ptr = new QScriptValuePrivate(QScriptEnginePrivate::get(engine), value);
    else
        d_ptr = new QScriptValuePrivate(value);
}

/*!
  \obsolete

  Constructs a new QScriptValue with the string \a value and
  registers it with the script \a engine.
*/
QScriptValue::QScriptValue(QScriptEngine* engine, const QString& value)
{
    if (engine)
        d_ptr = new QScriptValuePrivate(QScriptEnginePrivate::get(engine), value);
    else
        d_ptr = new QScriptValuePrivate(value);
}

/*!
  \obsolete

  Constructs a new QScriptValue with the string \a value and
  registers it with the script \a engine.
*/
QScriptValue::QScriptValue(QScriptEngine* engine, const char* value)
{
    if (engine)
        d_ptr = new QScriptValuePrivate(QScriptEnginePrivate::get(engine), QString::fromUtf8(value));
    else
        d_ptr = new QScriptValuePrivate(QString::fromUtf8(value));
}

/*!
  \obsolete

  Constructs a new QScriptValue with the special \a value and
  registers it with the script \a engine.
*/
QScriptValue::QScriptValue(QScriptEngine* engine, SpecialValue value)
{
    if (engine)
        d_ptr = new QScriptValuePrivate(QScriptEnginePrivate::get(engine), value);
    else
        d_ptr = new QScriptValuePrivate(value);
}

/*!
  Constructs a new QScriptValue that is a copy of \a other.

  Note that if \a other is an object (i.e., isObject() would return
  true), then only a reference to the underlying object is copied into
  the new script value (i.e., the object itself is not copied).
*/
QScriptValue::QScriptValue(const QScriptValue& other)
    : d_ptr(other.d_ptr)
{
}

/*!
    Destroys this QScriptValue.
*/
QScriptValue::~QScriptValue()
{
}

/*!
  Returns true if this QScriptValue is valid; otherwise returns
  false.
*/
bool QScriptValue::isValid() const
{
    return d_ptr->isValid();
}

/*!
  Returns true if this QScriptValue is of the primitive type Boolean;
  otherwise returns false.

  \sa toBool()
*/
bool QScriptValue::isBool() const
{
    return d_ptr->isBool();
}

/*!
  \obsolete

  Use isBool() instead.
  Returns true if this QScriptValue is of the primitive type Boolean;
  otherwise returns false.
*/
bool QScriptValue::isBoolean() const
{
    return d_ptr->isBool();
}

/*!
  Returns true if this QScriptValue is of the primitive type Number;
  otherwise returns false.

  \sa toNumber()
*/
bool QScriptValue::isNumber() const
{
    return d_ptr->isNumber();
}

/*!
  Returns true if this QScriptValue is of the primitive type Null;
  otherwise returns false.

  \sa QScriptEngine::nullValue()
*/
bool QScriptValue::isNull() const
{
    return d_ptr->isNull();
}

/*!
  Returns true if this QScriptValue is of the primitive type String;
  otherwise returns false.

  \sa toString()
*/
bool QScriptValue::isString() const
{
    return d_ptr->isString();
}

/*!
  Returns true if this QScriptValue is of the primitive type Undefined;
  otherwise returns false.

  \sa QScriptEngine::undefinedValue()
*/
bool QScriptValue::isUndefined() const
{
    return d_ptr->isUndefined();
}

/*!
  Returns true if this QScriptValue is an object of the Error class;
  otherwise returns false.

  \sa QScriptContext::throwError()
*/
bool QScriptValue::isError() const
{
    return d_ptr->isError();
}

/*!
  Returns true if this QScriptValue is of the Object type; otherwise
  returns false.

  Note that function values, variant values, and QObject values are
  objects, so this function returns true for such values.

  \sa toObject(), QScriptEngine::newObject()
*/
bool QScriptValue::isObject() const
{
    return d_ptr->isObject();
}

/*!
  Returns true if this QScriptValue is a function; otherwise returns
  false.

  \sa call()
*/
bool QScriptValue::isFunction() const
{
    return d_ptr->isFunction();
}

/*!
  Returns the string value of this QScriptValue, as defined in
  \l{ECMA-262} section 9.8, "ToString".

  Note that if this QScriptValue is an object, calling this function
  has side effects on the script engine, since the engine will call
  the object's toString() function (and possibly valueOf()) in an
  attempt to convert the object to a primitive value (possibly
  resulting in an uncaught script exception).

  \sa isString()
*/
QString QScriptValue::toString() const
{
    return d_ptr->toString();
}

/*!
  Returns the number value of this QScriptValue, as defined in
  \l{ECMA-262} section 9.3, "ToNumber".

  Note that if this QScriptValue is an object, calling this function
  has side effects on the script engine, since the engine will call
  the object's valueOf() function (and possibly toString()) in an
  attempt to convert the object to a primitive value (possibly
  resulting in an uncaught script exception).

  \sa isNumber(), toInteger(), toInt32(), toUInt32(), toUInt16()
*/
qsreal QScriptValue::toNumber() const
{
    return d_ptr->toNumber();
}

/*!
  Returns the boolean value of this QScriptValue, using the conversion
  rules described in \l{ECMA-262} section 9.2, "ToBoolean".

  Note that if this QScriptValue is an object, calling this function
  has side effects on the script engine, since the engine will call
  the object's valueOf() function (and possibly toString()) in an
  attempt to convert the object to a primitive value (possibly
  resulting in an uncaught script exception).

  \sa isBool()
*/
bool QScriptValue::toBool() const
{
    return d_ptr->toBool();
}

/*!
  \obsolete

  Use toBool() instead.
*/
bool QScriptValue::toBoolean() const
{
    return d_ptr->toBool();
}

/*!
  Returns the integer value of this QScriptValue, using the conversion
  rules described in \l{ECMA-262} section 9.4, "ToInteger".

  Note that if this QScriptValue is an object, calling this function
  has side effects on the script engine, since the engine will call
  the object's valueOf() function (and possibly toString()) in an
  attempt to convert the object to a primitive value (possibly
  resulting in an uncaught script exception).

  \sa toNumber()
*/
qsreal QScriptValue::toInteger() const
{
    return d_ptr->toInteger();
}

/*!
  Returns the signed 32-bit integer value of this QScriptValue, using
  the conversion rules described in \l{ECMA-262} section 9.5, "ToInt32".

  Note that if this QScriptValue is an object, calling this function
  has side effects on the script engine, since the engine will call
  the object's valueOf() function (and possibly toString()) in an
  attempt to convert the object to a primitive value (possibly
  resulting in an uncaught script exception).

  \sa toNumber(), toUInt32()
*/
qint32 QScriptValue::toInt32() const
{
    return d_ptr->toInt32();
}

/*!
  Returns the unsigned 32-bit integer value of this QScriptValue, using
  the conversion rules described in \l{ECMA-262} section 9.6, "ToUint32".

  Note that if this QScriptValue is an object, calling this function
  has side effects on the script engine, since the engine will call
  the object's valueOf() function (and possibly toString()) in an
  attempt to convert the object to a primitive value (possibly
  resulting in an uncaught script exception).

  \sa toNumber(), toInt32()
*/
quint32 QScriptValue::toUInt32() const
{
    return d_ptr->toUInt32();
}

/*!
  Returns the unsigned 16-bit integer value of this QScriptValue, using
  the conversion rules described in \l{ECMA-262} section 9.7, "ToUint16".

  Note that if this QScriptValue is an object, calling this function
  has side effects on the script engine, since the engine will call
  the object's valueOf() function (and possibly toString()) in an
  attempt to convert the object to a primitive value (possibly
  resulting in an uncaught script exception).

  \sa toNumber()
*/
quint16 QScriptValue::toUInt16() const
{
    return d_ptr->toUInt16();
}

/*!
  Calls this QScriptValue as a function, using \a thisObject as
  the `this' object in the function call, and passing \a args
  as arguments to the function. Returns the value returned from
  the function.

  If this QScriptValue is not a function, call() does nothing
  and returns an invalid QScriptValue.

  Note that if \a thisObject is not an object, the global object
  (see \l{QScriptEngine::globalObject()}) will be used as the
  `this' object.

  Calling call() can cause an exception to occur in the script engine;
  in that case, call() returns the value that was thrown (typically an
  \c{Error} object). You can call
  QScriptEngine::hasUncaughtException() to determine if an exception
  occurred.

  \snippet doc/src/snippets/code/src_script_qscriptvalue.cpp 2

  \sa construct()
*/
QScriptValue QScriptValue::call(const QScriptValue& thisObject, const QScriptValueList& args)
{
    return d_ptr->call(thisObject.d_ptr.data(), args);
}

/*!
  Returns the QScriptEngine that created this QScriptValue,
  or 0 if this QScriptValue is invalid or the value is not
  associated with a particular engine.
*/
QScriptEngine* QScriptValue::engine() const
{
    QScriptEnginePrivate* engine = d_ptr->engine();
    if (engine)
        return QScriptEnginePrivate::get(engine);
    return 0;
}

/*!
  Assigns the \a other value to this QScriptValue.

  Note that if \a other is an object (isObject() returns true),
  only a reference to the underlying object will be assigned;
  the object itself will not be copied.
*/
QScriptValue& QScriptValue::operator=(const QScriptValue& other)
{
    d_ptr = other.d_ptr;
    return *this;
}

/*!
  Returns true if this QScriptValue is equal to \a other, otherwise
  returns false. The comparison follows the behavior described in
  \l{ECMA-262} section 11.9.3, "The Abstract Equality Comparison
  Algorithm".

  This function can return true even if the type of this QScriptValue
  is different from the type of the \a other value; i.e. the
  comparison is not strict.  For example, comparing the number 9 to
  the string "9" returns true; comparing an undefined value to a null
  value returns true; comparing a \c{Number} object whose primitive
  value is 6 to a \c{String} object whose primitive value is "6"
  returns true; and comparing the number 1 to the boolean value
  \c{true} returns true. If you want to perform a comparison
  without such implicit value conversion, use strictlyEquals().

  Note that if this QScriptValue or the \a other value are objects,
  calling this function has side effects on the script engine, since
  the engine will call the object's valueOf() function (and possibly
  toString()) in an attempt to convert the object to a primitive value
  (possibly resulting in an uncaught script exception).

  \sa strictlyEquals(), lessThan()
*/
bool QScriptValue::equals(const QScriptValue& other) const
{
    return d_ptr == other.d_ptr || d_ptr->equals(QScriptValuePrivate::get(other));
}

/*!
  Returns true if this QScriptValue is equal to \a other using strict
  comparison (no conversion), otherwise returns false. The comparison
  follows the behavior described in \l{ECMA-262} section 11.9.6, "The
  Strict Equality Comparison Algorithm".

  If the type of this QScriptValue is different from the type of the
  \a other value, this function returns false. If the types are equal,
  the result depends on the type, as shown in the following table:

    \table
    \header \o Type \o Result
    \row    \o Undefined  \o true
    \row    \o Null       \o true
    \row    \o Boolean    \o true if both values are true, false otherwise
    \row    \o Number     \o false if either value is NaN (Not-a-Number); true if values are equal, false otherwise
    \row    \o String     \o true if both values are exactly the same sequence of characters, false otherwise
    \row    \o Object     \o true if both values refer to the same object, false otherwise
    \endtable

  \sa equals()
*/
bool QScriptValue::strictlyEquals(const QScriptValue& other) const
{
    return d_ptr == other.d_ptr || d_ptr->strictlyEquals(QScriptValuePrivate::get(other));
}
