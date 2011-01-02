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

#ifndef qscriptoriginalglobalobject_p_h
#define qscriptoriginalglobalobject_p_h

#include <JavaScriptCore/JavaScript.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <QtCore/qvector.h>

/*!
    \internal
    This class is a workaround for missing JSC C API functionality. This class keeps all important
    properties of an original (default) global object, so we can use it even if the global object was
    changed.

    FIXME this class is a container for workarounds :-) it should be replaced by proper JSC C API calls.

    The class have to be created on the QScriptEnginePrivate creation time (before any change got applied to
    global object).
*/
class QScriptOriginalGlobalObject {
public:
    inline QScriptOriginalGlobalObject(JSGlobalContextRef context);
    inline ~QScriptOriginalGlobalObject();

    inline bool objectHasOwnProperty(JSObjectRef object, JSStringRef property) const;
    inline QVector<JSStringRef> objectGetOwnPropertyNames(JSObjectRef object) const;

    inline bool isDate(JSValueRef value) const;
    inline bool isArray(JSValueRef value) const;
    inline bool isError(JSValueRef value) const;

    inline JSValueRef functionPrototype() const;
private:
    inline bool isType(JSValueRef value, JSObjectRef constructor, JSValueRef prototype) const;
    inline void initializeMember(JSObjectRef globalObject, JSStringRef prototypeName, const char* type, JSObjectRef& constructor, JSValueRef& prototype);

    // Copy of the global context reference (the same as in QScriptEnginePrivate).
    JSGlobalContextRef m_context;

    // Copy of constructors and prototypes used in isType functions.
    JSObjectRef m_arrayConstructor;
    JSValueRef m_arrayPrototype;
    JSObjectRef m_errorConstructor;
    JSValueRef m_errorPrototype;
    JSObjectRef m_functionConstructor;
    JSValueRef m_functionPrototype;
    JSObjectRef m_dateConstructor;
    JSValueRef m_datePrototype;

    // Reference to standard JS functions that are not exposed by JSC C API.
    JSObjectRef m_hasOwnPropertyFunction;
    JSObjectRef m_getOwnPropertyNamesFunction;
};

QScriptOriginalGlobalObject::QScriptOriginalGlobalObject(JSGlobalContextRef context)
    : m_context(JSGlobalContextRetain(context))
{
    JSObjectRef globalObject = JSContextGetGlobalObject(m_context);
    JSValueRef exception = 0;
    JSRetainPtr<JSStringRef> propertyName;

    propertyName.adopt(JSStringCreateWithUTF8CString("prototype"));
    initializeMember(globalObject, propertyName.get(), "Array", m_arrayConstructor, m_arrayPrototype);
    initializeMember(globalObject, propertyName.get(), "Error", m_errorConstructor, m_errorPrototype);
    initializeMember(globalObject, propertyName.get(), "Function", m_functionConstructor, m_functionPrototype);
    initializeMember(globalObject, propertyName.get(), "Date", m_dateConstructor, m_datePrototype);

    propertyName.adopt(JSStringCreateWithUTF8CString("hasOwnProperty"));
    m_hasOwnPropertyFunction = const_cast<JSObjectRef>(JSObjectGetProperty(m_context, globalObject, propertyName.get(), &exception));
    JSValueProtect(m_context, m_hasOwnPropertyFunction);
    Q_ASSERT(JSValueIsObject(m_context, m_hasOwnPropertyFunction));
    Q_ASSERT(JSObjectIsFunction(m_context, m_hasOwnPropertyFunction));
    Q_ASSERT(!exception);

    propertyName.adopt(JSStringCreateWithUTF8CString("Object"));
    JSObjectRef objectConstructor
            = const_cast<JSObjectRef>(JSObjectGetProperty(m_context, globalObject, propertyName.get(), &exception));
    propertyName.adopt(JSStringCreateWithUTF8CString("getOwnPropertyNames"));
    m_getOwnPropertyNamesFunction
            = const_cast<JSObjectRef>(JSObjectGetProperty(m_context, objectConstructor, propertyName.get(), &exception));
    JSValueProtect(m_context, m_getOwnPropertyNamesFunction);
    Q_ASSERT(JSValueIsObject(m_context, m_getOwnPropertyNamesFunction));
    Q_ASSERT(JSObjectIsFunction(m_context, m_getOwnPropertyNamesFunction));
    Q_ASSERT(!exception);
}

