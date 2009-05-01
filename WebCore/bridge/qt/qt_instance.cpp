/*
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "ArgList.h"
#include "JSDOMBinding.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "qt_class.h"
#include "qt_runtime.h"
#include "PropertyNameArray.h"
#include "runtime_object.h"
#include "ObjectPrototype.h"
#include "Error.h"

#include <qmetaobject.h>
#include <qdebug.h>
#include <qmetatype.h>
#include <qhash.h>

namespace JSC {
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
    QtRuntimeObjectImp(ExecState*, PassRefPtr<Instance>);
    ~QtRuntimeObjectImp();
    virtual void invalidate();

    static const ClassInfo s_info;

    virtual void mark()
    {
        QtInstance* instance = static_cast<QtInstance*>(getInternalInstance());
        if (instance)
            instance->mark();
        RuntimeObjectImp::mark();
    }

protected:
    void removeFromCache();
        
private:
    virtual const ClassInfo* classInfo() const { return &s_info; }
};

const ClassInfo QtRuntimeObjectImp::s_info = { "QtRuntimeObject", &RuntimeObjectImp::s_info, 0, 0 };

QtRuntimeObjectImp::QtRuntimeObjectImp(ExecState* exec, PassRefPtr<Instance> instance)
    : RuntimeObjectImp(exec, WebCore::getDOMStructure<QtRuntimeObjectImp>(exec), instance)
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
    JSLock lock(false);
    QtInstance* key = cachedObjects.key(this);
    if (key)
        cachedObjects.remove(key);
}

// QtInstance
QtInstance::QtInstance(QObject* o, PassRefPtr<RootObject> rootObject, QScriptEngine::ValueOwnership ownership)
    : Instance(rootObject)
    , m_class(0)
    , m_object(o)
    , m_hashkey(o)
    , m_defaultMethod(0)
    , m_ownership(ownership)
{
}

QtInstance::~QtInstance()
{
    JSLock lock(false);

    cachedObjects.remove(this);
    cachedInstances.remove(m_hashkey);

    // clean up (unprotect from gc) the JSValues we've created
    m_methods.clear();

    foreach(QtField* f, m_fields.values()) {
        delete f;
    }
    m_fields.clear();

    if (m_object) {
        switch (m_ownership) {
        case QScriptEngine::QtOwnership:
            break;
        case QScriptEngine::AutoOwnership:
            if (m_object->parent())
                break;
            // fall through!
        case QScriptEngine::ScriptOwnership:
            delete m_object;
            break;
        }
    }
}

PassRefPtr<QtInstance> QtInstance::getQtInstance(QObject* o, PassRefPtr<RootObject> rootObject, QScriptEngine::ValueOwnership ownership)
{
    JSLock lock(false);

    foreach(QtInstance* instance, cachedInstances.values(o)) {
        if (instance->rootObject() == rootObject)
            return instance;
    }

    RefPtr<QtInstance> ret = QtInstance::create(o, rootObject, ownership);
    cachedInstances.insert(o, ret.get());

    return ret.release();
}

bool QtInstance::getOwnPropertySlot(JSObject* object, ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return object->JSObject::getOwnPropertySlot(exec, propertyName, slot);
}

void QtInstance::put(JSObject* object, ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    object->JSObject::put(exec, propertyName, value, slot);
}

QtInstance* QtInstance::getInstance(JSObject* object)
{
    if (!object)
        return 0;
    if (!object->inherits(&QtRuntimeObjectImp::s_info))
        return 0;
    return static_cast<QtInstance*>(static_cast<RuntimeObjectImp*>(object)->getInternalInstance());
}

Class* QtInstance::getClass() const
{
    if (!m_class)
        m_class = QtClass::classForObject(m_object);
    return m_class;
}

RuntimeObjectImp* QtInstance::createRuntimeObject(ExecState* exec)
{
    JSLock lock(false);
    RuntimeObjectImp* ret = static_cast<RuntimeObjectImp*>(cachedObjects.value(this));
    if (!ret) {
        ret = new (exec) QtRuntimeObjectImp(exec, this);
        cachedObjects.insert(this, ret);
        ret = static_cast<RuntimeObjectImp*>(cachedObjects.value(this));
    }
    return ret;
}

void QtInstance::mark()
{
    if (m_defaultMethod)
        m_defaultMethod->mark();
    foreach(JSObject* val, m_methods.values()) {
        if (val && !val->marked())
            val->mark();
    }
    foreach(JSValue val, m_children.values()) {
        if (val && !val.marked())
            val.mark();
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

JSValue QtInstance::invokeMethod(ExecState*, const MethodList&, const ArgList&)
{
    // Implemented via fallbackMethod & QtRuntimeMetaMethod::callAsFunction
    return jsUndefined();
}


JSValue QtInstance::defaultValue(ExecState* exec, PreferredPrimitiveType hint) const
{
    if (hint == PreferString)
        return stringValue(exec);
    if (hint == PreferNumber)
        return numberValue(exec);
    return valueOf(exec);
}

JSValue QtInstance::stringValue(ExecState* exec) const
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

JSValue QtInstance::numberValue(ExecState* exec) const
{
    return jsNumber(exec, 0);
}

JSValue QtInstance::booleanValue() const
{
    // ECMA 9.2
    return jsBoolean(true);
}

JSValue QtInstance::valueOf(ExecState* exec) const
{
    return stringValue(exec);
}

// In qt_runtime.cpp
JSValue convertQVariantToValue(ExecState*, PassRefPtr<RootObject> root, const QVariant& variant);
QVariant convertValueToQVariant(ExecState*, JSValue, QMetaType::Type hint, int *distance);

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

JSValue QtField::valueFromInstance(ExecState* exec, const Instance* inst) const
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

        JSValue ret = convertQVariantToValue(exec, inst->rootObject(), val);

        // Need to save children so we can mark them
        if (m_type == ChildObject)
            instance->m_children.insert(ret);

        return ret;
    } else {
        QString msg = QString(QLatin1String("cannot access member `%1' of deleted QObject")).arg(QLatin1String(name()));
        return throwError(exec, GeneralError, msg.toLatin1().constData());
    }
}

void QtField::setValueToInstance(ExecState* exec, const Instance* inst, JSValue aValue) const
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
