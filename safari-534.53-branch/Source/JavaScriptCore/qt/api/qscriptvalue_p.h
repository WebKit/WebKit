/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

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

#ifndef qscriptvalue_p_h
#define qscriptvalue_p_h

#include "qscriptconverter_p.h"
#include "qscriptengine_p.h"
#include "qscriptvalue.h"
#include <JavaScriptCore/JavaScript.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JSObjectRefPrivate.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qmath.h>
#include <QtCore/qnumeric.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qvarlengtharray.h>

class QScriptEngine;
class QScriptValue;

/*
  \internal
  \class QScriptValuePrivate

  Implementation of QScriptValue.
  The implementation is based on a state machine. The states names are included in
  QScriptValuePrivate::State. Each method should check for the current state and then perform a
  correct action.

  State:
    Invalid -> QSVP is invalid, no assumptions should be made about class members (apart from m_value).
    CString -> QSVP is created from QString or const char* and no JSC engine has been associated yet.
        Current value is kept in m_string,
    CNumber -> QSVP is created from int, uint, double... and no JSC engine has been bind yet. Current
        value is kept in m_number
    CBool -> QSVP is created from bool and no JSC engine has been associated yet. Current value is kept
        in m_bool
    CNull -> QSVP is null, but a JSC engine hasn't been associated yet.
    CUndefined -> QSVP is undefined, but a JSC engine hasn't been associated yet.
    JSValue -> QSVP is associated with engine, but there is no information about real type, the state
        have really short live cycle. Normally it is created as a function call result.
    JSPrimitive -> QSVP is associated with engine, and it is sure that it isn't a JavaScript object.
    JSObject -> QSVP is associated with engine, and it is sure that it is a JavaScript object.

  Each state keep all necessary information to invoke all methods, if not it should be changed to
  a proper state. Changed state shouldn't be reverted.

  The QScriptValuePrivate use the JSC C API directly. The QSVP type is equal to combination of
  the JSValueRef and the JSObjectRef, and it could be automatically casted to these types by cast
  operators.
*/

class QScriptValuePrivate : public QSharedData {
public:
    inline static QScriptValuePrivate* get(const QScriptValue& q);
    inline static QScriptValue get(const QScriptValuePrivate* d);
    inline static QScriptValue get(QScriptValuePrivate* d);

    inline ~QScriptValuePrivate();

    inline QScriptValuePrivate();
    inline QScriptValuePrivate(const QString& string);
    inline QScriptValuePrivate(bool value);
    inline QScriptValuePrivate(int number);
    inline QScriptValuePrivate(uint number);
    inline QScriptValuePrivate(qsreal number);
    inline QScriptValuePrivate(QScriptValue::SpecialValue value);

    inline QScriptValuePrivate(const QScriptEnginePrivate* engine, bool value);
    inline QScriptValuePrivate(const QScriptEnginePrivate* engine, int value);
    inline QScriptValuePrivate(const QScriptEnginePrivate* engine, uint value);
    inline QScriptValuePrivate(const QScriptEnginePrivate* engine, qsreal value);
    inline QScriptValuePrivate(const QScriptEnginePrivate* engine, const QString& value);
    inline QScriptValuePrivate(const QScriptEnginePrivate* engine, QScriptValue::SpecialValue value);

    inline QScriptValuePrivate(const QScriptEnginePrivate* engine, JSValueRef value);
    inline QScriptValuePrivate(const QScriptEnginePrivate* engine, JSObjectRef object);

    inline bool isValid() const;
    inline bool isBool();
    inline bool isNumber();
    inline bool isNull();
    inline bool isString();
    inline bool isUndefined();
    inline bool isError();
    inline bool isObject();
    inline bool isFunction();
    inline bool isArray();
    inline bool isDate();

    inline QString toString() const;
    inline qsreal toNumber() const;
    inline bool toBool() const;
    inline qsreal toInteger() const;
    inline qint32 toInt32() const;
    inline quint32 toUInt32() const;
    inline quint16 toUInt16() const;

    inline QScriptValuePrivate* toObject(QScriptEnginePrivate* engine);
    inline QScriptValuePrivate* toObject();
    inline QDateTime toDateTime();
    inline QScriptValuePrivate* prototype();
    inline void setPrototype(QScriptValuePrivate* prototype);

    inline bool equals(QScriptValuePrivate* other);
    inline bool strictlyEquals(QScriptValuePrivate* other);
    inline bool instanceOf(QScriptValuePrivate* other);
    inline bool assignEngine(QScriptEnginePrivate* engine);

    inline QScriptValuePrivate* property(const QString& name, const QScriptValue::ResolveFlags& mode);
    inline QScriptValuePrivate* property(const QScriptStringPrivate* name, const QScriptValue::ResolveFlags& mode);
    inline QScriptValuePrivate* property(quint32 arrayIndex, const QScriptValue::ResolveFlags& mode);
    inline JSValueRef property(quint32 property, JSValueRef* exception);
    inline JSValueRef property(JSStringRef property, JSValueRef* exception);
    inline bool hasOwnProperty(quint32 property);
    inline bool hasOwnProperty(JSStringRef property);
    template<typename T>
    inline QScriptValuePrivate* property(T name, const QScriptValue::ResolveFlags& mode);

