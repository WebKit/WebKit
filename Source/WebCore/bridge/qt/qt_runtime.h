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

#ifndef BINDINGS_QT_RUNTIME_H_
#define BINDINGS_QT_RUNTIME_H_

#include "BridgeJSC.h"
#include "JSDOMBinding.h"
#include "JavaScript.h"
#include "Weak.h"
#include "qt_instance.h"
#include "runtime_method.h"

#include <qbytearray.h>
#include <qmetaobject.h>
#include <qpointer.h>
#include <qvariant.h>

namespace JSC {
namespace Bindings {

class QtField : public Field {
public:

    typedef enum {
        MetaProperty,
#ifndef QT_NO_PROPERTIES
        DynamicProperty,
#endif
        ChildObject
    } QtFieldType;

    QtField(const QMetaProperty &p)
        : m_type(MetaProperty), m_property(p)
        {}

#ifndef QT_NO_PROPERTIES
    QtField(const QByteArray &b)
        : m_type(DynamicProperty), m_dynamicProperty(b)
        {}
#endif

    QtField(QObject *child)
        : m_type(ChildObject), m_childObject(child)
        {}

    virtual JSValue valueFromInstance(ExecState*, const Instance*) const;
    virtual void setValueToInstance(ExecState*, const Instance*, JSValue) const;
    QByteArray name() const;
    QtFieldType fieldType() const {return m_type;}
private:
    QtFieldType m_type;
    QByteArray m_dynamicProperty;
    QMetaProperty m_property;
    QPointer<QObject> m_childObject;
};


class QtMethod : public Method
{
public:
    QtMethod(const QMetaObject *mo, int i, const QByteArray &ident, int numParameters)
        : m_metaObject(mo),
          m_index(i),
          m_identifier(ident),
          m_nParams(numParameters)
        { }

    virtual const char* name() const { return m_identifier.constData(); }
    virtual int numParameters() const { return m_nParams; }

private:
    friend class QtInstance;
    const QMetaObject *m_metaObject;
    int m_index;
    QByteArray m_identifier;
    int m_nParams;
};


template <typename T> class QtArray : public Array
{
public:
    QtArray(QList<T> list, QMetaType::Type type, PassRefPtr<RootObject>);
    virtual ~QtArray();

    RootObject* rootObject() const;

    virtual void setValueAt(ExecState*, unsigned index, JSValue) const;
    virtual JSValue valueAt(ExecState*, unsigned index) const;
    virtual unsigned int getLength() const {return m_length;}

private:
    mutable QList<T> m_list; // setValueAt is const!
    unsigned int m_length;
    QMetaType::Type m_type;
};

// Based on RuntimeMethod

// Extra data classes (to avoid the CELL_SIZE limit on JS objects)
class QtRuntimeMethod;
class QtRuntimeMethodData : public WeakHandleOwner {
    public:
        virtual ~QtRuntimeMethodData();
        RefPtr<QtInstance> m_instance;
        Weak<QtRuntimeMethod> m_finalizer;

    private:
        void finalize(Handle<Unknown>, void*);
};

class QtRuntimeConnectionMethod;
class QtRuntimeMetaMethodData : public QtRuntimeMethodData {
    public:
        ~QtRuntimeMetaMethodData();
        QByteArray m_signature;
        bool m_allowPrivate;
        int m_index;
        WriteBarrier<QtRuntimeConnectionMethod> m_connect;
        WriteBarrier<QtRuntimeConnectionMethod> m_disconnect;
};

class QtRuntimeConnectionMethodData : public QtRuntimeMethodData {
    public:
        ~QtRuntimeConnectionMethodData();
        QByteArray m_signature;
        int m_index;
        bool m_isConnect;
};

// Common base class (doesn't really do anything interesting)
class QtRuntimeMethod : public InternalFunction {
public:
    virtual ~QtRuntimeMethod();

    static const ClassInfo s_info;

    static FunctionPrototype* createPrototype(ExecState*, JSGlobalObject* globalObject)
    {
        return globalObject->functionPrototype();
    }

    static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(globalData, globalObject, prototype, TypeInfo(ObjectType,  StructureFlags), &s_info);
    }

protected:
    void finishCreation(ExecState*, const Identifier&, PassRefPtr<QtInstance>);
    static const unsigned StructureFlags = OverridesGetOwnPropertySlot | OverridesGetPropertyNames | InternalFunction::StructureFlags | OverridesVisitChildren;

    QtRuntimeMethodData *d_func() const {return d_ptr;}
    QtRuntimeMethod(QtRuntimeMethodData *dd, ExecState *, Structure*, const Identifier &name, PassRefPtr<QtInstance>);
    QtRuntimeMethodData *d_ptr;
};

class QtRuntimeMetaMethod : public QtRuntimeMethod {
public:
    typedef QtRuntimeMethod Base;

