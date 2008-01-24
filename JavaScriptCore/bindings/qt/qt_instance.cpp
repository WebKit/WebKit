/*
 * Copyright (C) 2006 Trolltech ASA
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "qt_instance.h"

#include "JSGlobalObject.h"
#include "list.h"
#include "qt_class.h"
#include "qt_runtime.h"
#include "PropertyNameArray.h"
#include "runtime_object.h"
#include "object_object.h"

#include <qmetaobject.h>
#include <qdebug.h>
#include <qmetatype.h>
#include <qhash.h>

namespace KJS {
namespace Bindings {

// Cache QtInstances
typedef QMultiHash<void*, QtInstance*> QObjectInstanceMap;
static QObjectInstanceMap cachedInstances;

// Cache JSObjects
typedef QHash<QtInstance*, JSObject*> InstanceJSObjectMap;
static InstanceJSObjectMap cachedObjects;

// Derived RuntimeObject
class QtRuntimeObjectImp : public RuntimeObjectImp {
    public:
        QtRuntimeObjectImp(Instance*);
        ~QtRuntimeObjectImp();
        virtual void invalidate();

        // Additions
        virtual bool implementsConstruct() const {return implementsCall();}
        virtual JSObject* construct(ExecState* exec, const List& args);
    protected:
        void removeFromCache();
};

QtRuntimeObjectImp::QtRuntimeObjectImp(Instance* instance)
    : RuntimeObjectImp(instance)
{
}

QtRuntimeObjectImp::~QtRuntimeObjectImp()
{
    removeFromCache();
}

void QtRuntimeObjectImp::invalidate()
{
    removeFromCache();
    RuntimeObjectImp::invalidate();
}

void QtRuntimeObjectImp::removeFromCache()
{
    JSLock lock;
    QtInstance* key = cachedObjects.key(this);
    if (key)
        cachedObjects.remove(key);
}

JSObject* QtRuntimeObjectImp::construct(ExecState* exec, const List& args)
{
    // ECMA 15.2.2.1 (?)
    JSValue *val = callAsFunction(exec, this, args);

    if (!val || val->type() == NullType || val->type() == UndefinedType)
        return new JSObject(exec->lexicalGlobalObject()->objectPrototype());
    else
        return val->toObject(exec);
}

// QtInstance
QtInstance::QtInstance(QObject* o, PassRefPtr<RootObject> rootObject)
    : Instance(rootObject)
    , m_class(0)
    , m_object(o)
    , m_hashkey(o)
    , m_defaultMethod(0)
    , m_defaultMethodIndex(-2)
{
}

QtInstance::~QtInstance()
{
    JSLock lock;

    cachedObjects.remove(this);
    cachedInstances.remove(m_hashkey);

    // clean up (unprotect from gc) the JSValues we've created
    foreach(JSValue* val, m_methods.values()) {
        gcUnprotect(val);
    }
    m_methods.clear();

    foreach(QtField* f, m_fields.values()) {
        delete f;
    }
    m_fields.clear();

    if (m_defaultMethod)
        gcUnprotect(m_defaultMethod);
}

QtInstance* QtInstance::getQtInstance(QObject* o, PassRefPtr<RootObject> rootObject)
{
    JSLock lock;

    foreach(QtInstance* instance, cachedInstances.values(o)) {
        if (instance->rootObject() == rootObject)
            return instance;
    }

    QtInstance* ret = new QtInstance(o, rootObject);
    cachedInstances.insert(o, ret);

    return ret;
}

JSObject* QtInstance::getRuntimeObject(QtInstance* instance)
{
    JSLock lock;
    JSObject* ret = cachedObjects.value(instance);
    if (!ret) {
        ret = new QtRuntimeObjectImp(instance);
        cachedObjects.insert(instance, ret);
    }
    return ret;
}

Class* QtInstance::getClass() const
{
    if (!m_class)
        m_class = QtClass::classForObject(m_object);
    return m_class;
}

void QtInstance::begin()
{
    // Do nothing.
}

void QtInstance::end()
{
    // Do nothing.
}

void QtInstance::getPropertyNames(ExecState* , PropertyNameArray& array)
{
    // This is the enumerable properties, so put:
    // properties
    // dynamic properties
    // slots
    QObject* obj = getObject();
    if (obj) {
        const QMetaObject* meta = obj->metaObject();

        int i;
        for (i=0; i < meta->propertyCount(); i++) {
            QMetaProperty prop = meta->property(i);
            if (prop.isScriptable()) {
                array.add(Identifier(prop.name()));
            }
        }

        QList<QByteArray> dynProps = obj->dynamicPropertyNames();
        foreach(QByteArray ba, dynProps) {
            array.add(Identifier(ba.constData()));
        }

        for (i=0; i < meta->methodCount(); i++) {
            QMetaMethod method = meta->method(i);
            if (method.access() != QMetaMethod::Private) {
                array.add(Identifier(method.signature()));
            }
        }
    }
}

JSValue* QtInstance::invokeMethod(ExecState*, const MethodList&, const List&)
{
    // Implemented via fallbackMethod & QtRuntimeMetaMethod::callAsFunction
    return jsUndefined();
}

bool QtInstance::implementsCall() const
{
    // See if we have qscript_call
    if (m_defaultMethodIndex == -2) {
        if (m_object) {
            const QMetaObject* meta = m_object->metaObject();
            int count = meta->methodCount();
            const QByteArray defsig("qscript_call");
            for (int index = count - 1; index >= 0; --index) {
                const QMetaMethod m = meta->method(index);

                QByteArray signature = m.signature();
                signature.truncate(signature.indexOf('('));

                if (defsig == signature) {
                    m_defaultMethodIndex = index;
                    break;
                }
            }
        }

        if (m_defaultMethodIndex == -2) // Not checked
            m_defaultMethodIndex = -1; // No qscript_call
    }

    // typeof object that implements call == function
    return (m_defaultMethodIndex >= 0);
}

JSValue* QtInstance::invokeDefaultMethod(ExecState* exec, const List& args)
{
    // QtScript tries to invoke a meta method qscript_call
    if (!getObject())
        return throwError(exec, GeneralError, "cannot call function of deleted QObject");

    // implementsCall will update our default method cache, if possible
    if (implementsCall()) {
        if (!m_defaultMethod) {
            m_defaultMethod = new QtRuntimeMetaMethod(exec, Identifier("[[Call]]"),this, m_defaultMethodIndex, QByteArray("qscript_call"), true);
            gcProtect(m_defaultMethod);
        }

        return m_defaultMethod->callAsFunction(exec, 0, args); // Luckily QtRuntimeMetaMethod ignores the obj parameter
    } else
        return throwError(exec, TypeError, "not a function");
}

JSValue* QtInstance::defaultValue(JSType hint) const
{
    if (hint == StringType)
        return stringValue();
    if (hint == NumberType)
        return numberValue();
    if (hint == BooleanType)
        return booleanValue();
    return valueOf();
}

JSValue* QtInstance::stringValue() const
{
    // Hmm.. see if there is a toString defined
    QByteArray buf;
    bool useDefault = true;
    getClass();
    QObject* obj = getObject();
    if (m_class && obj) {
        // Cheat and don't use the full name resolution
        int index = obj->metaObject()->indexOfMethod("toString()");
        if (index >= 0) {
            QMetaMethod m = obj->metaObject()->method(index);
            // Check to see how much we can call it
            if (m.access() != QMetaMethod::Private
                && m.methodType() != QMetaMethod::Signal
                && m.parameterTypes().count() == 0) {
                const char* retsig = m.typeName();
                if (retsig && *retsig) {
                    QVariant ret(QMetaType::type(retsig), (void*)0);
                    void * qargs[1];
                    qargs[0] = ret.data();

                    if (obj->qt_metacall(QMetaObject::InvokeMetaMethod, index, qargs) < 0) {
                        if (ret.isValid() && ret.canConvert(QVariant::String)) {
                            buf = ret.toString().toLatin1().constData(); // ### Latin 1? Ascii?
                            useDefault = false;
                        }
                    }
                }
            }
        }
    }

    if (useDefault) {
        const QMetaObject* meta = obj ? obj->metaObject() : &QObject::staticMetaObject;
        QString name = obj ? obj->objectName() : QString::fromUtf8("unnamed");
        QString str = QString::fromUtf8("%0(name = \"%1\")")
                      .arg(QLatin1String(meta->className())).arg(name);

        buf = str.toLatin1();
    }
    return jsString(buf.constData());
}

JSValue* QtInstance::numberValue() const
{
    return jsNumber(0);
}

JSValue* QtInstance::booleanValue() const
{
    // ECMA 9.2
    return jsBoolean(true);
}

JSValue* QtInstance::valueOf() const
{
    return stringValue();
}

// In qt_runtime.cpp
JSValue* convertQVariantToValue(ExecState* exec, PassRefPtr<RootObject> root, const QVariant& variant);
QVariant convertValueToQVariant(ExecState* exec, JSValue* value, QMetaType::Type hint, int *distance);

const char* QtField::name() const
{
    if (m_type == MetaProperty)
        return m_property.name();
    else if (m_type == ChildObject && m_childObject)
        return m_childObject->objectName().toLatin1();
    else if (m_type == DynamicProperty)
        return m_dynamicProperty.constData();
    return ""; // deleted child object
}

JSValue* QtField::valueFromInstance(ExecState* exec, const Instance* inst) const
{
    const QtInstance* instance = static_cast<const QtInstance*>(inst);
    QObject* obj = instance->getObject();

    if (obj) {
        QVariant val;
        if (m_type == MetaProperty) {
            if (m_property.isReadable())
                val = m_property.read(obj);
            else
                return jsUndefined();
        } else if (m_type == ChildObject)
            val = QVariant::fromValue((QObject*) m_childObject);
        else if (m_type == DynamicProperty)
            val = obj->property(m_dynamicProperty);

        return convertQVariantToValue(exec, inst->rootObject(), val);
    } else {
        QString msg = QString("cannot access member `%1' of deleted QObject").arg(name());
        return throwError(exec, GeneralError, msg.toLatin1().constData());
    }
}

void QtField::setValueToInstance(ExecState* exec, const Instance* inst, JSValue* aValue) const
{
    if (m_type == ChildObject) // QtScript doesn't allow setting to a named child
        return;

    const QtInstance* instance = static_cast<const QtInstance*>(inst);
    QObject* obj = instance->getObject();
    if (obj) {
        QMetaType::Type argtype = QMetaType::Void;
        if (m_type == MetaProperty)
            argtype = (QMetaType::Type) QMetaType::type(m_property.typeName());

        // dynamic properties just get any QVariant
        QVariant val = convertValueToQVariant(exec, aValue, argtype, 0);
        if (m_type == MetaProperty) {
            if (m_property.isWritable())
                m_property.write(obj, val);
        } else if (m_type == DynamicProperty)
            obj->setProperty(m_dynamicProperty.constData(), val);
    } else {
        QString msg = QString("cannot access member `%1' of deleted QObject").arg(name());
        throwError(exec, GeneralError, msg.toLatin1().constData());
    }
}


}
}