    inline void setProperty(const QString& name, QScriptValuePrivate* value, const QScriptValue::PropertyFlags& flags);
    inline void setProperty(const QScriptStringPrivate* name, QScriptValuePrivate* value, const QScriptValue::PropertyFlags& flags);
    inline void setProperty(const quint32 indexArray, QScriptValuePrivate* value, const QScriptValue::PropertyFlags& flags);
    inline void setProperty(quint32 property, JSValueRef value, JSPropertyAttributes flags, JSValueRef* exception);
    inline void setProperty(JSStringRef property, JSValueRef value, JSPropertyAttributes flags, JSValueRef* exception);
    inline void deleteProperty(quint32 property, JSValueRef* exception);
    inline void deleteProperty(JSStringRef property, JSValueRef* exception);
    template<typename T>
    inline void setProperty(T name, QScriptValuePrivate* value, const QScriptValue::PropertyFlags& flags);

    QScriptValue::PropertyFlags propertyFlags(const QString& name, const QScriptValue::ResolveFlags& mode);
    QScriptValue::PropertyFlags propertyFlags(const QScriptStringPrivate* name, const QScriptValue::ResolveFlags& mode);
    QScriptValue::PropertyFlags propertyFlags(const JSStringRef name, const QScriptValue::ResolveFlags& mode);

    inline QScriptValuePrivate* call(const QScriptValuePrivate* , const QScriptValueList& args);

    inline operator JSValueRef() const;
    inline operator JSObjectRef() const;

    inline QScriptEnginePrivate* engine() const;

private:
    // Please, update class documentation when you change the enum.
    enum State {
        Invalid = 0,
        CString = 0x1000,
        CNumber,
        CBool,
        CNull,
        CUndefined,
        JSValue = 0x2000, // JS values are equal or higher then this value.
        JSPrimitive,
        JSObject
    } m_state;
    QScriptEnginePtr m_engine;
    union Value
    {
        bool m_bool;
        qsreal m_number;
        JSValueRef m_value;
        JSObjectRef m_object;
        QString* m_string;

        Value() : m_number(0) {}
        Value(bool value) : m_bool(value) {}
        Value(int number) : m_number(number) {}
        Value(uint number) : m_number(number) {}
        Value(qsreal number) : m_number(number) {}
        Value(JSValueRef value) : m_value(value) {}
        Value(JSObjectRef object) : m_object(object) {}
        Value(QString* string) : m_string(string) {}
    } u;

    inline State refinedJSValue();

    inline bool isJSBased() const;
    inline bool isNumberBased() const;
    inline bool isStringBased() const;
};

QScriptValuePrivate* QScriptValuePrivate::get(const QScriptValue& q) { return q.d_ptr.data(); }

QScriptValue QScriptValuePrivate::get(const QScriptValuePrivate* d)
{
    return QScriptValue(const_cast<QScriptValuePrivate*>(d));
}

QScriptValue QScriptValuePrivate::get(QScriptValuePrivate* d)
{
    return QScriptValue(d);
}

QScriptValuePrivate::~QScriptValuePrivate()
{
    if (isJSBased())
        JSValueUnprotect(*m_engine, u.m_value);
    else if (isStringBased())
        delete u.m_string;
}

QScriptValuePrivate::QScriptValuePrivate()
    : m_state(Invalid)
{
}

QScriptValuePrivate::QScriptValuePrivate(const QString& string)
    : m_state(CString)
    , u(new QString(string))
{
}

QScriptValuePrivate::QScriptValuePrivate(bool value)
    : m_state(CBool)
    , u(value)
{
}

QScriptValuePrivate::QScriptValuePrivate(int number)
    : m_state(CNumber)
    , u(number)
{
}

QScriptValuePrivate::QScriptValuePrivate(uint number)
    : m_state(CNumber)
    , u(number)
{
}

QScriptValuePrivate::QScriptValuePrivate(qsreal number)
    : m_state(CNumber)
    , u(number)
{
}

QScriptValuePrivate::QScriptValuePrivate(QScriptValue::SpecialValue value)
    : m_state(value == QScriptValue::NullValue ? CNull : CUndefined)
{
}

QScriptValuePrivate::QScriptValuePrivate(const QScriptEnginePrivate* engine, bool value)
    : m_state(JSPrimitive)
    , m_engine(const_cast<QScriptEnginePrivate*>(engine))
    , u(engine->makeJSValue(value))
{
    Q_ASSERT(engine);
    JSValueProtect(*m_engine, u.m_value);
}

QScriptValuePrivate::QScriptValuePrivate(const QScriptEnginePrivate* engine, int value)
    : m_state(JSPrimitive)
    , m_engine(const_cast<QScriptEnginePrivate*>(engine))
    , u(m_engine->makeJSValue(value))
{
    Q_ASSERT(engine);
    JSValueProtect(*m_engine, u.m_value);
}