    static QtRuntimeMetaMethod* create(ExecState* exec, const Identifier& name, PassRefPtr<QtInstance> instance, int index, const QByteArray& signature, bool allowPrivate)
    {
        Structure* domStructure = WebCore::deprecatedGetDOMStructure<QtRuntimeMetaMethod>(exec);
        QtRuntimeMetaMethod* method = new (allocateCell<QtRuntimeMetaMethod>(*exec->heap())) QtRuntimeMetaMethod(exec, domStructure, name);
        method->finishCreation(exec, name, instance, index, signature, allowPrivate);
        return method;
    }

    static bool getOwnPropertySlot(JSCell*, ExecState *, const Identifier&, PropertySlot&);
    static bool getOwnPropertyDescriptor(JSObject*, ExecState*, const Identifier&, PropertyDescriptor&);
    static void getOwnPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);

    static void visitChildren(JSCell*, SlotVisitor&);
    
    static const ClassInfo s_info;

    static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(globalData, globalObject, prototype, TypeInfo(ObjectType,  StructureFlags), &s_info);
    }

protected:
    QtRuntimeMetaMethodData* d_func() const {return reinterpret_cast<QtRuntimeMetaMethodData*>(d_ptr);}

private:
    QtRuntimeMetaMethod(ExecState*, Structure*, const Identifier&);
    void finishCreation(ExecState*, const Identifier&, PassRefPtr<QtInstance>, int index, const QByteArray& signature, bool allowPrivate);

    static CallType getCallData(JSCell*, CallData&);
    static EncodedJSValue JSC_HOST_CALL call(ExecState* exec);
    static JSValue lengthGetter(ExecState*, JSValue, const Identifier&);
    static JSValue connectGetter(ExecState*, JSValue, const Identifier&);
    static JSValue disconnectGetter(ExecState*, JSValue, const Identifier&);
};

class QtConnectionObject;
class QtRuntimeConnectionMethod : public QtRuntimeMethod
{
public:
    typedef QtRuntimeMethod Base;

    static QtRuntimeConnectionMethod* create(ExecState* exec, const Identifier& name, bool isConnect, PassRefPtr<QtInstance> instance, int index, const QByteArray& signature)
    {
        Structure* domStructure = WebCore::deprecatedGetDOMStructure<QtRuntimeConnectionMethod>(exec);
        QtRuntimeConnectionMethod* method = new (allocateCell<QtRuntimeConnectionMethod>(*exec->heap())) QtRuntimeConnectionMethod(exec, domStructure, name);
        method->finishCreation(exec, name, isConnect, instance, index, signature);
        return method;
    }

    static bool getOwnPropertySlot(JSCell*, ExecState *, const Identifier&, PropertySlot&);
    static bool getOwnPropertyDescriptor(JSObject*, ExecState*, const Identifier&, PropertyDescriptor&);
    static void getOwnPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
 
    static const ClassInfo s_info;

    static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(globalData, globalObject, prototype, TypeInfo(ObjectType,  StructureFlags), &s_info);
    }

protected:
    QtRuntimeConnectionMethodData* d_func() const {return reinterpret_cast<QtRuntimeConnectionMethodData*>(d_ptr);}

private:
    QtRuntimeConnectionMethod(ExecState*, Structure*, const Identifier&);
    void finishCreation(ExecState*, const Identifier&, bool isConnect, PassRefPtr<QtInstance>, int index, const QByteArray& signature);

    static CallType getCallData(JSCell*, CallData&);
    static EncodedJSValue JSC_HOST_CALL call(ExecState* exec);
    static JSValue lengthGetter(ExecState*, JSValue, const Identifier&);
    static QMultiMap<QObject *, QtConnectionObject *> connections;
    friend class QtConnectionObject;
};

class QtConnectionObject: public QObject
{
public:
    QtConnectionObject(JSGlobalData&, PassRefPtr<QtInstance> instance, int signalIndex, JSObject* thisObject, JSObject* funcObject);
    ~QtConnectionObject();

    static const QMetaObject staticMetaObject;
    virtual const QMetaObject *metaObject() const;
    virtual void *qt_metacast(const char *);
    virtual int qt_metacall(QMetaObject::Call, int, void **argv);

    bool match(QObject *sender, int signalIndex, JSObject* thisObject, JSObject *funcObject);

    // actual slot:
    void execute(void **argv);

private:
    RefPtr<QtInstance> m_instance;
    int m_signalIndex;
    QObject* m_originalObject; // only used as a key, not dereferenced
    Strong<JSObject> m_thisObject;
    Strong<JSObject> m_funcObject;
};

QVariant convertValueToQVariant(ExecState* exec, JSValue value, QMetaType::Type hint, int *distance);
JSValue convertQVariantToValue(ExecState* exec, PassRefPtr<RootObject> root, const QVariant& variant);

} // namespace Bindings
} // namespace JSC

#endif
