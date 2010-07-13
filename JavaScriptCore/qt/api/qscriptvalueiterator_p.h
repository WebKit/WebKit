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

#ifndef qscriptvalueiterator_p_h
#define qscriptvalueiterator_p_h

#include "qscriptvalue_p.h"
#include <JavaScriptCore/JavaScript.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qvector.h>

class QScriptValueIteratorPrivate: public QSharedData {
public:
    inline QScriptValueIteratorPrivate(const QScriptValuePrivate* value);
    inline ~QScriptValueIteratorPrivate();

    inline bool hasNext();
    inline void next();

    inline bool hasPrevious();
    inline void previous();

    inline QString name() const;
    inline QScriptStringPrivate* scriptName() const;

    inline QScriptValuePrivate* value() const;
    inline void setValue(const QScriptValuePrivate* value);

    inline void remove();

    inline void toFront();
    inline void toBack();

    QScriptValue::PropertyFlags flags() const;

    inline bool isValid() const;
private:
    inline QScriptEnginePrivate* engine() const;

    QExplicitlySharedDataPointer<QScriptValuePrivate> m_object;
    QVector<JSStringRef> m_names;
    QMutableVectorIterator<JSStringRef> m_idx;
};

inline QScriptValueIteratorPrivate::QScriptValueIteratorPrivate(const QScriptValuePrivate* value)
    : m_object(const_cast<QScriptValuePrivate*>(value))
    , m_idx(m_names)
{
    // FIXME There is assumption that global object wasn't changed (bug 41839).
    // FIXME We can't use C API function JSObjectGetPropertyNames as it returns only enumerable properties.
    if (const_cast<QScriptValuePrivate*>(value)->isObject()) {
        static JSStringRef objectName = QScriptConverter::toString("Object");
        static JSStringRef getOwnPropertyNamesName = QScriptConverter::toString("getOwnPropertyNames");

        JSValueRef exception = 0;
        JSObjectRef globalObject = JSContextGetGlobalObject(*engine());
        Q_ASSERT(JSValueIsObject(*engine(), globalObject));
        JSValueRef objectConstructor = JSObjectGetProperty(*engine(), globalObject, objectName, &exception);
        Q_ASSERT(JSValueIsObject(*engine(), objectConstructor));
        Q_ASSERT(!exception);
        JSValueRef propertyNamesGetter = JSObjectGetProperty(*engine(), const_cast<JSObjectRef>(objectConstructor), getOwnPropertyNamesName, &exception);
        Q_ASSERT(JSValueIsObject(*engine(), propertyNamesGetter));
        Q_ASSERT(!exception);

        JSValueRef arguments[] = { *m_object };
        JSObjectRef propertyNames
                = const_cast<JSObjectRef>(JSObjectCallAsFunction(*engine(),
                                                                const_cast<JSObjectRef>(propertyNamesGetter),
                                                                /* thisObject */ 0,
                                                                /* argumentCount */ 1,
                                                                arguments,
                                                                &exception));
        Q_ASSERT(JSValueIsObject(*engine(), propertyNames));
        Q_ASSERT(!exception);
        static JSStringRef lengthName = QScriptConverter::toString("length");
        int count = JSValueToNumber(*engine(), JSObjectGetProperty(*engine(), propertyNames, lengthName, &exception), &exception);

        Q_ASSERT(!exception);
        m_names.reserve(count);
        for (int i = 0; i < count; ++i) {
            JSValueRef tmp = JSObjectGetPropertyAtIndex(*engine(), propertyNames, i, &exception);
            m_names.append(JSValueToStringCopy(*engine(), tmp, &exception));
            Q_ASSERT(!exception);
        }
        m_idx = m_names;
    } else
        m_object = 0;
}

inline QScriptValueIteratorPrivate::~QScriptValueIteratorPrivate()
{
    QVector<JSStringRef>::const_iterator i = m_names.constBegin();
    for (; i != m_names.constEnd(); ++i)
        JSStringRelease(*i);
}

inline bool QScriptValueIteratorPrivate::hasNext()
{
    return m_idx.hasNext();
}

inline void QScriptValueIteratorPrivate::next()
{
    // FIXME (Qt5) This method should return a value (QTBUG-11226).
    m_idx.next();
}

inline bool QScriptValueIteratorPrivate::hasPrevious()
{
    return m_idx.hasPrevious();
}

inline void QScriptValueIteratorPrivate::previous()
{
    m_idx.previous();
}

inline QString QScriptValueIteratorPrivate::name() const
{
    if (!isValid())
        return QString();
    return QScriptConverter::toString(m_idx.value());
}

inline QScriptStringPrivate* QScriptValueIteratorPrivate::scriptName() const
{
    if (!isValid())
        return new QScriptStringPrivate();
    return new QScriptStringPrivate(QScriptConverter::toString(m_idx.value()));
}

inline QScriptValuePrivate* QScriptValueIteratorPrivate::value() const
{
    if (!isValid())
        return new QScriptValuePrivate();
    JSValueRef exception = 0;
    JSValueRef value = m_object->property(m_idx.value(), &exception);
    engine()->setException(exception);
    return new QScriptValuePrivate(engine(), value);
}

inline void QScriptValueIteratorPrivate::setValue(const QScriptValuePrivate* value)
{
    if (!isValid())
        return;
    JSValueRef exception = 0;
    m_object->setProperty(m_idx.value(), *value, /* flags */ 0, &exception);
    engine()->setException(exception);
}

inline void QScriptValueIteratorPrivate::remove()
{
    if (!isValid())
        return;
    JSValueRef exception = 0;
    m_object->deleteProperty(m_idx.value(), &exception);
    engine()->setException(exception);
    m_idx.remove();
}

inline void QScriptValueIteratorPrivate::toFront()
{
    m_idx.toFront();
}

inline void QScriptValueIteratorPrivate::toBack()
{
    m_idx.toBack();
}

QScriptValue::PropertyFlags QScriptValueIteratorPrivate::flags() const
{
    if (!isValid())
        return QScriptValue::PropertyFlags(0);
    return m_object->propertyFlags(m_idx.value(), QScriptValue::ResolveLocal);
}

inline bool QScriptValueIteratorPrivate::isValid() const
{
    return m_object;
}

inline QScriptEnginePrivate* QScriptValueIteratorPrivate::engine() const
{
    Q_ASSERT(isValid());
    return m_object->engine();
}

#endif // qscriptvalueiterator_p_h