QScriptValuePrivate::QScriptValuePrivate(const QScriptEnginePrivate* engine, uint value)
    : m_state(JSPrimitive)
    , m_engine(const_cast<QScriptEnginePrivate*>(engine))
    , u(m_engine->makeJSValue(value))
{
    Q_ASSERT(engine);
    JSValueProtect(*m_engine, u.m_value);
}

QScriptValuePrivate::QScriptValuePrivate(const QScriptEnginePrivate* engine, qsreal value)
    : m_state(JSPrimitive)
    , m_engine(const_cast<QScriptEnginePrivate*>(engine))
    , u(m_engine->makeJSValue(value))
{
    Q_ASSERT(engine);
    JSValueProtect(*m_engine, u.m_value);
}

QScriptValuePrivate::QScriptValuePrivate(const QScriptEnginePrivate* engine, const QString& value)
    : m_state(JSPrimitive)
    , m_engine(const_cast<QScriptEnginePrivate*>(engine))
    , u(m_engine->makeJSValue(value))
{
    Q_ASSERT(engine);
    JSValueProtect(*m_engine, u.m_value);
}

QScriptValuePrivate::QScriptValuePrivate(const QScriptEnginePrivate* engine, QScriptValue::SpecialValue value)
    : m_state(JSPrimitive)
    , m_engine(const_cast<QScriptEnginePrivate*>(engine))
    , u(m_engine->makeJSValue(value))
{
    Q_ASSERT(engine);
    JSValueProtect(*m_engine, u.m_value);
}

QScriptValuePrivate::QScriptValuePrivate(const QScriptEnginePrivate* engine, JSValueRef value)
    : m_state(JSValue)
    , m_engine(const_cast<QScriptEnginePrivate*>(engine))
    , u(value)
{
    Q_ASSERT(engine);
    Q_ASSERT(value);
    JSValueProtect(*m_engine, u.m_value);
}

QScriptValuePrivate::QScriptValuePrivate(const QScriptEnginePrivate* engine, JSObjectRef object)
    : m_state(JSObject)
    , m_engine(const_cast<QScriptEnginePrivate*>(engine))
    , u(object)
{
    Q_ASSERT(engine);
    Q_ASSERT(object);
    JSValueProtect(*m_engine, object);
}

bool QScriptValuePrivate::isValid() const { return m_state != Invalid; }

bool QScriptValuePrivate::isBool()
{
    switch (m_state) {
    case CBool:
        return true;
    case JSValue:
        if (refinedJSValue() != JSPrimitive)
            return false;
        // Fall-through.
    case JSPrimitive:
        return JSValueIsBoolean(*m_engine, *this);
    default:
        return false;
    }
}

bool QScriptValuePrivate::isNumber()
{
    switch (m_state) {
    case CNumber:
        return true;
    case JSValue:
        if (refinedJSValue() != JSPrimitive)
            return false;
        // Fall-through.
    case JSPrimitive:
        return JSValueIsNumber(*m_engine, *this);
    default:
        return false;
    }
}

bool QScriptValuePrivate::isNull()
{
    switch (m_state) {
    case CNull:
        return true;
    case JSValue:
        if (refinedJSValue() != JSPrimitive)
            return false;
        // Fall-through.
    case JSPrimitive:
        return JSValueIsNull(*m_engine, *this);
    default:
        return false;
    }
}

bool QScriptValuePrivate::isString()
{
    switch (m_state) {
    case CString:
        return true;
    case JSValue:
        if (refinedJSValue() != JSPrimitive)
            return false;
        // Fall-through.
    case JSPrimitive:
        return JSValueIsString(*m_engine, *this);
    default:
        return false;
    }
}

bool QScriptValuePrivate::isUndefined()
{
    switch (m_state) {
    case CUndefined:
        return true;
    case JSValue:
        if (refinedJSValue() != JSPrimitive)
            return false;
        // Fall-through.
    case JSPrimitive:
        return JSValueIsUndefined(*m_engine, *this);
    default:
        return false;
    }
}

bool QScriptValuePrivate::isError()
{
    switch (m_state) {
    case JSValue:
        if (refinedJSValue() != JSObject)
            return false;
        // Fall-through.
    case JSObject:
        return m_engine->isError(*this);
    default:
        return false;
    }
}

bool QScriptValuePrivate::isObject()
{
    switch (m_state) {
    case JSValue:
        return refinedJSValue() == JSObject;
    case JSObject:
        return true;

    default:
        return false;
    }
}

bool QScriptValuePrivate::isFunction()
{
    switch (m_state) {
    case JSValue:
        if (refinedJSValue() != JSObject)
            return false;
        // Fall-through.
    case JSObject:
        return JSObjectIsFunction(*m_engine, *this);
    default:
        return false;
    }
}

bool QScriptValuePrivate::isArray()
{
    switch (m_state) {
    case JSValue:
        if (refinedJSValue() != JSObject)
            return false;
        // Fall-through.
    case JSObject:
        return m_engine->isArray(*this);
    default:
        return false;
    }
}

bool QScriptValuePrivate::isDate()
{
    switch (m_state) {
    case JSValue:
        if (refinedJSValue() != JSObject)
            return false;
        // Fall-through.
    case JSObject:
        return m_engine->isDate(*this);
    default:
        return false;
    }
}

