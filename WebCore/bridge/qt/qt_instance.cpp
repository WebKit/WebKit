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
        QtRuntimeObjectImp(PassRefPtr<Instance>);
        ~QtRuntimeObjectImp();
        virtual void invalidate();

        virtual void mark() {
            QtInstance* instance = static_cast<QtInstance*>(getInternalInstance());
            if (instance)
                instance->mark();
            RuntimeObjectImp::mark();
        }

        // Additions
        virtual ConstructType getConstructData(ConstructData&);
        virtual JSObject* construct(ExecState* exec, const ArgList& args);
    protected:
        void removeFromCache();
};

QtRuntimeObjectImp::QtRuntimeObjectImp(PassRefPtr<Instance> instance)
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

ConstructType QtRuntimeObjectImp::getConstructData(ConstructData& constructData)
{
    CallData callData;
    ConstructType type = ConstructTypeNone;
    switch (getCallData(callData)) {
    case CallTypeNone:
        break;
    case CallTypeNative:
        type = ConstructTypeNative;
        break;
    case CallTypeJS:
        type = ConstructTypeJS;
        constructData.js.functionBody = callData.js.functionBody;
        constructData.js.scopeChain = callData.js.scopeChain;
        break;
    }
    return type;
}

JSObject* QtRuntimeObjectImp::construct(ExecState* exec, const ArgList& args)
{
    // ECMA 15.2.2.1 (?)
    JSValue *val = callAsFunction(exec, this, args);

    if (!val || val->type() == NullType || val->type() == UndefinedType)
        return new (exec) JSObject(exec->lexicalGlobalObject()->objectPrototype());
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
    m_methods.clear();

    foreach(QtField* f, m_fields.values()) {
        delete f;
    }
    m_fields.clear();
}

PassRefPtr<QtInstance> QtInstance::getQtInstance(QObject* o, PassRefPtr<RootObject> rootObject)
{
    JSLock lock;

    foreach(QtInstance* instance, cachedInstances.values(o)) {
        if (instance->rootObject() == rootObject)
            return instance;
    }

    RefPtr<QtInstance> ret = adoptRef(new QtInstance(o, rootObject));
    cachedInstances.insert(o, ret.get());

    return ret.release();
}

JSObject* QtInstance::getRuntimeObject(ExecState* exec, PassRefPtr<QtInstance> instance)
{
    JSLock lock;
    JSObject* ret = cachedObjects.value(instance.get());
    if (!ret) {
        ret = new (exec) QtRuntimeObjectImp(instance);
        cachedObjects.insert(instance.get(), ret);
    }
    return ret;
}

Class* QtInstance::getClass() const
{
    if (!m_class)
        m_class = QtClass::classForObject(m_object);
    return m_class;
}

void QtInstance::mark()
{
    if (m_defaultMethod)
        m_defaultMethod->mark();
    foreach(JSValue* val, m_methods.values()) {
        if (val && !val->marked())
            val->mark();
    }
    foreach(JSValue* val, m_children.values()) {
        if (val && !val->marked())
            val->mark();
    }
}

void QtInstance::begin()
{
    // Do nothing.
}

void QtInstance::end()
{
    // Do nothing.
}

void QtInstance::getPropertyNames(ExecState* exec, PropertyNameArray& array)
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
                array.add(Identifier(exec, prop.name()));
            }
        }

        QList<QByteArray> dynProps = obj->dynamicPropertyNames();
        foreach(QByteArray ba, dynProps) {
            array.add(Identifier(exec, ba.constData()));
        }

        for (i=0; i < meta->methodCount(); i++) {
            QMetaMethod method = meta->method(i);
            if (method.access() != QMetaMethod::Private) {
                array.add(Identifier(exec, method.signature()));
            }
        }
    }
}

JSValue* QtInstance::invokeMethod(ExecState*, const MethodList&, const ArgList&)
{
    // Implemented via fallbackMethod & QtRuntimeMetaMethod::callAsFunction
    return jsUndefined();
}

CallType QtInstance::getCallData(CallData&)
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
    return (m_defaultMethodIndex >= 0 ? CallTypeNative : CallTypeNone);
}

JSValue* QtInstance::invokeDefaultMethod(ExecState* exec, const ArgList& args)
{
    // QtScript tries to invoke a meta method qscript_call
    if (!getObject())
        return throwError(exec, GeneralError, "cannot call function of deleted QObject");

    // implementsCall will update our default method cache, if possible
    CallData d;
    if (getCallData(d) != CallTypeNone) {
        if (!m_defaultMethod)
            m_defaultMethod = new (exec) QtRuntimeMetaMethod(exec, Identifier(exec, "[[Call]]"),this, m_defaultMethodIndex, QByteArray("qscript_call"), true);

        return m_defaultMethod->callAsFunction(exec, 0, args); // Luckily QtRuntimeMetaMethod ignores the obj parameter
    } else
        return throwError(exec, TypeError, "not a function");
}

JSValue* QtInstance::defaultValue(ExecState* exec, JSType hint) const
{
    if (hint == StringType)
        return stringValue(exec);
    if (hint == NumberType)
        return numberValue(exec);
    if (hint == BooleanType)
        return booleanValue();
    return valueOf(exec);
}

JSValue* QtInstance::stringValue(ExecState* exec) const
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
    return jsString(exec, buf.constData());
}

JSValue* QtInstance::numberValue(ExecState* exec) const
{
    return jsNumber(exec, 0);
}

JSValue* QtInstance::booleanValue() const
{
    // ECMA 9.2
    return jsBoolean(true);
}

JSValue* QtInstance::valueOf(ExecState* exec) const
{
    return stringValue(exec);
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

        JSValue* ret = convertQVariantToValue(exec, inst->rootObject(), val);

        // Need to save children so we can mark them
        if (m_type == ChildObject)
            instance->m_children.insert(ret);

        return ret;
    } else {
        QString msg = QString(QLatin1String("cannot access member `%1' of deleted QObject")).arg(QLatin1String(name()));
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
        QString msg = QString(QLatin1String("cannot access member `%1' of deleted QObject")).arg(QLatin1String(name()));
        throwError(exec, GeneralError, msg.toLatin1().constData());
    }
}


}
}