inline void QScriptOriginalGlobalObject::initializeMember(JSObjectRef globalObject, JSStringRef prototypeName, const char* type, JSObjectRef& constructor, JSValueRef& prototype)
{
    JSRetainPtr<JSStringRef> typeName(Adopt, JSStringCreateWithUTF8CString(type));
    JSValueRef exception = 0;

    // Save references to the Type constructor and prototype.
    JSValueRef typeConstructor = JSObjectGetProperty(m_context, globalObject, typeName.get(), &exception);
    Q_ASSERT(JSValueIsObject(m_context, typeConstructor));
    constructor = JSValueToObject(m_context, typeConstructor, &exception);
    JSValueProtect(m_context, constructor);

    // Note that this is not the [[Prototype]] internal property (which we could
    // get via JSObjectGetPrototype), but the Type.prototype, that will be set
    // as [[Prototype]] of Type instances.
    prototype = JSObjectGetProperty(m_context, constructor, prototypeName, &exception);
    Q_ASSERT(JSValueIsObject(m_context, prototype));
    JSValueProtect(m_context, prototype);
    Q_ASSERT(!exception);
}

QScriptOriginalGlobalObject::~QScriptOriginalGlobalObject()
{
    JSValueUnprotect(m_context, m_arrayConstructor);
    JSValueUnprotect(m_context, m_arrayPrototype);
    JSValueUnprotect(m_context, m_errorConstructor);
    JSValueUnprotect(m_context, m_errorPrototype);
    JSValueUnprotect(m_context, m_functionConstructor);
    JSValueUnprotect(m_context, m_functionPrototype);
    JSValueUnprotect(m_context, m_dateConstructor);
    JSValueUnprotect(m_context, m_datePrototype);
    JSValueUnprotect(m_context, m_hasOwnPropertyFunction);
    JSValueUnprotect(m_context, m_getOwnPropertyNamesFunction);
    JSGlobalContextRelease(m_context);
}

inline bool QScriptOriginalGlobalObject::objectHasOwnProperty(JSObjectRef object, JSStringRef property) const
{
    // FIXME This function should be replaced by JSC C API.
    JSValueRef exception = 0;
    JSValueRef propertyName[] = { JSValueMakeString(m_context, property) };
    JSValueRef result = JSObjectCallAsFunction(m_context, m_hasOwnPropertyFunction, object, 1, propertyName, &exception);
    return exception ? false : JSValueToBoolean(m_context, result);
}

/*!
    \internal
    This method gives ownership of all JSStringRefs.
*/
inline QVector<JSStringRef> QScriptOriginalGlobalObject::objectGetOwnPropertyNames(JSObjectRef object) const
{
    JSValueRef exception = 0;
    JSObjectRef propertyNames
            = const_cast<JSObjectRef>(JSObjectCallAsFunction(m_context,
                                                            m_getOwnPropertyNamesFunction,
                                                            /* thisObject */ 0,
                                                            /* argumentCount */ 1,
                                                            &object,
                                                            &exception));
    Q_ASSERT(JSValueIsObject(m_context, propertyNames));
    Q_ASSERT(!exception);
    JSStringRef lengthName = QScriptConverter::toString("length");
    int count = JSValueToNumber(m_context, JSObjectGetProperty(m_context, propertyNames, lengthName, &exception), &exception);

    Q_ASSERT(!exception);
    QVector<JSStringRef> names;
    names.reserve(count);
    for (int i = 0; i < count; ++i) {
        JSValueRef tmp = JSObjectGetPropertyAtIndex(m_context, propertyNames, i, &exception);
        names.append(JSValueToStringCopy(m_context, tmp, &exception));
        Q_ASSERT(!exception);
    }
    return names;
}

inline bool QScriptOriginalGlobalObject::isDate(JSValueRef value) const
{
    return isType(value, m_dateConstructor, m_datePrototype);
}

inline bool QScriptOriginalGlobalObject::isArray(JSValueRef value) const
{
    return isType(value, m_arrayConstructor, m_arrayPrototype);
}

inline bool QScriptOriginalGlobalObject::isError(JSValueRef value) const
{
    return isType(value, m_errorConstructor, m_errorPrototype);
}

inline JSValueRef QScriptOriginalGlobalObject::functionPrototype() const
{
    return m_functionPrototype;
}

inline bool QScriptOriginalGlobalObject::isType(JSValueRef value, JSObjectRef constructor, JSValueRef prototype) const
{
    // JSC API doesn't export the [[Class]] information for the builtins. But we know that a value
    // is an object of the Type if it was created with the Type constructor or if it is the Type.prototype.
    JSValueRef exception = 0;
    bool result = JSValueIsInstanceOfConstructor(m_context, value, constructor, &exception) || JSValueIsStrictEqual(m_context, value, prototype);
    Q_ASSERT(!exception);
    return result;
}

#endif // qscriptoriginalglobalobject_p_h