QString QScriptValuePrivate::toString() const
{
    switch (m_state) {
    case Invalid:
        return QString();
    case CBool:
        return u.m_bool ? QString::fromLatin1("true") : QString::fromLatin1("false");
    case CString:
        return *u.m_string;
    case CNumber:
        return QScriptConverter::toString(u.m_number);
    case CNull:
        return QString::fromLatin1("null");
    case CUndefined:
        return QString::fromLatin1("undefined");
    case JSValue:
    case JSPrimitive:
    case JSObject:
        JSValueRef exception = 0;
        JSRetainPtr<JSStringRef> ptr(Adopt, JSValueToStringCopy(*m_engine, *this, &exception));
        m_engine->setException(exception);
        return QScriptConverter::toString(ptr.get());
    }

    Q_ASSERT_X(false, "toString()", "Not all states are included in the previous switch statement.");
    return QString(); // Avoid compiler warning.
}

qsreal QScriptValuePrivate::toNumber() const
{
    switch (m_state) {
    case JSValue:
    case JSPrimitive:
    case JSObject:
        {
            JSValueRef exception = 0;
            qsreal result = JSValueToNumber(*m_engine, *this, &exception);
            m_engine->setException(exception);
            return result;
        }
    case CNumber:
        return u.m_number;
    case CBool:
        return u.m_bool ? 1 : 0;
    case CNull:
    case Invalid:
        return 0;
    case CUndefined:
        return qQNaN();
    case CString:
        bool ok;
        qsreal result = u.m_string->toDouble(&ok);
        if (ok)
            return result;
        result = u.m_string->toInt(&ok, 0); // Try other bases.
        if (ok)
            return result;
        if (*u.m_string == "Infinity" || *u.m_string == "-Infinity")
            return qInf();
        return u.m_string->length() ? qQNaN() : 0;
    }

    Q_ASSERT_X(false, "toNumber()", "Not all states are included in the previous switch statement.");
    return 0; // Avoid compiler warning.
}

bool QScriptValuePrivate::toBool() const
{
    switch (m_state) {
    case JSValue:
    case JSPrimitive:
        return JSValueToBoolean(*m_engine, *this);
    case JSObject:
        return true;
    case CNumber:
        return !(qIsNaN(u.m_number) || !u.m_number);
    case CBool:
        return u.m_bool;
    case Invalid:
    case CNull:
    case CUndefined:
        return false;
    case CString:
        return u.m_string->length();
    }

    Q_ASSERT_X(false, "toBool()", "Not all states are included in the previous switch statement.");
    return false; // Avoid compiler warning.
}

qsreal QScriptValuePrivate::toInteger() const
{
    qsreal result = toNumber();
    if (qIsNaN(result))
        return 0;
    if (qIsInf(result))
        return result;
    return (result > 0) ? qFloor(result) : -1 * qFloor(-result);
}

qint32 QScriptValuePrivate::toInt32() const
{
    qsreal result = toInteger();
    // Orginaly it should look like that (result == 0 || qIsInf(result) || qIsNaN(result)), but
    // some of these operation are invoked in toInteger subcall.
    if (qIsInf(result))
        return 0;
    return result;
}

quint32 QScriptValuePrivate::toUInt32() const
{
    qsreal result = toInteger();
    // Orginaly it should look like that (result == 0 || qIsInf(result) || qIsNaN(result)), but
    // some of these operation are invoked in toInteger subcall.
    if (qIsInf(result))
        return 0;
    return result;
}

quint16 QScriptValuePrivate::toUInt16() const
{
    return toInt32();
}

/*!
  Creates a copy of this value and converts it to an object. If this value is an object
  then pointer to this value will be returned.
  \attention it should not happen but if this value is bounded to a different engine that the given, the first
  one will be used.
  \internal
  */
QScriptValuePrivate* QScriptValuePrivate::toObject(QScriptEnginePrivate* engine)
{
    switch (m_state) {
    case Invalid:
    case CNull:
    case CUndefined:
        return new QScriptValuePrivate;
    case CString:
        {
            // Exception can't occur here.
            JSObjectRef object = JSValueToObject(*engine, engine->makeJSValue(*u.m_string), /* exception */ 0);
            Q_ASSERT(object);
            return new QScriptValuePrivate(engine, object);
        }
    case CNumber:
        {
            // Exception can't occur here.
            JSObjectRef object = JSValueToObject(*engine, engine->makeJSValue(u.m_number), /* exception */ 0);
            Q_ASSERT(object);
            return new QScriptValuePrivate(engine, object);
        }
    case CBool:
        {
            // Exception can't occure here.
            JSObjectRef object = JSValueToObject(*engine, engine->makeJSValue(u.m_bool), /* exception */ 0);
            Q_ASSERT(object);
            return new QScriptValuePrivate(engine, object);
        }
    case JSValue:
        if (refinedJSValue() != JSPrimitive)
            break;
        // Fall-through.
    case JSPrimitive:
        {
            if (engine != this->engine())
                qWarning("QScriptEngine::toObject: cannot convert value created in a different engine");
            JSValueRef exception = 0;
            JSObjectRef object = JSValueToObject(*m_engine, *this, &exception);
            if (object)
                return new QScriptValuePrivate(m_engine.constData(), object);
            else
                m_engine->setException(exception, QScriptEnginePrivate::NotNullException);

        }
        return new QScriptValuePrivate;
    case JSObject:
        break;
    }

    if (engine != this->engine())
        qWarning("QScriptEngine::toObject: cannot convert value created in a different engine");
    Q_ASSERT(m_state == JSObject);
    return this;
}

/*!
  This method is created only for QScriptValue::toObject() purpose which is obsolete.
  \internal
 */
QScriptValuePrivate* QScriptValuePrivate::toObject()
{
    if (isJSBased())
        return toObject(m_engine.data());

    // Without an engine there is not much we can do.
    return new QScriptValuePrivate;
}

QDateTime QScriptValuePrivate::toDateTime()
{
    if (!isDate())
        return QDateTime();

    JSValueRef exception = 0;
    qsreal t = JSValueToNumber(*m_engine, *this, &exception);

    if (exception) {
        m_engine->setException(exception, QScriptEnginePrivate::NotNullException);
        return QDateTime();
    }

    QDateTime result;
    result.setMSecsSinceEpoch(qint64(t));
    return result;
}

inline QScriptValuePrivate* QScriptValuePrivate::prototype()
{
    if (isObject()) {
        JSValueRef prototype = JSObjectGetPrototype(*m_engine, *this);
        if (JSValueIsNull(*m_engine, prototype))
            return new QScriptValuePrivate(engine(), prototype);
        // The prototype could be either a null or a JSObject, so it is safe to cast the prototype
        // to the JSObjectRef here.
        return new QScriptValuePrivate(engine(), const_cast<JSObjectRef>(prototype));
    }
    return new QScriptValuePrivate;
}

inline void QScriptValuePrivate::setPrototype(QScriptValuePrivate* prototype)
{
    if (isObject() && prototype->isValid() && (prototype->isObject() || prototype->isNull())) {
        if (engine() != prototype->engine()) {
            qWarning("QScriptValue::setPrototype() failed: cannot set a prototype created in a different engine");
            return;
        }
        // FIXME: This could be replaced by a new, faster API
        // look at https://bugs.webkit.org/show_bug.cgi?id=40060
        JSObjectSetPrototype(*m_engine, *this, *prototype);
        JSValueRef proto = JSObjectGetPrototype(*m_engine, *this);
        if (!JSValueIsStrictEqual(*m_engine, proto, *prototype))
            qWarning("QScriptValue::setPrototype() failed: cyclic prototype value");
    }
}

bool QScriptValuePrivate::equals(QScriptValuePrivate* other)
{
    if (!isValid())
        return !other->isValid();

    if (!other->isValid())
        return false;

    if (!isJSBased() && !other->isJSBased()) {
        switch (m_state) {
        case CNull:
        case CUndefined:
            return other->isUndefined() || other->isNull();
        case CNumber:
            switch (other->m_state) {
            case CBool:
            case CString:
                return u.m_number == other->toNumber();
            case CNumber:
                return u.m_number == other->u.m_number;
            default:
                return false;
            }
        case CBool:
            switch (other->m_state) {
            case CBool:
                return u.m_bool == other->u.m_bool;
            case CNumber:
                return toNumber() == other->u.m_number;
            case CString:
                return toNumber() == other->toNumber();
            default:
                return false;
            }
        case CString:
            switch (other->m_state) {
            case CBool:
                return toNumber() == other->toNumber();
            case CNumber:
                return toNumber() == other->u.m_number;
            case CString:
                return *u.m_string == *other->u.m_string;
            default:
                return false;
            }
        default:
            Q_ASSERT_X(false, "equals()", "Not all states are included in the previous switch statement.");
        }
    }

    if (isJSBased() && !other->isJSBased()) {
        if (!other->assignEngine(engine())) {
            qWarning("equals(): Cannot compare to a value created in a different engine");
            return false;
        }
    } else if (!isJSBased() && other->isJSBased()) {
        if (!assignEngine(other->engine())) {
            qWarning("equals(): Cannot compare to a value created in a different engine");
            return false;
        }
    }

    JSValueRef exception = 0;
    bool result = JSValueIsEqual(*m_engine, *this, *other, &exception);
    m_engine->setException(exception);
    return result;
}

bool QScriptValuePrivate::strictlyEquals(QScriptValuePrivate* other)
{
    if (isJSBased()) {
        // We can't compare these two values without binding to the same engine.
        if (!other->isJSBased()) {
            if (other->assignEngine(engine()))
                return JSValueIsStrictEqual(*m_engine, *this, *other);
            return false;
        }
        if (other->engine() != engine()) {
            qWarning("strictlyEquals(): Cannot compare to a value created in a different engine");
            return false;
        }
        return JSValueIsStrictEqual(*m_engine, *this, *other);
    }
    if (isStringBased()) {
        if (other->isStringBased())
            return *u.m_string == *(other->u.m_string);
        if (other->isJSBased()) {
            assignEngine(other->engine());
            return JSValueIsStrictEqual(*m_engine, *this, *other);
        }
    }
    if (isNumberBased()) {
        if (other->isNumberBased())
            return u.m_number == other->u.m_number;
        if (other->isJSBased()) {
            assignEngine(other->engine());
            return JSValueIsStrictEqual(*m_engine, *this, *other);
        }
    }
    if (!isValid() && !other->isValid())
        return true;

    return false;
}

inline bool QScriptValuePrivate::instanceOf(QScriptValuePrivate* other)
{
    if (!isJSBased() || !other->isObject())
        return false;
    JSValueRef exception = 0;
    bool result = JSValueIsInstanceOfConstructor(*m_engine, *this, *other, &exception);
    m_engine->setException(exception);
    return result;
}

/*!
  Tries to assign \a engine to this value. Returns true on success; otherwise returns false.
*/
bool QScriptValuePrivate::assignEngine(QScriptEnginePrivate* engine)
{
    Q_ASSERT(engine);
    JSValueRef value;
    switch (m_state) {
    case CBool:
        value = engine->makeJSValue(u.m_bool);
        break;
    case CString:
        value = engine->makeJSValue(*u.m_string);
        delete u.m_string;
        break;
    case CNumber:
        value = engine->makeJSValue(u.m_number);
        break;
    case CNull:
        value = engine->makeJSValue(QScriptValue::NullValue);
        break;
    case CUndefined:
        value = engine->makeJSValue(QScriptValue::UndefinedValue);
        break;
    default:
        if (!isJSBased())
            Q_ASSERT_X(!isJSBased(), "assignEngine()", "Not all states are included in the previous switch statement.");
        else
            qWarning("JSValue can't be rassigned to an another engine.");
        return false;
    }
    m_engine = engine;
    m_state = JSPrimitive;
    u.m_value = value;
    JSValueProtect(*m_engine, value);
    return true;
}

inline QScriptValuePrivate* QScriptValuePrivate::property(const QString& name, const QScriptValue::ResolveFlags& mode)
{
    JSRetainPtr<JSStringRef> propertyName(Adopt, QScriptConverter::toString(name));
    return property<JSStringRef>(propertyName.get(), mode);
}

inline QScriptValuePrivate* QScriptValuePrivate::property(const QScriptStringPrivate* name, const QScriptValue::ResolveFlags& mode)
{
    return property<JSStringRef>(*name, mode);
}

inline QScriptValuePrivate* QScriptValuePrivate::property(quint32 arrayIndex, const QScriptValue::ResolveFlags& mode)
{
    return property<quint32>(arrayIndex, mode);
}

/*!
    \internal
    This method was created to unify access to the JSObjectGetPropertyAtIndex and the JSObjectGetProperty.
*/
inline JSValueRef QScriptValuePrivate::property(quint32 property, JSValueRef* exception)
{
    return JSObjectGetPropertyAtIndex(*m_engine, *this, property, exception);
}

/*!
    \internal
    This method was created to unify access to the JSObjectGetPropertyAtIndex and the JSObjectGetProperty.
*/
inline JSValueRef QScriptValuePrivate::property(JSStringRef property, JSValueRef* exception)
{
    return JSObjectGetProperty(*m_engine, *this, property, exception);
}

/*!
    \internal
    This method was created to unify acccess to hasOwnProperty, same function for an array index
    and a property name access.
*/
inline bool QScriptValuePrivate::hasOwnProperty(quint32 property)
{
    Q_ASSERT(isObject());
    // FIXME it could be faster, but JSC C API doesn't expose needed functionality.
    JSRetainPtr<JSStringRef> propertyName(Adopt, QScriptConverter::toString(QString::number(property)));
    return hasOwnProperty(propertyName.get());
}

/*!
    \internal
    This method was created to unify acccess to hasOwnProperty, same function for an array index
    and a property name access.
*/
inline bool QScriptValuePrivate::hasOwnProperty(JSStringRef property)
{
    Q_ASSERT(isObject());
    return m_engine->objectHasOwnProperty(*this, property);
}

/*!
    \internal
    This function gets property of an object.
    \arg propertyName could be type of quint32 (an array index) or JSStringRef (a property name).
*/
template<typename T>
inline QScriptValuePrivate* QScriptValuePrivate::property(T propertyName, const QScriptValue::ResolveFlags& mode)
{
    if (!isObject())
        return new QScriptValuePrivate();

    if ((mode == QScriptValue::ResolveLocal) && (!hasOwnProperty(propertyName)))
        return new QScriptValuePrivate();

    JSValueRef exception = 0;
    JSValueRef value = property(propertyName, &exception);

    if (exception) {
        m_engine->setException(exception, QScriptEnginePrivate::NotNullException);
        return new QScriptValuePrivate(engine(), exception);
    }
    if (JSValueIsUndefined(*m_engine, value))
        return new QScriptValuePrivate;
    return new QScriptValuePrivate(engine(), value);
}

inline void QScriptValuePrivate::setProperty(const QString& name, QScriptValuePrivate* value, const QScriptValue::PropertyFlags& flags)
{
    JSRetainPtr<JSStringRef> propertyName(Adopt, QScriptConverter::toString(name));
    setProperty<JSStringRef>(propertyName.get(), value, flags);
}

inline void QScriptValuePrivate::setProperty(quint32 arrayIndex, QScriptValuePrivate* value, const QScriptValue::PropertyFlags& flags)
{
    setProperty<quint32>(arrayIndex, value, flags);
}

inline void QScriptValuePrivate::setProperty(const QScriptStringPrivate* name, QScriptValuePrivate* value, const QScriptValue::PropertyFlags& flags)
{
    setProperty<JSStringRef>(*name, value, flags);
}

/*!
    \internal
    This method was created to unify access to the JSObjectSetPropertyAtIndex and the JSObjectSetProperty.
*/
inline void QScriptValuePrivate::setProperty(quint32 property, JSValueRef value, JSPropertyAttributes flags, JSValueRef* exception)
{
    Q_ASSERT(isObject());
    if (flags) {
        // FIXME This could be better, but JSC C API doesn't expose needed functionality. It is
        // not possible to create / modify a property attribute via an array index.
        JSRetainPtr<JSStringRef> propertyName(Adopt, QScriptConverter::toString(QString::number(property)));
        JSObjectSetProperty(*m_engine, *this, propertyName.get(), value, flags, exception);
        return;
    }
    JSObjectSetPropertyAtIndex(*m_engine, *this, property, value, exception);
}

/*!
    \internal
    This method was created to unify access to the JSObjectSetPropertyAtIndex and the JSObjectSetProperty.
*/
inline void QScriptValuePrivate::setProperty(JSStringRef property, JSValueRef value, JSPropertyAttributes flags, JSValueRef* exception)
{
    Q_ASSERT(isObject());
    JSObjectSetProperty(*m_engine, *this, property, value, flags, exception);
}

/*!
    \internal
    This method was created to unify access to the JSObjectDeleteProperty and a teoretical JSObjectDeletePropertyAtIndex
    which doesn't exist now.
*/
inline void QScriptValuePrivate::deleteProperty(quint32 property, JSValueRef* exception)
{
    // FIXME It could be faster, we need a JSC C API for deleting array index properties.
    JSRetainPtr<JSStringRef> propertyName(Adopt, QScriptConverter::toString(QString::number(property)));
    JSObjectDeleteProperty(*m_engine, *this, propertyName.get(), exception);
}

/*!
    \internal
    This method was created to unify access to the JSObjectDeleteProperty and a teoretical JSObjectDeletePropertyAtIndex.
*/
inline void QScriptValuePrivate::deleteProperty(JSStringRef property, JSValueRef* exception)
{
    Q_ASSERT(isObject());
    JSObjectDeleteProperty(*m_engine, *this, property, exception);
}

template<typename T>
inline void QScriptValuePrivate::setProperty(T name, QScriptValuePrivate* value, const QScriptValue::PropertyFlags& flags)
{
    if (!isObject())
        return;

    if (!value->isJSBased())
        value->assignEngine(engine());

    JSValueRef exception = 0;
    if (!value->isValid()) {
        // Remove the property.
        deleteProperty(name, &exception);
        m_engine->setException(exception);
        return;
    }
    if (m_engine != value->m_engine) {
        qWarning("QScriptValue::setProperty() failed: cannot set value created in a different engine");
        return;
    }

    setProperty(name, *value, QScriptConverter::toPropertyFlags(flags), &exception);
    m_engine->setException(exception);
}

inline QScriptValue::PropertyFlags QScriptValuePrivate::propertyFlags(const QString& name, const QScriptValue::ResolveFlags& mode)
{
    JSRetainPtr<JSStringRef> propertyName(Adopt, QScriptConverter::toString(name));
    return propertyFlags(propertyName.get(), mode);
}

inline QScriptValue::PropertyFlags QScriptValuePrivate::propertyFlags(const QScriptStringPrivate* name, const QScriptValue::ResolveFlags& mode)
{
    return propertyFlags(*name, mode);
}

inline QScriptValue::PropertyFlags QScriptValuePrivate::propertyFlags(JSStringRef name, const QScriptValue::ResolveFlags& mode)
{
    unsigned flags = 0;
    if (!isObject())
        return QScriptValue::PropertyFlags(flags);

    // FIXME It could be faster and nicer, but new JSC C API should be created.
    static JSStringRef objectName = QScriptConverter::toString("Object");
    static JSStringRef propertyDescriptorName = QScriptConverter::toString("getOwnPropertyDescriptor");

    // FIXME This is dangerous if global object was modified (bug 41839).
    JSValueRef exception = 0;
    JSObjectRef globalObject = JSContextGetGlobalObject(*m_engine);
    JSValueRef objectConstructor = JSObjectGetProperty(*m_engine, globalObject, objectName, &exception);
    Q_ASSERT(JSValueIsObject(*m_engine, objectConstructor));
    JSValueRef propertyDescriptorGetter = JSObjectGetProperty(*m_engine, const_cast<JSObjectRef>(objectConstructor), propertyDescriptorName, &exception);
    Q_ASSERT(JSValueIsObject(*m_engine, propertyDescriptorGetter));

    JSValueRef arguments[] = { *this, JSValueMakeString(*m_engine, name) };
    JSObjectRef propertyDescriptor
            = const_cast<JSObjectRef>(JSObjectCallAsFunction(*m_engine,
                                                            const_cast<JSObjectRef>(propertyDescriptorGetter),
                                                            /* thisObject */ 0,
                                                            /* argumentCount */ 2,
                                                            arguments,
                                                            &exception));
    if (exception) {
        // Invalid property.
        return QScriptValue::PropertyFlags(flags);
    }

    if (!JSValueIsObject(*m_engine, propertyDescriptor)) {
        // Property isn't owned by this object.
        JSObjectRef proto;
        if (mode == QScriptValue::ResolveLocal
                || ((proto = const_cast<JSObjectRef>(JSObjectGetPrototype(*m_engine, *this))) && JSValueIsNull(*m_engine, proto))) {
            return QScriptValue::PropertyFlags(flags);
        }
        QScriptValuePrivate p(engine(), proto);
        return p.propertyFlags(name, QScriptValue::ResolvePrototype);
    }

    static JSStringRef writableName = QScriptConverter::toString("writable");
    static JSStringRef configurableName = QScriptConverter::toString("configurable");
    static JSStringRef enumerableName = QScriptConverter::toString("enumerable");

    bool readOnly = !JSValueToBoolean(*m_engine, JSObjectGetProperty(*m_engine, propertyDescriptor, writableName, &exception));
    if (!exception && readOnly)
        flags |= QScriptValue::ReadOnly;
    bool undeletable = !JSValueToBoolean(*m_engine, JSObjectGetProperty(*m_engine, propertyDescriptor, configurableName, &exception));
    if (!exception && undeletable)
        flags |= QScriptValue::Undeletable;
    bool skipInEnum = !JSValueToBoolean(*m_engine, JSObjectGetProperty(*m_engine, propertyDescriptor, enumerableName, &exception));
    if (!exception && skipInEnum)
        flags |= QScriptValue::SkipInEnumeration;

    return QScriptValue::PropertyFlags(flags);
}

QScriptValuePrivate* QScriptValuePrivate::call(const QScriptValuePrivate*, const QScriptValueList& args)
{
    switch (m_state) {
    case JSValue:
        if (refinedJSValue() != JSObject)
            return new QScriptValuePrivate;
        // Fall-through.
    case JSObject:
        {
            // Convert all arguments and bind to the engine.
            int argc = args.size();
            QVarLengthArray<JSValueRef, 8> argv(argc);
            QScriptValueList::const_iterator i = args.constBegin();
            for (int j = 0; i != args.constEnd(); j++, i++) {
                QScriptValuePrivate* value = QScriptValuePrivate::get(*i);
                if (!value->assignEngine(engine())) {
                    qWarning("QScriptValue::call() failed: cannot call function with values created in a different engine");
                    return new QScriptValuePrivate;
                }
                argv[j] = *value;
            }

            // Make the call
            JSValueRef exception = 0;
            JSValueRef result = JSObjectCallAsFunction(*m_engine, *this, /* thisObject */ 0, argc, argv.constData(), &exception);
            if (!result && exception) {
                m_engine->setException(exception);
                return new QScriptValuePrivate(engine(), exception);
            }
            if (result && !exception)
                return new QScriptValuePrivate(engine(), result);
        }
        // this QSV is not a function <-- !result && !exception. Fall-through.
    default:
        return new QScriptValuePrivate;
    }
}

QScriptEnginePrivate* QScriptValuePrivate::engine() const
{
    // As long as m_engine is an autoinitializated pointer we can safely return it without
    // checking current state.
    return m_engine.data();
}

QScriptValuePrivate::operator JSValueRef() const
{
    Q_ASSERT(isJSBased());
    Q_ASSERT(u.m_value);
    return u.m_value;
}

QScriptValuePrivate::operator JSObjectRef() const
{
    Q_ASSERT(m_state == JSObject);
    Q_ASSERT(u.m_object);
    return u.m_object;
}

/*!
  \internal
  Refines the state of this QScriptValuePrivate. Returns the new state.
*/
QScriptValuePrivate::State QScriptValuePrivate::refinedJSValue()
{
    Q_ASSERT(m_state == JSValue);
    if (!JSValueIsObject(*m_engine, *this)) {
        m_state = JSPrimitive;
    } else {
        // Difference between JSValueRef and JSObjectRef is only in their type, binarywise it is the same.
        // As m_value and m_object are stored in the u union, it is enough to change the m_state only.
        m_state = JSObject;
    }
    return m_state;
}

/*!
  \internal
  Returns true if QSV have an engine associated.
*/
bool QScriptValuePrivate::isJSBased() const { return m_state >= JSValue; }

/*!
  \internal
  Returns true if current value of QSV is placed in m_number.
*/
bool QScriptValuePrivate::isNumberBased() const { return m_state == CNumber || m_state == CBool; }

/*!
  \internal
  Returns true if current value of QSV is placed in m_string.
*/
bool QScriptValuePrivate::isStringBased() const { return m_state == CString; }

#endif // qscriptvalue_p_h
